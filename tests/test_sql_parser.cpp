#include <gtest/gtest.h>
#include "minidb/sql/Tokenizer.h"
#include "minidb/sql/Parser.h"

using namespace minidb;

// --- Tests for the Tokenizer ---

TEST(TokenizerTest, BasicTokenization) 
{
    std::string sql = "SELECT * FROM users;";
    Tokenizer tokenizer(sql);
    auto tokens = tokenizer.tokenize();

    ASSERT_EQ(tokens.size(), 6);
    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::STAR);
    EXPECT_EQ(tokens[2].type, TokenType::FROM);
    EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].value, "users");
    EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[5].type, TokenType::END_OF_FILE);
}

// --- Tests for the Parser ---

TEST(ParserTest, CreateTableStatement) 
{
    std::string sql = "CREATE TABLE users (id INT, name VARCHAR);";
    Tokenizer tokenizer(sql);
    auto tokens = tokenizer.tokenize();

    Parser parser(tokens);
    auto stmt = parser.parse();

    ASSERT_NE(stmt, nullptr);

    // Check that the statement was parsed as CreateTableStatement
    auto create_stmt = dynamic_cast<CreateTableStatement*>(stmt.get());
    ASSERT_NE(create_stmt, nullptr);

    EXPECT_EQ(create_stmt->table_name, "users");
    ASSERT_EQ(create_stmt->columns.size(), 2);

    EXPECT_EQ(create_stmt->columns[0].name, "id");
    EXPECT_EQ(create_stmt->columns[0].type, "INT");

    EXPECT_EQ(create_stmt->columns[1].name, "name");
    EXPECT_EQ(create_stmt->columns[1].type, "VARCHAR");
}

TEST(ParserTest, InsertStatement) 
{
    std::string sql = "INSERT INTO users VALUES (1, 'Henrik');";
    Tokenizer tokenizer(sql);
    auto tokens = tokenizer.tokenize();

    Parser parser(tokens);
    auto stmt = parser.parse();

    ASSERT_NE(stmt, nullptr);

    auto insert_stmt = dynamic_cast<InsertStatement*>(stmt.get());
    ASSERT_NE(insert_stmt, nullptr);

    EXPECT_EQ(insert_stmt->table_name, "users");
    ASSERT_EQ(insert_stmt->values.size(), 2);
    EXPECT_EQ(insert_stmt->values[0], "1");
    EXPECT_EQ(insert_stmt->values[1], "Henrik");
}

TEST(ParserTest, SelectStatement) 
{
    std::string sql = "SELECT * FROM users;";
    Tokenizer tokenizer(sql);
    auto tokens = tokenizer.tokenize();

    Parser parser(tokens);
    auto stmt = parser.parse();

    ASSERT_NE(stmt, nullptr);

    auto select_stmt = dynamic_cast<SelectStatement*>(stmt.get());
    ASSERT_NE(select_stmt, nullptr);

    EXPECT_EQ(select_stmt->table_name, "users");
    EXPECT_TRUE(select_stmt->select_all);
}

TEST(ParserTest, SyntaxErrorThrows) 
{
    // Error: missing TABLE keyword
    std::string sql = "CREATE users (id INT);";
    Tokenizer tokenizer(sql);
    auto tokens = tokenizer.tokenize();

    Parser parser(tokens);
    EXPECT_THROW(parser.parse(), std::runtime_error);
}