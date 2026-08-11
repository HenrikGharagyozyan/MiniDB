#pragma once

#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <string>
#include <memory>

namespace minidb 
{

    class LockManager 
    {
    public:
        LockManager() = default;
        ~LockManager() = default;

        // Disable copy and move operations for the lock manager
        LockManager(const LockManager&) = delete;
        LockManager& operator=(const LockManager&) = delete;

        // Acquire a shared lock (Shared / Read) on a specific key
        bool lock_shared(const std::string& key, int timeout_ms = 1000);

        // Acquire an exclusive lock (Exclusive / Write) on a specific key
        bool lock_exclusive(const std::string& key, int timeout_ms = 1000);

        // Release a shared lock
        void unlock_shared(const std::string& key);

        // Release an exclusive lock
        void unlock_exclusive(const std::string& key);

    private:
        // Helper method: find or create the shared_mutex for a key
        std::shared_timed_mutex* get_mutex_for_key(const std::string& key);

    private:
        // Protects the hash table lock_table_ itself when adding new keys
        std::mutex table_mutex_;

        // Hash table: String key -> pointer to its individual shared_mutex
        std::unordered_map<std::string, std::unique_ptr<std::shared_timed_mutex>> lock_table_;

    };

} // namespace minidb