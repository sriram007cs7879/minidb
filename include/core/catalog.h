#pragma once

#include "core/table.h"
#include <string>
#include <map>
#include <memory>
#include <vector>

namespace minidb {

// Catalog manages all tables in the database.
// Each table is a .db file in the data directory.
//
// Example:
//   Catalog catalog("data/");
//   catalog.createTable(schema);
//   Table* t = catalog.getTable("students");

class Catalog {
public:
    explicit Catalog(const std::string& data_dir);

    void createTable(const Schema& schema);
    Table* getTable(const std::string& name);
    bool tableExists(const std::string& name);
    std::vector<std::string> listTables();
    void saveAll();
    void loadAll();

private:
    std::string data_dir_;
    std::map<std::string, std::unique_ptr<Table>> tables_;
    std::string getTablePath(const std::string& name);
};

} // namespace minidb
