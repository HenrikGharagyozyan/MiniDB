#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>


namespace minidb 
{
    // Base class for all SQL statements
    struct SQLStatement 
    {
        virtual ~SQLStatement() = default;
    };


    // --- Structures for CREATE TABLE ---

    // Definition of a single column (e.g. id INT)
    struct ColumnDef 
    {
        std::string name;
        std::string type; // "INT" or "VARCHAR"
    };

    // AST for a query: CREATE TABLE <name> (col1 type1, col2 type2);
    struct CreateTableStatement : public SQLStatement 
    {
        std::string table_name;
        std::vector<ColumnDef> columns;
    };


    // --- Structures for INSERT ---

    // AST for a query: INSERT INTO <name> VALUES ('val1', 'val2');
    struct InsertStatement : public SQLStatement 
    {
        std::string table_name;
        std::vector<std::string> values;
    };

    
    // --- Structures for SELECT ---

    // Structure for WHERE condition (e.g. id = 1)
    struct WhereClause 
    {
        std::string column_name;
        std::string op; // "="
        std::string value;
    };

    // AST for a query: SELECT * FROM <name>;
    // (For now we only support selecting all columns via '*')
    struct SelectStatement : public SQLStatement 
    {
        std::string table_name;
        bool select_all = true;
        std::optional<WhereClause> where_clause;
    };

    
    // --- Structures for DELETE ---

    // AST for a query: DELETE FROM <name> WHERE <cond>;
    struct DeleteStatement : public SQLStatement 
    {
        std::string table_name;
        std::optional<WhereClause> where_clause;
    };

} // namespace minidb