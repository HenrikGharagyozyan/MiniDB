#pragma once

#include "minidb/ReadView.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_set>
#include <string>


namespace minidb 
{

    using txn_id_t = uint64_t;

    enum class TransactionState 
    {
        ACTIVE,
        COMMITTED,
        ABORTED
    };


    class LogRecord;


    class Transaction 
    {
    public:
        explicit Transaction(txn_id_t txn_id)
            : txn_id_(txn_id), state_(TransactionState::ACTIVE) 
        {
        }

        ~Transaction() = default;

        txn_id_t get_transaction_id() const { return txn_id_; }
        TransactionState get_state() const { return state_; }
        void set_state(TransactionState state) { state_ = state; }

        void add_log_record(std::shared_ptr<LogRecord> record) { undo_logs_.push_back(record); }
        const std::vector<std::shared_ptr<LogRecord>>& get_undo_logs() const { return undo_logs_; }

        // --- Strict 2PL: held lock management ---
        void add_shared_lock(const std::string& key) { shared_locks_.insert(key); }
        void add_exclusive_lock(const std::string& key) { exclusive_locks_.insert(key); }

        bool holds_shared_lock(const std::string& key) const { return shared_locks_.count(key) > 0; }
        bool holds_exclusive_lock(const std::string& key) const { return exclusive_locks_.count(key) > 0; }

        const std::unordered_set<std::string>& get_shared_locks() const { return shared_locks_; }
        const std::unordered_set<std::string>& get_exclusive_locks() const { return exclusive_locks_; }

        void remove_shared_lock(const std::string& key) { shared_locks_.erase(key); }

        void clear_locks() 
        {
            shared_locks_.clear();
            exclusive_locks_.clear();
        }
    private:
        txn_id_t txn_id_;
        TransactionState state_;

        // Vector storing all changes made by this transaction
        std::vector<std::shared_ptr<LogRecord>> undo_logs_;

        // Locks held by the transaction until the end (Strict 2PL)
        std::unordered_set<std::string> shared_locks_;
        std::unordered_set<std::string> exclusive_locks_;
    };

} // namespace minidb