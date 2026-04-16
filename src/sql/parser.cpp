#include "sql/parser.h"
#include <stdexcept>
#include <algorithm>

namespace minidb {

// ============================================================
// MAIN PARSE FUNCTION
// ============================================================
// Looks at the first token to decide what kind of statement it is,
// then calls the appropriate parse method.

std::unique_ptr<Statement> Parser::parse(const std::vector<Token>& tokens) {
    Parser parser(tokens);

    Token first = parser.current();

    if (first.type == TokenType::CREATE) {
        return parser.parseCreateTable();
    } else if (first.type == TokenType::INSERT) {
        return parser.parseInsert();
    } else if (first.type == TokenType::SELECT) {
        return parser.parseSelect();
    } else if (first.type == TokenType::DELETE) {
        return parser.parseDelete();
    }

    throw std::runtime_error("Unknown statement. Expected CREATE, INSERT, SELECT, or DELETE.");
}

// ============================================================
// CREATE TABLE students (id INT, name TEXT, age INT);
// ============================================================
std::unique_ptr<CreateTableStatement> Parser::parseCreateTable() {
    auto stmt = std::make_unique<CreateTableStatement>();

    expect(TokenType::CREATE);   // consume "CREATE"
    expect(TokenType::TABLE);    // consume "TABLE"

    // Table name
    Token name_token = expect(TokenType::IDENTIFIER);
    stmt->table_name = name_token.value;

    expect(TokenType::LPAREN);   // consume "("

    // Parse column definitions: id INT, name TEXT, age INT
    while (true) {
        Column col;
        // Column name
        Token col_name = expect(TokenType::IDENTIFIER);
        col.name = col_name.value;

        // Column type
        Token col_type = current();
        if (col_type.type == TokenType::INT_TYPE) {
            col.type = ColumnType::INT;
            advance();
        } else if (col_type.type == TokenType::TEXT_TYPE) {
            col.type = ColumnType::TEXT;
            advance();
        } else {
            throw std::runtime_error("Expected INT or TEXT, got: " + col_type.value);
        }

        stmt->columns.push_back(col);

        // If next is comma, there are more columns
        if (!match(TokenType::COMMA)) break;
    }

    expect(TokenType::RPAREN);   // consume ")"
    match(TokenType::SEMICOLON); // optional ";"

    return stmt;
}

// ============================================================
// INSERT INTO students VALUES (1, 'Sri Ram', 20);
// ============================================================
std::unique_ptr<InsertStatement> Parser::parseInsert() {
    auto stmt = std::make_unique<InsertStatement>();

    expect(TokenType::INSERT);   // consume "INSERT"
    expect(TokenType::INTO);     // consume "INTO"

    // Table name
    Token name_token = expect(TokenType::IDENTIFIER);
    stmt->table_name = name_token.value;

    expect(TokenType::VALUES);   // consume "VALUES"
    expect(TokenType::LPAREN);   // consume "("

    // Parse values: 1, 'Sri Ram', 20
    while (true) {
        Token val = current();
        if (val.type == TokenType::INT_LITERAL) {
            stmt->values.push_back(std::stoi(val.value));
            advance();
        } else if (val.type == TokenType::STRING_LITERAL) {
            stmt->values.push_back(val.value);
            advance();
        } else {
            throw std::runtime_error("Expected a value (number or string), got: " + val.value);
        }

        if (!match(TokenType::COMMA)) break;
    }

    expect(TokenType::RPAREN);   // consume ")"
    match(TokenType::SEMICOLON); // optional ";"

    return stmt;
}

// ============================================================
// SELECT * FROM students WHERE age > 18;
// SELECT id, name FROM students;
// ============================================================
std::unique_ptr<SelectStatement> Parser::parseSelect() {
    auto stmt = std::make_unique<SelectStatement>();

    expect(TokenType::SELECT);   // consume "SELECT"

    // Parse column list: * or col1, col2, col3
    if (match(TokenType::STAR)) {
        stmt->columns.push_back("*");
    } else {
        while (true) {
            Token col = expect(TokenType::IDENTIFIER);
            stmt->columns.push_back(col.value);
            if (!match(TokenType::COMMA)) break;
        }
    }

    expect(TokenType::FROM);     // consume "FROM"

    // Table name
    Token name_token = expect(TokenType::IDENTIFIER);
    stmt->table_name = name_token.value;

    // Optional WHERE clause
    if (match(TokenType::WHERE)) {
        stmt->where = parseWhere();
    }

    match(TokenType::SEMICOLON); // optional ";"

    return stmt;
}

// ============================================================
// DELETE FROM students WHERE id = 1;
// ============================================================
std::unique_ptr<DeleteStatement> Parser::parseDelete() {
    auto stmt = std::make_unique<DeleteStatement>();

    expect(TokenType::DELETE);   // consume "DELETE"
    expect(TokenType::FROM);     // consume "FROM"

    // Table name
    Token name_token = expect(TokenType::IDENTIFIER);
    stmt->table_name = name_token.value;

    // Optional WHERE clause
    if (match(TokenType::WHERE)) {
        stmt->where = parseWhere();
    }

    match(TokenType::SEMICOLON); // optional ";"

    return stmt;
}

// ============================================================
// WHERE clause: age > 18 AND name = 'Sri Ram'
// ============================================================
WhereClause Parser::parseWhere() {
    WhereClause where;

    where.conditions.push_back(parseCondition());

    // Check for AND/OR
    while (current().type == TokenType::AND || current().type == TokenType::OR) {
        Token logic = advance();
        std::string upper = logic.value;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        where.logic_ops.push_back(upper);
        where.conditions.push_back(parseCondition());
    }

    return where;
}

// ============================================================
// Single condition: id = 1, age > 18, name = 'Sri Ram'
// ============================================================
Condition Parser::parseCondition() {
    Condition cond;

    // Column name
    Token col = expect(TokenType::IDENTIFIER);
    cond.column = col.value;

    // Operator
    Token op = current();
    if (op.type == TokenType::EQUALS || op.type == TokenType::NOT_EQUALS ||
        op.type == TokenType::LESS || op.type == TokenType::GREATER ||
        op.type == TokenType::LESS_EQ || op.type == TokenType::GREATER_EQ) {
        cond.op = op.value;
        advance();
    } else {
        throw std::runtime_error("Expected comparison operator (=, !=, <, >, <=, >=), got: " + op.value);
    }

    // Value
    Token val = current();
    if (val.type == TokenType::INT_LITERAL) {
        cond.value = std::stoi(val.value);
        advance();
    } else if (val.type == TokenType::STRING_LITERAL) {
        cond.value = val.value;
        advance();
    } else {
        throw std::runtime_error("Expected a value after operator, got: " + val.value);
    }

    return cond;
}

// ============================================================
// HELPER METHODS
// ============================================================

Token Parser::current() {
    if (pos_ >= tokens_->size()) {
        return {TokenType::END_OF_INPUT, ""};
    }
    return (*tokens_)[pos_];
}

Token Parser::advance() {
    Token tok = current();
    pos_++;
    return tok;
}

Token Parser::expect(TokenType type) {
    Token tok = current();
    if (tok.type != type) {
        throw std::runtime_error("Unexpected token: " + tok.value);
    }
    pos_++;
    return tok;
}

bool Parser::match(TokenType type) {
    if (current().type == type) {
        pos_++;
        return true;
    }
    return false;
}

bool Parser::isAtEnd() {
    return current().type == TokenType::END_OF_INPUT;
}

} // namespace minidb
