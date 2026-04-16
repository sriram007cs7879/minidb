#pragma once

#include "core/table.h"
#include <string>
#include <map>
#include <memory>

namespace minidb {

// Catalog = manager of all tables in the database.
// It knows which tables exist and where their files are stored.
//
// Example:
//   Catalog catalog("data/");
//   catalog.createTable(schema);
//   Table* t = catalog.getTable("students");

class Catalog {
public:
    // data_dir: directory where table files are stored
    explicit Catalog(const std::string& data_dir);

    // Create a new table. Throws if already exists.
    void createTable(const Schema& schema);

    // Get a table by name. Returns nullptr if not found.
    Table* getTable(const std::string& name);

    // Check if a table exists
    bool tableExists(const std::string& name);

    // List all table names
    std::vector<std::string> listTables();

    // Save all tables to disk
    void saveAll();

    // Load catalog from disk (reads all .dat files in data_dir)
    void loadAll();

private:
    std::string data_dir_;
    std::map<std::string, std::unique_ptr<Table>> tables_;

    std::string getTablePath(const std::string& name);
};

} // namespace minidb
