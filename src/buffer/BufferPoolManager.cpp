#include "minidb/buffer/BufferPoolManager.h"

namespace minidb 
{
    BufferPoolManager::BufferPoolManager(size_t pool_size, Pager& pager)
        : pool_size_(pool_size)
        , pager_(pager)
    {
        replacer_ = std::make_unique<LRUReplacer>(pool_size_);
        
        // Allocate memory for pool_size frames.
        // Frame initializes all fields with default values automatically.
        frames_.resize(pool_size_);

        // Initially all frames are free
        for (size_t i = 0; i < pool_size_; ++i) 
        {
            free_list_.push_back(static_cast<FrameId>(i));
        }
    }

    BufferPoolManager::~BufferPoolManager() 
    {
        flush_all_pages();
    }

    bool BufferPoolManager::find_victim_frame(FrameId* frame_id) 
    {
        // 1. First look for a completely free frame
        if (!free_list_.empty()) 
        {
            *frame_id = free_list_.front();
            free_list_.pop_front();
            return true;
        }

        // 2. If there are no free frames, ask LRU for the oldest candidate
        if (replacer_->victim(frame_id)) 
        {
            // If the old page was modified, flush it to disk
            if (frames_[*frame_id].is_dirty) 
            {
                pager_.write_page(frames_[*frame_id].page_id, frames_[*frame_id].data);
                frames_[*frame_id].is_dirty = false;
            }
            
            // Remove the old page from the table
            page_table_.erase(frames_[*frame_id].page_id);
            return true;
        }

        // If all frames are pinned, we cannot evict anything
        return false;
    }

    PageData* BufferPoolManager::fetch_page(PageId page_id) 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // If the page is ALREADY in memory
        if (page_table_.find(page_id) != page_table_.end()) 
        {
            FrameId frame_id = page_table_[page_id];
            frames_[frame_id].pin_count++;
            replacer_->pin(frame_id);
            return &frames_[frame_id].data;
        }

        // If the page is not in memory, find a slot to load it into
        FrameId frame_id;
        if (!find_victim_frame(&frame_id)) 
        {
            return nullptr; // Catastrophe: no memory available (all pages pinned)
        }

        // Read from disk into the selected frame
        pager_.read_page(page_id, frames_[frame_id].data);
        
        // Update metadata in the frame and table
        page_table_[page_id] = frame_id;
        frames_[frame_id].page_id = page_id;
        frames_[frame_id].pin_count = 1;
        frames_[frame_id].is_dirty = false;
        
        // Pin the page while it is in use
        replacer_->pin(frame_id);

        return &frames_[frame_id].data;
    }

    PageData* BufferPoolManager::new_page(PageId* page_id) 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        FrameId frame_id;
        if (!find_victim_frame(&frame_id)) 
        {
            return nullptr;
        }

        // Create a new page on disk
        *page_id = pager_.allocate_page();

        // Update metadata
        page_table_[*page_id] = frame_id;
        frames_[frame_id].page_id = *page_id;
        frames_[frame_id].pin_count = 1;
        frames_[frame_id].is_dirty = false;
        
        // Zero out the frame memory (clear old garbage)
        frames_[frame_id].data = PageData{};

        replacer_->pin(frame_id);

        return &frames_[frame_id].data;
    }

    bool BufferPoolManager::unpin_page(PageId page_id, bool is_dirty) 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (page_table_.find(page_id) == page_table_.end()) 
        {
            return false;
        }

        FrameId frame_id = page_table_[page_id];
        
        if (frames_[frame_id].pin_count <= 0) 
        {
            return false; // The page is already unpinned
        }

        frames_[frame_id].pin_count--;
        
        if (is_dirty) 
        {
            frames_[frame_id].is_dirty = true;
        }

        // If nobody else holds this page, hand it over to LRU
        if (frames_[frame_id].pin_count == 0) 
        {
            replacer_->unpin(frame_id);
        }

        return true;
    }

    bool BufferPoolManager::flush_page(PageId page_id) 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (page_table_.find(page_id) == page_table_.end()) 
        {
            return false;
        }

        FrameId frame_id = page_table_[page_id];
        pager_.write_page(page_id, frames_[frame_id].data);
        frames_[frame_id].is_dirty = false;

        return true;
    }

    void BufferPoolManager::flush_all_pages() 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto const& [page_id, frame_id] : page_table_) 
        {
            if (frames_[frame_id].is_dirty) 
            {
                pager_.write_page(page_id, frames_[frame_id].data);
                frames_[frame_id].is_dirty = false;
            }
        }
    }

} // namespace minidb