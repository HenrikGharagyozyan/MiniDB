#pragma once

#include "minidb/sql/Token.h"
#include "minidb/sql/AST.h"

#include <vector>
#include <memory>
#include <stdexcept>


namespace minidb 
{

    class Parser 
    {
    public:
        explicit Parser(const std::vector<Token>& tokens);

        // Main method that returns the abstract syntax tree (or throws on error)
        std::unique_ptr<SQLStatement> parse();

    private:
        // Helper methods for working with tokens
        const Token& peek() const;
        const Token& previous() const;
        bool is_at_end() const;
        
        // Checks the current token and advances the pointer if it matches
        bool match(TokenType type);
        
        // Requires the current token to be a specific type, otherwise throws an error
        Token consume(TokenType type, const std::string& error_message);

        // Methods for parsing specific commands
        std::unique_ptr<SQLStatement> parse_create_table();
        std::unique_ptr<SQLStatement> parse_insert();
        std::unique_ptr<SQLStatement> parse_select();
        std::unique_ptr<SQLStatement> parse_delete();

    private:
        std::vector<Token> tokens_;
        size_t pos_;

    };

} // namespace minidb