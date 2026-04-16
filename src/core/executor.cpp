#include "core/executor.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace minidb {

// ============================================================
// MAIN EXECUTE — routes to the right handler
// ============================================================
std::string Executor::execute(Statement* stmt, Catalog& catalog) {
    switch (stmt->type) {
        case StatementType::CREATE_TABLE:
            return executeCreate(static_cast<CreateTableStatement*>(stmt), catalog);
        case StatementType::INSERT:
            return executeInsert(static_cast<InsertStatement*>(stmt), catalog);
        case StatementType::SELECT:
            return executeSelect(static_cast<SelectStatement*>(stmt), catalog);
        case StatementType::DELETE:
            return executeDelete(static_cast<DeleteStatement*>(stmt), catalog);
    }
    return "Unknown statement type.";
}

// ============================================================
// CREATE TABLE
// ============================================================
std::string Executor::executeCreate(CreateTableStatement* stmt, Catalog& catalog) {
    Schema schema;
    schema.table_name = stmt->table_name;
    schema.columns = stmt->columns;

    catalog.createTable(schema);
    return "Table '" + stmt->table_name + "' created.";
}

// ============================================================
// INSERT
// ============================================================
std::string Executor::executeInsert(InsertStatement* stmt, Catalog& catalog) {
    Table* table = catalog.getTable(stmt->table_name);
    if (!table) {
        throw std::runtime_error("Table not found: " + stmt->table_name);
    }

    table->insert(stmt->values);
    return "Inserted 1 row.";
}

// ============================================================
// SELECT — the most complex one
// ============================================================
std::string Executor::executeSelect(SelectStatement* stmt, Catalog& catalog) {
    Table* table = catalog.getTable(stmt->table_name);
    if (!table) {
        throw std::runtime_error("Table not found: " + stmt->table_name);
    }

    const Schema& schema = table->getSchema();

    // Get rows (with or without WHERE)
    std::vector<Row> rows;
    if (stmt->where.empty()) {
        rows = table->selectAll();
    } else {
        auto filter = buildFilter(stmt->where, schema);
        rows = table->selectWhere(filter);
    }

    // Figure out which columns to display
    std::vector<int> col_indices;
    std::vector<std::string> col_names;

    if (stmt->columns.size() == 1 && stmt->columns[0] == "*") {
        // All columns
        for (int i = 0; i < (int)schema.columns.size(); i++) {
            col_indices.push_back(i);
            col_names.push_back(schema.columns[i].name);
        }
    } else {
        // Specific columns
        for (auto& col_name : stmt->columns) {
            bool found = false;
            for (int i = 0; i < (int)schema.columns.size(); i++) {
                if (schema.columns[i].name == col_name) {
                    col_indices.push_back(i);
                    col_names.push_back(col_name);
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Column not found: " + col_name);
            }
        }
    }

    // Calculate column widths for pretty printing
    std::vector<size_t> widths(col_names.size());
    for (size_t i = 0; i < col_names.size(); i++) {
        widths[i] = col_names[i].size();
    }
    for (auto& row : rows) {
        for (size_t i = 0; i < col_indices.size(); i++) {
            size_t val_len = valueToString(row[col_indices[i]]).size();
            widths[i] = std::max(widths[i], val_len);
        }
    }

    // Build the output table
    std::ostringstream out;

    // Header
    out << "| ";
    for (size_t i = 0; i < col_names.size(); i++) {
        out << std::left << std::setw(widths[i]) << col_names[i];
        if (i < col_names.size() - 1) out << " | ";
    }
    out << " |\n";

    // Separator
    out << "|-";
    for (size_t i = 0; i < col_names.size(); i++) {
        out << std::string(widths[i], '-');
        if (i < col_names.size() - 1) out << "-|-";
    }
    out << "-|\n";

    // Rows
    for (auto& row : rows) {
        out << "| ";
        for (size_t i = 0; i < col_indices.size(); i++) {
            out << std::left << std::setw(widths[i]) << valueToString(row[col_indices[i]]);
            if (i < col_indices.size() - 1) out << " | ";
        }
        out << " |\n";
    }

    // Row count
    out << "(" << rows.size() << (rows.size() == 1 ? " row)" : " rows)");

    return out.str();
}

// ============================================================
// DELETE
// ============================================================
std::string Executor::executeDelete(DeleteStatement* stmt, Catalog& catalog) {
    Table* table = catalog.getTable(stmt->table_name);
    if (!table) {
        throw std::runtime_error("Table not found: " + stmt->table_name);
    }

    int count;
    if (stmt->where.empty()) {
        // DELETE FROM table (delete all)
        count = table->deleteWhere([](const Row&) { return true; });
    } else {
        auto filter = buildFilter(stmt->where, table->getSchema());
        count = table->deleteWhere(filter);
    }

    return "Deleted " + std::to_string(count) + (count == 1 ? " row." : " rows.");
}

// ============================================================
// BUILD FILTER — converts WHERE clause to a C++ function
// ============================================================
// WHERE age > 18 AND name = 'Sri Ram'
// becomes a function that takes a Row and returns true/false

std::function<bool(const Row&)> Executor::buildFilter(
    const WhereClause& where, const Schema& schema) {

    return [&where, &schema](const Row& row) -> bool {
        bool result = true;  // start with true, AND narrows it down

        for (size_t i = 0; i < where.conditions.size(); i++) {
            const Condition& cond = where.conditions[i];

            // Find the column index
            int col_idx = -1;
            for (int j = 0; j < (int)schema.columns.size(); j++) {
                if (schema.columns[j].name == cond.column) {
                    col_idx = j;
                    break;
                }
            }
            if (col_idx == -1) {
                throw std::runtime_error("Column not found in WHERE: " + cond.column);
            }

            // Evaluate the condition
            const Value& row_val = row[col_idx];
            bool cond_result = false;

            if (std::holds_alternative<int>(row_val) && std::holds_alternative<int>(cond.value)) {
                int a = std::get<int>(row_val);
                int b = std::get<int>(cond.value);
                if (cond.op == "=")       cond_result = (a == b);
                else if (cond.op == "!=") cond_result = (a != b);
                else if (cond.op == "<")  cond_result = (a < b);
                else if (cond.op == ">")  cond_result = (a > b);
                else if (cond.op == "<=") cond_result = (a <= b);
                else if (cond.op == ">=") cond_result = (a >= b);
            } else {
                // Compare as strings
                std::string a = valueToString(row_val);
                std::string b = valueToString(cond.value);
                if (cond.op == "=")       cond_result = (a == b);
                else if (cond.op == "!=") cond_result = (a != b);
                else if (cond.op == "<")  cond_result = (a < b);
                else if (cond.op == ">")  cond_result = (a > b);
                else if (cond.op == "<=") cond_result = (a <= b);
                else if (cond.op == ">=") cond_result = (a >= b);
            }

            // Combine with previous result using AND/OR
            if (i == 0) {
                result = cond_result;
            } else {
                std::string logic = where.logic_ops[i - 1];
                if (logic == "AND") {
                    result = result && cond_result;
                } else {
                    result = result || cond_result;
                }
            }
        }

        return result;
    };
}

} // namespace minidb
