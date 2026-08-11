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

TEST_F(Strict2PLTest, ExclusiveLockBlocksWriteUntilCommit) 
{
    LockManager lock_mgr;
    Database db(test_db_file);
    TransactionManager txn_mgr;

    db.set_lock_manager(&lock_mgr);
    txn_mgr.set_lock_manager(&lock_mgr); // Здесь мы про это не забыли :)

    db.set("account_a", "100");

    std::atomic<bool> txn1_written{false};
    std::atomic<bool> txn2_write_done{false};

    // Поток 1: Писатель А
    std::thread t1([&]() 
        {
            auto txn1 = txn_mgr.begin();
            db.set("account_a", "200", txn1.get()); // Берет X-Lock
            txn1_written = true;

            // Имитируем долгую работу, удерживая блокировку
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            txn_mgr.commit(txn1.get());
        });

    // Поток 2: Писатель B (пытается обновить те же данные)
    std::thread t2([&]() 
        {
            while (!txn1_written) std::this_thread::yield();

            auto txn2 = txn_mgr.begin();
            
            // Эта операция ЗАБЛОКИРУЕТСЯ, пока t1 не сделает commit,
            // так как X-Lock конфликтует с X-Lock.
            db.set("account_a", "300", txn2.get()); 
            txn2_write_done = true;
            
            txn_mgr.commit(txn2.get());
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(txn2_write_done);
    
    // Проверяем, что в итоге сохранилось последнее значение от Писателя B
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
    db.set("A", "10", txn1.get()); // Берет X-Lock

    std::atomic<bool> exception_thrown{false};

    std::thread t2([&]() 
        {
            auto txn2 = txn_mgr.begin();
            try 
            {
                // Транзакция 2 пытается сделать ЗАПИСЬ (берет X-Lock).
                // Так как Транзакция 1 еще не закомитилась, будет таймаут!
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