#pragma once

#include "sql/tokenizer.h"
#include "core/types.h"
#include <string>
#include <vector>
#include <memory>

namespace minidb {

// ============================================================
// AST (Abstract Syntax Tree) — the parsed form of SQL
// ============================================================
// When you type: SELECT * FROM students WHERE id = 1
// The parser converts it to a SelectStatement object:
//   table_name = "students"
//   columns = ["*"]
//   where_clause = Condition{column="id", op="=", value=1}
//
// This structured form is easy for the Executor to work with.

// A WHERE condition: column op value
//   Example: id = 1, age > 18, name = 'Sri Ram'
struct Condition {
    std::string column;
    std::string op;       // "=", "!=", "<", ">", "<=", ">="
    Value value;
};

// A WHERE clause can have multiple conditions joined by AND/OR
struct WhereClause {
    std::vector<Condition> conditions;
    std::vector<std::string> logic_ops;  // "AND" or "OR" between conditions
    bool empty() const { return conditions.empty(); }
};

// Types of SQL statements
enum class StatementType {
    CREATE_TABLE,
    INSERT,
    SELECT,
    DELETE
};

// Base statement
struct Statement {
    StatementType type;
    virtual ~Statement() = default;
};

// CREATE TABLE students (id INT, name TEXT, age INT);
struct CreateTableStatement : Statement {
    std::string table_name;
    std::vector<Column> columns;
    CreateTableStatement() { type = StatementType::CREATE_TABLE; }
};

// INSERT INTO students VALUES (1, 'Sri Ram', 20);
struct InsertStatement : Statement {
    std::string table_name;
    std::vector<Value> values;
    InsertStatement() { type = StatementType::INSERT; }
};

// SELECT * FROM students WHERE age > 18;
struct SelectStatement : Statement {
    std::string table_name;
    std::vector<std::string> columns;  // ["*"] or ["id", "name"]
    WhereClause where;
    SelectStatement() { type = StatementType::SELECT; }
};

// DELETE FROM students WHERE id = 1;
struct DeleteStatement : Statement {
    std::string table_name;
    WhereClause where;
    DeleteStatement() { type = StatementType::DELETE; }
};

class Parser {
public:
    // Parse tokens into a Statement
    // Throws runtime_error on syntax errors
    static std::unique_ptr<Statement> parse(const std::vector<Token>& tokens);

private:
    // Parser state
    const std::vector<Token>* tokens_;
    size_t pos_;

    Parser(const std::vector<Token>& tokens) : tokens_(&tokens), pos_(0) {}

    // Parsing methods — one per statement type
    std::unique_ptr<CreateTableStatement> parseCreateTable();
    std::unique_ptr<InsertStatement> parseInsert();
    std::unique_ptr<SelectStatement> parseSelect();
    std::unique_ptr<DeleteStatement> parseDelete();
    WhereClause parseWhere();
    Condition parseCondition();

    // Helper methods
    Token current();                         // get current token
    Token advance();                         // move to next token, return current
    Token expect(TokenType type);            // advance and check type
    bool match(TokenType type);              // if current matches, advance and return true
    bool isAtEnd();
};

} // namespace minidb
