#include "storage/storage.h"
#include <fstream>
#include <sstream>
#include <cstdio>

namespace minidb {

// ============================================================
// SAVE TABLE — Write schema + rows to a text file
// ============================================================
// File format example:
//   TABLE:students
//   COLUMNS:id:INT,name:TEXT,age:INT
//   ROW:1|Sri Ram|20
//   ROW:2|Arun|19

void Storage::saveTable(const std::string& filename,
                        const Schema& schema,
                        const std::vector<std::pair<int, Row>>& rows) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    // Write table name
    file << "TABLE:" << schema.table_name << "\n";

    // Write column definitions
    file << "COLUMNS:";
    for (size_t i = 0; i < schema.columns.size(); i++) {
        if (i > 0) file << ",";
        file << schema.columns[i].name << ":";
        file << (schema.columns[i].type == ColumnType::INT ? "INT" : "TEXT");
    }
    file << "\n";

    // Write rows
    for (auto& [key, row] : rows) {
        file << "ROW:" << serializeRow(row, schema) << "\n";
    }

    file.close();
}

// ============================================================
// LOAD TABLE — Read schema + rows from a text file
// ============================================================

bool Storage::loadTable(const std::string& filename,
                        Schema& schema,
                        std::vector<std::pair<int, Row>>& rows) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line;
    rows.clear();

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        if (line.substr(0, 6) == "TABLE:") {
            // Parse table name
            schema.table_name = line.substr(6);

        } else if (line.substr(0, 8) == "COLUMNS:") {
            // Parse column definitions: "id:INT,name:TEXT,age:INT"
            schema.columns.clear();
            std::string cols_str = line.substr(8);
            std::stringstream ss(cols_str);
            std::string col_def;
            while (std::getline(ss, col_def, ',')) {
                size_t colon = col_def.find(':');
                Column col;
                col.name = col_def.substr(0, colon);
                col.type = (col_def.substr(colon + 1) == "INT")
                           ? ColumnType::INT : ColumnType::TEXT;
                schema.columns.push_back(col);
            }

        } else if (line.substr(0, 4) == "ROW:") {
            // Parse a row
            Row row = deserializeRow(line.substr(4), schema);
            // Use first INT column as key
            int key = 0;
            if (!row.empty() && std::holds_alternative<int>(row[0])) {
                key = std::get<int>(row[0]);
            }
            rows.push_back({key, row});
        }
    }

    file.close();
    return true;
}

// ============================================================
// DELETE FILE
// ============================================================
void Storage::deleteFile(const std::string& filename) {
    std::remove(filename.c_str());
}

// ============================================================
// SERIALIZE / DESERIALIZE
// ============================================================
// Row {1, "Sri Ram", 20} with schema (INT, TEXT, INT) → "1|Sri Ram|20"

std::string Storage::serializeRow(const Row& row, const Schema& schema) {
    std::string result;
    for (size_t i = 0; i < row.size(); i++) {
        if (i > 0) result += "|";
        result += valueToString(row[i]);
    }
    return result;
}

// "1|Sri Ram|20" with schema (INT, TEXT, INT) → Row{1, "Sri Ram", 20}

Row Storage::deserializeRow(const std::string& line, const Schema& schema) {
    Row row;
    std::stringstream ss(line);
    std::string field;
    size_t col_idx = 0;

    while (std::getline(ss, field, '|') && col_idx < schema.columns.size()) {
        if (schema.columns[col_idx].type == ColumnType::INT) {
            try {
                row.push_back(std::stoi(field));
            } catch (...) {
                row.push_back(0);
            }
        } else {
            row.push_back(field);
        }
        col_idx++;
    }

    return row;
}

} // namespace minidb
