#pragma once

#include "minidb/storage/Pager.h"
#include <cstdint>
#include <string>
#include <optional>
#include <utility>

namespace minidb 
{

    constexpr uint32_t INVALID_PAGE_ID = 0xFFFFFFFF;

    enum class NodeType : uint8_t 
    {
        LEAF = 0,
        INTERNAL = 1
    };

    #pragma pack(push, 1) // Disable padding
    struct PageHeader 
    {
        NodeType type;                  // 1 byte: node type (Leaf / Internal)
        uint8_t is_root;                // 1 byte: 1 if root, 0 otherwise
        uint16_t num_cells;             // 2 bytes: number of records/keys
        uint16_t free_space_pointer;    // 2 bytes: offset to free space
        uint32_t parent_page_id;        // 4 bytes: parent page ID
        uint32_t next_leaf_id;          // 4 bytes: right sibling ID (for range scan)
        uint32_t rightmost_child;       // 4 bytes: rightmost child ID (for internal node)
    };
    #pragma pack(pop)


    class Page 
    {
    public:
        explicit Page(PageData& raw_data);

        // Initialization. Default creates a leaf node.
        void init(NodeType type = NodeType::LEAF, bool is_root = false);

        // Getters and setters for B+ tree metadata
        NodeType node_type() const;
        bool is_root() const;
        void set_root(bool is_root);

        uint32_t parent_id() const;
        void set_parent_id(uint32_t parent_id);

        uint32_t next_leaf_id() const;
        void set_next_leaf_id(uint32_t next_leaf_id);

        uint32_t rightmost_child() const;
        void set_rightmost_child(uint32_t child_id);

        // Binary search for the key position on the page
        uint16_t find_cell_index(const std::string& key) const;

        // Search for a value by key in O(log K)
        std::optional<std::string> get(const std::string& key) const;

        // Insert a key-value pair. Returns false if there is no room on the page
        bool insert_record(const std::string& key, const std::string& value);
        // Get a record by its sequential index
        std::optional<std::pair<std::string, std::string>> get_record(uint16_t index) const;

        // Internal nodes (Internal Nodes)
        bool insert_internal_cell(const std::string& key, PageId left_child_id);
        PageId find_internal_child(const std::string& key) const;

        std::pair<std::string, PageId> get_internal_cell(uint16_t index) const;

        // Helper methods
        uint16_t num_records() const;
        size_t free_space_left() const;

    private:
        // Helper methods for slot handling
        uint16_t cell_offset(uint16_t index) const;
        void set_cell_offset(uint16_t index, uint16_t offset);
        std::string get_key(uint16_t index) const;
    
    private:
        PageData& data_;

        PageHeader* header();
        const PageHeader* header() const;

    };

} // namespace minidb