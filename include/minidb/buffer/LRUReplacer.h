#pragma once

#include <list>
#include <unordered_map>
#include <mutex>


namespace minidb 
{

    // FrameId is the "slot number" in our memory pool
    using FrameId = int32_t;

    class LRUReplacer 
    {
    public:
        // Create an LRU cache with the given slot limit
        explicit LRUReplacer(size_t num_frames);
        ~LRUReplacer() = default;

        // Select a victim - the oldest frame to evict.
        // Writes the frame ID to the pointer and returns true. Returns false if there is nothing to evict.
        bool victim(FrameId* frame_id);

        // Pin a frame. This means some part of the database is currently reading it, and it must not be evicted.
        void pin(FrameId frame_id);

        // Unpin a frame. The database is done with it, so it can be evicted if needed.
        void unpin(FrameId frame_id);

        // How many frames are currently evictable?
        size_t size();

    private:
        // Doubly linked list maintains the order. Newest frames at the front, oldest at the back.
        std::list<FrameId> lru_list_;
        
        // Hash table for O(1) lookup of a frame in the list
        std::unordered_map<FrameId, std::list<FrameId>::iterator> lru_hash_;
        
        // Frame limit
        size_t capacity_;
        
        // (Stage 8), to prevent different threads from breaking the cache
        std::mutex mutex_;
    };

} // namespace minidb