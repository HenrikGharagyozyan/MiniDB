#pragma once

#include "minidb/core/Database.h"
#include "minidb/sql/AST.h"
#include "minidb/sql/Catalog.h"
#include <string>
#include <memory>
#include <iostream>

namespace minidb 
{
    class Executor 
    {
    public:
        Executor(Database& db, Catalog& catalog);

        // Main execution method for any SQL statement
        void execute(const SQLStatement* stmt, Transaction* txn = nullptr);

    private:
        void execute_create(const CreateTableStatement* stmt);

        void execute_insert(const InsertStatement* stmt, Transaction* txn);

        void execute_select(const SelectStatement* stmt, Transaction* txn);

        void execute_delete(const DeleteStatement* stmt, Transaction* txn);

        void execute_update(const UpdateStatement* stmt, Transaction* txn);

    private:
        Database& db_;
        Catalog& catalog_;
    };

} // namespace minidb