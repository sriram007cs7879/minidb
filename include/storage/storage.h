#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <utility>

namespace minidb {

// Storage handles saving/loading table data to/from simple text files.
//
// File format:
//   Line 1: TABLE:students
//   Line 2: COLUMNS:id:INT,name:TEXT,age:INT
//   Line 3+: ROW:1|Sri Ram|20
//            ROW:2|Arun|19
//
// Simple and human-readable — you can open the .dat file in any text editor.

class Storage {
public:
    // Save a table's schema and rows to a file
    static void saveTable(const std::string& filename,
                          const Schema& schema,
                          const std::vector<std::pair<int, Row>>& rows);

    // Load a table's schema and rows from a file
    // Returns true if file existed and was loaded, false if file not found
    static bool loadTable(const std::string& filename,
                          Schema& schema,
                          std::vector<std::pair<int, Row>>& rows);

    // Delete a table's file
    static void deleteFile(const std::string& filename);

private:
    static std::string serializeRow(const Row& row, const Schema& schema);
    static Row deserializeRow(const std::string& line, const Schema& schema);
};

} // namespace minidb
