#include "minidb/index/BTree.h"

#include <vector>
#include <algorithm>
#include <cstring> // For std::memcpy

namespace minidb 
{

    BTree::BTree(BufferPoolManager& bpm, PageId root_page_id)
        : bpm_(bpm)
        , root_page_id_(root_page_id)
    {
    }

    PageId BTree::find_leaf_page(PageId current_page_id, const std::string& key)
    {
        PageData* raw_page = bpm_.fetch_page(current_page_id);
        Page page(*raw_page);

        // If we've reached a leaf, return its ID
        if (page.node_type() == NodeType::LEAF) 
        {
            bpm_.unpin_page(current_page_id, false);
            return current_page_id;
        }

        PageId child_id = page.find_internal_child(key);

        // Unpin the current node BEFORE descending!
        bpm_.unpin_page(current_page_id, false); 
        
        return find_leaf_page(child_id, key);
    }

    PageId BTree::find_leftmost_leaf()
    {
        PageId current_id = root_page_id_;
        
        while (true) 
        {
            PageData* raw_page = bpm_.fetch_page(current_id);
            Page page(*raw_page);

            // If we've reached a leaf, return its ID
            if (page.node_type() == NodeType::LEAF) 
            {
                bpm_.unpin_page(current_id, false);
                return current_id;
            }

            // This is an internal node. We need the leftmost child.
            PageId next_id;
            if (page.num_records() > 0) 
            {
                next_id = page.get_internal_cell(0).second;
            } 
            else 
            {
                // If there are no records (shouldn't happen), take the rightmost child
                next_id = page.rightmost_child();
            }

            bpm_.unpin_page(current_id, false);
            current_id = next_id; // Descend one level
        }
    }

    std::optional<std::string> BTree::get(const std::string& key)
    {
        PageId leaf_id = find_leaf_page(root_page_id_, key);
        
        PageData* raw_page = bpm_.fetch_page(leaf_id);
        Page leaf_page(*raw_page);

        auto result = leaf_page.get(key);
        bpm_.unpin_page(leaf_id, false);
        return result;
    }

    void BTree::insert(const std::string& key, const std::string& value)
    {
        PageId leaf_id = find_leaf_page(root_page_id_, key);

        PageData* raw_page = bpm_.fetch_page(leaf_id);
        Page leaf_page(*raw_page);

        // Try inserting into the leaf
        bool success = leaf_page.insert_record(key, value);
        if (success) 
        {
            bpm_.unpin_page(leaf_id, true); // We wrote it, so it's dirty
            return;
        }

        // If there is no space, unpin it (we haven't modified the page yet) and call split
        bpm_.unpin_page(leaf_id, false);
        split_leaf(leaf_id, key, value);
    }

    BTreeIterator BTree::begin()
    {
        PageId first_leaf_id = find_leftmost_leaf();
        return BTreeIterator(&bpm_, first_leaf_id, 0);
    }

    void BTree::split_leaf(PageId leaf_id, const std::string& new_key, const std::string& new_value)
    {
        PageData* old_raw_page = bpm_.fetch_page(leaf_id);
        Page old_leaf(*old_raw_page);

        // 1. Collect all existing records plus the new record
        std::vector<std::pair<std::string, std::string>> records;
        for (uint16_t i = 0; i < old_leaf.num_records(); ++i)
        {
            auto rec = old_leaf.get_record(i);
            // insert() is an upsert, so when the incoming key is already on the
            // page the old version is superseded. Keeping it would put two cells
            // with the same key into the tree.
            if (rec && rec->first != new_key)
            {
                records.push_back(*rec);
            }
        }
        records.push_back({new_key, new_value});

        std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        // 2. Allocate a new page for the right leaf
        PageId new_leaf_id;
        PageData* new_raw_page = bpm_.new_page(&new_leaf_id);
        Page new_leaf(*new_raw_page);

        bool was_root = old_leaf.is_root();
        uint32_t parent_id = old_leaf.parent_id();
        uint32_t old_next = old_leaf.next_leaf_id();

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

        // Both pages have changed
        bpm_.unpin_page(leaf_id, true);
        bpm_.unpin_page(new_leaf_id, true);

        // 5. Separator is the first key of the right leaf
        std::string separator_key = records[mid].first;

        insert_into_parent(leaf_id, separator_key, new_leaf_id);
    }

    void BTree::insert_into_parent(PageId left_child_id, const std::string& key, PageId right_child_id)
    {
        PageData* left_raw_page = bpm_.fetch_page(left_child_id);
        Page left_child(*left_raw_page);

        // If the root split, create a new internal node as the root
        if (left_child.is_root()) 
        {
            PageId new_root_id;
            PageData* new_root_raw = bpm_.new_page(&new_root_id);
            Page new_root(*new_root_raw);

            new_root.init(NodeType::INTERNAL, true);
            new_root.insert_internal_cell(key, left_child_id);
            new_root.set_rightmost_child(right_child_id);

            left_child.set_root(false);
            left_child.set_parent_id(new_root_id);

            PageData* right_raw = bpm_.fetch_page(right_child_id);
            Page right_child(*right_raw);
            right_child.set_parent_id(new_root_id);

            bpm_.unpin_page(right_child_id, true);
            bpm_.unpin_page(left_child_id, true);
            bpm_.unpin_page(new_root_id, true);

            root_page_id_ = new_root_id;

            // Update the Meta Page (Page 0) with the new root address
            PageData* meta_raw = bpm_.fetch_page(0);
            std::memcpy(meta_raw->data(), &root_page_id_, sizeof(PageId));
            bpm_.unpin_page(0, true);

            return;
        }

        // If the node was not root, insert into the existing parent node
        PageId parent_id = left_child.parent_id();
        
        // Unpin the left child because in the else branch (not root) we did not modify it
        bpm_.unpin_page(left_child_id, false);

        PageData* parent_raw = bpm_.fetch_page(parent_id);
        Page parent(*parent_raw);

        bool inserted = false;

        if (parent.rightmost_child() == left_child_id)
        {
            // The page we split was the rightmost child, so the separator simply
            // becomes the last cell and the new page becomes the rightmost child.
            inserted = parent.insert_internal_cell(key, left_child_id);
            if (inserted)
            {
                parent.set_rightmost_child(right_child_id);
            }
        }
        else
        {
            // The page we split is referenced by an existing cell. After the split
            // that cell's subtree starts at the separator, so the new cell
            // (separator -> left page) goes in front of it and the existing cell
            // has to be repointed at the new right page. Without this the right
            // page is unreachable from the root and every key in it is lost to
            // get(), even though scan() still sees it through the leaf chain.
            inserted = parent.insert_internal_cell(key, left_child_id);

            if (inserted)
            {
                // The cell we just inserted sits at idx, the stale reference to
                // the split page is the one directly behind it.
                uint16_t idx = parent.find_cell_index(key);
                if (idx + 1 < parent.num_records() &&
                    parent.get_internal_cell(idx + 1).second == left_child_id)
                {
                    parent.set_internal_child(idx + 1, right_child_id);
                }
            }
        }

        bpm_.unpin_page(parent_id, true);

        // Splitting internal nodes is not implemented yet: once a parent runs out
        // of room the separator cannot be recorded, and silently dropping it would
        // corrupt the tree. Fail loudly instead.
        if (!inserted)
        {
            throw std::runtime_error("BTree: internal node is full (internal node splits are not implemented)");
        }
    }

} // namespace minidb