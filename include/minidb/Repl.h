#pragma once

#include "minidb/Database.h"
#include "minidb/TransactionManager.h"
#include "minidb/LockManager.h"

#include <string>
#include <memory>


namespace minidb 
{

    class Repl 
    {
    public:
        // Take a reference to the database so Repl does not own it, only uses it
        explicit Repl(Database& db);

        // Main loop
        void run();

    private:
        // Handler for a single input line
        bool executeCommand(const std::string& line);

    private:
        Database& db_;
        
        TransactionManager txn_manager_;
        std::shared_ptr<Transaction> current_txn_;

        LockManager lock_manager_;

    };

} // namespace minidb