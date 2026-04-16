#include "core/catalog.h"
#include "core/executor.h"
#include "sql/tokenizer.h"
#include "sql/parser.h"

#include <iostream>
#include <string>

using namespace minidb;

void printHelp() {
    std::cout << "\nMiniDB — A mini relational database engine\n";
    std::cout << "============================================\n";
    std::cout << "\nSQL Commands:\n";
    std::cout << "  CREATE TABLE <name> (<col> <type>, ...);  Create a new table\n";
    std::cout << "  INSERT INTO <name> VALUES (...);          Insert a row\n";
    std::cout << "  SELECT * FROM <name>;                     Query rows\n";
    std::cout << "  SELECT * FROM <name> WHERE <condition>;   Query with filter\n";
    std::cout << "  DELETE FROM <name> WHERE <condition>;     Delete rows\n";
    std::cout << "\nMeta Commands:\n";
    std::cout << "  .tables    Show all tables\n";
    std::cout << "  .schema    Show all table schemas\n";
    std::cout << "  .help      Show this help message\n";
    std::cout << "  .exit      Exit MiniDB\n";
    std::cout << "\nTypes: INT, TEXT\n";
    std::cout << "Operators: =, !=, <, >, <=, >=\n";
    std::cout << "Logic: AND, OR\n\n";
}

int main(int argc, char* argv[]) {
    // Determine data directory
    std::string data_dir = "minidb_data";
    if (argc > 1) {
        data_dir = argv[1];
    }

    std::cout << "MiniDB v1.0.0\n";
    std::cout << "A mini relational database engine with B+ Tree index and page-based storage\n";
    std::cout << "Data directory: " << data_dir << "/\n";
    std::cout << "Type .help for usage information.\n\n";

    // Create catalog and load existing tables
    Catalog catalog(data_dir);
    catalog.loadAll();

    // Report loaded tables
    auto tables = catalog.listTables();
    if (!tables.empty()) {
        std::cout << "Loaded " << tables.size() << " table(s): ";
        for (size_t i = 0; i < tables.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << tables[i];
        }
        std::cout << "\n\n";
    }

    // REPL — Read-Eval-Print Loop
    std::string line;
    while (true) {
        std::cout << "minidb> ";
        if (!std::getline(std::cin, line)) {
            // EOF (Ctrl+D)
            std::cout << "\nBye!\n";
            break;
        }

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Remove trailing whitespace
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(0, end + 1);

        if (line.empty()) continue;

        // ============================
        // Meta commands (start with .)
        // ============================
        if (line[0] == '.') {
            if (line == ".exit" || line == ".quit") {
                catalog.saveAll();
                std::cout << "Bye!\n";
                break;
            }
            else if (line == ".help") {
                printHelp();
            }
            else if (line == ".tables") {
                auto names = catalog.listTables();
                if (names.empty()) {
                    std::cout << "No tables.\n";
                } else {
                    for (auto& name : names) {
                        std::cout << "  " << name << "\n";
                    }
                }
            }
            else if (line == ".schema") {
                auto names = catalog.listTables();
                if (names.empty()) {
                    std::cout << "No tables.\n";
                } else {
                    for (auto& name : names) {
                        Table* t = catalog.getTable(name);
                        const Schema& s = t->getSchema();
                        std::cout << "CREATE TABLE " << s.table_name << " (";
                        for (size_t i = 0; i < s.columns.size(); i++) {
                            if (i > 0) std::cout << ", ";
                            std::cout << s.columns[i].name << " ";
                            std::cout << (s.columns[i].type == ColumnType::INT ? "INT" : "TEXT");
                        }
                        std::cout << ");\n";
                    }
                }
            }
            else {
                std::cout << "Unknown command: " << line << "\n";
                std::cout << "Type .help for usage information.\n";
            }
            continue;
        }

        // ============================
        // SQL commands
        // ============================
        try {
            // Step 1: Tokenize
            auto tokens = Tokenizer::tokenize(line);

            // Step 2: Parse
            auto stmt = Parser::parse(tokens);

            // Step 3: Execute
            std::string result = Executor::execute(stmt.get(), catalog);

            std::cout << result << "\n";
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }

    return 0;
}
