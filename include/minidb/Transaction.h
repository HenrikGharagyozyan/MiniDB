#pragma once

#include <cstdint>


namespace minidb 
{

    using txn_id_t = uint64_t;

    enum class TransactionState 
    {
        ACTIVE,
        COMMITTED,
        ABORTED
    };


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

    private:
        txn_id_t txn_id_;
        TransactionState state_;
        
    };

} // namespace minidb