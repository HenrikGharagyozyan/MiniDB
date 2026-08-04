#include "minidb/Pager.h"
#include <iostream>

namespace minidb 
{

    Pager::Pager(const std::string& filename) 
        : filename_(filename) 
    {
        file_stream_.open(filename, std::ios::in | std::ios::out | std::ios::binary);

        if (!file_stream_.is_open()) 
        {
            file_stream_.clear();
            file_stream_.open(filename, std::ios::out | std::ios::binary);
            file_stream_.close();
            file_stream_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        }

        file_stream_.seekg(0, std::ios::end);
        auto file_length = file_stream_.tellg();
        num_pages_ = static_cast<uint32_t>(file_length / PAGE_SIZE);
    }

    Pager::~Pager() 
    {
        if (file_stream_.is_open()) 
            file_stream_.close();
    }

    bool Pager::read_page(PageId page_id, PageData& out_page) 
    {
        if (page_id >= num_pages_) 
        {
            std::cerr << "Error: PageId " << page_id << " out of bounds (total: " << num_pages_ << ")\n";
            return false;
        }

        file_stream_.seekg(page_id * PAGE_SIZE, std::ios::beg);
        file_stream_.read(reinterpret_cast<char*>(out_page.data()), PAGE_SIZE);
        return true;
    }

    bool Pager::write_page(PageId page_id, const PageData& page) 
    {
        file_stream_.seekp(page_id * PAGE_SIZE, std::ios::beg);
        file_stream_.write(reinterpret_cast<const char*>(page.data()), PAGE_SIZE);
        file_stream_.flush();

        if (page_id >= num_pages_) 
            num_pages_ = page_id + 1;

        return true;
    }

    PageId Pager::allocate_page() 
    {
        PageId new_page_id = num_pages_;
        PageData empty_page{};
        write_page(new_page_id, empty_page);
        return new_page_id;
    }

} // namespace minidb