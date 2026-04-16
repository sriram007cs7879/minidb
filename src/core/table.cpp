#include "core/table.h"
#include <stdexcept>

namespace minidb {

Table::Table(const Schema& schema, const std::string& filepath)
    : schema_(schema), filepath_(filepath), next_id_(1) {}

// ============================================================
// INSERT
// ============================================================
// Uses the first INT column as the B-Tree key.
// Example: INSERT INTO students VALUES (1, 'Sri Ram', 20);
//   → key = 1, row = {1, "Sri Ram", 20}

void Table::insert(const Row& row) {
    if (row.size() != schema_.columns.size()) {
        throw std::runtime_error(
            "Column count mismatch: expected " +
            std::to_string(schema_.columns.size()) +
            ", got " + std::to_string(row.size()));
    }

    // Use first column as key (must be INT)
    int key;
    if (schema_.columns[0].type == ColumnType::INT && std::holds_alternative<int>(row[0])) {
        key = std::get<int>(row[0]);
    } else {
        // Auto-increment if first column isn't a usable int
        key = next_id_++;
    }

    // Check for duplicate key
    if (btree_.search(key) != nullptr) {
        throw std::runtime_error("Duplicate key: " + std::to_string(key));
    }

    btree_.insert(key, row);

    // Keep next_id ahead of the highest key
    if (key >= next_id_) {
        next_id_ = key + 1;
    }

    // Auto-save after every insert
    save();
}

// ============================================================
// SELECT ALL
// ============================================================
std::vector<Row> Table::selectAll() {
    auto all = btree_.getAll();
    std::vector<Row> rows;
    rows.reserve(all.size());
    for (auto& [key, row] : all) {
        rows.push_back(row);
    }
    return rows;
}

// ============================================================
// SELECT WHERE
// ============================================================
std::vector<Row> Table::selectWhere(std::function<bool(const Row&)> condition) {
    auto all = btree_.getAll();
    std::vector<Row> result;
    for (auto& [key, row] : all) {
        if (condition(row)) {
            result.push_back(row);
        }
    }
    return result;
}

// ============================================================
// DELETE WHERE
// ============================================================
int Table::deleteWhere(std::function<bool(const Row&)> condition) {
    auto all = btree_.getAll();
    std::vector<int> keys_to_delete;

    for (auto& [key, row] : all) {
        if (condition(row)) {
            keys_to_delete.push_back(key);
        }
    }

    for (int key : keys_to_delete) {
        btree_.remove(key);
    }

    if (!keys_to_delete.empty()) {
        save();
    }

    return keys_to_delete.size();
}

// ============================================================
// SAVE / LOAD
// ============================================================
void Table::save() {
    auto all = btree_.getAll();
    Storage::saveTable(filepath_, schema_, all);
}

void Table::load() {
    Schema schema;
    std::vector<std::pair<int, Row>> rows;

    if (Storage::loadTable(filepath_, schema, rows)) {
        schema_ = schema;
        // Re-insert all rows into B-Tree
        for (auto& [key, row] : rows) {
            btree_.insert(key, row);
            if (key >= next_id_) {
                next_id_ = key + 1;
            }
        }
    }
}

} // namespace minidb
