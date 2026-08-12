#pragma once

#include "minidb/concurrency/Transaction.h"
#include "minidb/concurrency/LockManager.h"
#include "minidb/concurrency/LogRecord.h"

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
        explicit TransactionManager(LockManager* lock_manager = nullptr)
            : lock_manager_(lock_manager) 
        {
        }

        ~TransactionManager() = default;

        void set_lock_manager(LockManager* lock_manager) { lock_manager_ = lock_manager; }

        // Begin a new transaction
        std::shared_ptr<Transaction> begin();

        // Commit the transaction and release locks
        void commit(Transaction* txn);

        // Abort the transaction (rollback changes) and release locks
        void abort(Transaction* txn, Database* db);

        // Method to generate a global LSN
        lsn_t get_next_lsn() { return next_lsn_.fetch_add(1); }

        // Method to store a record in the global Undo Log
        void add_undo_record(std::shared_ptr<LogRecord> record);

        // Method to retrieve an old version by LSN
        std::shared_ptr<LogRecord> get_undo_record(lsn_t lsn);

        // Очистка устаревших записей Undo Log (Garbage Collection)
        void vacuum();

    private:
        // Release all locks held by the transaction
        void release_locks(Transaction* txn);

    private:
        LockManager* lock_manager_{nullptr};

        // Counter for issuing unique IDs
        std::atomic<txn_id_t> next_txn_id_{0};
        
        std::mutex mutex_;
        
        // Store active transactions
        std::unordered_map<txn_id_t, std::shared_ptr<Transaction>> active_transactions_;


        std::atomic<lsn_t> next_lsn_{1}; // Start from 1. (0 means "no previous version")
        
        std::mutex undo_mutex_;
        std::unordered_map<lsn_t, std::shared_ptr<LogRecord>> global_undo_log_;
    };

} // namespace minidb