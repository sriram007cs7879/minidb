#include "core/table.h"
#include <stdexcept>

namespace minidb {

Table::Table(const Schema& schema, const std::string& filepath)
    : btree_(std::make_unique<BTree>(filepath, schema)) {}

void Table::insert(const Row& row) {
    const Schema& schema = btree_->getSchema();
    if (row.size() != schema.columns.size()) {
        throw std::runtime_error(
            "Column count mismatch: expected " +
            std::to_string(schema.columns.size()) +
            ", got " + std::to_string(row.size()));
    }

    // Use first column as key (must be INT)
    if (schema.columns[0].type != ColumnType::INT || !std::holds_alternative<int>(row[0])) {
        throw std::runtime_error("First column must be an INT (used as primary key)");
    }
    int key = std::get<int>(row[0]);

    btree_->insert(key, row);
}

std::vector<Row> Table::selectAll() {
    auto all = btree_->getAll();
    std::vector<Row> rows;
    rows.reserve(all.size());
    for (auto& [key, row] : all) {
        rows.push_back(row);
    }
    return rows;
}

std::vector<Row> Table::selectWhere(std::function<bool(const Row&)> condition) {
    auto all = btree_->getAll();
    std::vector<Row> result;
    for (auto& [key, row] : all) {
        if (condition(row)) {
            result.push_back(row);
        }
    }
    return result;
}

int Table::deleteWhere(std::function<bool(const Row&)> condition) {
    auto all = btree_->getAll();
    std::vector<int> keys_to_delete;

    for (auto& [key, row] : all) {
        if (condition(row)) {
            keys_to_delete.push_back(key);
        }
    }

    for (int key : keys_to_delete) {
        btree_->remove(key);
    }

    return keys_to_delete.size();
}

void Table::flush() {
    btree_->flush();
}

} // namespace minidb
