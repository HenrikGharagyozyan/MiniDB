#include "minidb/TransactionManager.h"
#include "minidb/Database.h"
#include "minidb/LogRecord.h"


namespace minidb 
{

    std::shared_ptr<Transaction> TransactionManager::begin() 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Issue an ID and increment the counter in a thread-safe way
        txn_id_t txn_id = next_txn_id_.fetch_add(1);

        // Собираем список ID всех текущих АКТИВНЫХ транзакций
        std::unordered_set<txn_id_t> active_ids;
        for (const auto& [id, txn_ptr] : active_transactions_) 
        {
            active_ids.insert(id);
        }

        // Создаем ReadView: наш ID, список активных, и ID следующей транзакции
        // (next_txn_id_.load() вернет ID, который получит следующая транзакция)
        ReadView view(txn_id, active_ids, next_txn_id_.load());

        // Передаем снимок в конструктор транзакции
        auto txn = std::make_shared<Transaction>(txn_id, std::move(view));

        // Добавляем новую транзакцию в список активных
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

        // Strict 2PL: release all locks on COMMIT
        release_locks(txn);

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

        // Strict 2PL: release all locks on ROLLBACK
        release_locks(txn);

        // Remove the transaction from active transactions
        std::lock_guard<std::mutex> lock(mutex_);
        active_transactions_.erase(txn->get_transaction_id());
    }

    void TransactionManager::release_locks(Transaction * txn)
    {
        if (!lock_manager_) 
            return;

        // Release all shared locks
        for (const auto& key : txn->get_shared_locks()) 
            lock_manager_->unlock_shared(key);

        // Release all exclusive locks
        for (const auto& key : txn->get_exclusive_locks()) 
            lock_manager_->unlock_exclusive(key);

        txn->clear_locks();
    } 

} //namespace minidb