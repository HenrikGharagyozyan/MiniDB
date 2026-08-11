#pragma once

#include <cstdint>

namespace minidb 
{

    using txn_id_t = uint64_t;
    using lsn_t = uint64_t; // Log Sequence Number

    struct TupleMeta 
    {
        // Какая транзакция создала/обновила эту версию
        txn_id_t txn_id;

        // Если это запись в B-Tree, undo_lsn указывает на 
        // ID записи в Undo Log, где лежит предыдущая версия.
        // Если это 0, значит старых версий нет.
        lsn_t undo_lsn;

        // Флаг, удалена ли эта версия логически (Tombstone). 
        // Полезно для MVCC DELETE.
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