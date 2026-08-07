#include <gtest/gtest.h>

#include "minidb/Pager.h"
#include "minidb/BufferPoolManager.h"

#include <cstdio>
#include <cstring>


class BufferPoolManagerTest : public ::testing::Test 
{
protected:
    const std::string test_file = "test_bpm.db";

    void TearDown() override 
    {
        std::remove(test_file.c_str());
    }
};

TEST_F(BufferPoolManagerTest, EvictionAndFetchTest) 
{
    minidb::Pager pager(test_file);
    // Create a pool with ONLY 2 frames (to artificially force low memory)
    minidb::BufferPoolManager bpm(2, pager);

    minidb::PageId page0_id, page1_id, page2_id;

    // 1. Create 2 pages (pool is 100% full)
    auto page0 = bpm.new_page(&page0_id);
    auto page1 = bpm.new_page(&page1_id);

    ASSERT_NE(page0, nullptr);
    ASSERT_NE(page1, nullptr);

    // Modify data in page0 and mark it dirty (is_dirty = true)
    std::memcpy(page0->data(), "Henrik", 7);
    bpm.unpin_page(page0_id, true);
    
    // Just unpin page1 without modifications
    bpm.unpin_page(page1_id, false);

    // 2. Request the 3rd page. There is no free space!
    // The LRU cache should evict the oldest page (page0) and write "Henrik" to disk.
    auto page2 = bpm.new_page(&page2_id);
    ASSERT_NE(page2, nullptr);
    bpm.unpin_page(page2_id, false);

    // 3. Request page0 again.
    auto fetched_page0 = bpm.fetch_page(page0_id);
    ASSERT_NE(fetched_page0, nullptr);
    
    // Check that the data survived the round trip from RAM to disk and back.
    EXPECT_STREQ(reinterpret_cast<char*>(fetched_page0->data()), "Henrik");
    
    bpm.unpin_page(page0_id, false);
}