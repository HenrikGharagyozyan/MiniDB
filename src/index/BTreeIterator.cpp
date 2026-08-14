#include "minidb/index/BTreeIterator.h"
#include <stdexcept>

namespace minidb 
{
    BTreeIterator::BTreeIterator(BufferPoolManager* bpm, PageId current_page_id, uint16_t record_index)
        : bpm_(bpm), current_page_id_(current_page_id), record_index_(record_index)
    {
        // When creating the iterator, immediately load the page into memory
        fetch_page();
    }

    BTreeIterator::~BTreeIterator() 
    {
        // When the iterator is destroyed, unpin the page so the BufferPool can reuse it.
        unpin_page();
    }

    void BTreeIterator::fetch_page() 
    {
        if (current_page_id_ != INVALID_PAGE_ID && bpm_ != nullptr) 
        {
            current_page_data_ = bpm_->fetch_page(current_page_id_);
            if (!current_page_data_) 
            {
                throw std::runtime_error("BTreeIterator: Failed to fetch page from BufferPool");
            }
        }
    }

    void BTreeIterator::unpin_page() 
    {
        if (current_page_data_ != nullptr && bpm_ != nullptr) 
        {
            // Pass false because the iterator only reads data and does not modify it (is_dirty = false)
            bpm_->unpin_page(current_page_id_, false); 
            current_page_data_ = nullptr;
        }
    }

    bool BTreeIterator::is_end() const 
    {
        // Iterator reached the end if the page ID is invalid
        return current_page_id_ == INVALID_PAGE_ID || current_page_data_ == nullptr;
    }

    std::string BTreeIterator::get_key() 
    {
        if (is_end()) 
            return "";
        
        Page page(*current_page_data_);

        // Защита от чтения за пределами страницы
        if (record_index_ >= page.num_records()) 
            return "";

        auto record = page.get_record(record_index_);
        return record ? record->first : "";
    }

    std::string BTreeIterator::get_value() 
    {
        if (is_end()) 
            return "";
        
        Page page(*current_page_data_);

        // Защита от чтения за пределами страницы
        if (record_index_ >= page.num_records()) 
            return "";

        auto record = page.get_record(record_index_);
        return record ? record->second : "";
    }

    void BTreeIterator::advance() 
    {
        if (is_end()) 
            return;

        Page page(*current_page_data_);
        record_index_++; // Move to the next record on the current page

        // Check whether we've exceeded the number of records on this page
        if (record_index_ >= page.num_records()) 
        {
            // If so, take the ID of the next page (via the B+ tree linked list)
            PageId next_id = page.next_leaf_id();
            
            // 0 - это Meta Page. Если next_id == 0 или INVALID_PAGE_ID, значит следующего листа нет.
            if (next_id == 0 || next_id == INVALID_PAGE_ID) 
            {
                unpin_page();
                current_page_id_ = INVALID_PAGE_ID; // Принудительно завершаем итератор
                return;
            }

            // Unpin the current page, since we no longer need it
            unpin_page(); 
            
            // Move to the new page
            current_page_id_ = next_id;
            record_index_ = 0; // Start reading the new page from index zero
            
            // Load the new page into memory
            fetch_page(); 
        }
    }

} // namespace minidb