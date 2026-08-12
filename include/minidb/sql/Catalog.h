#pragma once

#include "minidb/sql/AST.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace minidb 
{

    // Description of a table in the catalog
    struct TableSchema 
    {
        std::string name;
        std::vector<ColumnDef> columns;
    };

    class Catalog 
    {
    public:
        Catalog() = default;

        // Create a new table
        void create_table(const std::string& name, const std::vector<ColumnDef>& columns) 
        {
            if (tables_.find(name) != tables_.end()) 
            {
                throw std::runtime_error("Table already exists: " + name);
            }
            tables_[name] = TableSchema{name, columns};
        }

        // Check whether a table exists
        bool has_table(const std::string& name) const 
        {
            return tables_.find(name) != tables_.end();
        }

        // Get the schema for a table
        const TableSchema& get_table(const std::string& name) const 
        {
            auto it = tables_.find(name);
            if (it == tables_.end()) 
            {
                throw std::runtime_error("Table not found: " + name);
            }
            return it->second;
        }

    private:
        std::unordered_map<std::string, TableSchema> tables_;
    };

} // namespace minidb