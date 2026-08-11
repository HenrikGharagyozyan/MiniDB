#include <gtest/gtest.h>
#include "minidb/core/Database.h"
#include "minidb/concurrency/TransactionManager.h"
#include "minidb/concurrency/LockManager.h"
#include <filesystem>

using namespace minidb;

class MVCCTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        std::filesystem::remove("test_mvcc.db");
        std::filesystem::remove("test_mvcc.db.log");

        db_ = std::make_unique<Database>("test_mvcc.db");
        lock_mgr_ = std::make_unique<LockManager>();
        txn_mgr_ = std::make_unique<TransactionManager>();

        db_->set_lock_manager(lock_mgr_.get());
        txn_mgr_->set_lock_manager(lock_mgr_.get()); 
        db_->set_transaction_manager(txn_mgr_.get());
    }

    void TearDown() override 
    {
        db_.reset();
        lock_mgr_.reset();
        txn_mgr_.reset();

        std::filesystem::remove("test_mvcc.db");
        std::filesystem::remove("test_mvcc.db.log");
    }

    std::unique_ptr<Database> db_;
    std::unique_ptr<LockManager> lock_mgr_;
    std::unique_ptr<TransactionManager> txn_mgr_;
};

TEST_F(MVCCTest, NonBlockingReadersAndSnapshotIsolation) 
{
    // 1. Initialize data in the database (initial state)
    auto txn_init = txn_mgr_->begin();
    db_->set("user", "Henrik", txn_init.get());
    txn_mgr_->commit(txn_init.get()); // <-- FIXED

    // 2. Start Transaction A (Reader)
    auto txn_A = txn_mgr_->begin();

    // 3. Start Transaction B (Writer)
    auto txn_B = txn_mgr_->begin();
    db_->set("user", "John", txn_B.get());

    // The B-Tree now physically contains "John" with txn_id = txn_B!

    // 4. Transaction A reads key "user".
    // It does NOT block and sees the OLD version "Henrik"!
    auto val_A1 = db_->get("user", txn_A.get());
    ASSERT_TRUE(val_A1.has_value());
    EXPECT_EQ(*val_A1, "Henrik");

    // Transaction B sees its own uncommitted changes ("John")
    auto val_B = db_->get("user", txn_B.get());
    ASSERT_TRUE(val_B.has_value());
    EXPECT_EQ(*val_B, "John");

    // 5. Transaction B committed the changes (COMMIT)
    txn_mgr_->commit(txn_B.get()); // <-- FIXED

    // 6. Transaction A reads the key AGAIN.
    // Thanks to Snapshot Isolation, it still sees "Henrik",
    // because B was active when snapshot A was created!
    auto val_A2 = db_->get("user", txn_A.get());
    ASSERT_TRUE(val_A2.has_value());
    EXPECT_EQ(*val_A2, "Henrik");

    // 7. Start a NEW Transaction C (a reader from the future)
    auto txn_C = txn_mgr_->begin();
    auto val_C = db_->get("user", txn_C.get());
    ASSERT_TRUE(val_C.has_value());
    EXPECT_EQ(*val_C, "John"); // C observed the committed version!

    txn_mgr_->commit(txn_A.get()); // <-- FIXED
    txn_mgr_->commit(txn_C.get()); // <-- FIXED
}