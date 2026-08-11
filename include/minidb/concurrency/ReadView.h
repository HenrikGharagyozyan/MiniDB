#pragma once

#include <unordered_set>
#include <cstdint>


namespace minidb 
{

    using txn_id_t = uint64_t;

    class ReadView 
    {
    public:
        // Pass our ID, the list of active transactions at startup, and the ID of the next transaction
        ReadView(txn_id_t creator_txn_id, const std::unordered_set<txn_id_t>& active_txns, txn_id_t next_txn_id);
        ~ReadView() = default;

        // Visibility engine: does our transaction see a version created by version_txn_id?
        bool is_visible(txn_id_t version_txn_id) const;

    private:
        txn_id_t creator_txn_id_;
        std::unordered_set<txn_id_t> active_txn_ids_;
        
        txn_id_t min_txn_id_; // Lower bound (everything smaller is visible)
        txn_id_t max_txn_id_; // Upper bound (everything greater than or equal is not visible)
    };

} // namespace minidb