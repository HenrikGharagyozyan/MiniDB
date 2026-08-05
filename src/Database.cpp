#include "minidb/Database.h"
#include "minidb/Page.h"
#include <utility>


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

        // If the file is new/empty, initialize the first page (root)
        if (pager_->num_pages() == 0) 
        {
            PageId root_page_id = pager_->allocate_page();
            PageData raw_page{};
            Page page(raw_page);
            page.init();
            pager_->write_page(root_page_id, raw_page);
        }

        // --- Crash Recovery Stage ---
        auto uncommitted_records = wal_->recover();
        if (!uncommitted_records.empty())
        {
            for (const auto& record : uncommitted_records)
            {
                if (record.type == LogRecordType::SET) 
                {
                    write_to_page(record.key, record.value);
                } 
                else if (record.type == LogRecordType::DELETE) 
                {
                    write_to_page(record.key, TOMBSTONE);
                }
            }
            // After successfully applying all records to pages, clear the WAL
            wal_->clear();
        }
    }

    void Database::write_to_page(const std::string& key, const std::string& value)
    {
        if (pager_->num_pages() == 0) 
            return;

        PageId last_page_id = pager_->num_pages() - 1;
        PageData raw_page{};
        pager_->read_page(last_page_id, raw_page);

        Page page(raw_page);
        
        // If the current page has no room for the record, allocate a new one
        if (!page.insert_record(key, value)) 
        {
            last_page_id = pager_->allocate_page();
            PageData new_raw_page{};
            Page new_page(new_raw_page);
            new_page.init();
            
            // Insert into the new page
            new_page.insert_record(key, value);
            pager_->write_page(last_page_id, new_raw_page);
        } 
        else 
        {
            // Write the updated page back to disk
            pager_->write_page(last_page_id, raw_page);
        }
    }

    void Database::set(const std::string& key, const std::string& value)
    {
        // First write strictly to the WAL and flush to disk
        wal_->append_set(key, value);
        wal_->flush();

        // Only after that modify pages
        write_to_page(key, value);
    }

    std::optional<std::string> Database::get(const std::string& key) 
    {
        if (pager_->num_pages() == 0) 
            return std::nullopt;

        // Search from end to beginning (append-only: the latest record is the freshest)
        for (int32_t page_id = pager_->num_pages() - 1; page_id >= 0; --page_id) 
        {
            PageData raw_page{};
            pager_->read_page(static_cast<PageId>(page_id), raw_page);
            
            Page page(raw_page);
            
            // Scan records on the page from the end as well
            for (int32_t rec_idx = page.num_records() - 1; rec_idx >= 0; --rec_idx) 
            {
                auto rec = page.get_record(static_cast<uint16_t>(rec_idx));
                if (rec && rec->first == key) 
                {
                    if (rec->second == TOMBSTONE) 
                    {
                        return std::nullopt; // Key was deleted
                    }
                    return rec->second; // Found the latest value!
                }
            }
        }
        
        return std::nullopt;
    }

    void Database::remove(const std::string& key) 
    {
        // Write deletion to the WAL
        wal_->append_delete(key);
        wal_->flush();

        // Write a tombstone to the page
        write_to_page(key, TOMBSTONE);
    }

} // namespace minidb   