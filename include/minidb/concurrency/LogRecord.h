#pragma once

#include "minidb/index/TupleMeta.h"

#include <string>


namespace minidb 
{

    enum class UndoLogType 
    {
        INSERT, // To undo, a DELETE will be needed
        UPDATE, // To undo, the old_value must be restored
        DELETE  // To undo, an INSERT(old_value) will be needed
        
    };

    class LogRecord 
    {
    public:
        LogRecord(txn_id_t txn_id, lsn_t lsn, UndoLogType type, const std::string& key, 
                  const std::string& old_value = "", const TupleMeta& old_meta = TupleMeta())
            : txn_id_(txn_id), lsn_(lsn), type_(type), key_(key)
            , old_value_(old_value), old_meta_(old_meta) 
        {
        }

        txn_id_t get_txn_id() const { return txn_id_; }
        lsn_t get_lsn() const { return lsn_; }
        UndoLogType get_type() const { return type_; }
        const std::string& get_key() const { return key_; }
        const std::string& get_old_value() const { return old_value_; }
        const TupleMeta& get_old_meta() const { return old_meta_; }

    private:
        txn_id_t txn_id_;
        lsn_t lsn_;            // Unique ID of this log record
        UndoLogType type_;
        std::string key_;
        
        // Store the old value (not needed for INSERT, but critical for UPDATE and DELETE)
        std::string old_value_;
        
        TupleMeta old_meta_;   // Metadata of the old version
    };
} // namespace minidb