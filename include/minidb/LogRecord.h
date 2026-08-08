#pragma once

#include "minidb/Transaction.h"
#include <string>


namespace minidb 
{

    enum class LogRecordType 
    {
        INSERT, // To undo, a DELETE will be needed
        UPDATE, // To undo, the old_value must be restored
        DELETE  // To undo, an INSERT(old_value) will be needed
    };

    class LogRecord 
    {
    public:
        LogRecord(txn_id_t txn_id, LogRecordType type, const std::string& key, const std::string& old_value = "")
            : txn_id_(txn_id), type_(type), key_(key), old_value_(old_value) 
        {
        }

        txn_id_t get_txn_id() const { return txn_id_; }
        LogRecordType get_type() const { return type_; }
        const std::string& get_key() const { return key_; }
        const std::string& get_old_value() const { return old_value_; }

    private:
        txn_id_t txn_id_;
        LogRecordType type_;
        std::string key_;
        
        // Store the old value (not needed for INSERT, but critical for UPDATE and DELETE)
        std::string old_value_;
    };
} // namespace minidb