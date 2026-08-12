#include "minidb/concurrency/TransactionManager.h"
#include "minidb/core/Database.h"
#include "minidb/concurrency/LogRecord.h"

#include <algorithm>


namespace minidb 
{

    std::shared_ptr<Transaction> TransactionManager::begin() 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Issue an ID and increment the counter in a thread-safe way
        txn_id_t txn_id = next_txn_id_.fetch_add(1);

        // Collect the IDs of all currently ACTIVE transactions
        std::unordered_set<txn_id_t> active_ids;
        for (const auto& [id, txn_ptr] : active_transactions_) 
        {
            active_ids.insert(id);
        }

        // Create a ReadView: our ID, the active set, and the next transaction ID
        // (next_txn_id_.load() returns the ID that the next transaction will receive)
        ReadView view(txn_id, active_ids, next_txn_id_.load());

        // Pass the snapshot into the transaction constructor
        auto txn = std::make_shared<Transaction>(txn_id, std::move(view));

        // Add the new transaction to the active transactions map
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
    
    void TransactionManager::add_undo_record(std::shared_ptr<LogRecord> record)
    {
        std::lock_guard<std::mutex> lock(undo_mutex_);
        global_undo_log_[record->get_lsn()] = record;
    }

    std::shared_ptr<LogRecord> TransactionManager::get_undo_record(lsn_t lsn)
    {
        std::lock_guard<std::mutex> lock(undo_mutex_);
        auto it = global_undo_log_.find(lsn);
        if (it != global_undo_log_.end()) 
        {
            return it->second;
        }
        return nullptr;
    }

    void TransactionManager::vacuum() 
    {
        // By default, assume the oldest active transaction is the next one that will be created
        // (in case there are no active transactions right now).
        txn_id_t min_active_id = next_txn_id_.load();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            // Find the minimum ID among all active transactions
            for (const auto& [id, txn] : active_transactions_) 
            {
                if (id < min_active_id) 
                {
                    min_active_id = id;
                }
            }
        } // Release the mutex so we don't block the database during cleanup

        {
            std::lock_guard<std::mutex> lock(undo_mutex_);
            
            // With C++20 we can use std::erase_if for clean removal from unordered_map
            std::erase_if(global_undo_log_, [min_active_id](const auto& pair) 
                    {
                        // If the log was created by a transaction older than the oldest active one,
                        // that version is fully invisible (not needed) anymore. Remove it.
                        return pair.second->get_txn_id() < min_active_id; 
                    });
        }
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