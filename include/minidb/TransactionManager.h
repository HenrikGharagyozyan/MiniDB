#pragma once

#include "minidb/Transaction.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace minidb 
{

    class Database;

    class TransactionManager 
    {
    public:
        TransactionManager() = default;
        ~TransactionManager() = default;

        // Begin a new transaction
        std::shared_ptr<Transaction> begin();

        // Commit (successfully complete)
        void commit(Transaction* txn);

        // Abort (cancel)
        void abort(Transaction* txn, Database* db);

    private:
        // Counter for issuing unique IDs
        std::atomic<txn_id_t> next_txn_id_{0};
        
        std::mutex mutex_;
        
        // Store active transactions
        std::unordered_map<txn_id_t, std::shared_ptr<Transaction>> active_transactions_;
    };

} // namespace minidb