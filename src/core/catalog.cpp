#include "core/catalog.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace minidb {

Catalog::Catalog(const std::string& data_dir) : data_dir_(data_dir) {
    fs::create_directories(data_dir_);
}

void Catalog::createTable(const Schema& schema) {
    if (tableExists(schema.table_name)) {
        throw std::runtime_error("Table already exists: " + schema.table_name);
    }

    std::string path = getTablePath(schema.table_name);
    auto table = std::make_unique<Table>(schema, path);
    tables_[schema.table_name] = std::move(table);
}

Table* Catalog::getTable(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) return nullptr;
    return it->second.get();
}

bool Catalog::tableExists(const std::string& name) {
    return tables_.find(name) != tables_.end();
}

std::vector<std::string> Catalog::listTables() {
    std::vector<std::string> names;
    for (auto& [name, table] : tables_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void Catalog::saveAll() {
    for (auto& [name, table] : tables_) {
        table->flush();
    }
}

// Load all existing .db files from the data directory
void Catalog::loadAll() {
    if (!fs::exists(data_dir_)) return;

    for (auto& entry : fs::directory_iterator(data_dir_)) {
        if (entry.path().extension() == ".db") {
            std::string table_name = entry.path().stem().string();
            // Open the B+ Tree file — it contains the schema in page 0
            Schema placeholder;
            placeholder.table_name = table_name;
            auto table = std::make_unique<Table>(placeholder, entry.path().string());
            tables_[table->getSchema().table_name] = std::move(table);
        }
    }
}

std::string Catalog::getTablePath(const std::string& name) {
    return data_dir_ + "/" + name + ".db";
}

} // namespace minidb
