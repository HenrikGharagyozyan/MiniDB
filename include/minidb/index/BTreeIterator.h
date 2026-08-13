#pragma once

#include "minidb/storage/Page.h"
#include "minidb/buffer/BufferPoolManager.h"

#include <string>


namespace minidb 
{
    class BTreeIterator 
    {
    public:
        // Конструктор принимает BufferPoolManager, ID стартовой страницы и индекс записи
        BTreeIterator(BufferPoolManager* bpm, PageId current_page_id, uint16_t record_index);
        
        // Деструктор обязательно должен освобождать (unpin) страницу в BufferPool
        ~BTreeIterator();

        // Проверка, достигли ли мы конца дерева
        bool is_end() const;

        // Получить ключ и значение по текущей позиции
        std::string get_key();
        std::string get_value();

        // Сдвинуть итератор к следующей записи
        void advance();
    private:
        // Вспомогательные методы для работы с BufferPoolManager
        void fetch_page();
        void unpin_page();
        
    private:
        BufferPoolManager* bpm_;
        PageId current_page_id_;
        uint16_t record_index_;
        
        // Указатель на сырые данные страницы, загруженные из BufferPool
        PageData* current_page_data_{nullptr}; 
        
    };

} // namespace minidb