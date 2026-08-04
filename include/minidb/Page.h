#pragma once

#include "minidb/Pager.h"
#include <cstdint>
#include <string>
#include <optional>
#include <utility>

namespace minidb 
{

    #pragma pack(push, 1) // Отключаем выравнивание компилятора, чтобы структура весила ровно 6 байт
    struct PageHeader 
    {
        uint16_t page_type{0};          // 0 = Leaf/Data Page
        uint16_t num_records{0};        // Количество записей
        uint16_t free_space_offset{6};  // Смещение, откуда можно писать новую запись (начинается сразу за заголовком)
    };
    #pragma pack(pop)

    class Page 
    {
    public:
        explicit Page(PageData& raw_data);

        // Инициализация новой чистой страницы
        void init();

        // Вставка пары key-value. Возвращает false, если на странице нет места
        bool insert_record(const std::string& key, const std::string& value);

        // Получение записи по ее порядковому номеру (index)
        std::optional<std::pair<std::string, std::string>> get_record(uint16_t index) const;

        // Вспомогательные методы
        uint16_t num_records() const;
        size_t free_space_left() const;

    private:
        PageData& data_;

        PageHeader* header();
        const PageHeader* header() const;
    };

} // namespace minidb