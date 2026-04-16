#pragma once

#include <string>
#include <vector>

namespace minidb {

// All possible token types in our SQL dialect
enum class TokenType {
    // Keywords
    SELECT, INSERT, INTO, FROM, WHERE, CREATE, TABLE, DELETE,
    VALUES, AND, OR, NOT,

    // Type keywords
    INT_TYPE, TEXT_TYPE,

    // Literals
    INT_LITERAL,     // 42, 100, -5
    STRING_LITERAL,  // 'hello', 'Sri Ram'

    // Identifiers (table names, column names)
    IDENTIFIER,      // students, name, id

    // Symbols
    STAR,       // *
    COMMA,      // ,
    LPAREN,     // (
    RPAREN,     // )
    SEMICOLON,  // ;
    EQUALS,     // =
    NOT_EQUALS, // !=
    LESS,       // <
    GREATER,    // >
    LESS_EQ,    // <=
    GREATER_EQ, // >=

    // End of input
    END_OF_INPUT
};

// A token = a piece of SQL with its type
// Example: "SELECT" → Token{SELECT, "SELECT"}
//          "42"     → Token{INT_LITERAL, "42"}
//          "'hello'"→ Token{STRING_LITERAL, "hello"}
struct Token {
    TokenType type;
    std::string value;
};

class Tokenizer {
public:
    // Break a SQL string into tokens
    // Example: "SELECT * FROM students WHERE id = 1"
    //   → [{SELECT,"SELECT"}, {STAR,"*"}, {FROM,"FROM"},
    //      {IDENTIFIER,"students"}, {WHERE,"WHERE"},
    //      {IDENTIFIER,"id"}, {EQUALS,"="}, {INT_LITERAL,"1"},
    //      {END_OF_INPUT,""}]
    static std::vector<Token> tokenize(const std::string& sql);

private:
    static bool isKeyword(const std::string& word, TokenType& type);
};

} // namespace minidb
