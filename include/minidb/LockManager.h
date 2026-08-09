#pragma once

#include <shared_mutex>
#include <unordered_map>
#include <mutex>
#include <memory>

// Forward declarations for context
class Transaction;

// Resource identifier (for example, Record ID or Page ID)
struct RID 
{
    int page_id;
    int slot_id;

    bool operator==(const RID& other) const 
    {
        return page_id == other.page_id && slot_id == other.slot_id;
    }
};

// Hash function for using RID as a key in unordered_map
namespace std 
{
    template <>
    struct hash<RID> 
    {
        size_t operator()(const RID& rid) const 
        {
            return hash<int>()(rid.page_id) ^ hash<int>()(rid.slot_id);
        }
    };
}

class LockManager 
{
public:
    LockManager() = default;
    ~LockManager() = default;

    // Disable copy and move operations for the lock manager
    LockManager(const LockManager&) = delete;
    LockManager& operator=(const LockManager&) = delete;

    /**
     * @brief Request a shared lock (Shared/Read lock).
     * Multiple transactions may hold an S-lock on the same resource simultaneously.
     */
    bool LockShared(Transaction* txn, const RID& rid);

    /**
     * @brief Request an exclusive lock (Exclusive/Write lock).
     * Only one transaction may hold an X-lock at a time.
     */
    bool LockExclusive(Transaction* txn, const RID& rid);

    /**
     * @brief Upgrade a lock from Shared to Exclusive.
     * Useful when a transaction has read data and now wants to modify it.
     */
    bool LockUpgrade(Transaction* txn, const RID& rid);

    /**
     * @brief Release the lock (of any type) on the resource for the given transaction.
     */
    bool Unlock(Transaction* txn, const RID& rid);

private:
    /**
     * @brief Helper method to get the mutex for a resource.
     * If a mutex for the given RID does not exist yet, it will be created.
     */
    std::shared_mutex* GetMutexForResource(const RID& rid);

private:
    // Global mutex to protect the lock table itself
    std::mutex lock_table_mutex_;

    // Lock table. We use unique_ptr so mutex addresses do not change
    // when the unordered_map is rehashed.
    std::unordered_map<RID, std::unique_ptr<std::shared_mutex>> lock_table_;

};