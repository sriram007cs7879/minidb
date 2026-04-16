#include "sql/tokenizer.h"
#include <cctype>
#include <algorithm>
#include <stdexcept>

namespace minidb {

// ============================================================
// TOKENIZE — Break SQL string into tokens
// ============================================================
// Example: "SELECT * FROM students WHERE id = 1;"
//
// Step by step:
//   "SELECT"   → Token{SELECT, "SELECT"}
//   " "        → skip whitespace
//   "*"        → Token{STAR, "*"}
//   " "        → skip
//   "FROM"     → Token{FROM, "FROM"}
//   " "        → skip
//   "students" → Token{IDENTIFIER, "students"}
//   " "        → skip
//   "WHERE"    → Token{WHERE, "WHERE"}
//   " "        → skip
//   "id"       → Token{IDENTIFIER, "id"}
//   " "        → skip
//   "="        → Token{EQUALS, "="}
//   " "        → skip
//   "1"        → Token{INT_LITERAL, "1"}
//   ";"        → Token{SEMICOLON, ";"}

std::vector<Token> Tokenizer::tokenize(const std::string& sql) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < sql.size()) {
        char c = sql[i];

        // Skip whitespace
        if (std::isspace(c)) {
            i++;
            continue;
        }

        // Single-character symbols
        if (c == '*') { tokens.push_back({TokenType::STAR, "*"}); i++; continue; }
        if (c == ',') { tokens.push_back({TokenType::COMMA, ","}); i++; continue; }
        if (c == '(') { tokens.push_back({TokenType::LPAREN, "("}); i++; continue; }
        if (c == ')') { tokens.push_back({TokenType::RPAREN, ")"}); i++; continue; }
        if (c == ';') { tokens.push_back({TokenType::SEMICOLON, ";"}); i++; continue; }

        // Two-character operators: !=, <=, >=
        if (c == '!' && i + 1 < sql.size() && sql[i + 1] == '=') {
            tokens.push_back({TokenType::NOT_EQUALS, "!="});
            i += 2;
            continue;
        }
        if (c == '<' && i + 1 < sql.size() && sql[i + 1] == '=') {
            tokens.push_back({TokenType::LESS_EQ, "<="});
            i += 2;
            continue;
        }
        if (c == '>' && i + 1 < sql.size() && sql[i + 1] == '=') {
            tokens.push_back({TokenType::GREATER_EQ, ">="});
            i += 2;
            continue;
        }

        // Single-character operators
        if (c == '=') { tokens.push_back({TokenType::EQUALS, "="}); i++; continue; }
        if (c == '<') { tokens.push_back({TokenType::LESS, "<"}); i++; continue; }
        if (c == '>') { tokens.push_back({TokenType::GREATER, ">"}); i++; continue; }

        // String literal: 'hello world'
        if (c == '\'') {
            i++;  // skip opening quote
            std::string str;
            while (i < sql.size() && sql[i] != '\'') {
                str += sql[i];
                i++;
            }
            if (i >= sql.size()) {
                throw std::runtime_error("Unterminated string literal");
            }
            i++;  // skip closing quote
            tokens.push_back({TokenType::STRING_LITERAL, str});
            continue;
        }

        // Number: 42, -5
        if (std::isdigit(c) || (c == '-' && i + 1 < sql.size() && std::isdigit(sql[i + 1]))) {
            std::string num;
            if (c == '-') { num += c; i++; }
            while (i < sql.size() && std::isdigit(sql[i])) {
                num += sql[i];
                i++;
            }
            tokens.push_back({TokenType::INT_LITERAL, num});
            continue;
        }

        // Word: could be a keyword (SELECT, FROM) or identifier (students, name)
        if (std::isalpha(c) || c == '_') {
            std::string word;
            while (i < sql.size() && (std::isalnum(sql[i]) || sql[i] == '_')) {
                word += sql[i];
                i++;
            }

            // Check if it's a keyword
            TokenType keyword_type;
            if (isKeyword(word, keyword_type)) {
                tokens.push_back({keyword_type, word});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, word});
            }
            continue;
        }

        throw std::runtime_error(std::string("Unexpected character: ") + c);
    }

    tokens.push_back({TokenType::END_OF_INPUT, ""});
    return tokens;
}

// Check if a word is a SQL keyword (case-insensitive)
bool Tokenizer::isKeyword(const std::string& word, TokenType& type) {
    // Convert to uppercase for comparison
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "SELECT")  { type = TokenType::SELECT; return true; }
    if (upper == "INSERT")  { type = TokenType::INSERT; return true; }
    if (upper == "INTO")    { type = TokenType::INTO; return true; }
    if (upper == "FROM")    { type = TokenType::FROM; return true; }
    if (upper == "WHERE")   { type = TokenType::WHERE; return true; }
    if (upper == "CREATE")  { type = TokenType::CREATE; return true; }
    if (upper == "TABLE")   { type = TokenType::TABLE; return true; }
    if (upper == "DELETE")  { type = TokenType::DELETE; return true; }
    if (upper == "VALUES")  { type = TokenType::VALUES; return true; }
    if (upper == "AND")     { type = TokenType::AND; return true; }
    if (upper == "OR")      { type = TokenType::OR; return true; }
    if (upper == "NOT")     { type = TokenType::NOT; return true; }
    if (upper == "INT")     { type = TokenType::INT_TYPE; return true; }
    if (upper == "TEXT")    { type = TokenType::TEXT_TYPE; return true; }

    return false;
}

} // namespace minidb
