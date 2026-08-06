#include "minidb/BTree.h"

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

        // If there's no room, split_leaf(leaf_id) will be called here
    }

} // namespace minidb