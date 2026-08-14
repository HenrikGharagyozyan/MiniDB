#include "minidb/sql/Tokenizer.h"
#include <cctype>

namespace minidb 
{
    Tokenizer::Tokenizer(const std::string& input) 
        : input_(input), pos_(0) 
    {
    }

    char Tokenizer::current_char() 
    {
        if (pos_ >= input_.length()) 
            return '\0';
        return input_[pos_];
    }

    void Tokenizer::advance() 
    {
        pos_++;
    }

    void Tokenizer::skip_whitespace() 
    {
        while (std::isspace(current_char())) 
        {
            advance();
        }
    }

    std::vector<Token> Tokenizer::tokenize() 
    {
        std::vector<Token> tokens;

        while (pos_ < input_.length()) 
        {
            skip_whitespace();
            char c = current_char();

            if (c == '\0') 
                break;

            if (std::isalpha(c)) 
            {
                tokens.push_back(consume_identifier_or_keyword());
            } 
            else if (std::isdigit(c)) 
            {
                tokens.push_back(consume_number());
            } 
            else if (c == '\'') // SQL strings are written in single quotes: 'text'
            {
                tokens.push_back(consume_string());
            } 
            else 
            {
                // Handle operators that can be 1 or 2 characters (!=, >=, <=)
                if (c == '!') 
                {
                    advance();
                    if (current_char() == '=') 
                    { 
                        tokens.push_back({ TokenType::NOT_EQUALS, "!=" }); 
                        advance(); 
                    }
                    else 
                    { 
                        tokens.push_back({ TokenType::INVALID, "!" }); 
                    }
                }
                else if (c == '>') 
                {
                    advance();
                    if (current_char() == '=') 
                    { 
                        tokens.push_back({ TokenType::GREATER_EQUALS, ">=" }); 
                        advance(); 
                    }
                    else 
                    { 
                        tokens.push_back({ TokenType::GREATER, ">" }); 
                    }
                }
                else if (c == '<') 
                {
                    advance();
                    if (current_char() == '=') 
                    { 
                        tokens.push_back({ TokenType::LESS_EQUALS, "<=" }); 
                        advance(); 
                    }
                    else 
                    { 
                        tokens.push_back({ TokenType::LESS, "<" }); 
                    }
                }
                else
                {
                    // Handle single-character tokens
                    switch (c) 
                    {
                        case '*': tokens.push_back({ TokenType::STAR, "*"        });  advance(); break;
                        case ',': tokens.push_back({ TokenType::COMMA, ","       });  advance(); break;
                        case ';': tokens.push_back({ TokenType::SEMICOLON, ";"   });  advance(); break;
                        case '(': tokens.push_back({ TokenType::LEFT_PAREN, "("  });  advance(); break;
                        case ')': tokens.push_back({ TokenType::RIGHT_PAREN, ")" });  advance(); break;
                        case '=': tokens.push_back({ TokenType::EQUALS, "="      });  advance(); break;
                        default:
                            // If we encountered an unknown character
                            tokens.push_back({ TokenType::INVALID, std::string(1, c) });
                            advance();
                            break;
                    }
                }
            }
        }
        
        // Always add an end-of-file token at the end to simplify parsing
        tokens.push_back({TokenType::END_OF_FILE, ""});
        return tokens;
    }

    Token Tokenizer::consume_number() 
    {
        std::string val;
        while (std::isdigit(current_char())) 
        {
            val += current_char();
            advance();
        }
        return { TokenType::NUMBER, val };
    }

    Token Tokenizer::consume_string() 
    {
        std::string val;
        advance(); // Skip the opening quote
        
        while (current_char() != '\'' && current_char() != '\0') 
        {
            val += current_char();
            advance();
        }
        
        if (current_char() == '\'') 
        {
            advance(); // Skip the closing quote
        }
        return { TokenType::STRING, val };
    }

    Token Tokenizer::consume_identifier_or_keyword() 
    {
        std::string val;
        // Read letters, digits, and underscores (e.g. user_name1)
        while (std::isalnum(current_char()) || current_char() == '_') 
        {
            val += current_char();
            advance();
        }

        // Convert to uppercase to compare with keywords
        // This lets us treat Select, SELECT, and select as the same token
        std::string upper_val = val;
        for (char& c : upper_val) 
            c = std::toupper(c);

        if (upper_val == "SELECT")  return { TokenType::SELECT, val  };
        if (upper_val == "FROM")    return { TokenType::FROM, val    };
        if (upper_val == "WHERE")   return { TokenType::WHERE, val   };
        if (upper_val == "INSERT")  return { TokenType::INSERT, val  };
        if (upper_val == "INTO")    return { TokenType::INTO, val    };
        if (upper_val == "VALUES")  return { TokenType::VALUES, val  };
        if (upper_val == "CREATE")  return { TokenType::CREATE, val  };
        if (upper_val == "TABLE")   return { TokenType::TABLE, val   };
        if (upper_val == "INT")     return { TokenType::INT, val     };
        if (upper_val == "VARCHAR") return { TokenType::VARCHAR, val };

        // If this is not a keyword, then it is just an identifier (table name, column name, etc.)
        return { TokenType::IDENTIFIER, val };
    }

} // namespace minidb