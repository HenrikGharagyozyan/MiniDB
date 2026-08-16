#include "minidb/sql/Parser.h"

namespace minidb 
{

    Parser::Parser(const std::vector<Token>& tokens) 
        : tokens_(tokens), pos_(0) 
    {
    }

    const Token& Parser::peek() const 
    {
        return tokens_[pos_];
    }

    const Token& Parser::previous() const 
    {
        return tokens_[pos_ - 1];
    }

    bool Parser::is_at_end() const 
    {
        return peek().type == TokenType::END_OF_FILE;
    }

    bool Parser::match(TokenType type) 
    {
        if (is_at_end()) 
            return false;
        if (peek().type != type) 
            return false;
        
        pos_++;
        return true;
    }

    Token Parser::consume(TokenType type, const std::string& error_message) 
    {
        if (peek().type == type) 
        {
            pos_++;
            return previous();
        }
        throw std::runtime_error("Syntax Error: " + error_message + " (Got: " + peek().value + ")");
    }

    std::unique_ptr<SQLStatement> Parser::parse() 
    {
        if (match(TokenType::CREATE)) 
        {
            return parse_create_table();
        } 
        else if (match(TokenType::INSERT)) 
        {
            return parse_insert();
        } 
        else if (match(TokenType::SELECT)) 
        {
            return parse_select();
        }
        else if (match(TokenType::DELETE)) 
        {
            return parse_delete();
        }
        else if (match(TokenType::UPDATE)) 
        {
            return parse_update();
        }

        throw std::runtime_error("Syntax Error: Unknown or unsupported SQL command.");
    }

    std::unique_ptr<SQLStatement> Parser::parse_create_table() 
    {
        // We already consumed CREATE in parse(), now expect TABLE
        consume(TokenType::TABLE, "Expected 'TABLE' after 'CREATE'");
        
        auto stmt = std::make_unique<CreateTableStatement>();
        stmt->table_name = consume(TokenType::IDENTIFIER, "Expected table name").value;
        
        consume(TokenType::LEFT_PAREN, "Expected '(' after table name");

        // Read columns
        do 
        {
            ColumnDef col;
            col.name = consume(TokenType::IDENTIFIER, "Expected column name").value;
            
            // Column type must be INT or VARCHAR
            if (match(TokenType::INT) || match(TokenType::VARCHAR)) 
            {
                col.type = previous().value;
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected column type (INT or VARCHAR)");
            }

            stmt->columns.push_back(col);
        } while (match(TokenType::COMMA)); // If there is a comma, there is another column

        consume(TokenType::RIGHT_PAREN, "Expected ')' after columns definition");
        consume(TokenType::SEMICOLON, "Expected ';' at the end of statement");

        return stmt;
    }

    std::unique_ptr<SQLStatement> Parser::parse_insert() 
    {
        consume(TokenType::INTO, "Expected 'INTO' after 'INSERT'");
        
        auto stmt = std::make_unique<InsertStatement>();
        stmt->table_name = consume(TokenType::IDENTIFIER, "Expected table name").value;
        
        consume(TokenType::VALUES, "Expected 'VALUES'");
        consume(TokenType::LEFT_PAREN, "Expected '('");

        // Read values
        do 
        {
            if (match(TokenType::STRING) || match(TokenType::NUMBER)) 
            {
                stmt->values.push_back(previous().value);
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected string or number value");
            }
        } while (match(TokenType::COMMA));

        consume(TokenType::RIGHT_PAREN, "Expected ')'");
        consume(TokenType::SEMICOLON, "Expected ';' at the end of statement");

        return stmt;
    }

    std::unique_ptr<SQLStatement> Parser::parse_select() 
    {
        auto stmt = std::make_unique<SelectStatement>();
        
        // For now we only support SELECT *
        consume(TokenType::STAR, "Expected '*' (Only SELECT * is supported currently)");
        
        consume(TokenType::FROM, "Expected 'FROM'");
        stmt->table_name = consume(TokenType::IDENTIFIER, "Expected table name").value;

        // Check for optional WHERE clause using your TokenTypes!
        if (!is_at_end() && peek().type == TokenType::WHERE) 
        {
            consume(TokenType::WHERE, "Expected WHERE");
            
            WhereClause where;
            where.column_name = consume(TokenType::IDENTIFIER, "Expected column name after WHERE").value;
            
            // Check operator for new types
            TokenType op_type = peek().type;
            if (op_type == TokenType::EQUALS || op_type == TokenType::NOT_EQUALS ||
                op_type == TokenType::GREATER || op_type == TokenType::LESS ||
                op_type == TokenType::GREATER_EQUALS || op_type == TokenType::LESS_EQUALS) 
            {
                where.op = peek().value;
                pos_++; // Consume operator
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected operator (=, !=, >, <, >=, <=) in WHERE clause");
            }

            if (match(TokenType::NUMBER) || match(TokenType::STRING) || match(TokenType::IDENTIFIER)) 
            {
                where.value = previous().value;
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected value after operator in WHERE clause");
            }

            stmt->where_clause = where;
        }
        
        consume(TokenType::SEMICOLON, "Expected ';' at the end of statement");

        return stmt;
    }

    std::unique_ptr<SQLStatement> Parser::parse_delete() 
    {
        auto stmt = std::make_unique<DeleteStatement>();
        
        consume(TokenType::FROM, "Expected 'FROM' after 'DELETE'");
        stmt->table_name = consume(TokenType::IDENTIFIER, "Expected table name").value;

        if (!is_at_end() && peek().type == TokenType::WHERE) 
        {
            consume(TokenType::WHERE, "Expected WHERE");
            
            WhereClause where;
            where.column_name = consume(TokenType::IDENTIFIER, "Expected column name after WHERE").value;
            
            TokenType op_type = peek().type;
            if (op_type == TokenType::EQUALS || op_type == TokenType::NOT_EQUALS ||
                op_type == TokenType::GREATER || op_type == TokenType::LESS ||
                op_type == TokenType::GREATER_EQUALS || op_type == TokenType::LESS_EQUALS) 
            {
                where.op = peek().value;
                pos_++; 
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected operator in WHERE clause");
            }

            if (match(TokenType::NUMBER) || match(TokenType::STRING) || match(TokenType::IDENTIFIER)) 
            {
                where.value = previous().value;
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected value after operator in WHERE clause");
            }

            stmt->where_clause = where;
        }
        
        consume(TokenType::SEMICOLON, "Expected ';' at the end of statement");

        return stmt;
    }

    std::unique_ptr<SQLStatement> Parser::parse_update() 
    {
        auto stmt = std::make_unique<UpdateStatement>();
        
        stmt->table_name = consume(TokenType::IDENTIFIER, "Expected table name after UPDATE").value;
        
        consume(TokenType::SET, "Expected 'SET'");
        stmt->set_column_name = consume(TokenType::IDENTIFIER, "Expected column name after SET").value;
        
        consume(TokenType::EQUALS, "Expected '=' in SET clause");
        
        if (match(TokenType::NUMBER) || match(TokenType::STRING)) 
        {
            stmt->set_value = previous().value;
        } 
        else 
        {
            throw std::runtime_error("Syntax Error: Expected value in SET clause");
        }

        if (!is_at_end() && peek().type == TokenType::WHERE) 
        {
            consume(TokenType::WHERE, "Expected WHERE");
            
            WhereClause where;
            where.column_name = consume(TokenType::IDENTIFIER, "Expected column name after WHERE").value;
            
            TokenType op_type = peek().type;
            if (op_type == TokenType::EQUALS || op_type == TokenType::NOT_EQUALS ||
                op_type == TokenType::GREATER || op_type == TokenType::LESS ||
                op_type == TokenType::GREATER_EQUALS || op_type == TokenType::LESS_EQUALS) 
            {
                where.op = peek().value;
                pos_++; 
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected operator in WHERE clause");
            }

            if (match(TokenType::NUMBER) || match(TokenType::STRING) || match(TokenType::IDENTIFIER)) 
            {
                where.value = previous().value;
            } 
            else 
            {
                throw std::runtime_error("Syntax Error: Expected value after operator in WHERE clause");
            }

            stmt->where_clause = where;
        }
        
        consume(TokenType::SEMICOLON, "Expected ';' at the end of statement");
        return stmt;
    }

} // namespace minidb