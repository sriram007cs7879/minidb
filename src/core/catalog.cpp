#include "core/catalog.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace minidb {

Catalog::Catalog(const std::string& data_dir) : data_dir_(data_dir) {
    // Create the data directory if it doesn't exist
    fs::create_directories(data_dir_);
}

// ============================================================
// CREATE TABLE
// ============================================================
void Catalog::createTable(const Schema& schema) {
    if (tableExists(schema.table_name)) {
        throw std::runtime_error("Table already exists: " + schema.table_name);
    }

    std::string path = getTablePath(schema.table_name);
    auto table = std::make_unique<Table>(schema, path);
    table->save();  // Create the file
    tables_[schema.table_name] = std::move(table);
}

// ============================================================
// GET TABLE
// ============================================================
Table* Catalog::getTable(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) return nullptr;
    return it->second.get();
}

// ============================================================
// TABLE EXISTS
// ============================================================
bool Catalog::tableExists(const std::string& name) {
    return tables_.find(name) != tables_.end();
}

// ============================================================
// LIST TABLES
// ============================================================
std::vector<std::string> Catalog::listTables() {
    std::vector<std::string> names;
    for (auto& [name, table] : tables_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// ============================================================
// SAVE ALL
// ============================================================
void Catalog::saveAll() {
    for (auto& [name, table] : tables_) {
        table->save();
    }
}

// ============================================================
// LOAD ALL — scan data directory for .dat files
// ============================================================
void Catalog::loadAll() {
    if (!fs::exists(data_dir_)) return;

    for (auto& entry : fs::directory_iterator(data_dir_)) {
        if (entry.path().extension() == ".dat") {
            Schema schema;
            std::vector<std::pair<int, Row>> rows;

            if (Storage::loadTable(entry.path().string(), schema, rows)) {
                auto table = std::make_unique<Table>(schema, entry.path().string());
                table->load();
                tables_[schema.table_name] = std::move(table);
            }
        }
    }
}

// ============================================================
// HELPER
// ============================================================
std::string Catalog::getTablePath(const std::string& name) {
    return data_dir_ + "/" + name + ".dat";
}

} // namespace minidb
