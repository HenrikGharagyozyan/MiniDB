#pragma once

#include "minidb/storage/Page.h"
#include "minidb/buffer/BufferPoolManager.h"

#include <string>


namespace minidb 
{
    class BTreeIterator 
    {
    public:
        // Constructor takes a BufferPoolManager, the start page ID and a record index
        BTreeIterator(BufferPoolManager* bpm, PageId current_page_id, uint16_t record_index);
        
        // Destructor must unpin the page in the BufferPool
        ~BTreeIterator();

        // Check whether we've reached the end of the tree
        bool is_end() const;

        // Get key and value at the current position
        std::string get_key();
        std::string get_value();

        // Advance the iterator to the next record
        void advance();
    private:
        // Helper methods for working with the BufferPoolManager
        void fetch_page();
        void unpin_page();
        
    private:
        BufferPoolManager* bpm_;
        PageId current_page_id_;
        uint16_t record_index_;
        
        // Pointer to the raw page data loaded from the BufferPool
        PageData* current_page_data_{nullptr}; 
        
    };

} // namespace minidb