#include "minidb/BTree.h"

#include <vector>
#include <algorithm>


namespace minidb 
{

    BTree::BTree(Pager& pager, PageId root_page_id)
        : pager_(pager)
        , root_page_id_(root_page_id)
    {
    }

    PageId BTree::find_leaf_page(PageId current_page_id, const std::string& key)
    {
        PageData raw_page{};
        pager_.read_page(current_page_id, raw_page);
        Page page(raw_page);

        // If we've reached a leaf, return its ID
        if (page.node_type() == NodeType::LEAF) 
        {
            return current_page_id;
        }

        // Navigation logic for internal nodes
        [[maybe_unused]] uint16_t idx = page.find_cell_index(key);
        
        // In the future this will read child_page_id from the appropriate cell
        // For now, while the tree is just a single root-leaf node, return the root
        return current_page_id;
    }

    std::optional<std::string> BTree::get(const std::string& key)
    {
        PageId leaf_id = find_leaf_page(root_page_id_, key);
        
        PageData raw_page{};
        pager_.read_page(leaf_id, raw_page);
        Page leaf_page(raw_page);

        return leaf_page.get(key);
    }

    void BTree::insert(const std::string& key, const std::string& value)
    {
        PageId leaf_id = find_leaf_page(root_page_id_, key);

        PageData raw_page{};
        pager_.read_page(leaf_id, raw_page);
        Page leaf_page(raw_page);

        // Try inserting into the leaf
        bool success = leaf_page.insert_record(key, value);
        if (success) 
        {
            pager_.write_page(leaf_id, raw_page);
            return;
        }

        // If there is no room, split the leaf
        split_leaf(leaf_id, key, value);
    }

    void BTree::split_leaf(PageId leaf_id, const std::string& new_key, const std::string& new_value)
    {
        PageData old_raw_page{};
        pager_.read_page(leaf_id, old_raw_page);
        Page old_leaf(old_raw_page);

        // 1. Collect all existing records plus the new record
        std::vector<std::pair<std::string, std::string>> records;
        for (uint16_t i = 0; i < old_leaf.num_records(); ++i) 
        {
            auto rec = old_leaf.get_record(i);
            if (rec) 
            {
                records.push_back(*rec);
            }
        }
        records.push_back({new_key, new_value});

        std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        // 2. Allocate a new page for the right leaf
        PageId new_leaf_id = pager_.allocate_page();
        PageData new_raw_page{};
        Page new_leaf(new_raw_page);

        bool was_root = old_leaf.is_root();
        uint32_t parent_id = old_leaf.parent_id();
        uint32_t old_next = old_leaf.next_leaf_id();

        // 3. Clear the pages and distribute records evenly
        old_leaf.init(NodeType::LEAF, was_root);
        old_leaf.set_parent_id(parent_id);

        new_leaf.init(NodeType::LEAF, false);
        new_leaf.set_parent_id(parent_id);

        size_t mid = records.size() / 2;

        for (size_t i = 0; i < mid; ++i) 
        {
            old_leaf.insert_record(records[i].first, records[i].second);
        }

        for (size_t i = mid; i < records.size(); ++i) 
        {
            new_leaf.insert_record(records[i].first, records[i].second);
        }

        // 4. Configure linked list pointers
        old_leaf.set_next_leaf_id(new_leaf_id);
        new_leaf.set_next_leaf_id(old_next);

        pager_.write_page(leaf_id, old_raw_page);
        pager_.write_page(new_leaf_id, new_raw_page);

        // 5. Separator is the first key of the right leaf
        std::string separator_key = records[mid].first;

        insert_into_parent(leaf_id, separator_key, new_leaf_id);
    }

    void BTree::insert_into_parent(PageId left_child_id, const std::string& key, PageId right_child_id)
    {
        PageData left_raw_page{};
        pager_.read_page(left_child_id, left_raw_page);
        Page left_child(left_raw_page);

        // If the root split, create a new internal node as the root
        if (left_child.is_root()) 
        {
            PageId new_root_id = pager_.allocate_page();
            PageData new_root_raw{};
            Page new_root(new_root_raw);

            new_root.init(NodeType::INTERNAL, true);
            new_root.insert_internal_cell(key, left_child_id);
            new_root.set_rightmost_child(right_child_id);

            left_child.set_root(false);
            left_child.set_parent_id(new_root_id);

            PageData right_raw{};
            pager_.read_page(right_child_id, right_raw);
            Page right_child(right_raw);
            right_child.set_parent_id(new_root_id);

            pager_.write_page(left_child_id, left_raw_page);
            pager_.write_page(right_child_id, right_raw);
            pager_.write_page(new_root_id, new_root_raw);

            root_page_id_ = new_root_id;
            return;
        }

        // If the node was not root, insert into the existing parent node
        PageId parent_id = left_child.parent_id();
        PageData parent_raw{};
        pager_.read_page(parent_id, parent_raw);
        Page parent(parent_raw);

        if (parent.rightmost_child() == left_child_id) 
        {
            parent.insert_internal_cell(key, left_child_id);
            parent.set_rightmost_child(right_child_id);
        } 
        else 
        {
            parent.insert_internal_cell(key, left_child_id);
        }

        pager_.write_page(parent_id, parent_raw);
    }

} // namespace minidb