#include "minidb/index/BTreeIterator.h"
#include <stdexcept>

namespace minidb 
{
    BTreeIterator::BTreeIterator(BufferPoolManager* bpm, PageId current_page_id, uint16_t record_index)
        : bpm_(bpm), current_page_id_(current_page_id), record_index_(record_index)
    {
        // При создании итератора сразу загружаем страницу в память
        fetch_page();
    }

    BTreeIterator::~BTreeIterator() 
    {
        // Когда итератор уничтожается, обязательно открепляем страницу,
        // чтобы BufferPool мог использовать эту память для других нужд.
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
            // Передаем false, так как итератор только читает данные и не изменяет их (is_dirty = false)
            bpm_->unpin_page(current_page_id_, false); 
            current_page_data_ = nullptr;
        }
    }

    bool BTreeIterator::is_end() const 
    {
        // Итератор дошел до конца, если ID страницы невалидный
        return current_page_id_ == INVALID_PAGE_ID || current_page_data_ == nullptr;
    }

    std::string BTreeIterator::get_key() 
    {
        if (is_end()) 
            return "";
        
        Page page(*current_page_data_);
        auto record = page.get_record(record_index_);
        return record ? record->first : "";
    }

    std::string BTreeIterator::get_value() 
    {
        if (is_end()) 
            return "";
        
        Page page(*current_page_data_);
        auto record = page.get_record(record_index_);
        return record ? record->second : "";
    }

    void BTreeIterator::advance() 
    {
        if (is_end()) 
            return;

        Page page(*current_page_data_);
        record_index_++; // Сдвигаемся к следующей записи на текущей странице

        // Проверяем, не вышли ли мы за пределы количества записей на этой странице
        if (record_index_ >= page.num_records()) 
        {
            // Если вышли, берем ID следующей страницы (по связному списку B+ дерева)
            PageId next_id = page.next_leaf_id();
            
            // Открепляем текущую страницу, так как она нам больше не нужна
            unpin_page(); 
            
            // Переходим на новую страницу
            current_page_id_ = next_id;
            record_index_ = 0; // Начинаем читать новую страницу с нулевого индекса
            
            // Загружаем новую страницу в память
            fetch_page(); 
        }
    }

} // namespace minidb