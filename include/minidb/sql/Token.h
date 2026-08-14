#pragma once
#include <string>

namespace minidb 
{

    // All token types the database understands
    enum class TokenType 
    {
        // SQL keywords
        SELECT, FROM, WHERE, INSERT, INTO, VALUES, CREATE, TABLE, INT, VARCHAR,
        
        // Identifiers (e.g. table names like "users" or column names like "id")
        IDENTIFIER, 
        
        // Literals (the actual data)
        NUMBER,     // e.g.: 123
        STRING,     // e.g.: 'Henrik'
        
        // Symbols and operators
        STAR,       // *
        COMMA,      // ,
        SEMICOLON,  // ;
        LEFT_PAREN, // (
        RIGHT_PAREN,// )
        EQUALS,     // =
        NOT_EQUALS,     // !=
        GREATER,        // >
        LESS,           // <
        GREATER_EQUALS, // >=
        LESS_EQUALS,    // <=
        
        // Control tokens
        END_OF_FILE,
        INVALID     // Parse error (e.g. unknown character)
    };

    // Structure for a single scanned token
    struct Token 
    {
        TokenType type;
        std::string value; 
    };

} // namespace minidb