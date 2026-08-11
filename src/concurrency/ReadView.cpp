#include "minidb/concurrency/ReadView.h"

#include <limits>


namespace minidb 
{
    ReadView::ReadView(txn_id_t creator_txn_id, const std::unordered_set<txn_id_t>& active_txns, txn_id_t next_txn_id)
        : creator_txn_id_(creator_txn_id)
        , active_txn_ids_(active_txns)
        , max_txn_id_(next_txn_id)
    {
        // Compute the minimum active ID
        min_txn_id_ = max_txn_id_;
        for (auto id : active_txns) 
        {
            if (id < min_txn_id_) 
            {
                min_txn_id_ = id;
            }
        }
    }

    bool ReadView::is_visible(txn_id_t version_txn_id) const 
    {
        // Rule 1: We always see our own changes
        if (version_txn_id == creator_txn_id_) 
        {
            return true;
        }

        // Rule 2: The version was created before all currently active transactions (committed long ago)
        if (version_txn_id < min_txn_id_) 
        {
            return true;
        }

        // Rule 3: The version was created by a transaction from the future (after our start)
        if (version_txn_id >= max_txn_id_) 
        {
            return false;
        }

        // Rule 4: The version lies between min and max.
        // It is visible only if its creator is NOT in the set of active transactions in our snapshot
        // (meaning it committed right before our start).
        return active_txn_ids_.count(version_txn_id) == 0;
    }

} // namespace minidb