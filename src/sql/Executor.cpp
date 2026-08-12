#include "Executor.h"

namespace minidb
{

    Executor::Executor(Database& db, Catalog& catalog)
        : db_(db), catalog_(catalog) 
    {
    }

    void Executor::execute(const SQLStatement* stmt, Transaction* txn)
    {
        if (auto create_stmt = dynamic_cast<const CreateTableStatement*>(stmt)) 
        {
            execute_create(create_stmt);
        } 
        else if (auto insert_stmt = dynamic_cast<const InsertStatement*>(stmt)) 
        {
            execute_insert(insert_stmt, txn);
        } 
        else if (auto select_stmt = dynamic_cast<const SelectStatement*>(stmt)) 
        {
            execute_select(select_stmt, txn);
        } 
        else 
        {
            throw std::runtime_error("Execution error: Unknown statement type");
        }
    }

    void Executor::execute_create(const CreateTableStatement* stmt)
    {
        catalog_.create_table(stmt->table_name, stmt->columns);
        std::cout << "Table '" << stmt->table_name << "' created successfully.\n";
    }

    void Executor::execute_insert(const InsertStatement* stmt, Transaction* txn)
    {
        const auto& schema = catalog_.get_table(stmt->table_name);
        
        if (stmt->values.size() != schema.columns.size()) 
        {
            throw std::runtime_error("Insert error: Value count does not match column count");
        }

        // Build the internal key: "table_name:first_value" (e.g. "users:1")
        std::string key = stmt->table_name + ":" + stmt->values[0];

        // Combine column values into a single comma-separated string
        std::string serialized_row;
        for (size_t i = 0; i < stmt->values.size(); ++i) 
        {
            serialized_row += stmt->values[i];
            if (i + 1 < stmt->values.size()) 
                serialized_row += ",";
        }

        // Store it in our database!
        db_.set(key, serialized_row, txn);
        std::cout << "1 row inserted into '" << stmt->table_name << "'.\n";
    }

    void Executor::execute_select(const SelectStatement* stmt, Transaction* txn)
    {
        const auto& schema = catalog_.get_table(stmt->table_name);
        
        // Print column headers
        for (size_t i = 0; i < schema.columns.size(); ++i) 
        {
            std::cout << schema.columns[i].name << (i + 1 < schema.columns.size() ? " | " : "\n");
        }
        std::cout << "---------------------\n";

        // For demonstration, try fetching the first record (key "users:1")
        std::string test_key = stmt->table_name + ":1";
        auto val = db_.get(test_key, txn);
        if (val) 
        {
            std::cout << *val << "\n";
        } 
        else 
        {
            std::cout << "(No rows found)\n";
        }
    }

} // namespace minidb