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
        h->free_space_pointer = PAGE_SIZE; // Free space for records grows from the very end (4096)
        h->parent_page_id = INVALID_PAGE_ID;
        h->next_leaf_id = INVALID_PAGE_ID;
        h->rightmost_child = INVALID_PAGE_ID;
    }

    // --- Metadata ---

    NodeType Page::node_type() const { return header()->type; }
    bool Page::is_root() const { return header()->is_root != 0; }
    void Page::set_root(bool is_root) { header()->is_root = is_root ? 1 : 0; }
    
    uint32_t Page::parent_id() const { return header()->parent_page_id; }
    void Page::set_parent_id(uint32_t parent_id) { header()->parent_page_id = parent_id; }
    
    uint32_t Page::next_leaf_id() const { return header()->next_leaf_id; }
    void Page::set_next_leaf_id(uint32_t next_leaf_id) { header()->next_leaf_id = next_leaf_id; }

    uint32_t Page::rightmost_child() const { return header()->rightmost_child; }
    void Page::set_rightmost_child(uint32_t child_id) { header()->rightmost_child = child_id; }

    uint16_t Page::num_records() const 
    {
        return header()->num_cells;
    }

    size_t Page::free_space_left() const 
    {
        const PageHeader* h = header();
        // The slot array ends at: sizeof(PageHeader) + num_cells * 2
        size_t slots_end = sizeof(PageHeader) + h->num_cells * sizeof(uint16_t);
        
        if (h->free_space_pointer <= slots_end) 
        {
            return 0;
        }
        
        return h->free_space_pointer - slots_end;
    }

    // --- Slots and navigation ---

    uint16_t Page::cell_offset(uint16_t index) const 
    {
        const uint8_t* ptr = data_.data() + sizeof(PageHeader) + index * sizeof(uint16_t);
        uint16_t offset = 0;
        std::memcpy(&offset, ptr, sizeof(uint16_t));
        return offset;
    }

    void Page::set_cell_offset(uint16_t index, uint16_t offset) 
    {
        uint8_t* ptr = data_.data() + sizeof(PageHeader) + index * sizeof(uint16_t);
        std::memcpy(ptr, &offset, sizeof(uint16_t));
    }

    std::string Page::get_key(uint16_t index) const 
    {
        uint16_t offset = cell_offset(index);
        const uint8_t* ptr = data_.data() + offset;

        uint16_t key_len = 0;
        std::memcpy(&key_len, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t) + sizeof(uint16_t); // Skip key_len and val_len

        return std::string(reinterpret_cast<const char*>(ptr), key_len);
    }

    // --- Search and insert ---

    uint16_t Page::find_cell_index(const std::string& key) const 
    {
        uint16_t low = 0;
        uint16_t high = header()->num_cells;

        while (low < high) 
        {
            uint16_t mid = low + (high - low) / 2;
            std::string mid_key = get_key(mid);

            if (mid_key < key) 
            {
                low = mid + 1;
            } 
            else 
            {
                high = mid;
            }
        }

        return low; // Returns the exact index or the insertion position for sorted order
    }

    std::optional<std::string> Page::get(const std::string& key) const 
    {
        uint16_t idx = find_cell_index(key);
        if (idx < header()->num_cells && get_key(idx) == key) 
        {
            auto record = get_record(idx);
            if (record) 
            {
                return record->second;
            }
        }
        return std::nullopt;
    }

    bool Page::insert_record(const std::string& key, const std::string& value) 
    {
        uint16_t key_len = static_cast<uint16_t>(key.size());
        uint16_t val_len = static_cast<uint16_t>(value.size());
        size_t payload_size = sizeof(uint16_t) + sizeof(uint16_t) + key_len + val_len;
        
        PageHeader* h = header();
        uint16_t idx = find_cell_index(key);
        
        // Check whether the key already exists on the page
        bool is_update = (idx < h->num_cells && get_key(idx) == key);

        // If this is an update, we do not need 2 bytes for a new slot
        size_t needed_space = payload_size;
        if (!is_update) 
        {
            needed_space += sizeof(uint16_t); 
        }

        if (free_space_left() < needed_space) 
        {
            return false; 
        }

        // Write the payload into the bottom part of the page
        h->free_space_pointer -= static_cast<uint16_t>(payload_size);
        uint8_t* write_ptr = data_.data() + h->free_space_pointer;

        std::memcpy(write_ptr, &key_len, sizeof(uint16_t));
        write_ptr += sizeof(uint16_t);

        std::memcpy(write_ptr, &val_len, sizeof(uint16_t));
        write_ptr += sizeof(uint16_t);

        std::memcpy(write_ptr, key.data(), key_len);
        write_ptr += key_len;

        std::memcpy(write_ptr, value.data(), val_len);

        if (is_update) 
        {
            // Update the existing slot to point to the new data
            set_cell_offset(idx, h->free_space_pointer);
        } 
        else 
        {
            // Shift 2-byte slots to the right to preserve sorted order
            for (uint16_t i = h->num_cells; i > idx; --i) 
            {
                set_cell_offset(i, cell_offset(i - 1));
            }

            // Write the offset into the correct sorted position
            set_cell_offset(idx, h->free_space_pointer);
            h->num_cells += 1;
        }

        return true;
    }

    std::optional<std::pair<std::string, std::string>> Page::get_record(uint16_t index) const 
    {
        if (index >= header()->num_cells) 
        {
            return std::nullopt;
        }

        uint16_t offset = cell_offset(index);
        const uint8_t* ptr = data_.data() + offset;

        uint16_t key_len = 0;
        uint16_t val_len = 0;

        std::memcpy(&key_len, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t);

        std::memcpy(&val_len, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t);

        std::string key(reinterpret_cast<const char*>(ptr), key_len);
        std::string value(reinterpret_cast<const char*>(ptr + key_len), val_len);

        return std::make_pair(key, value);
    }

    // --- Internal nodes ---

    bool Page::insert_internal_cell(const std::string& key, PageId left_child_id) 
    {
        uint16_t key_len = static_cast<uint16_t>(key.size());
        size_t payload_size = sizeof(uint32_t) + sizeof(uint16_t) + key_len;
        size_t needed_space = payload_size + sizeof(uint16_t);

        if (free_space_left() < needed_space) 
        {
            return false;
        }

        PageHeader* h = header();
        uint16_t idx = find_cell_index(key);

        h->free_space_pointer -= static_cast<uint16_t>(payload_size);
        uint8_t* write_ptr = data_.data() + h->free_space_pointer;

        std::memcpy(write_ptr, &left_child_id, sizeof(uint32_t));
        write_ptr += sizeof(uint32_t);

        std::memcpy(write_ptr, &key_len, sizeof(uint16_t));
        write_ptr += sizeof(uint16_t);

        std::memcpy(write_ptr, key.data(), key_len);

        for (uint16_t i = h->num_cells; i > idx; --i) 
        {
            set_cell_offset(i, cell_offset(i - 1));
        }

        set_cell_offset(idx, h->free_space_pointer);
        h->num_cells += 1;

        return true;
    }

    PageId Page::find_internal_child(const std::string& key) const 
    {
        uint16_t idx = find_cell_index(key);
        const PageHeader* h = header();

        if (idx < h->num_cells) 
        {
            std::string cell_k = get_key(idx);
            if (key == cell_k) 
            {
                // If keys are equal, descend into the right subtree (idx + 1)
                if (idx + 1 < h->num_cells) 
                {
                    uint16_t offset = cell_offset(idx + 1);
                    PageId child_id = 0;
                    std::memcpy(&child_id, data_.data() + offset, sizeof(uint32_t));
                    return child_id;
                }
                return h->rightmost_child;
            }

            uint16_t offset = cell_offset(idx);
            PageId child_id = 0;
            std::memcpy(&child_id, data_.data() + offset, sizeof(uint32_t));
            return child_id;
        }

        return h->rightmost_child;
    }

} // namespace minidb