#include "minidb/Page.h"

#include <cstring>


namespace minidb 
{

    Page::Page(PageData& raw_data) 
        : data_(raw_data) 
    {
    }

    PageHeader* Page::header() 
    {
        return reinterpret_cast<PageHeader*>(data_.data());
    }

    const PageHeader* Page::header() const 
    {
        return reinterpret_cast<const PageHeader*>(data_.data());
    }

    void Page::init() 
    {
        PageHeader* h = header();
        h->page_type = 0;
        h->num_records = 0;
        h->free_space_offset = sizeof(PageHeader); // Смещение = 6 байт
    }

    uint16_t Page::num_records() const 
    {
        return header()->num_records;
    }

    size_t Page::free_space_left() const 
    {
        if (header()->free_space_offset >= PAGE_SIZE) 
        {
            return 0;
        }
        return PAGE_SIZE - header()->free_space_offset;
    }

    bool Page::insert_record(const std::string& key, const std::string& value) 
    {
        // Формат записи: [uint16_t key_len][uint16_t val_len][key bytes][value bytes]
        uint16_t key_len = static_cast<uint16_t>(key.size());
        uint16_t val_len = static_cast<uint16_t>(value.size());
        size_t total_record_size = sizeof(uint16_t) + sizeof(uint16_t) + key_len + val_len;

        if (free_space_left() < total_record_size) 
        {
            return false; // Страница переполнена
        }

        PageHeader* h = header();
        uint8_t* write_ptr = data_.data() + h->free_space_offset;

        // Пишем длины
        std::memcpy(write_ptr, &key_len, sizeof(uint16_t));
        write_ptr += sizeof(uint16_t);

        std::memcpy(write_ptr, &val_len, sizeof(uint16_t));
        write_ptr += sizeof(uint16_t);

        // Пишем сами строки
        std::memcpy(write_ptr, key.data(), key_len);
        write_ptr += key_len;

        std::memcpy(write_ptr, value.data(), val_len);
        write_ptr += val_len;

        // Обновляем метаданные в заголовке страницы
        h->free_space_offset += static_cast<uint16_t>(total_record_size);
        h->num_records += 1;

        return true;
    }

    std::optional<std::pair<std::string, std::string>> Page::get_record(uint16_t index) const 
    {
        const PageHeader* h = header();
        if (index >= h->num_records) 
        {
            return std::nullopt;
        }

        // Сканируем запись за записью от начала данных (сразу за заголовком)
        const uint8_t* read_ptr = data_.data() + sizeof(PageHeader);

        for (uint16_t i = 0; i < h->num_records; ++i) 
        {
            uint16_t key_len = 0;
            uint16_t val_len = 0;

            std::memcpy(&key_len, read_ptr, sizeof(uint16_t));
            read_ptr += sizeof(uint16_t);

            std::memcpy(&val_len, read_ptr, sizeof(uint16_t));
            read_ptr += sizeof(uint16_t);

            if (i == index) 
            {
                std::string key(reinterpret_cast<const char*>(read_ptr), key_len);
                std::string value(reinterpret_cast<const char*>(read_ptr + key_len), val_len);
                return std::make_pair(key, value);
            }

            read_ptr += key_len + val_len;
        }

        return std::nullopt;
    }

} // namespace minidb