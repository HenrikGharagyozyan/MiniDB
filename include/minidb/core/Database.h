#pragma once

#include "minidb/storage/Pager.h"
#include "minidb/buffer/BufferPoolManager.h"
#include "minidb/storage/Wal.h"
#include "minidb/index/BTree.h"
#include "minidb/concurrency/Transaction.h"
#include "minidb/concurrency/LockManager.h"

#include <string>
#include <optional>
#include <memory>


namespace minidb 
{

    class TransactionManager;
    struct TupleMeta;

    class Database 
    {
    public:
        explicit Database(std::string filename);
        ~Database() = default;

        void set_lock_manager(LockManager* lock_mgr) { lock_mgr_ = lock_mgr; }

        void set_transaction_manager(TransactionManager* txn_mgr) { txn_mgr_ = txn_mgr; }

        void set(const std::string& key, const std::string& value, Transaction* txn = nullptr);
        std::optional<std::string> get(const std::string& key, Transaction* txn = nullptr);
        void remove(const std::string& key, Transaction* txn = nullptr);

    private:
        // Helpers for packing and unpacking data for the B-Tree
        std::string encode_value(const TupleMeta& meta, const std::string& val);
        std::pair<TupleMeta, std::string> decode_value(const std::string& raw_val);

    private:
        std::string filename_;
       
        std::unique_ptr<Pager> pager_;
        std::unique_ptr<BufferPoolManager> bpm_;
        std::unique_ptr<Wal> wal_;
        std::unique_ptr<BTree> btree_;

        LockManager* lock_mgr_{ nullptr };

        TransactionManager* txn_mgr_{ nullptr };
    };

} // namespace minidb