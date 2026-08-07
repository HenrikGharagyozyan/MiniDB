#pragma once

#include <list>
#include <unordered_map>
#include <mutex>

namespace minidb 
{
    // FrameId - это "номер слота" в нашей оперативной памяти
    using FrameId = int32_t;

    class LRUReplacer 
    {
    public:
        // Создаем LRU кэш с заданным лимитом слотов
        explicit LRUReplacer(size_t num_frames);
        ~LRUReplacer() = default;

        // Выбрать "жертву" (victim) — самый старый фрейм для вытеснения.
        // Запишет ID фрейма в указатель и вернет true. Если вытеснять нечего, вернет false.
        bool victim(FrameId* frame_id);

        // Закрепить (pin) фрейм. Это значит, что какая-то часть базы прямо сейчас 
        // читает этот фрейм, и его КАТЕГОРИЧЕСКИ нельзя вытеснять из памяти.
        void pin(FrameId frame_id);

        // Открепить (unpin) фрейм. База закончила работу с ним, теперь его 
        // можно вытеснять, если понадобится место.
        void unpin(FrameId frame_id);

        // Сколько сейчас фреймов можно вытеснить?
        size_t size();

    private:
        // Двусвязный список хранит порядок. В начале - самые новые, в конце - самые старые.
        std::list<FrameId> lru_list_;
        
        // Хэш-таблица для быстрого поиска фрейма в списке за О(1) времени
        std::unordered_map<FrameId, std::list<FrameId>::iterator> lru_hash_;
        
        // Лимит фреймов
        size_t capacity_;
        
        // (Stage 8), чтобы разные потоки не сломали кэш
        std::mutex mutex_;
    };

} // namespace minidb