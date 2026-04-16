# MiniDB

A mini relational database engine built from scratch in C++17. No external dependencies.

MiniDB supports basic SQL commands (`CREATE TABLE`, `INSERT`, `SELECT`, `DELETE`) with a **disk-based B+ Tree index** backed by a **page-based storage engine (Pager)** — the same architecture used by SQLite, MySQL, and PostgreSQL.

## Architecture

```
┌─────────────────────────────────────────┐
│              REPL (main.cpp)            │  Interactive SQL shell
├─────────────────────────────────────────┤
│     SQL Tokenizer → Parser              │  Converts SQL text to AST
├─────────────────────────────────────────┤
│        Executor + Catalog               │  Runs queries, manages tables
├─────────────────────────────────────────┤
│          B+ Tree Index                  │  Organizes rows for fast lookup
├─────────────────────────────────────────┤
│         Pager (Disk I/O)                │  Reads/writes 4KB pages to file
└─────────────────────────────────────────┘
```

### How Data is Stored on Disk

Each table is a single `.db` file divided into **4096-byte pages**:

```
students.db
┌──────────────┬──────────────┬──────────────┬──────────────┐
│    Page 0    │    Page 1    │    Page 2    │    Page 3    │
│  (Metadata)  │  (B+ Tree   │  (B+ Tree   │  (B+ Tree   │
│  Schema info │   leaf)     │   leaf)     │  internal)  │
│  Root page   │  Rows 1-15  │  Rows 16-30 │   Keys      │
└──────────────┴──────────────┴──────────────┴──────────────┘
```

- **Page 0** stores table metadata (name, column definitions, root page pointer)
- **Leaf pages** store actual row data (key + serialized row bytes)
- **Internal pages** store only keys + child page pointers (for navigation)

The Pager caches pages in memory — repeated reads hit the cache instead of disk.

### Components

| Component | Files | Purpose |
|-----------|-------|---------|
| **Pager** | `storage/pager.h`, `storage/pager.cpp` | Reads/writes the file in 4KB page chunks. Caches pages in memory. |
| **B+ Tree** | `storage/btree.h`, `storage/btree.cpp` | Disk-based B+ Tree. Leaf nodes store rows. Internal nodes store keys for navigation. Handles page splits on insert. |
| **Tokenizer** | `sql/tokenizer.h`, `sql/tokenizer.cpp` | Breaks SQL strings into tokens (keywords, identifiers, literals, operators). |
| **Parser** | `sql/parser.h`, `sql/parser.cpp` | Converts token stream into an AST using recursive descent parsing. |
| **Table** | `core/table.h`, `core/table.cpp` | Wraps a B+ Tree. Provides insert/select/delete operations. |
| **Catalog** | `core/catalog.h`, `core/catalog.cpp` | Manages all tables. Discovers existing `.db` files on startup. |
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

## How the Pager Works

The Pager manages a file as a collection of fixed-size **4096-byte pages** (same size as the OS disk block for efficient I/O):

```
File: students.db
                    Page 0           Page 1           Page 2
                ┌────────────┐  ┌────────────┐  ┌────────────┐
File offset 0 → │ 4096 bytes │  │ 4096 bytes │  │ 4096 bytes │ ...
                └────────────┘  └────────────┘  └────────────┘

pager.getPage(1) → reads bytes 4096-8191 from file, caches in RAM
pager.getPage(1) → returns from cache (no disk read!)
pager.flushPage(1) → writes cached page back to file
```

This is the same principle used by the OS for virtual memory pages — but applied to database file I/O.

## How the B+ Tree Works

MiniDB uses a **B+ Tree** — the industry-standard index structure for databases.

Key properties:
- **Leaf nodes** store actual data (key + row bytes)
- **Internal nodes** store only keys + child pointers (very compact → high fan-out)
- All leaves are at the same depth (always balanced)

```
                     ┌─────────────────┐
                     │  Internal Node  │
                     │   keys: [15]    │
                     │  (just 8 bytes  │
                     │   per entry)    │
                     └───────┬─────────┘
                      ╱              ╲
          ┌──────────────┐    ┌──────────────┐
          │  Leaf Node   │    │  Leaf Node   │
          │ rows 1-15    │    │ rows 16-30   │
          │ (full row    │    │ (full row    │
          │  data here)  │    │  data here)  │
          └──────────────┘    └──────────────┘
```

### Performance

For a table with schema `(id INT, name TEXT, age INT)`:
- Row size: 4 + 256 + 4 = **264 bytes**
- Entries per leaf page: **(4096 - 3) / 268 = 15 rows**
- Keys per internal page: **(4096 - 7) / 8 = 511 keys**

| Tree Depth | Max Rows | Disk Reads per Search |
|-----------|----------|----------------------|
| 1 (just root leaf) | 15 | 1 |
| 2 (root + leaves) | 511 × 15 = **7,665** | 2 |
| 3 levels | 511 × 511 × 15 = **3.9 million** | 3 |

**3 page reads to find any row among 3.9 million** — that's the power of B+ Trees with high fan-out.

## How the SQL Parser Works

The parser uses **recursive descent parsing** — each SQL statement type has its own parsing function:

```
Input: "SELECT * FROM students WHERE age > 18;"

Step 1 — Tokenizer:
  [SELECT] [*] [FROM] [students] [WHERE] [age] [>] [18] [;]

Step 2 — Parser (recursive descent):
  SelectStatement {
    table: "students",
    columns: ["*"],
    where: { column: "age", op: ">", value: 18 }
  }

Step 3 — Executor:
  → Looks up "students" in catalog
  → Traverses B+ Tree, filters rows where age > 18
  → Formats and returns result table
```

## On-Disk File Format

Each `.db` file uses a binary format with pages:

**Page 0 (Metadata):**
```
Bytes 0-3:   Magic number (0x4D494E49 = "MINI")
Bytes 4-7:   Root page number
Bytes 8-11:  Row count
Bytes 12-15: Next free page
Bytes 16-19: Number of columns
Bytes 20+:   Column definitions (64 bytes each)
```

**Leaf Node Pages:**
```
Byte 0:    Type = 1 (leaf)
Bytes 1-2: Number of cells
Bytes 3+:  Cells [key: 4 bytes | row data: ROW_SIZE bytes]
```

**Internal Node Pages:**
```
Byte 0:    Type = 2 (internal)
Bytes 1-2: Number of keys
Bytes 3-6: Rightmost child page
Bytes 7+:  Cells [child page: 4 bytes | key: 4 bytes]
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
│   │   └── parser.h         # SQL parser + AST definitions
│   └── storage/
│       ├── pager.h           # Page-based disk I/O
│       └── btree.h           # Disk-based B+ Tree
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
│       ├── pager.cpp
│       └── btree.cpp
├── tests/
│   ├── test_framework.h      # Minimal test framework
│   ├── test_main.cpp
│   ├── test_btree.cpp
│   ├── test_tokenizer.cpp
│   └── test_integration.cpp
└── examples/
    └── example.sql
```

## Tech Stack

- **Language**: C++17
- **Build**: CMake
- **Dependencies**: None (everything from scratch)
- **Storage**: Pager (4KB page-based disk I/O with in-memory cache)
- **Index**: Disk-based B+ Tree (same as SQLite/MySQL/PostgreSQL)
- **Parsing**: Recursive descent SQL parser
- **File Format**: Custom binary format with metadata + B+ Tree node pages

## What I Learned

- How databases use **page-based I/O** to efficiently read/write data to disk
- How **B+ Trees** provide O(log n) lookups with high fan-out (511 children per internal node)
- How SQL is **tokenized and parsed** into an Abstract Syntax Tree using recursive descent
- How to design a **layered architecture** (pager → B+ tree → query engine → REPL)
- **Binary serialization**: packing rows into fixed-size byte layouts on disk pages
- **C++17 features**: `std::variant`, `std::optional`, `std::filesystem`, `std::unique_ptr`

## License

MIT
