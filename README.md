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

---

## Detailed Report

### Data Flow

**INSERT INTO students VALUES (1, 'Sri Ram', 20):**
```
User Input
  → Tokenizer: ["INSERT", "INTO", "students", "VALUES", "(", "1", ",", "'Sri Ram'", ",", "20", ")"]
  → Parser: InsertStatement { table: "students", values: [1, "Sri Ram", 20] }
  → Executor: calls table.insert({1, "Sri Ram", 20})
  → Table: extracts key=1, calls btree.insert(1, row)
  → B+ Tree: traverses internal nodes to find correct leaf page
  → Pager: getPage(leaf_page_num), writes row bytes into page
  → Pager: flushPage() writes 4096 bytes to disk
```

**SELECT * FROM students WHERE id = 1:**
```
User Input
  → Tokenizer → Parser: SelectStatement with WHERE condition
  → Executor: builds filter function, calls table.selectWhere(filter)
  → Table: calls btree.getAll(), applies filter
  → B+ Tree: in-order traversal of leaf nodes
  → Pager: loads each node page from cache (or disk)
  → Executor: formats matching rows as a table
  → REPL: prints result
```

### Pager — Implementation Details

The Pager is the lowest layer. It treats a file as an array of fixed-size 4096-byte pages.

```
getPage(2):
  1. Check cache: pages_[2] != nullptr?
     YES → return pages_[2]               (O(1) — cache hit)
     NO  → go to step 2
  2. Allocate 4096 bytes of memory
  3. fseek(file, 2 * 4096, SEEK_SET)      (jump to byte offset 8192)
  4. fread(buffer, 1, 4096, file)          (read 4096 bytes)
  5. Store in pages_[2], return pointer
```

**Why 4096 bytes?**
- Matches the OS page size and disk block size
- Reading less than 4096 bytes from disk still reads a full disk block, so smaller pages waste I/O
- This is the same page size SQLite uses by default

### B+ Tree — Why B+ Tree over B-Tree?

| Property | B-Tree | B+ Tree (our choice) |
|----------|--------|---------------------|
| Data location | All nodes store data | Only leaf nodes store data |
| Internal node size | Large (key + full row) | Small (key + page pointer = 8 bytes) |
| Fan-out | Low (~15) | **High (~511)** |
| Range queries | Must traverse all levels | Scan leaves sequentially |
| Real-world usage | Rare | **SQLite, MySQL InnoDB, PostgreSQL** |

### Row Serialization (Binary Format)

```
Schema: (id INT, name TEXT, age INT)
Row: {1, "Sri Ram", 20}

Serialized bytes:
[01 00 00 00]                              ← id = 1 (4 bytes, little-endian)
[53 72 69 20 52 61 6D 00 00 00 ... (256B)] ← name = "Sri Ram" (256 bytes, null-padded)
[14 00 00 00]                              ← age = 20 (4 bytes)

Total ROW_SIZE = 4 + 256 + 4 = 264 bytes
```

### B+ Tree Capacity Analysis

For schema `(id INT, name TEXT, age INT)` with ROW_SIZE = 264 bytes:

| Metric | Value |
|--------|-------|
| Leaf cell size | 4 + 264 = **268 bytes** |
| Cells per leaf | (4096 - 3) / 268 = **15** |
| Keys per internal node | (4096 - 7) / 8 = **511** |
| Fan-out (children per internal) | **512** |

| Tree Depth | Max Rows | Disk Reads per Search |
|-----------|----------|----------------------|
| 1 (just root leaf) | 15 | 1 |
| 2 (root + leaves) | 511 × 15 = **7,665** | 2 |
| 3 levels | 511² × 15 = **3,918,165** | 3 |
| 4 levels | 511³ × 15 = **~2 billion** | 4 |

### Insert with Page Split

```
Before (leaf is full with 15 cells, inserting key 16):

  Leaf Page 1: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]  ← FULL

After split:

  Leaf Page 1: [1, 2, 3, 4, 5, 6, 7, 8]     ← left half stays
  Leaf Page 3: [9, 10, 11, 12, 13, 14, 15, 16]  ← right half + new key

  Parent gets separator key 9:
  Internal Node: [..., child=Page1, key=9, child=Page3, ...]
```

### SQL Tokenizer Details

Converts raw SQL into tokens via single-pass left-to-right scan:

```
Input:  "SELECT * FROM students WHERE age > 18;"

Output: [SELECT] [STAR] [FROM] [IDENTIFIER:"students"] [WHERE]
        [IDENTIFIER:"age"] [GREATER] [INT_LITERAL:"18"] [SEMICOLON]
```

**23 token types**: Keywords (SELECT, INSERT, FROM, WHERE, CREATE, TABLE, DELETE, VALUES, INTO, AND, OR, NOT), Type keywords (INT_TYPE, TEXT_TYPE), Literals (INT_LITERAL, STRING_LITERAL), IDENTIFIER, Symbols (STAR, COMMA, LPAREN, RPAREN, SEMICOLON, EQUALS, NOT_EQUALS, LESS, GREATER, LESS_EQ, GREATER_EQ), END_OF_INPUT.

### SQL Parser — Recursive Descent

Each grammar rule becomes a function. The parser reads tokens left-to-right:

```
parse()
  ├─ sees SELECT → parseSelect()
  │    ├─ parseColumnList()
  │    ├─ expect(FROM)
  │    ├─ parseTableName()
  │    └─ optional: parseWhere()
  │         ├─ parseCondition()
  │         └─ optional: AND/OR → parseCondition()
  │
  ├─ sees INSERT → parseInsert()
  ├─ sees CREATE → parseCreateTable()
  └─ sees DELETE → parseDelete()
```

**Example AST output:**
```
SQL: "SELECT name, age FROM students WHERE age >= 20 AND name = 'Sri Ram'"

AST:
  SelectStatement {
    table_name: "students"
    columns: ["name", "age"]
    where: WhereClause {
      conditions: [
        { column: "age", op: ">=", value: 20 },
        { column: "name", op: "=", value: "Sri Ram" }
      ]
      logic_ops: ["AND"]
    }
  }
```

### Time Complexity Summary

| Operation | Complexity | Explanation |
|-----------|-----------|-------------|
| **Tokenize** | O(n) | Single pass over SQL string of length n |
| **Parse** | O(t) | Single pass over t tokens |
| **INSERT** | O(log_b n) | B+ Tree traversal + possible split. b=511 |
| **Search by key** | O(log_b n) | B+ Tree traversal. 3 page reads for ~4M rows |
| **SELECT *** | O(n) | Full leaf scan |
| **SELECT WHERE** | O(n × w) | Full scan × w conditions in WHERE |
| **DELETE** | O(n) | Simplified: rebuilds tree |
| **CREATE TABLE** | O(c) | Write c column definitions to metadata page |
| **Page read (cache hit)** | O(1) | Direct array lookup |
| **Page read (cache miss)** | O(1) + disk I/O | fseek + fread of 4096 bytes |

Where: **n** = number of rows, **b** = fan-out (511), **log_b n** with b=511 means log₅₁₁(1,000,000) ≈ **2.2** — effectively constant for practical sizes.

### Space Complexity Summary

| Component | Space | Notes |
|-----------|-------|-------|
| Pager cache | O(P × 4096) bytes | P = pages accessed |
| B+ Tree on disk | O(n / L) pages | L = cells per leaf (15 typical) |
| Token list | O(t) | t = tokens in SQL |
| AST | O(t) | Proportional to tokens |
| Result set | O(r × c) | r = result rows, c = columns |

### Comparison with Real Databases

| Feature | MiniDB | SQLite | MySQL InnoDB |
|---------|--------|--------|-------------|
| Index structure | B+ Tree | B+ Tree | B+ Tree |
| Page size | 4KB | 4KB (default) | 16KB |
| Storage | Single file per table | Single file for all | Tablespace files |
| SQL support | 4 statements | Full SQL | Full SQL |
| Transactions | No | Yes (WAL/journal) | Yes (redo/undo logs) |
| Concurrency | Single-threaded | File locking | MVCC |
| Query optimizer | None (full scan) | Cost-based | Cost-based |
| Secondary indexes | No | Yes | Yes |

### Testing

16 tests across 3 categories:

| Category | Tests | What They Verify |
|----------|-------|-----------------|
| B+ Tree | 7 | Insert, search, delete, sorted traversal, page splits, data persistence across sessions |
| Tokenizer | 5 | All token types, operators, case-insensitive keywords |
| Integration | 4 | Full SQL pipeline: tokenize → parse → execute → verify results |

### Limitations and Possible Extensions

**Current limitations:** No UPDATE, no secondary indexes, no JOIN, no transactions, no concurrency, fixed TEXT size (256 bytes), simplified DELETE (rebuilds tree).

| Possible Extension | Difficulty | What It Adds |
|-----------|-----------|-------------|
| `UPDATE` statement | Easy | Modify existing rows |
| `ORDER BY` / `LIMIT` | Easy | Sort results, pagination |
| `COUNT`, `SUM`, `AVG` | Medium | Aggregate functions |
| Secondary indexes | Medium | Fast lookups on non-key columns |
| `JOIN` (nested loop) | Medium | Multi-table queries |
| Write-Ahead Log (WAL) | Hard | Crash recovery |
| Proper B+ Tree deletion | Hard | O(log n) delete with node merging |
| Concurrency | Hard | Multi-threaded access |

## License

MIT
