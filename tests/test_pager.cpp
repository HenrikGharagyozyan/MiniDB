#include <gtest/gtest.h>
#include "minidb/Pager.h"
#include <filesystem>

TEST(PagerTest, AllocateAndReadPage) 
{
    const std::string test_file = "test_pager.db";
    
    // Clean up the file before the test if it remained from previous runs
    if (std::filesystem::exists(test_file)) 
    {
        std::filesystem::remove(test_file);
    }

    minidb::Pager pager(test_file);
    EXPECT_EQ(pager.num_pages(), 0);

    minidb::PageId id = pager.allocate_page();
    EXPECT_EQ(id, 0);
    EXPECT_EQ(pager.num_pages(), 1);

    minidb::PageData write_data{};
    write_data[0] = 42;
    write_data[4095] = 24;
    
    EXPECT_TRUE(pager.write_page(id, write_data));

    minidb::PageData read_data{};
    EXPECT_TRUE(pager.read_page(id, read_data));
    
    EXPECT_EQ(read_data[0], 42);
    EXPECT_EQ(read_data[4095], 24);

    std::filesystem::remove(test_file); // Clean up after ourselves
}