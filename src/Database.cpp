#include "minidb/Database.h"
#include "minidb/Page.h"
#include "minidb/LogRecord.h"

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
                    btree_->insert(record.key, record.value);
                } 
                else if (record.type == LogRecordType::DELETE) 
                {
                    btree_->insert(record.key, TOMBSTONE);
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
                // Lock Upgrade: if we already read this key, release S-Lock before acquiring X-Lock
                if (txn->holds_shared_lock(key)) 
                {
                    lock_mgr_->unlock_shared(key);
                    txn->remove_shared_lock(key);
                }
                
                // Проверяем результат!
                if (!lock_mgr_->lock_exclusive(key)) 
                {
                    throw std::runtime_error("Lock wait timeout exceeded");
                }
                txn->add_exclusive_lock(key);
            }
        }

        // Write the Undo Log (call get WITHOUT a transaction to avoid double locking!)
        if (txn != nullptr) 
        {
            auto old_value = get(key); 
            if (old_value)  
            {
                txn->add_log_record(std::make_shared<LogRecord>(
                    txn->get_transaction_id(), UndoLogType::UPDATE, key, *old_value));
            } 
            else 
            {
                txn->add_log_record(std::make_shared<LogRecord>(
                    txn->get_transaction_id(), UndoLogType::INSERT, key));
            }
        }

        // First write strictly to the WAL and flush to disk
        wal_->append_set(key, value);
        wal_->flush();

        // Only after that modify the BTree
        btree_->insert(key, value);
    }

    std::optional<std::string> Database::get(const std::string& key, Transaction* txn) 
    {
        // 2PL: Acquire Shared lock (S-Lock)
        if (txn != nullptr && lock_mgr_ != nullptr) 
        {
            // If we already hold X-Lock or S-Lock, we don't need to acquire another
            if (!txn->holds_shared_lock(key) && !txn->holds_exclusive_lock(key)) 
            {
                // Check the result!
                if (!lock_mgr_->lock_shared(key)) 
                {
                    throw std::runtime_error("Lock wait timeout exceeded");
                }
                txn->add_shared_lock(key);
            }
        }

        // Use our new fast O(log N) search via BTree!
        auto val_opt = btree_->get(key);
        
        if (val_opt) 
        {
            if (*val_opt == TOMBSTONE) 
            {
                return std::nullopt; // Key was deleted
            }
            return *val_opt; // Found the latest value
        }
        
        return std::nullopt;
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
                
                // Проверяем результат!
                if (!lock_mgr_->lock_exclusive(key)) 
                {
                    throw std::runtime_error("Lock wait timeout exceeded");
                }
                txn->add_exclusive_lock(key);
            }
        }

        // Write the undo log before deletion
        if (txn != nullptr) 
        {
            auto old_value = get(key);
            if (old_value) 
            {
                // Record the deleted value so rollback can restore it
                txn->add_log_record(std::make_shared<LogRecord>(
                    txn->get_transaction_id(), UndoLogType::DELETE, key, *old_value));
            }
        }

        // Write deletion to the WAL
        wal_->append_delete(key);
        wal_->flush();

        // Write a tombstone to the BTree
        btree_->insert(key, TOMBSTONE);
    }

} // namespace minidb   