#pragma once

#include "core/types.h"
#include "storage/btree.h"
#include <string>
#include <functional>
#include <memory>

namespace minidb {

// Table = B+ Tree file + schema.
// Each table is backed by a single .db file managed by the B+ Tree.
//
// Example:
//   Table students(schema, "data/students.db");
//   students.insert({1, "Sri Ram", 20});
//   auto results = students.selectAll();

class Table {
public:
    Table(const Schema& schema, const std::string& filepath);

    // Insert a row (first INT column is used as the B-Tree key)
    void insert(const Row& row);

    // Select all rows
    std::vector<Row> selectAll();

    // Select rows matching a condition
    std::vector<Row> selectWhere(std::function<bool(const Row&)> condition);

    // Delete rows matching a condition, returns count deleted
    int deleteWhere(std::function<bool(const Row&)> condition);

    // Flush data to disk
    void flush();

    const Schema& getSchema() const { return btree_->getSchema(); }

private:
    std::unique_ptr<BTree> btree_;
};

} // namespace minidb
