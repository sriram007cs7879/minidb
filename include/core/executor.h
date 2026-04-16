#pragma once

#include "core/catalog.h"
#include "sql/parser.h"
#include <string>

namespace minidb {

// Executor takes a parsed SQL statement and runs it against the catalog.
// Returns a string result to display to the user.
//
// Example:
//   auto stmt = Parser::parse(tokens);
//   string result = Executor::execute(stmt.get(), catalog);
//   cout << result << endl;

class Executor {
public:
    static std::string execute(Statement* stmt, Catalog& catalog);

private:
    static std::string executeCreate(CreateTableStatement* stmt, Catalog& catalog);
    static std::string executeInsert(InsertStatement* stmt, Catalog& catalog);
    static std::string executeSelect(SelectStatement* stmt, Catalog& catalog);
    static std::string executeDelete(DeleteStatement* stmt, Catalog& catalog);

    // Build a filter function from a WHERE clause
    static std::function<bool(const Row&)> buildFilter(
        const WhereClause& where, const Schema& schema);
};

} // namespace minidb
