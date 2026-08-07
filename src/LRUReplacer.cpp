#include "minidb/LRUReplacer.h"

namespace minidb 
{

    LRUReplacer::LRUReplacer(size_t num_frames)
        : capacity_(num_frames)
    {
    }

    bool LRUReplacer::victim(FrameId* frame_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // If there is nothing to evict, return false
        if (lru_list_.empty()) 
        {
            return false;
        }

        // The oldest frame is always at the back of the list
        *frame_id = lru_list_.back();
        
        // Remove it from the hash table and from the list
        lru_hash_.erase(*frame_id);
        lru_list_.pop_back();
        
        return true;
    }

    void LRUReplacer::pin(FrameId frame_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = lru_hash_.find(frame_id);
        if (it != lru_hash_.end()) 
        {
            // Someone started using this frame!
            // It is no longer a candidate for eviction, so remove it.
            lru_list_.erase(it->second);
            lru_hash_.erase(it);
        }
    }

    void LRUReplacer::unpin(FrameId frame_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // If the frame is already in the candidate list, do nothing
        if (lru_hash_.find(frame_id) != lru_hash_.end()) 
        {
            return;
        }

        // Protection against overflow (although BufferPoolManager keeps the limit)
        if (lru_list_.size() >= capacity_) 
        {
            return; 
        }

        // Add the frame to the front of the list (it is the newest unneeded one)
        lru_list_.push_front(frame_id);
        
        // Store the iterator in the hash table for fast access
        lru_hash_[frame_id] = lru_list_.begin();
    }

    size_t LRUReplacer::size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lru_list_.size();
    }

} // namespace minidb