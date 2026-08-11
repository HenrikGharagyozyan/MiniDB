#include <gtest/gtest.h>
#include "minidb/core/Database.h"
#include <filesystem>

class DatabaseTest : public ::testing::Test 
{
protected:
    const std::string db_file = "test_db_engine.db";

    void SetUp() override 
    {
        if (std::filesystem::exists(db_file)) 
        {
            std::filesystem::remove(db_file);
        }
    }

    void TearDown() override 
    {
        if (std::filesystem::exists(db_file)) 
        {
            std::filesystem::remove(db_file);
        }
    }
};

TEST_F(DatabaseTest, SetAndGet) 
{
    minidb::Database db(db_file);

    db.set("user", "Henrik");
    db.set("lang", "C++");

    EXPECT_EQ(db.get("user"), "Henrik");
    EXPECT_EQ(db.get("lang"), "C++");
    EXPECT_EQ(db.get("unknown"), std::nullopt);
}

TEST_F(DatabaseTest, OverwriteKey) 
{
    minidb::Database db(db_file);

    db.set("key", "version1");
    EXPECT_EQ(db.get("key"), "version1");

    db.set("key", "version2");
    EXPECT_EQ(db.get("key"), "version2");
}

TEST_F(DatabaseTest, RemoveKey) 
{
    minidb::Database db(db_file);

    db.set("key", "value");
    EXPECT_EQ(db.get("key"), "value");

    db.remove("key");
    EXPECT_EQ(db.get("key"), std::nullopt);
}

TEST_F(DatabaseTest, PersistenceAcrossInstances) 
{
    {
        minidb::Database db(db_file);
        db.set("persistent_key", "persistent_value");
    } // db closes and is destroyed

    {
        // Reopen the same file
        minidb::Database db(db_file);
        EXPECT_EQ(db.get("persistent_key"), "persistent_value");
    }
}