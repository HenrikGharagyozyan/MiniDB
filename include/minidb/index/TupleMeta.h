#pragma once

#include <cstdint>

namespace minidb 
{

    using txn_id_t = uint64_t;
    using lsn_t = uint64_t; // Log Sequence Number

    struct TupleMeta 
    {
        // Which transaction created/updated this version
        txn_id_t txn_id;

        // If this is a B-Tree record, undo_lsn points to
        // the ID of the record in the Undo Log that contains the previous version.
        // If this is 0, there are no previous versions.
        lsn_t undo_lsn;

        // Flag indicating whether this version is logically deleted (Tombstone).
        // Useful for MVCC DELETE.
        bool is_deleted;

        TupleMeta() 
            : txn_id(0), undo_lsn(0), is_deleted(false) 
        {
        }

        TupleMeta(txn_id_t id, lsn_t lsn, bool del) 
            : txn_id(id), undo_lsn(lsn), is_deleted(del) 
        {
        }
    };

} // namespace minidb