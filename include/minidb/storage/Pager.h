#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <string>


namespace minidb 
{

    // Page size is fixed: 4 KB
    constexpr size_t PAGE_SIZE = 4096;
    using PageId = uint32_t;
    using PageData = std::array<uint8_t, PAGE_SIZE>;

    class Pager 
    {
    public:
        explicit Pager(const std::string& filename);
        ~Pager();

        // Read a page by its ID
        bool read_page(PageId page_id, PageData& out_page);

        // Write a page by its ID
        bool write_page(PageId page_id, const PageData& page);

        // Allocate a new empty page at the end of the file
        PageId allocate_page();

        // Number of pages in the file
        uint32_t num_pages() const { return num_pages_; }

    private:
        std::fstream file_stream_;
        std::string filename_;
        uint32_t num_pages_{0};
    };

} // namespace minidb