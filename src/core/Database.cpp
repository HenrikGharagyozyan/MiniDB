#include "minidb/core/Database.h"
#include "minidb/storage/Page.h"
#include "minidb/concurrency/LogRecord.h"
#include "minidb/concurrency/TransactionManager.h"
#include "minidb/index/TupleMeta.h"

#include <utility>
#include <cstring>


namespace minidb 
{

    // Special marker for deleted keys
    constexpr const char* TOMBSTONE = "__DELETED__";


    Database::Database(std::string filename) 
        : filename_(std::move(filename)) 
    {
        pager_ = std::make_unique<Pager>(filename_);

        // Create a 10-page pool in memory
        bpm_ = std::make_unique<BufferPoolManager>(10, *pager_);

        // The WAL file is named after the database plus ".log"
        wal_ = std::make_unique<Wal>(filename_ + ".log");

        PageId root_page_id = 0;

        // If the file is new/empty, initialize the Meta Page and the first root page
        if (pager_->num_pages() == 0) 
        {
            PageId meta_page_id;
            // allocate_page() is hidden inside new_page()
            PageData* meta_raw = bpm_->new_page(&meta_page_id); 
            
            PageData* root_raw = bpm_->new_page(&root_page_id); 
            
            Page root_page(*root_raw);
            root_page.init(NodeType::LEAF, true);
            
            // Write the root ID into page zero
            std::memcpy(meta_raw->data(), &root_page_id, sizeof(PageId));

            // Unpin the pages (is_dirty = true because we modified them)
            bpm_->unpin_page(root_page_id, true);
            bpm_->unpin_page(meta_page_id, true);
        }
        else 
        {
            // The database already exists. Read the Meta Page (Page 0) to find the root
            // Read the meta page through the BPM
            PageData* meta_raw = bpm_->fetch_page(0);
            std::memcpy(&root_page_id, meta_raw->data(), sizeof(PageId));
            
            // Only read, no changes were made
            bpm_->unpin_page(0, false);
        }

        // Initialize the B-tree with the correct root
        btree_ = std::make_unique<BTree>(*bpm_, root_page_id);

        // --- Crash Recovery Stage ---
        auto uncommitted_records = wal_->recover();
        if (!uncommitted_records.empty())
        {
            for (const auto& record : uncommitted_records)
            {
                if (record.type == LogRecordType::SET) 
                {
                    TupleMeta meta(0, 0, false);
                    btree_->insert(record.key, encode_value(meta, record.value));
                } 
                else if (record.type == LogRecordType::DELETE) 
                {
                    TupleMeta meta(0, 0, true);
                    btree_->insert(record.key, encode_value(meta, TOMBSTONE));
                }
            }
            // After successfully applying all records to pages, clear the WAL
            wal_->clear();
        }
    }

    void Database::set(const std::string& key, const std::string& value, Transaction* txn)
    {
        // 2PL: Acquire Exclusive lock (X-Lock)
        if (txn != nullptr && lock_mgr_ != nullptr) 
        {
            if (!txn->holds_exclusive_lock(key)) 
            {
                if (!lock_mgr_->lock_exclusive(key)) 
                {
                    throw std::runtime_error("Lock wait timeout exceeded");
                }
                txn->add_exclusive_lock(key);
            }
        }

        lsn_t undo_lsn = 0;

        if (txn != nullptr && txn_mgr_ != nullptr) 
        {
            // Get the latest physical version to link the new one to
            auto raw_opt = btree_->get(key);
            if (raw_opt) 
            {
                auto [old_meta, old_val] = decode_value(*raw_opt);
                
                // Generate an LSN and save the old version to the log
                undo_lsn = txn_mgr_->get_next_lsn();
                auto log_record = std::make_shared<LogRecord>(
                    txn->get_transaction_id(), undo_lsn, UndoLogType::UPDATE, key, old_val, old_meta
                );
                
                txn->add_log_record(log_record);           // For ROLLBACK
                txn_mgr_->add_undo_record(log_record);     // For MVCC version chain
            }
            else 
            {
                // No previous version yet (INSERT)
                txn->add_log_record(std::make_shared<LogRecord>(
                    txn->get_transaction_id(), 0, UndoLogType::INSERT, key
                ));
            }
        }

        // Create metadata for the NEW version
        TupleMeta new_meta;
        if (txn != nullptr) 
        {
            new_meta.txn_id = txn->get_transaction_id();
            new_meta.undo_lsn = undo_lsn; // REFERENCE TO THE PREVIOUS VERSION
            new_meta.is_deleted = false;
        }

        // Encode and store!
        std::string encoded_value = encode_value(new_meta, value);

        // First write strictly to the WAL and flush to disk
        wal_->append_set(key, value);
        wal_->flush();

        // Only after that modify the BTree
        btree_->insert(key, encoded_value);
    }

    std::optional<std::string> Database::get(const std::string& key, Transaction* txn) 
    {
        // In MVCC, readers do not acquire S-locks anymore.
        // MVCC motto in action: Readers do not block writers.

        // Get the most recent physical version from the B-Tree
        auto raw_opt = btree_->get(key);
        if (!raw_opt) 
        {
            return std::nullopt; // Key does not exist in the database
        }

        auto [meta, val] = decode_value(*raw_opt);

        // If there is no transaction (system call), return the value as is
        if (txn == nullptr) 
        {
            if (meta.is_deleted || val == TOMBSTONE) return std::nullopt;
            return val;
        }

        // --- Visibility Engine: traverse the Version Chain ---
        auto current_meta = meta;
        auto current_val = val;
        
        // While the current version is NOT visible to us
        while (!txn->get_read_view().is_visible(current_meta.txn_id)) 
        {
            if (current_meta.undo_lsn == 0) 
            {
                // No older versions remain, so the key does not exist for our transaction
                return std::nullopt;
            }

            // Retrieve the previous version from the global Undo Log
            auto undo_record = txn_mgr_->get_undo_record(current_meta.undo_lsn);
            if (!undo_record) 
            {
                return std::nullopt; // The log was cleared by the Vacuum
            }

            current_meta = undo_record->get_old_meta();
            current_val = undo_record->get_old_value();
        }

        // Found a visible version
        if (current_meta.is_deleted || current_val == TOMBSTONE) 
        {
            return std::nullopt;
        }
        
        return current_val;
    }

    void Database::remove(const std::string& key, Transaction* txn) 
    {
        // 2PL: Acquire Exclusive lock (X-Lock)
        if (txn != nullptr && lock_mgr_ != nullptr) 
        {
            if (!txn->holds_exclusive_lock(key)) 
            {
                if (txn->holds_shared_lock(key)) 
                {
                    lock_mgr_->unlock_shared(key);
                    txn->remove_shared_lock(key);
                }
                
                if (!lock_mgr_->lock_exclusive(key)) 
                {
                    throw std::runtime_error("Lock wait timeout exceeded");
                }
                txn->add_exclusive_lock(key);
            }
        }

        lsn_t undo_lsn = 0;

        if (txn != nullptr && txn_mgr_ != nullptr) 
        {
            auto raw_opt = btree_->get(key);
            if (raw_opt) 
            {
                auto [old_meta, old_val] = decode_value(*raw_opt);
                
                undo_lsn = txn_mgr_->get_next_lsn();
                auto log_record = std::make_shared<LogRecord>(
                    txn->get_transaction_id(), undo_lsn, UndoLogType::DELETE, key, old_val, old_meta
                );
                
                txn->add_log_record(log_record);
                txn_mgr_->add_undo_record(log_record);
            }
        }

        // Form metadata for the deleted record (Tombstone)
        TupleMeta new_meta;
        new_meta.is_deleted = true; // <-- deletion flag, also for system deletes (txn == nullptr)
        if (txn != nullptr)
        {
            new_meta.txn_id = txn->get_transaction_id();
            new_meta.undo_lsn = undo_lsn;
        }

        std::string encoded_value = encode_value(new_meta, TOMBSTONE);

        wal_->append_delete(key);
        wal_->flush();

        btree_->insert(key, encoded_value);
    }

    std::vector<std::pair<std::string, std::string>> Database::scan(Transaction* txn) 
    {
        std::vector<std::pair<std::string, std::string>> result;
        
        // Get an iterator pointing to the very first record in the B+ tree
        auto it = btree_->begin();

        // Manage the temporary transaction through a smart pointer
        std::shared_ptr<Transaction> temp_txn = nullptr;
        Transaction* effective_txn = txn;

        if (effective_txn == nullptr && txn_mgr_ != nullptr)
        {
            temp_txn = txn_mgr_->begin();
            effective_txn = temp_txn.get();
        }

        while (!it.is_end()) 
        {
            std::string key = it.get_key();
            std::string raw_val = it.get_value();
            
            // Advance the iterator to the next record while the current page remains pinned
            it.advance();

            // If key or value are empty (e.g., deleted or corrupted cells), skip
            if (key.empty() || raw_val.empty()) 
            {
                continue;
            }

            auto [meta, val] = decode_value(raw_val);

            if (effective_txn == nullptr) 
            {
                if (!meta.is_deleted && val != TOMBSTONE) 
                {
                    result.push_back({key, val});
                }
                continue;
            }

            // --- Visibility Engine (MVCC): visibility check for the current transaction ---
            auto current_meta = meta;
            auto current_val = val;
            bool is_visible = true;
            
            while (!effective_txn->get_read_view().is_visible(current_meta.txn_id)) 
            {
                if (current_meta.undo_lsn == 0) 
                {
                    is_visible = false;
                    break;
                }

                auto undo_record = txn_mgr_->get_undo_record(current_meta.undo_lsn);
                if (!undo_record) 
                {
                    is_visible = false;
                    break;
                }

                current_meta = undo_record->get_old_meta();
                current_val = undo_record->get_old_value();
            }

            if (is_visible && !current_meta.is_deleted && current_val != TOMBSTONE) 
            {
                result.push_back({key, current_val});
            }
        }

        // Close the temporary transaction so it does not leak
        if (temp_txn != nullptr && txn_mgr_ != nullptr)
        {
            txn_mgr_->commit(temp_txn.get());
        }

        return result;
    }

    std::string Database::encode_value(const TupleMeta& meta, const std::string& val) 
    {
        std::string res;
        // Reserve memory for the structure size + string size
        res.resize(sizeof(TupleMeta) + val.size());
        
        // Copy the bytes of the structure to the beginning
        std::memcpy(res.data(), &meta, sizeof(TupleMeta));
        // Copy the string itself after the structure
        std::memcpy(res.data() + sizeof(TupleMeta), val.data(), val.size());
        
        return res;
    }

    std::pair<TupleMeta, std::string> Database::decode_value(const std::string& raw_val) 
    {
        TupleMeta meta;
        // Read the structure from the beginning of the string
        std::memcpy(&meta, raw_val.data(), sizeof(TupleMeta));
        
        // Trim off the rest - that's the actual value
        std::string val = raw_val.substr(sizeof(TupleMeta));
        
        return {meta, val};
    }

} // namespace minidb   