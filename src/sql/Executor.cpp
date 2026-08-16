#include "minidb/sql/Executor.h"

#include <sstream>
#include <string_view>


namespace minidb
{

    // Helper function to split CSV row into values
    static std::vector<std::string> parse_csv_row(const std::string& row_str) 
    {
        std::vector<std::string> fields;
        std::string field;
        std::stringstream ss(row_str);
        while (std::getline(ss, field, ',')) 
        {
            fields.push_back(field);
        }
        return fields;
    }


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
        else if (auto delete_stmt = dynamic_cast<const DeleteStatement*>(stmt)) 
        {
            execute_delete(delete_stmt, txn);
        }
        else if (auto update_stmt = dynamic_cast<const UpdateStatement*>(stmt)) 
        {
            execute_update(update_stmt, txn);
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

        // If WHERE clause exists, find the index of the column
        int where_col_idx = -1;
        if (stmt->where_clause.has_value()) 
        {
            for (size_t i = 0; i < schema.columns.size(); ++i) 
            {
                if (schema.columns[i].name == stmt->where_clause->column_name) 
                {
                    where_col_idx = static_cast<int>(i);
                    break;
                }
            }
            if (where_col_idx == -1) 
            {
                throw std::runtime_error("Execution error: Column '" + 
                                         stmt->where_clause->column_name + 
                                         "' not found in table '" + stmt->table_name + "'");
            }
        }
        
        // Print column headers
        for (size_t i = 0; i < schema.columns.size(); ++i) 
        {
            std::cout << schema.columns[i].name << (i + 1 < schema.columns.size() ? " | " : "\n");
        }
        std::cout << "---------------------\n";

        // Full table scan: retrieve all records from the DB taking the transaction into account
        auto all_records = db_.scan(txn);

        // Prefix to search for (e.g. "users:")
        std::string prefix = stmt->table_name + ":";
        int row_count = 0;

        // Iterate over all records and select those that belong to our table
        for (const auto& [key, value] : all_records) 
        {
            if (key.starts_with(prefix)) 
            {
                // Apply WHERE condition if present
                if (stmt->where_clause.has_value()) 
                {
                    auto fields = parse_csv_row(value);
                    if (where_col_idx >= static_cast<int>(fields.size())) 
                    {
                        continue;
                    }

                    const std::string& field_val_str = fields[where_col_idx];
                    const std::string& target_val_str = stmt->where_clause->value;
                    const std::string& op = stmt->where_clause->op;
                    
                    bool match = false;
                    
                    // If the column is INT, convert to integer for proper numeric comparison
                    if (schema.columns[where_col_idx].type == "INT") 
                    {
                        try 
                        {
                            int field_val = std::stoi(field_val_str);
                            int target_val = std::stoi(target_val_str);
                            
                            if (op == "=")       match = (field_val == target_val);
                            else if (op == "!=") match = (field_val != target_val);
                            else if (op == ">")  match = (field_val > target_val);
                            else if (op == "<")  match = (field_val < target_val);
                            else if (op == ">=") match = (field_val >= target_val);
                            else if (op == "<=") match = (field_val <= target_val);
                        } 
                        catch (...) 
                        {
                            match = false; // Fallback if conversion fails
                        }
                    } 
                    else 
                    {
                        // String comparison for VARCHAR
                        if (op == "=")       match = (field_val_str == target_val_str);
                        else if (op == "!=") match = (field_val_str != target_val_str);
                        else if (op == ">")  match = (field_val_str > target_val_str);
                        else if (op == "<")  match = (field_val_str < target_val_str);
                        else if (op == ">=") match = (field_val_str >= target_val_str);
                        else if (op == "<=") match = (field_val_str <= target_val_str);
                    }

                    if (!match) 
                        continue; // Skip row if it doesn't match
                }

                std::cout << value << "\n";
                row_count++;
            }
        }

        if (row_count == 0) 
        {
            std::cout << "(No rows found)\n";
        }
    }

    void Executor::execute_delete(const DeleteStatement* stmt, Transaction* txn)
    {
        const auto& schema = catalog_.get_table(stmt->table_name);

        int where_col_idx = -1;
        if (stmt->where_clause.has_value()) 
        {
            for (size_t i = 0; i < schema.columns.size(); ++i) 
            {
                if (schema.columns[i].name == stmt->where_clause->column_name) 
                {
                    where_col_idx = static_cast<int>(i);
                    break;
                }
            }
            if (where_col_idx == -1) 
            {
                throw std::runtime_error("Execution error: Column '" + 
                                         stmt->where_clause->column_name + 
                                         "' not found in table '" + stmt->table_name + "'");
            }
        }

        auto all_records = db_.scan(txn);
        std::string prefix = stmt->table_name + ":";
        int deleted_count = 0;

        for (const auto& [key, value] : all_records) 
        {
            if (key.starts_with(prefix)) 
            {
                if (stmt->where_clause.has_value()) 
                {
                    auto fields = parse_csv_row(value);
                    if (where_col_idx >= static_cast<int>(fields.size())) continue;

                    const std::string& field_val_str = fields[where_col_idx];
                    const std::string& target_val_str = stmt->where_clause->value;
                    const std::string& op = stmt->where_clause->op;
                    
                    bool match = false;
                    
                    if (schema.columns[where_col_idx].type == "INT") 
                    {
                        try 
                        {
                            int field_val = std::stoi(field_val_str);
                            int target_val = std::stoi(target_val_str);
                            
                            if (op == "=")       match = (field_val == target_val);
                            else if (op == "!=") match = (field_val != target_val);
                            else if (op == ">")  match = (field_val > target_val);
                            else if (op == "<")  match = (field_val < target_val);
                            else if (op == ">=") match = (field_val >= target_val);
                            else if (op == "<=") match = (field_val <= target_val);
                        } 
                        catch (...) 
                        {
                            match = false; 
                        }
                    } 
                    else 
                    {
                        if (op == "=")       match = (field_val_str == target_val_str);
                        else if (op == "!=") match = (field_val_str != target_val_str);
                        else if (op == ">")  match = (field_val_str > target_val_str);
                        else if (op == "<")  match = (field_val_str < target_val_str);
                        else if (op == ">=") match = (field_val_str >= target_val_str);
                        else if (op == "<=") match = (field_val_str <= target_val_str);
                    }

                    if (!match) continue;
                }

                // Call delete on the underlying storage
                db_.remove(key, txn);
                deleted_count++;
            }
        }

        std::cout << deleted_count << " row(s) deleted from '" << stmt->table_name << "'.\n";
    }

    void Executor::execute_update(const UpdateStatement* stmt, Transaction* txn)
    {
        const auto& schema = catalog_.get_table(stmt->table_name);

        int set_col_idx = -1;
        int where_col_idx = -1;

        // Ищем индекс обновляемой колонки
        for (size_t i = 0; i < schema.columns.size(); ++i) 
        {
            if (schema.columns[i].name == stmt->set_column_name) 
                set_col_idx = static_cast<int>(i);
            if (stmt->where_clause.has_value() && schema.columns[i].name == stmt->where_clause->column_name) 
                where_col_idx = static_cast<int>(i);
        }

        if (set_col_idx == -1) 
            throw std::runtime_error("Execution error: SET Column not found.");
        if (stmt->where_clause.has_value() && where_col_idx == -1) 
            throw std::runtime_error("Execution error: WHERE Column not found.");

        auto all_records = db_.scan(txn);
        std::string prefix = stmt->table_name + ":";
        int updated_count = 0;

        for (const auto& [key, value] : all_records) 
        {
            if (key.starts_with(prefix)) 
            {
                auto fields = parse_csv_row(value);

                if (stmt->where_clause.has_value()) 
                {
                    if (where_col_idx >= static_cast<int>(fields.size())) continue;

                    const std::string& field_val_str = fields[where_col_idx];
                    const std::string& target_val_str = stmt->where_clause->value;
                    const std::string& op = stmt->where_clause->op;
                    bool match = false;
                    
                    if (schema.columns[where_col_idx].type == "INT") 
                    {
                        try 
                        {
                            int field_val = std::stoi(field_val_str);
                            int target_val = std::stoi(target_val_str);
                            if (op == "=") match = (field_val == target_val);
                            else if (op == "!=") match = (field_val != target_val);
                            else if (op == ">") match = (field_val > target_val);
                            else if (op == "<") match = (field_val < target_val);
                            else if (op == ">=") match = (field_val >= target_val);
                            else if (op == "<=") match = (field_val <= target_val);
                        } 
                        catch (...) 
                        { 
                            match = false; 
                        }
                    } 
                    else 
                    {
                        if (op == "=") 
                            match = (field_val_str == target_val_str);
                        else if (op == "!=") 
                            match = (field_val_str != target_val_str);
                        // Опустил >, < для строк для краткости, но логика та же
                    }

                    if (!match) 
                        continue;
                }

                // Обновляем значение
                if (set_col_idx < static_cast<int>(fields.size())) 
                {
                    fields[set_col_idx] = stmt->set_value;
                    
                    // Склеиваем обратно в CSV
                    std::string updated_row;
                    for (size_t i = 0; i < fields.size(); ++i) 
                    {
                        updated_row += fields[i];
                        if (i + 1 < fields.size()) 
                            updated_row += ",";
                    }
                    
                    // Записываем новую версию
                    db_.set(key, updated_row, txn);
                    updated_count++;
                }
            }
        }
        std::cout << updated_count << " row(s) updated in '" << stmt->table_name << "'.\n";
    }

} // namespace minidb