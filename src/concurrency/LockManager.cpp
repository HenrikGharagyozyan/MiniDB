#include "minidb/concurrency/LockManager.h"


namespace minidb 
{

    std::shared_timed_mutex* LockManager::get_mutex_for_key(const std::string& key) 
    {
        // Acquire the global table mutex only for the duration of lookup/creation
        std::lock_guard<std::mutex> lock(table_mutex_);

        auto it = lock_table_.find(key);
        if (it == lock_table_.end()) 
        {
            // If no mutex exists for this key yet, create it
            auto [new_it, inserted] = lock_table_.emplace(
                key, 
                std::make_unique<std::shared_timed_mutex>()
            );
            return new_it->second.get();
        }

        return it->second.get();
    }

    bool LockManager::lock_shared(const std::string& key, int timeout_ms) 
    {
        // Get the mutex and request shared access (S-Lock)
        return get_mutex_for_key(key)->try_lock_shared_for(std::chrono::milliseconds(timeout_ms));
    }

    bool LockManager::lock_exclusive(const std::string& key, int timeout_ms) 
    {
        // Get the mutex and request exclusive access (X-Lock)
        return get_mutex_for_key(key)->try_lock_for(std::chrono::milliseconds(timeout_ms));
    }

    void LockManager::unlock_shared(const std::string& key) 
    {
        get_mutex_for_key(key)->unlock_shared();
    }

    void LockManager::unlock_exclusive(const std::string& key) 
    {
        get_mutex_for_key(key)->unlock();
    }

} // namespace minidb