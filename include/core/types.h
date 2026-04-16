#pragma once

#include <string>
#include <vector>
#include <variant>
#include <functional>

namespace minidb {

// A Value can be either an integer or a string
using Value = std::variant<int, std::string>;

// A Row is a list of values: {1, "Sri Ram", 20}
using Row = std::vector<Value>;

// Column types
enum class ColumnType { INT, TEXT };

// A column definition: name + type
struct Column {
    std::string name;
    ColumnType type;
};

// Schema describes the structure of a table
struct Schema {
    std::string table_name;
    std::vector<Column> columns;
};

// Convert a Value to string for display
inline std::string valueToString(const Value& val) {
    if (std::holds_alternative<int>(val)) {
        return std::to_string(std::get<int>(val));
    }
    return std::get<std::string>(val);
}

// Get int from a Value (returns 0 if it's a string)
inline int valueToInt(const Value& val) {
    if (std::holds_alternative<int>(val)) {
        return std::get<int>(val);
    }
    return 0;
}

} // namespace minidb
