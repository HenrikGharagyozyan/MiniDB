#include <gtest/gtest.h>
#include "minidb/BTree.h"
#include "minidb/Pager.h"
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
    minidb::PageId root_id = pager.allocate_page();

    minidb::PageData raw_page{};
    minidb::Page root_page(raw_page);
    root_page.init(minidb::NodeType::LEAF, true);
    pager.write_page(root_id, raw_page);

    minidb::BTree btree(pager, root_id);

    btree.insert("alpha", "100");
    btree.insert("beta", "200");

    EXPECT_EQ(btree.get("alpha"), "100");
    EXPECT_EQ(btree.get("beta"), "200");
    EXPECT_EQ(btree.get("gamma"), std::nullopt);
}

TEST_F(BTreeTest, LeafNodeSplit) 
{
    minidb::Pager pager(test_file);
    pager.allocate_page(); // Skip Page 0 (simulate Meta Page)
    minidb::PageId root_id = pager.allocate_page();

    minidb::PageData raw_page{};
    minidb::Page root_page(raw_page);
    root_page.init(minidb::NodeType::LEAF, true);
    pager.write_page(root_id, raw_page);

    minidb::BTree btree(pager, root_id);

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