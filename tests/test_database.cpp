#include <gtest/gtest.h>
#include "minidb/core/Database.h"
#include <filesystem>
#include <set>
#include <string>

class DatabaseTest : public ::testing::Test
{
protected:
    const std::string db_file = "test_db_engine.db";

    // The WAL has to go too, otherwise the next Database replays the previous
    // test's records during recovery.
    void remove_db_files()
    {
        std::filesystem::remove(db_file);
        std::filesystem::remove(db_file + ".log");
    }

    void SetUp() override
    {
        remove_db_files();
    }

    void TearDown() override
    {
        remove_db_files();
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

// Regression: set() is an upsert, but when the write that overflows a leaf targets
// a key already stored on that page, the split used to keep the old record and add
// the new one, leaving two cells with the same key in the tree.
TEST_F(DatabaseTest, UpdateThatSplitsLeafKeepsOneVersionPerKey)
{
    minidb::Database db(db_file);

    const int num_rows = 200;
    for (int i = 0; i < num_rows; ++i)
    {
        db.set("key_" + std::to_string(i), "small");
    }

    // Rewriting with a much larger value forces splits while the key is already
    // present on the page
    const std::string big_value(120, 'x');
    for (int i = 0; i < num_rows; ++i)
    {
        db.set("key_" + std::to_string(i), big_value);
    }

    auto rows = db.scan();
    EXPECT_EQ(rows.size(), static_cast<size_t>(num_rows));

    std::set<std::string> unique_keys;
    for (const auto& [key, value] : rows)
    {
        unique_keys.insert(key);
        EXPECT_EQ(value, big_value);
    }
    EXPECT_EQ(unique_keys.size(), static_cast<size_t>(num_rows));

    for (int i = 0; i < num_rows; ++i)
    {
        EXPECT_EQ(db.get("key_" + std::to_string(i)), big_value);
    }
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