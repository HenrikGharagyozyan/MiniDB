#include <gtest/gtest.h>
#include "minidb/LockManager.h"

#include <thread>
#include <chrono>
#include <atomic>

using namespace minidb;

// 1. Test: Multiple threads can hold a Shared Lock (S-Lock) concurrently
TEST(LockManagerTest, ConcurrentSharedLocks) 
{
    LockManager lock_mgr;
    std::string key = "user_account";

    std::atomic<bool> t1_acquired{false};
    std::atomic<bool> t2_acquired{false};

    // Thread 1 acquires the S-Lock and holds it for 100 ms
    std::thread t1([&]() 
        {
            lock_mgr.lock_shared(key);
            t1_acquired = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            lock_mgr.unlock_shared(key);
        });

    // Thread 2 starts concurrently and also requests the S-Lock
    std::thread t2([&]() 
        {
            // Wait until thread 1 has definitely acquired the lock
            while (!t1_acquired) 
            {
                std::this_thread::yield();
            }

            lock_mgr.lock_shared(key);
            t2_acquired = true; // Should acquire access IMMEDIATELY without waiting for t1 to finish
            lock_mgr.unlock_shared(key);
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(t1_acquired);
    EXPECT_TRUE(t2_acquired);
}

// 2. Test: Exclusive Lock (X-Lock) blocks other threads
TEST(LockManagerTest, ExclusiveLockBlocksOthers) 
{
    LockManager lock_mgr;
    std::string key = "counter";

    std::atomic<bool> x_locked{false};
    std::atomic<bool> s_locked_after_x_released{false};

    // Thread 1 acquires the X-Lock
    std::thread t1([&]() 
        {
            lock_mgr.lock_exclusive(key);
            x_locked = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            lock_mgr.unlock_exclusive(key);
            x_locked = false;
        });

    // Thread 2 attempts to acquire an S-Lock on the same key
    std::thread t2([&]() 
        {
            while (!x_locked) 
            {
                std::this_thread::yield();
            }

            // This call should BLOCK until t1 releases the X-Lock (or timeout)
            bool success = lock_mgr.lock_shared(key);
            
            if (success) 
            {
                // If we reach here, t1 should have already released the lock (x_locked == false)
                if (!x_locked) 
                {
                    s_locked_after_x_released = true;
                }
                
                lock_mgr.unlock_shared(key);
            }
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(s_locked_after_x_released);
}