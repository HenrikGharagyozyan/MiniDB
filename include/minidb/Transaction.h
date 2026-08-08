#pragma once

#include <cstdint>
#include <vector>
#include <memory>


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

    private:
        txn_id_t txn_id_;
        TransactionState state_;

        // Vector storing all changes made by this transaction
        std::vector<std::shared_ptr<LogRecord>> undo_logs_;
    };

} // namespace minidb