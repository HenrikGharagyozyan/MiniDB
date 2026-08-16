#include <gtest/gtest.h>
#include "minidb/index/BTree.h"
#include "minidb/storage/Pager.h"
#include "minidb/buffer/BufferPoolManager.h"
#include <cstdio>
#include <filesystem>
#include <string>

class BTreeTest : public ::testing::Test 
{
protected:
    const std::string test_file = "test_btree.db";

    void SetUp() override 
    {
        if (std::filesystem::exists(test_file)) 
        {
            std::filesystem::remove(test_file);
        }
    }

    void TearDown() override 
    {
        if (std::filesystem::exists(test_file)) 
        {
            std::filesystem::remove(test_file);
        }
    }
};

TEST_F(BTreeTest, InsertAndGetSingleNode) 
{
    minidb::Pager pager(test_file);
    // Create a buffer pool for tests
    minidb::BufferPoolManager bpm(10, pager);

    minidb::PageId root_id;
    minidb::PageData* raw_page = bpm.new_page(&root_id);

    minidb::Page root_page(*raw_page);
    root_page.init(minidb::NodeType::LEAF, true);
    // Unpin and mark it dirty because we initialized it
    bpm.unpin_page(root_id, true); 

    // Pass BPM instead of Pager
    minidb::BTree btree(bpm, root_id);

    btree.insert("alpha", "100");
    btree.insert("beta", "200");

    EXPECT_EQ(btree.get("alpha"), "100");
    EXPECT_EQ(btree.get("beta"), "200");
    EXPECT_EQ(btree.get("gamma"), std::nullopt);
}

TEST_F(BTreeTest, LeafNodeSplit) 
{
    minidb::Pager pager(test_file);
    minidb::BufferPoolManager bpm(10, pager);

    minidb::PageId meta_id;
    bpm.new_page(&meta_id); // Skip Page 0 (simulate the Meta Page)
    bpm.unpin_page(meta_id, false); 

    minidb::PageId root_id;
    minidb::PageData* raw_page = bpm.new_page(&root_id);

    minidb::Page root_page(*raw_page);
    root_page.init(minidb::NodeType::LEAF, true);
    bpm.unpin_page(root_id, true);

    minidb::BTree btree(bpm, root_id);

    // Insert enough long keys/values to force a split
    for (int i = 0; i < 100; ++i) 
    {
        std::string key = "key_" + std::to_string(i);
        std::string val = "value_payload_data_" + std::to_string(i * 10);
        btree.insert(key, val);
    }

    // Verify that all 100 keys read back correctly
    for (int i = 0; i < 100; ++i)
    {
        std::string key = "key_" + std::to_string(i);
        std::string expected_val = "value_payload_data_" + std::to_string(i * 10);
        EXPECT_EQ(btree.get(key), expected_val);
    }
}

// Helper: build a tree on a fresh file with page 0 reserved for the meta page
static minidb::PageId make_root(minidb::BufferPoolManager& bpm)
{
    minidb::PageId meta_id;
    bpm.new_page(&meta_id); // Skip Page 0 (simulate the Meta Page)
    bpm.unpin_page(meta_id, false);

    minidb::PageId root_id;
    minidb::PageData* raw_page = bpm.new_page(&root_id);

    minidb::Page root_page(*raw_page);
    root_page.init(minidb::NodeType::LEAF, true);
    bpm.unpin_page(root_id, true);

    return root_id;
}

// Payload big enough that a few hundred records span many leaf pages,
// which forces the root to become an internal node.
static std::string big_value(int i)
{
    return std::string(80, 'v') + std::to_string(i);
}

// Regression: internal cells are laid out as [child_id][key_len][key], while leaf
// cells are [key_len][val_len][key][value]. Decoding an internal cell with the leaf
// layout made every routing decision read a garbage key, so once the tree grew a
// second level get() landed on the wrong leaf and most keys became unreachable.
TEST_F(BTreeTest, LookupAcrossMultiLevelTree)
{
    minidb::Pager pager(test_file);
    minidb::BufferPoolManager bpm(10, pager);
    minidb::BTree btree(bpm, make_root(bpm));

    const int num_keys = 400;

    // Plain integers, so insertion order differs from lexicographic key order
    for (int i = 0; i < num_keys; ++i)
    {
        btree.insert("key_" + std::to_string(i), big_value(i));
    }

    for (int i = 0; i < num_keys; ++i)
    {
        EXPECT_EQ(btree.get("key_" + std::to_string(i)), big_value(i))
            << "key_" << i << " is unreachable from the root";
    }
}

// Regression: when the leaf that splits is not the parent's rightmost child, the
// parent cell that pointed at it has to be repointed at the new right page.
// Without that the right page stays orphaned and its keys are lost to get().
// Descending insertion always splits the leftmost leaf, which is never rightmost.
TEST_F(BTreeTest, SplitOfNonRightmostLeaf)
{
    minidb::Pager pager(test_file);
    minidb::BufferPoolManager bpm(10, pager);
    minidb::BTree btree(bpm, make_root(bpm));

    const int num_keys = 400;

    for (int i = num_keys - 1; i >= 0; --i)
    {
        char key[32];
        std::snprintf(key, sizeof(key), "key_%05d", i);
        btree.insert(key, big_value(i));
    }

    for (int i = 0; i < num_keys; ++i)
    {
        char key[32];
        std::snprintf(key, sizeof(key), "key_%05d", i);
        EXPECT_EQ(btree.get(key), big_value(i))
            << key << " is unreachable from the root";
    }
}