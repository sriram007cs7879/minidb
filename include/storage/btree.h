#pragma once

#include "core/types.h"
#include "storage/pager.h"
#include <vector>
#include <utility>
#include <optional>
#include <string>

namespace minidb {

// ============================================================
// DISK-BASED B+ TREE
// ============================================================
//
// This B+ Tree stores all data ON DISK using the Pager.
// Instead of in-memory pointers, nodes are pages in a file.
//
// KEY IDEA: Internal nodes store only keys + child page numbers (small → high fan-out).
//           Leaf nodes store keys + full row data.
//           All actual data lives in leaves.
//
// File layout:
//   Page 0: Metadata (table name, schema, root page, row count)
//   Page 1+: B+ Tree nodes (leaf or internal)
//
// This is the same architecture used by SQLite, MySQL, and PostgreSQL.
//
// ============================================================
// PAGE LAYOUTS (how bytes are organized in each page)
// ============================================================
//
// METADATA PAGE (Page 0):
//   Bytes 0-3:   Magic number (0x4D494E49 = "MINI")
//   Bytes 4-7:   Root page number (0 = empty tree)
//   Bytes 8-11:  Total row count
//   Bytes 12-15: Next free page number
//   Bytes 16-19: Number of columns
//   Bytes 20+:   Column definitions (64 bytes each)
//                  Byte 0: type (0=INT, 1=TEXT)
//                  Bytes 1-63: column name (null-terminated)
//
// LEAF NODE PAGE:
//   Byte 0:      Node type = 1
//   Bytes 1-2:   Number of cells (entries)
//   Bytes 3+:    Cells array
//                  Each cell: [key: 4 bytes] [row data: ROW_SIZE bytes]
//
// INTERNAL NODE PAGE:
//   Byte 0:      Node type = 2
//   Bytes 1-2:   Number of keys
//   Bytes 3-6:   Rightmost child page number
//   Bytes 7+:    Cells array
//                  Each cell: [child page: 4 bytes] [key: 4 bytes]
//
// ROW DATA FORMAT:
//   INT column:  4 bytes (int32_t)
//   TEXT column:  256 bytes (fixed size, null-padded)

// Constants
static constexpr uint32_t META_MAGIC = 0x4D494E49;  // "MINI"

// Metadata page offsets
static constexpr uint32_t META_MAGIC_OFFSET       = 0;
static constexpr uint32_t META_ROOT_PAGE_OFFSET    = 4;
static constexpr uint32_t META_ROW_COUNT_OFFSET    = 8;
static constexpr uint32_t META_NEXT_PAGE_OFFSET    = 12;
static constexpr uint32_t META_NUM_COLS_OFFSET     = 16;
static constexpr uint32_t META_COLUMNS_OFFSET      = 20;
static constexpr uint32_t META_COL_ENTRY_SIZE      = 64;

// Node header offsets
static constexpr uint32_t NODE_TYPE_OFFSET         = 0;   // 1 byte
static constexpr uint32_t NODE_NUM_KEYS_OFFSET     = 1;   // 2 bytes
static constexpr uint32_t NODE_HEADER_SIZE         = 3;

// Internal node
static constexpr uint32_t INTERNAL_RIGHT_CHILD_OFFSET = 3;  // 4 bytes
static constexpr uint32_t INTERNAL_CELLS_OFFSET    = 7;
static constexpr uint32_t INTERNAL_CELL_KEY_OFFSET = 4;  // within each cell
static constexpr uint32_t INTERNAL_CELL_SIZE       = 8;  // 4 (child) + 4 (key)

// Leaf node
static constexpr uint32_t LEAF_CELLS_OFFSET        = 3;

// Column sizes
static constexpr uint32_t INT_COL_SIZE  = 4;
static constexpr uint32_t TEXT_COL_SIZE = 256;

// Node types
static constexpr uint8_t NODE_TYPE_LEAF     = 1;
static constexpr uint8_t NODE_TYPE_INTERNAL = 2;

// Result of an insert that caused a split
struct SplitResult {
    bool did_split;
    int32_t separator_key;   // key to push up to parent
    uint32_t new_page_num;   // the new (right) node's page
};

class BTree {
public:
    // Open or create a B+ Tree backed by a file.
    // If file exists and has data, loads the existing tree.
    // If file is new, initializes with the given schema.
    BTree(const std::string& filename, const Schema& schema);
    ~BTree();

    // Insert a key-row pair
    void insert(int key, const Row& row);

    // Search for a key, returns the row if found
    std::optional<Row> search(int key);

    // Remove a key, returns true if found and removed
    bool remove(int key);

    // Get all key-row pairs in sorted order (in-order traversal)
    std::vector<std::pair<int, Row>> getAll();

    // Number of rows
    int size() const;

    // Flush everything to disk
    void flush();

    // Get the schema
    const Schema& getSchema() const { return schema_; }

    // Get computed stats (for README / debugging)
    uint32_t getRowSize() const { return row_size_; }
    uint32_t getLeafMaxCells() const { return leaf_max_cells_; }
    uint32_t getInternalMaxKeys() const { return internal_max_keys_; }

private:
    Pager pager_;
    Schema schema_;
    uint32_t row_size_;          // bytes per row (depends on schema)
    uint32_t leaf_cell_size_;    // key + row_size
    uint32_t leaf_max_cells_;    // how many cells fit in a leaf page
    uint32_t internal_max_keys_; // how many keys fit in an internal page

    // ---- Metadata (page 0) accessors ----
    void initMetadata();
    void loadMetadata();
    uint32_t getRootPage();
    void setRootPage(uint32_t page);
    uint32_t getRowCount();
    void setRowCount(uint32_t count);

    // ---- Row serialization ----
    void serializeRow(uint8_t* dest, const Row& row);
    Row deserializeRow(const uint8_t* src);

    // ---- Node accessors ----
    uint8_t getNodeType(uint32_t page_num);
    uint16_t getNumKeys(uint32_t page_num);
    void setNumKeys(uint32_t page_num, uint16_t n);

    // Leaf node accessors
    int32_t getLeafKey(uint32_t page_num, uint32_t cell_idx);
    Row getLeafRow(uint32_t page_num, uint32_t cell_idx);
    void setLeafCell(uint32_t page_num, uint32_t cell_idx, int32_t key, const Row& row);

    // Internal node accessors
    int32_t getInternalKey(uint32_t page_num, uint32_t idx);
    uint32_t getInternalChild(uint32_t page_num, uint32_t idx);
    uint32_t getRightChild(uint32_t page_num);
    void setRightChild(uint32_t page_num, uint32_t child);
    void setInternalCell(uint32_t page_num, uint32_t idx, uint32_t child, int32_t key);

    // ---- Core B+ Tree operations ----
    // Insert into subtree rooted at page_num. Returns split info if node split.
    SplitResult insertInto(uint32_t page_num, int key, const Row& row);
    SplitResult insertIntoLeaf(uint32_t page_num, int key, const Row& row);
    SplitResult insertIntoInternal(uint32_t page_num, int key, const Row& row);

    // Search in subtree
    std::optional<Row> searchIn(uint32_t page_num, int key);

    // Collect all entries from subtree
    void collectAll(uint32_t page_num, std::vector<std::pair<int, Row>>& result);

    // Find which child to descend into for a given key (internal nodes)
    uint32_t findChild(uint32_t page_num, int key);

    // Create a new leaf or internal node page
    uint32_t createLeafPage();
    uint32_t createInternalPage();
};

} // namespace minidb
