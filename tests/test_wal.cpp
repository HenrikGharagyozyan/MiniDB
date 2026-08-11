#include <gtest/gtest.h>
#include "minidb/storage/Wal.h"
#include <filesystem>

class WalTest : public ::testing::Test 
{
protected:
    const std::string log_file = "test_wal.log";

    void SetUp() override 
    {
        if (std::filesystem::exists(log_file)) 
        {
            std::filesystem::remove(log_file);
        }
    }

    void TearDown() override 
    {
        if (std::filesystem::exists(log_file)) 
        {
            std::filesystem::remove(log_file);
        }
    }
};

TEST_F(WalTest, WriteAndRecover) 
{
    {
        minidb::Wal wal(log_file);
        wal.append_set("user_id", "42");
        wal.append_delete("session_id");
        wal.flush();
    } // WAL closes, simulating a crash before pages are flushed

    {
        minidb::Wal wal(log_file);
        auto records = wal.recover();
        
        ASSERT_EQ(records.size(), 2);
        
        EXPECT_EQ(records[0].type, minidb::LogRecordType::SET);
        EXPECT_EQ(records[0].key, "user_id");
        EXPECT_EQ(records[0].value, "42");
        
        EXPECT_EQ(records[1].type, minidb::LogRecordType::DELETE);
        EXPECT_EQ(records[1].key, "session_id");
        EXPECT_EQ(records[1].value, "");
    }
}

TEST_F(WalTest, ClearLog) 
{
    {
        minidb::Wal wal(log_file);
        wal.append_set("key", "value");
        wal.flush();
        wal.clear(); // Simulate successful flush of pages to disk
    }

    {
        minidb::Wal wal(log_file);
        auto records = wal.recover();
        EXPECT_TRUE(records.empty()); // The log should be empty
    }
}