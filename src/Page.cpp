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

    void Page::init(NodeType type, bool is_root)
    {
        PageHeader* h = header();
        h->type = type;
        h->is_root = is_root ? 1 : 0;
        h->num_cells = 0;
        h->free_space_offset = sizeof(PageHeader); // Now offset = 18 bytes
        h->parent_page_id = INVALID_PAGE_ID;
        h->next_leaf_id = INVALID_PAGE_ID;
        h->rightmost_child = INVALID_PAGE_ID;
    }


    // ----------------------------
    // --- B+ tree metadata ---
    // ----------------------------
    NodeType Page::node_type() const { return header()->type; }
    bool Page::is_root() const { return header()->is_root != 0; }
    void Page::set_root(bool is_root) { header()->is_root = is_root ? 1 : 0; }
    
    uint32_t Page::parent_id() const { return header()->parent_page_id; }
    void Page::set_parent_id(uint32_t parent_id) { header()->parent_page_id = parent_id; }
    
    uint32_t Page::next_leaf_id() const { return header()->next_leaf_id; }
    void Page::set_next_leaf_id(uint32_t next_leaf_id) { header()->next_leaf_id = next_leaf_id; }


    // ----------------------------
    // --- Record handling ---
    // ----------------------------
    uint16_t Page::num_records() const 
    {
        return header()->num_cells;
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
        uint16_t key_len = static_cast<uint16_t>(key.size());
        uint16_t val_len = static_cast<uint16_t>(value.size());
        size_t total_record_size = sizeof(uint16_t) + sizeof(uint16_t) + key_len + val_len;

        if (free_space_left() < total_record_size) 
        {
            return false; 
        }

        PageHeader* h = header();
        uint8_t* write_ptr = data_.data() + h->free_space_offset;

        std::memcpy(write_ptr, &key_len, sizeof(uint16_t));
        write_ptr += sizeof(uint16_t);

        std::memcpy(write_ptr, &val_len, sizeof(uint16_t));
        write_ptr += sizeof(uint16_t);

        std::memcpy(write_ptr, key.data(), key_len);
        write_ptr += key_len;

        std::memcpy(write_ptr, value.data(), val_len);
        write_ptr += val_len;

        h->free_space_offset += static_cast<uint16_t>(total_record_size);
        h->num_cells += 1;

        return true;
    }

    std::optional<std::pair<std::string, std::string>> Page::get_record(uint16_t index) const 
    {
        const PageHeader* h = header();
        if (index >= h->num_cells) 
        {
            return std::nullopt;
        }

        // Read starting immediately after the new 18-byte header
        const uint8_t* read_ptr = data_.data() + sizeof(PageHeader);

        for (uint16_t i = 0; i < h->num_cells; ++i) 
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