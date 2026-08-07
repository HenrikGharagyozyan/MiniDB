#pragma once

#include "minidb/Pager.h"
#include "minidb/Page.h"
#include "minidb/LRUReplacer.h"

#include <unordered_map>
#include <vector>
#include <list>
#include <mutex>
#include <memory>

namespace minidb 
{
    class BufferPoolManager 
    {
    public:
        // Create a pool with pool_size frames (e.g. 10 pages in memory)
        BufferPoolManager(size_t pool_size, Pager& pager);
        ~BufferPoolManager();

        // Fetch a page (loads from disk if it is not in memory)
        PageData* fetch_page(PageId page_id);

        // Create a new empty page on disk and immediately pin it in memory
        PageData* new_page(PageId* page_id);

        // Tell the pool that we finished working with the page.
        // is_dirty = true means the data has changed and needs to be written to disk.
        bool unpin_page(PageId page_id, bool is_dirty);

        // Force a page to be written to disk (even if it is still pinned)
        bool flush_page(PageId page_id);

        // Force all dirty pages to be written to disk (useful on DB shutdown)
        void flush_all_pages();

    private:
        // Internal structure for storing a page and its metadata
        struct Frame 
        {
            PageData data{};
            PageId page_id{0};
            int pin_count{0};
            bool is_dirty{false};
        };

        size_t pool_size_;
        Pager& pager_;
        std::unique_ptr<LRUReplacer> replacer_;

        // Single array of frames (our "RAM")
        std::vector<Frame> frames_;
        
        // Page table: PageId -> FrameId
        std::unordered_map<PageId, FrameId> page_table_;
        
        // List of frames that have never been used
        std::list<FrameId> free_list_;

        std::mutex mutex_;

        // Helper method: find a free frame or evict an old one
        bool find_victim_frame(FrameId* frame_id);
    };

} // namespace minidb