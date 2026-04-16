#pragma once

#include "core/types.h"
#include "storage/btree.h"
#include "storage/storage.h"
#include <string>
#include <functional>

namespace minidb {

// Table = Schema + B-Tree (data) + file path
// One Table object per table in the database.
//
// Example:
//   Table students(schema, "data/students.dat");
//   students.insert(1, {1, "Sri Ram", 20});
//   auto results = students.selectAll();

class Table {
public:
    Table(const Schema& schema, const std::string& filepath);

    // Insert a row (key = first column value, which must be INT)
    void insert(const Row& row);

    // Select all rows
    std::vector<Row> selectAll();

    // Select rows matching a condition
    std::vector<Row> selectWhere(std::function<bool(const Row&)> condition);

    // Delete rows matching a condition, returns count deleted
    int deleteWhere(std::function<bool(const Row&)> condition);

    // Save to disk
    void save();

    // Load from disk
    void load();

    const Schema& getSchema() const { return schema_; }

private:
    Schema schema_;
    BTree btree_;
    std::string filepath_;
    int next_id_;  // auto-increment for row ID if first column is not an int
};

} // namespace minidb
