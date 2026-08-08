#include "minidb/TransactionManager.h"
#include "minidb/Database.h"
#include "minidb/LogRecord.h"

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

    void TransactionManager::abort(Transaction* txn, Database* db) 
    {
        if (txn->get_state() != TransactionState::ACTIVE) 
        {
            return;
        }

        // In the next commit, rollback (undo log) logic will be added here
        txn->set_state(TransactionState::ABORTED);

        // Get the logs and process them in reverse order (rbegin -> rend)
        const auto& logs = txn->get_undo_logs();
        for (auto it = logs.rbegin(); it != logs.rend(); ++it) 
        {
            auto log = *it;
            
            // Execute reverse operations. Pass nullptr so rollback is not logged!
            if (log->get_type() == UndoLogType::INSERT) 
            {
                // If it was INSERT, undo by deleting the key
                db->remove(log->get_key(), nullptr); 
            } 
            else if (log->get_type() == UndoLogType::UPDATE) 
            {
                // If it was UPDATE, restore the old value
                db->set(log->get_key(), log->get_old_value(), nullptr);
            } 
            else if (log->get_type() == UndoLogType::DELETE) 
            {
                // If it was DELETE, restore the old value by re-inserting it
                db->set(log->get_key(), log->get_old_value(), nullptr);
            }
        }

        // Remove the transaction from active transactions
        std::lock_guard<std::mutex> lock(mutex_);
        active_transactions_.erase(txn->get_transaction_id());
    }

} // namespace minidb