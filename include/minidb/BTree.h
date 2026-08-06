#pragma once

#include "minidb/Pager.h"
#include "minidb/Page.h"
#include <string>
#include <optional>

namespace minidb 
{

    class BTree 
    {
    public:
        explicit BTree(Pager& pager, PageId root_page_id);

        // Search for a value by key in O(log N)
        std::optional<std::string> get(const std::string& key);

        // Insert a record
        void insert(const std::string& key, const std::string& value);

        PageId root_page_id() const { return root_page_id_; }

    private:
        Pager& pager_;
        PageId root_page_id_;

        // Descend internal nodes to the correct leaf page
        PageId find_leaf_page(PageId current_page_id, const std::string& key);
        void split_leaf(PageId leaf_id, const std::string& key, const std::string& value);
        void insert_into_parent(PageId left_child_id, const std::string& key, PageId right_child_id);
    };

} // namespace minidb