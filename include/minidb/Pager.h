#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <string>


namespace minidb 
{

    // Размер страницы фиксирован: 4 КБ
    constexpr size_t PAGE_SIZE = 4096;
    using PageId = uint32_t;
    using PageData = std::array<uint8_t, PAGE_SIZE>;

    class Pager 
    {
    public:
        explicit Pager(const std::string& filename);
        ~Pager();

        // Чтение страницы по ее ID
        bool read_page(PageId page_id, PageData& out_page);

        // Запись страницы по ее ID
        bool write_page(PageId page_id, const PageData& page);

        // Выделение новой пустой страницы в конце файла
        PageId allocate_page();

        // Количество страниц в файле
        uint32_t num_pages() const { return num_pages_; }

    private:
        std::fstream file_stream_;
        std::string filename_;
        uint32_t num_pages_{0};
    };

} // namespace minidb