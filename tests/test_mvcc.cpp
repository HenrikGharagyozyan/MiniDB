#include <gtest/gtest.h>
#include "minidb/Database.h"
#include "minidb/TransactionManager.h"
#include "minidb/LockManager.h"
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
    // 1. Инициализируем данные в базе (начальное состояние)
    auto txn_init = txn_mgr_->begin();
    db_->set("user", "Henrik", txn_init.get());
    txn_mgr_->commit(txn_init.get()); // <-- ИСПРАВЛЕНО

    // 2. Старт Транзакции A (Читатель)
    auto txn_A = txn_mgr_->begin();

    // 3. Старт Транзакции B (Писатель)
    auto txn_B = txn_mgr_->begin();
    db_->set("user", "John", txn_B.get());

    // В B-Tree физически теперь лежит "John" с txn_id = txn_B!

    // 4. Транзакция A читает ключ "user".
    // Она НЕ блокируется и видит СТАРУЮ версию "Henrik"!
    auto val_A1 = db_->get("user", txn_A.get());
    ASSERT_TRUE(val_A1.has_value());
    EXPECT_EQ(*val_A1, "Henrik");

    // Транзакция B видит СВОИ собственные не закомиченные изменения ("John")
    auto val_B = db_->get("user", txn_B.get());
    ASSERT_TRUE(val_B.has_value());
    EXPECT_EQ(*val_B, "John");

    // 5. Транзакция B зафиксировала изменения (COMMIT)
    txn_mgr_->commit(txn_B.get()); // <-- ИСПРАВЛЕНО

    // 6. Транзакция A читает ключ СНОВА.
    // Благодаря Snapshot Isolation, она все равно продолжает видеть "Henrik",
    // потому что B была активна в момент создания снимка A!
    auto val_A2 = db_->get("user", txn_A.get());
    ASSERT_TRUE(val_A2.has_value());
    EXPECT_EQ(*val_A2, "Henrik");

    // 7. Старт НОВОЙ Транзакции C (Читатель из будущего)
    auto txn_C = txn_mgr_->begin();
    auto val_C = db_->get("user", txn_C.get());
    ASSERT_TRUE(val_C.has_value());
    EXPECT_EQ(*val_C, "John"); // C застала закомиченную версию!

    txn_mgr_->commit(txn_A.get()); // <-- ИСПРАВЛЕНО
    txn_mgr_->commit(txn_C.get()); // <-- ИСПРАВЛЕНО
}