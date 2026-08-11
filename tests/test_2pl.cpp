#include <gtest/gtest.h>
#include "minidb/core/Database.h"
#include "minidb/concurrency/TransactionManager.h"
#include "minidb/concurrency/LockManager.h"

#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>

using namespace minidb;

class Strict2PLTest : public ::testing::Test 
{
protected:
    const std::string test_db_file = "test_2pl.db";

    void SetUp() override 
    {
        std::filesystem::remove(test_db_file);
        std::filesystem::remove(test_db_file + ".log");
    }

    void TearDown() override 
    {
        std::filesystem::remove(test_db_file);
        std::filesystem::remove(test_db_file + ".log");
    }
};

TEST_F(Strict2PLTest, ExclusiveLockBlocksWriteUntilCommit) 
{
    LockManager lock_mgr;
    Database db(test_db_file);
    TransactionManager txn_mgr;

    db.set_lock_manager(&lock_mgr);
    txn_mgr.set_lock_manager(&lock_mgr); // We didn't forget to set this here :)

    db.set("account_a", "100");

    std::atomic<bool> txn1_written{false};
    std::atomic<bool> txn2_write_done{false};

    // Thread 1: Writer A
    std::thread t1([&]() 
        {
            auto txn1 = txn_mgr.begin();
            db.set("account_a", "200", txn1.get()); // Acquires X-Lock
            txn1_written = true;

            // Simulate long work while holding the lock
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            txn_mgr.commit(txn1.get());
        });

    // Thread 2: Writer B (attempts to update the same data)
    std::thread t2([&]() 
        {
            while (!txn1_written) std::this_thread::yield();

            auto txn2 = txn_mgr.begin();
            
            // This operation will BLOCK until t1 commits,
            // because an X-Lock conflicts with another X-Lock.
            db.set("account_a", "300", txn2.get()); 
            txn2_write_done = true;
            
            txn_mgr.commit(txn2.get());
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(txn2_write_done);
    
    // Verify that the final value is from Writer B
    auto txn_check = txn_mgr.begin();
    auto val = db.get("account_a", txn_check.get());
    EXPECT_EQ(*val, "300");
    txn_mgr.commit(txn_check.get());
}

TEST_F(Strict2PLTest, LockWaitTimeoutThrowsExceptionOnWrite) 
{
    LockManager lock_mgr;
    Database db(test_db_file);
    TransactionManager txn_mgr;

    db.set_lock_manager(&lock_mgr);
    txn_mgr.set_lock_manager(&lock_mgr);

    auto txn1 = txn_mgr.begin();
    db.set("A", "10", txn1.get()); // Acquires X-Lock

    std::atomic<bool> exception_thrown{false};

    std::thread t2([&]() 
        {
            auto txn2 = txn_mgr.begin();
            try 
            {
                // Transaction 2 attempts to WRITE (acquires X-Lock).
                // Since Transaction 1 has not committed yet, this will time out!
                db.set("A", "20", txn2.get());
            } 
            catch (const std::runtime_error& e) 
            {
                if (std::string(e.what()) == "Lock wait timeout exceeded") 
                {
                    exception_thrown = true;
                }
            }
            txn_mgr.abort(txn2.get(), &db);
        });

    t2.join();
    txn_mgr.commit(txn1.get());

    EXPECT_TRUE(exception_thrown);
}