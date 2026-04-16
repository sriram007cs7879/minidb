#include "test_framework.h"
#include "sql/tokenizer.h"
using namespace minidb;

TEST(tokenizer_select_star) {
    auto tokens = Tokenizer::tokenize("SELECT * FROM students;");
    ASSERT_EQ(tokens[0].type, TokenType::SELECT);
    ASSERT_EQ(tokens[1].type, TokenType::STAR);
    ASSERT_EQ(tokens[2].type, TokenType::FROM);
    ASSERT_EQ(tokens[3].type, TokenType::IDENTIFIER);
    ASSERT_EQ(tokens[3].value, std::string("students"));
    ASSERT_EQ(tokens[4].type, TokenType::SEMICOLON);
}

TEST(tokenizer_insert) {
    auto tokens = Tokenizer::tokenize("INSERT INTO students VALUES (1, 'Sri Ram', 20);");
    ASSERT_EQ(tokens[0].type, TokenType::INSERT);
    ASSERT_EQ(tokens[1].type, TokenType::INTO);
    ASSERT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    ASSERT_EQ(tokens[3].type, TokenType::VALUES);
    ASSERT_EQ(tokens[4].type, TokenType::LPAREN);
    ASSERT_EQ(tokens[5].type, TokenType::INT_LITERAL);
    ASSERT_EQ(tokens[5].value, std::string("1"));
    ASSERT_EQ(tokens[7].type, TokenType::STRING_LITERAL);
    ASSERT_EQ(tokens[7].value, std::string("Sri Ram"));
    ASSERT_EQ(tokens[9].type, TokenType::INT_LITERAL);
    ASSERT_EQ(tokens[9].value, std::string("20"));
}

TEST(tokenizer_where_with_operators) {
    auto tokens = Tokenizer::tokenize("SELECT * FROM t WHERE age >= 18 AND name != 'test';");
    // Find the >= token
    bool found_gte = false;
    bool found_neq = false;
    for (auto& t : tokens) {
        if (t.type == TokenType::GREATER_EQ) found_gte = true;
        if (t.type == TokenType::NOT_EQUALS) found_neq = true;
    }
    ASSERT_TRUE(found_gte);
    ASSERT_TRUE(found_neq);
}

TEST(tokenizer_case_insensitive_keywords) {
    auto tokens1 = Tokenizer::tokenize("select * from students");
    auto tokens2 = Tokenizer::tokenize("SELECT * FROM students");
    ASSERT_EQ(tokens1[0].type, tokens2[0].type);
    ASSERT_EQ(tokens1[2].type, tokens2[2].type);
}

TEST(tokenizer_create_table) {
    auto tokens = Tokenizer::tokenize("CREATE TABLE users (id INT, name TEXT);");
    ASSERT_EQ(tokens[0].type, TokenType::CREATE);
    ASSERT_EQ(tokens[1].type, TokenType::TABLE);
    ASSERT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    ASSERT_EQ(tokens[3].type, TokenType::LPAREN);
    ASSERT_EQ(tokens[4].type, TokenType::IDENTIFIER);
    ASSERT_EQ(tokens[5].type, TokenType::INT_TYPE);
}
