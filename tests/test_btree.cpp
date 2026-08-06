#include <gtest/gtest.h>
#include "minidb/BTree.h"
#include "minidb/Pager.h"
#include <filesystem>

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

    // Initialize the root as a leaf node
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