#include "minidb/Database.h"
#include "minidb/Page.h"
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
        // The WAL file is named after the database plus ".log"
        wal_ = std::make_unique<Wal>(filename_ + ".log");

        PageId root_page_id = 0;

        // If the file is new/empty, initialize the Meta Page and the first root page
        if (pager_->num_pages() == 0) 
        {
            PageId meta_page_id = pager_->allocate_page(); // This will be Page 0 (Meta Page)
            root_page_id = pager_->allocate_page();        // This will be Page 1 (actual root)
            
            // Initialize the actual root (Page 1)
            PageData root_raw{};
            Page root_page(root_raw);
            root_page.init(NodeType::LEAF, true);
            pager_->write_page(root_page_id, root_raw);

            // Write the root page ID into the Meta Page (Page 0)
            PageData meta_raw{};
            std::memcpy(meta_raw.data(), &root_page_id, sizeof(PageId));
            pager_->write_page(meta_page_id, meta_raw);
        }
        else 
        {
            // The database already exists. Read the Meta Page (Page 0) to find the root
            PageData meta_raw{};
            pager_->read_page(0, meta_raw);
            std::memcpy(&root_page_id, meta_raw.data(), sizeof(PageId));
        }

        // Initialize the B-tree with the correct root
        btree_ = std::make_unique<BTree>(*pager_, root_page_id);

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

    void Database::set(const std::string& key, const std::string& value)
    {
        // First write strictly to the WAL and flush to disk
        wal_->append_set(key, value);
        wal_->flush();

        // Only after that modify the BTree
        btree_->insert(key, value);
    }

    std::optional<std::string> Database::get(const std::string& key) 
    {
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

    void Database::remove(const std::string& key) 
    {
        // Write deletion to the WAL
        wal_->append_delete(key);
        wal_->flush();

        // Write a tombstone to the BTree
        btree_->insert(key, TOMBSTONE);
    }

} // namespace minidb   