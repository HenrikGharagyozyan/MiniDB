#pragma once

#include "minidb/sql/Token.h"
#include <vector>
#include <string>

namespace minidb 
{

    class Tokenizer 
    {
    public:
        // Constructor takes the raw query string (e.g. "SELECT id FROM users;")
        explicit Tokenizer(const std::string& input);

        // Main method that converts the string into a list of tokens
        std::vector<Token> tokenize();

    private:
        char current_char();
        void advance(); // Move the pointer to the next character
        void skip_whitespace(); // Skip spaces, tabs, and newlines
        
        // Methods to read specific token types
        Token consume_number();
        Token consume_string();
        Token consume_identifier_or_keyword();

    private:
        std::string input_;
        size_t pos_; // Current character position in the string

    };

} // namespace minidb