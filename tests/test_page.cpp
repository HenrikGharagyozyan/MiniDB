#include <gtest/gtest.h>
#include "minidb/storage/Page.h"

TEST(PageTest, InitAndInsertRecord) 
{
    minidb::PageData raw_data{};
    minidb::Page page(raw_data);
    page.init();

    EXPECT_EQ(page.num_records(), 0);

    EXPECT_TRUE(page.insert_record("key1", "value1"));
    EXPECT_EQ(page.num_records(), 1);

    auto rec = page.get_record(0);
    ASSERT_TRUE(rec.has_value());
    EXPECT_EQ(rec->first, "key1");
    EXPECT_EQ(rec->second, "value1");
}

TEST(PageTest, OutOfBoundsCheck) 
{
    minidb::PageData raw_data{};
    minidb::Page page(raw_data);
    page.init();

    auto rec = page.get_record(0);
    EXPECT_FALSE(rec.has_value()); // No records exist, should return nullopt
}