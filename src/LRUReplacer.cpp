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
        
        // Если вытеснять нечего, возвращаем false
        if (lru_list_.empty()) 
        {
            return false;
        }

        // Самый старый фрейм всегда "падает" в конец списка
        *frame_id = lru_list_.back();
        
        // Удаляем его из хэш-таблицы и из списка
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
            // Кто-то начал использовать этот фрейм! 
            // Он больше не кандидат на вытеснение, поэтому убираем его.
            lru_list_.erase(it->second);
            lru_hash_.erase(it);
        }
    }

    void LRUReplacer::unpin(FrameId frame_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Если фрейм уже в списке кандидатов, ничего не делаем
        if (lru_hash_.find(frame_id) != lru_hash_.end()) 
        {
            return;
        }

        // Защита от переполнения (хотя BufferPoolManager сам следит за лимитом)
        if (lru_list_.size() >= capacity_) 
        {
            return; 
        }

        // Добавляем фрейм в начало списка (он самый свежий из ненужных)
        lru_list_.push_front(frame_id);
        
        // Сохраняем итератор в хэш-таблицу для быстрого доступа
        lru_hash_[frame_id] = lru_list_.begin();
    }

    size_t LRUReplacer::size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lru_list_.size();
    }

} // namespace minidb