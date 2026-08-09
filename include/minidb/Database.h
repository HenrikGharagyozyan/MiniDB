#pragma once

#include "minidb/Pager.h"
#include "minidb/BufferPoolManager.h"
#include "minidb/Wal.h"
#include "minidb/BTree.h"
#include "minidb/Transaction.h"
#include "minidb/LockManager.h"

#include <string>
#include <optional>
#include <memory>


namespace minidb 
{

    class Database 
    {
    public:
        explicit Database(std::string filename);
        ~Database() = default;

        void set_lock_manager(LockManager* lock_mgr) { lock_mgr_ = lock_mgr; }

        void set(const std::string& key, const std::string& value, Transaction* txn = nullptr);
        std::optional<std::string> get(const std::string& key, Transaction* txn = nullptr);
        void remove(const std::string& key, Transaction* txn = nullptr);

    private:
        std::string filename_;
       
        std::unique_ptr<Pager> pager_;
        std::unique_ptr<BufferPoolManager> bpm_;
        std::unique_ptr<Wal> wal_;
        std::unique_ptr<BTree> btree_;

        LockManager* lock_mgr_{ nullptr };
    };

} // namespace minidb