#include <gtest/gtest.h>
#include "minidb/Database.h"
#include "minidb/TransactionManager.h"
#include "minidb/LockManager.h"

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

TEST_F(Strict2PLTest, ExclusiveLockBlocksReadUntilCommit) 
{
    LockManager lock_mgr;
    Database db(test_db_file);
    TransactionManager txn_mgr;

    db.set_lock_manager(&lock_mgr);
    txn_mgr.set_lock_manager(&lock_mgr);

    // Prepare initial value
    db.set("account_a", "100");

    std::atomic<bool> txn1_written{false};
    std::atomic<bool> txn2_read_done{false};
    std::string txn2_read_val = "";

    // Thread 1: start transaction 1 and update account_a -> 200
    std::thread t1([&]() 
        {
            auto txn1 = txn_mgr.begin();
            
            // Acquire X-Lock on account_a
            db.set("account_a", "200", txn1.get());
            txn1_written = true;

            // Simulate long-running work inside the transaction (150 ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(150));

            // Commit the transaction — this should release the lock
            txn_mgr.commit(txn1.get());
        });

    // Thread 2: attempts to read account_a while transaction 1 is running
    std::thread t2([&]() 
        {
            // Wait until thread 1 performs db.set
            while (!txn1_written) 
            {
                std::this_thread::yield();
            }

            auto txn2 = txn_mgr.begin();

            // This get() should block on the S-Lock until t1 commits()
            auto val = db.get("account_a", txn2.get());
            if (val) 
            {
                txn2_read_val = *val;
            }
            txn2_read_done = true;

            txn_mgr.commit(txn2.get());
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(txn2_read_done);
    // Thread 2 should wait for t1 to finish and then read the updated value "200"
    EXPECT_EQ(txn2_read_val, "200");
}

TEST_F(Strict2PLTest, LockWaitTimeoutThrowsException) 
{
    LockManager lock_mgr;
    Database db(test_db_file);
    TransactionManager txn_mgr;

    db.set_lock_manager(&lock_mgr);
    txn_mgr.set_lock_manager(&lock_mgr);

    // Thread 1 (main): start a transaction and lock key "A"
    auto txn1 = txn_mgr.begin();
    db.set("A", "10", txn1.get()); // Acquire X-Lock on "A"

    std::atomic<bool> exception_thrown{false};

    // Thread 2: attempts to read "A"
    std::thread t2([&]() 
        {
            auto txn2 = txn_mgr.begin();
            try 
            {
                // T2 will try to acquire an S-Lock on "A".
                // Since T1 holds the X-Lock, T2 will wait 50ms and then throw an exception.
                db.get("A", txn2.get());
            } 
            catch (const std::runtime_error& e) 
            {
                std::string err_msg = e.what();
                if (err_msg == "Lock wait timeout exceeded") 
                {
                    exception_thrown = true;
                }
            }
            txn_mgr.abort(txn2.get(), &db);
        });

    t2.join();
    
    // Finish the first transaction
    txn_mgr.commit(txn1.get());

    // Verify that the timeout really triggered!
    EXPECT_TRUE(exception_thrown);
}