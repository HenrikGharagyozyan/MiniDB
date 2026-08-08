#include "minidb/TransactionManager.h"

namespace minidb 
{

    std::shared_ptr<Transaction> TransactionManager::begin() 
    {
        // Issue an ID and increment the counter in a thread-safe way
        txn_id_t txn_id = next_txn_id_.fetch_add(1);
        auto txn = std::make_shared<Transaction>(txn_id);

        std::lock_guard<std::mutex> lock(mutex_);
        active_transactions_[txn_id] = txn;

        return txn;
    }

    void TransactionManager::commit(Transaction* txn) 
    {
        if (txn->get_state() != TransactionState::ACTIVE) 
        {
            return;
        }

        // In the next commit, undo log cleanup logic will be added here
        txn->set_state(TransactionState::COMMITTED);

        std::lock_guard<std::mutex> lock(mutex_);
        active_transactions_.erase(txn->get_transaction_id());
    }

    void TransactionManager::abort(Transaction* txn) 
    {
        if (txn->get_state() != TransactionState::ACTIVE) 
        {
            return;
        }

        // In the next commit, rollback (undo log) logic will be added here
        txn->set_state(TransactionState::ABORTED);

        std::lock_guard<std::mutex> lock(mutex_);
        active_transactions_.erase(txn->get_transaction_id());
    }

} // namespace minidb