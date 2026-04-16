# MiniDB

A mini relational database engine built from scratch in C++17. No external dependencies.

MiniDB supports basic SQL commands (`CREATE TABLE`, `INSERT`, `SELECT`, `DELETE`) with a B-Tree index for fast lookups and persistent file-based storage.

## Architecture

```
┌─────────────────────────────────────────┐
│              REPL (main.cpp)            │  Interactive SQL shell
├─────────────────────────────────────────┤
│     SQL Tokenizer → Parser              │  Converts SQL text to AST
├─────────────────────────────────────────┤
│        Executor + Catalog               │  Runs queries, manages tables
├─────────────────────────────────────────┤
│       B-Tree Index + File Storage       │  Organizes data, persists to disk
└─────────────────────────────────────────┘
```

### Components

| Component | Files | Purpose |
|-----------|-------|---------|
| **B-Tree** | `storage/btree.h`, `storage/btree.cpp` | Balanced tree (order 4) for indexed row storage. Supports insert, search, delete, and in-order traversal. |
| **Storage** | `storage/storage.h`, `storage/storage.cpp` | Serializes/deserializes tables to human-readable text files. |
| **Tokenizer** | `sql/tokenizer.h`, `sql/tokenizer.cpp` | Breaks SQL strings into tokens (keywords, identifiers, literals, operators). |
| **Parser** | `sql/parser.h`, `sql/parser.cpp` | Converts token stream into an Abstract Syntax Tree (AST) using recursive descent parsing. |
| **Table** | `core/table.h`, `core/table.cpp` | Wraps a B-Tree with a schema. Handles insert, select, and delete operations. |
| **Catalog** | `core/catalog.h`, `core/catalog.cpp` | Manages all tables. Handles persistence (save/load from data directory). |
| **Executor** | `core/executor.h`, `core/executor.cpp` | Takes a parsed AST and executes it against the catalog. Formats output. |
| **REPL** | `main.cpp` | Read-Eval-Print Loop — the interactive command line interface. |

## Building

Requirements: C++17 compiler (g++ 7+ or clang++ 5+), CMake 3.14+

```bash
# Create build directory
mkdir build && cd build

# Generate build files
cmake ..

# Compile
make

# Run tests
./minidb_tests

# Run the database
./minidb
```

Or compile directly without CMake:

```bash
g++ -std=c++17 -Iinclude src/**/*.cpp src/main.cpp -o minidb
```

## Usage

```
$ ./minidb
MiniDB v1.0.0
A mini relational database engine built from scratch in C++
Data directory: minidb_data/
Type .help for usage information.

minidb> CREATE TABLE students (id INT, name TEXT, age INT);
Table 'students' created.

minidb> INSERT INTO students VALUES (1, 'Sri Ram', 20);
Inserted 1 row.

minidb> INSERT INTO students VALUES (2, 'Arun', 19);
Inserted 1 row.

minidb> INSERT INTO students VALUES (3, 'Priya', 21);
Inserted 1 row.

minidb> SELECT * FROM students;
| id | name    | age |
|----|---------|-----|
| 1  | Sri Ram | 20  |
| 2  | Arun    | 19  |
| 3  | Priya   | 21  |
(3 rows)

minidb> SELECT * FROM students WHERE age > 19;
| id | name    | age |
|----|---------|-----|
| 1  | Sri Ram | 20  |
| 3  | Priya   | 21  |
(2 rows)

minidb> DELETE FROM students WHERE id = 2;
Deleted 1 row.

minidb> .tables
  students

minidb> .exit
Bye!
```

### Supported SQL

```sql
-- Create a table (types: INT, TEXT)
CREATE TABLE <name> (<column> <type>, ...);

-- Insert a row
INSERT INTO <name> VALUES (<value>, ...);

-- Select all rows
SELECT * FROM <name>;

-- Select specific columns with conditions
SELECT <col1>, <col2> FROM <name> WHERE <condition>;

-- Delete rows
DELETE FROM <name> WHERE <condition>;
```

### WHERE Clause

Operators: `=`, `!=`, `<`, `>`, `<=`, `>=`

Logic: `AND`, `OR`

```sql
SELECT * FROM students WHERE age >= 18 AND age <= 25;
SELECT * FROM students WHERE name = 'Sri Ram' OR name = 'Priya';
```

### Meta Commands

| Command | Description |
|---------|-------------|
| `.tables` | List all tables |
| `.schema` | Show CREATE TABLE statements |
| `.help` | Show help |
| `.exit` | Exit MiniDB |

## Data Persistence

Tables are saved as human-readable text files in the data directory (`minidb_data/` by default):

```
TABLE:students
COLUMNS:id:INT,name:TEXT,age:INT
ROW:1|Sri Ram|20
ROW:2|Arun|19
ROW:3|Priya|21
```

Specify a custom data directory: `./minidb my_data_folder`

## How the B-Tree Works

MiniDB uses a B-Tree of order 4 (max 3 keys per node) to index rows by their primary key (first INT column).

```
                    [15]
                   /    \
           [5, 10]      [20, 25, 30]
```

- **Insert**: O(log n) — walks down the tree, splits full nodes along the way
- **Search**: O(log n) — binary-search-like traversal from root to leaf
- **All rows**: O(n) — in-order traversal returns sorted results

This is the same data structure used by real databases like MySQL, PostgreSQL, and SQLite.

## How the SQL Parser Works

The parser uses **recursive descent parsing** — each SQL statement type has its own parsing function:

```
Input: "SELECT * FROM students WHERE age > 18;"

Step 1 — Tokenizer:
  [SELECT] [*] [FROM] [students] [WHERE] [age] [>] [18] [;]

Step 2 — Parser:
  SelectStatement {
    table: "students",
    columns: ["*"],
    where: { column: "age", op: ">", value: 18 }
  }

Step 3 — Executor:
  → Looks up "students" table in catalog
  → Scans B-Tree, filters rows where age > 18
  → Formats and returns result table
```

## Project Structure

```
minidb/
├── CMakeLists.txt
├── include/
│   ├── core/
│   │   ├── types.h          # Value, Row, Schema types
│   │   ├── table.h          # Table class
│   │   ├── catalog.h        # Table manager
│   │   └── executor.h       # Query executor
│   ├── sql/
│   │   ├── tokenizer.h      # SQL tokenizer
│   │   └── parser.h         # SQL parser + AST
│   └── storage/
│       ├── btree.h           # B-Tree index
│       └── storage.h         # File I/O
├── src/
│   ├── main.cpp              # REPL entry point
│   ├── core/
│   │   ├── table.cpp
│   │   ├── catalog.cpp
│   │   └── executor.cpp
│   ├── sql/
│   │   ├── tokenizer.cpp
│   │   └── parser.cpp
│   └── storage/
│       ├── btree.cpp
│       └── storage.cpp
├── tests/
│   ├── test_framework.h      # Minimal test framework
│   ├── test_main.cpp
│   ├── test_btree.cpp
│   ├── test_tokenizer.cpp
│   └── test_integration.cpp
└── examples/
    └── example.sql           # Example queries to try
```

## Tech Stack

- **Language**: C++17
- **Build**: CMake
- **Dependencies**: None (everything built from scratch)
- **Data Structures**: B-Tree (order 4)
- **Parsing**: Recursive descent parser
- **Storage**: Custom text-based file format

## What I Learned

- How databases store and retrieve data using **B-Tree indexing**
- How SQL is **tokenized and parsed** into an Abstract Syntax Tree (just like a compiler)
- How to design a **layered architecture** (storage → query processing → user interface)
- **File I/O** and data serialization in C++
- **C++17 features**: `std::variant`, `std::filesystem`, `std::unique_ptr`, structured bindings

## License

MIT
