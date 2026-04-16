#include "storage/btree.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace minidb {

// ============================================================
// CONSTRUCTOR — Open/create a B+ Tree file
// ============================================================
BTree::BTree(const std::string& filename, const Schema& schema)
    : pager_(filename), schema_(schema) {

    // Calculate row size based on schema
    //   INT = 4 bytes, TEXT = 256 bytes
    row_size_ = 0;
    for (auto& col : schema_.columns) {
        row_size_ += (col.type == ColumnType::INT) ? INT_COL_SIZE : TEXT_COL_SIZE;
    }

    // Calculate how many entries fit per page
    leaf_cell_size_ = 4 + row_size_;  // key (4 bytes) + row data
    leaf_max_cells_ = (PAGE_SIZE - NODE_HEADER_SIZE) / leaf_cell_size_;
    internal_max_keys_ = (PAGE_SIZE - INTERNAL_CELLS_OFFSET) / INTERNAL_CELL_SIZE;

    // Check if file already has data
    if (pager_.getNumPages() == 0) {
        // New file — initialize metadata page
        initMetadata();
    } else {
        // Existing file — load schema from metadata
        loadMetadata();
    }
}

BTree::~BTree() {
    flush();
}

// ============================================================
// METADATA — Page 0 stores table info
// ============================================================
//
// Layout:
//   [MAGIC 4B] [ROOT_PAGE 4B] [ROW_COUNT 4B] [NEXT_PAGE 4B]
//   [NUM_COLS 4B] [COL_0: type(1B) + name(63B)] [COL_1: ...] ...

void BTree::initMetadata() {
    uint8_t* page = pager_.getPage(0);

    writeU32(page, META_MAGIC_OFFSET, META_MAGIC);
    writeU32(page, META_ROOT_PAGE_OFFSET, 0);       // 0 = empty tree
    writeU32(page, META_ROW_COUNT_OFFSET, 0);
    writeU32(page, META_NEXT_PAGE_OFFSET, 1);        // page 1 is the first available
    writeU32(page, META_NUM_COLS_OFFSET, schema_.columns.size());

    // Write column definitions
    for (size_t i = 0; i < schema_.columns.size(); i++) {
        uint32_t offset = META_COLUMNS_OFFSET + i * META_COL_ENTRY_SIZE;
        page[offset] = (schema_.columns[i].type == ColumnType::INT) ? 0 : 1;
        // Copy column name (max 62 chars + null terminator)
        strncpy((char*)(page + offset + 1),
                schema_.columns[i].name.c_str(), META_COL_ENTRY_SIZE - 2);
    }

    pager_.flushPage(0);
}

void BTree::loadMetadata() {
    uint8_t* page = pager_.getPage(0);

    uint32_t magic = readU32(page, META_MAGIC_OFFSET);
    if (magic != META_MAGIC) {
        throw std::runtime_error("Invalid database file (bad magic number)");
    }

    // Read schema from metadata
    uint32_t num_cols = readU32(page, META_NUM_COLS_OFFSET);
    schema_.columns.clear();
    for (uint32_t i = 0; i < num_cols; i++) {
        uint32_t offset = META_COLUMNS_OFFSET + i * META_COL_ENTRY_SIZE;
        Column col;
        col.type = (page[offset] == 0) ? ColumnType::INT : ColumnType::TEXT;
        col.name = std::string((char*)(page + offset + 1));
        schema_.columns.push_back(col);
    }

    // Recalculate sizes
    row_size_ = 0;
    for (auto& col : schema_.columns) {
        row_size_ += (col.type == ColumnType::INT) ? INT_COL_SIZE : TEXT_COL_SIZE;
    }
    leaf_cell_size_ = 4 + row_size_;
    leaf_max_cells_ = (PAGE_SIZE - NODE_HEADER_SIZE) / leaf_cell_size_;
    internal_max_keys_ = (PAGE_SIZE - INTERNAL_CELLS_OFFSET) / INTERNAL_CELL_SIZE;
}

uint32_t BTree::getRootPage() {
    return readU32(pager_.getPage(0), META_ROOT_PAGE_OFFSET);
}

void BTree::setRootPage(uint32_t page) {
    writeU32(pager_.getPage(0), META_ROOT_PAGE_OFFSET, page);
    pager_.flushPage(0);
}

uint32_t BTree::getRowCount() {
    return readU32(pager_.getPage(0), META_ROW_COUNT_OFFSET);
}

void BTree::setRowCount(uint32_t count) {
    writeU32(pager_.getPage(0), META_ROW_COUNT_OFFSET, count);
}

// ============================================================
// ROW SERIALIZATION — Convert Row <-> bytes on a page
// ============================================================
// INT:  4 bytes (int32_t, raw bytes)
// TEXT: 256 bytes (fixed size, null-padded)
//
// Example: Row {1, "Sri Ram", 20} with schema (INT, TEXT, INT)
//   → [01 00 00 00] [S r i   R a m \0 \0 \0 ... (256 bytes)] [14 00 00 00]

void BTree::serializeRow(uint8_t* dest, const Row& row) {
    uint32_t offset = 0;
    for (size_t i = 0; i < schema_.columns.size(); i++) {
        if (schema_.columns[i].type == ColumnType::INT) {
            int32_t val = std::get<int>(row[i]);
            writeI32(dest, offset, val);
            offset += INT_COL_SIZE;
        } else {
            const std::string& str = std::get<std::string>(row[i]);
            memset(dest + offset, 0, TEXT_COL_SIZE);
            size_t copy_len = std::min(str.size(), (size_t)(TEXT_COL_SIZE - 1));
            memcpy(dest + offset, str.c_str(), copy_len);
            offset += TEXT_COL_SIZE;
        }
    }
}

Row BTree::deserializeRow(const uint8_t* src) {
    Row row;
    uint32_t offset = 0;
    for (auto& col : schema_.columns) {
        if (col.type == ColumnType::INT) {
            row.push_back((int)readI32(src, offset));
            offset += INT_COL_SIZE;
        } else {
            row.push_back(std::string((const char*)(src + offset)));
            offset += TEXT_COL_SIZE;
        }
    }
    return row;
}

// ============================================================
// NODE ACCESSORS — Read/write fields on node pages
// ============================================================

uint8_t BTree::getNodeType(uint32_t page_num) {
    return pager_.getPage(page_num)[NODE_TYPE_OFFSET];
}

uint16_t BTree::getNumKeys(uint32_t page_num) {
    return readU16(pager_.getPage(page_num), NODE_NUM_KEYS_OFFSET);
}

void BTree::setNumKeys(uint32_t page_num, uint16_t n) {
    writeU16(pager_.getPage(page_num), NODE_NUM_KEYS_OFFSET, n);
}

// ---- Leaf node ----
// Cell layout: [key: 4 bytes] [row: ROW_SIZE bytes]
// Cell i starts at: NODE_HEADER_SIZE + i * leaf_cell_size_

int32_t BTree::getLeafKey(uint32_t page_num, uint32_t cell_idx) {
    uint32_t offset = LEAF_CELLS_OFFSET + cell_idx * leaf_cell_size_;
    return readI32(pager_.getPage(page_num), offset);
}

Row BTree::getLeafRow(uint32_t page_num, uint32_t cell_idx) {
    uint32_t offset = LEAF_CELLS_OFFSET + cell_idx * leaf_cell_size_ + 4;
    return deserializeRow(pager_.getPage(page_num) + offset);
}

void BTree::setLeafCell(uint32_t page_num, uint32_t cell_idx, int32_t key, const Row& row) {
    uint8_t* page = pager_.getPage(page_num);
    uint32_t offset = LEAF_CELLS_OFFSET + cell_idx * leaf_cell_size_;
    writeI32(page, offset, key);
    serializeRow(page + offset + 4, row);
}

// ---- Internal node ----
// Layout: [type 1B] [num_keys 2B] [right_child 4B] [cells...]
// Cell layout: [child_page: 4 bytes] [key: 4 bytes]
// Cell i starts at: INTERNAL_CELLS_OFFSET + i * 8

int32_t BTree::getInternalKey(uint32_t page_num, uint32_t idx) {
    uint32_t offset = INTERNAL_CELLS_OFFSET + idx * INTERNAL_CELL_SIZE + 4;
    return readI32(pager_.getPage(page_num), offset);
}

uint32_t BTree::getInternalChild(uint32_t page_num, uint32_t idx) {
    uint32_t offset = INTERNAL_CELLS_OFFSET + idx * INTERNAL_CELL_SIZE;
    return readU32(pager_.getPage(page_num), offset);
}

uint32_t BTree::getRightChild(uint32_t page_num) {
    return readU32(pager_.getPage(page_num), INTERNAL_RIGHT_CHILD_OFFSET);
}

void BTree::setRightChild(uint32_t page_num, uint32_t child) {
    writeU32(pager_.getPage(page_num), INTERNAL_RIGHT_CHILD_OFFSET, child);
}

void BTree::setInternalCell(uint32_t page_num, uint32_t idx, uint32_t child, int32_t key) {
    uint8_t* page = pager_.getPage(page_num);
    uint32_t offset = INTERNAL_CELLS_OFFSET + idx * INTERNAL_CELL_SIZE;
    writeU32(page, offset, child);
    writeI32(page, offset + 4, key);
}

// ============================================================
// CREATE NEW PAGES
// ============================================================

uint32_t BTree::createLeafPage() {
    uint32_t page_num = pager_.allocatePage();
    uint8_t* page = pager_.getPage(page_num);
    page[NODE_TYPE_OFFSET] = NODE_TYPE_LEAF;
    writeU16(page, NODE_NUM_KEYS_OFFSET, 0);
    return page_num;
}

uint32_t BTree::createInternalPage() {
    uint32_t page_num = pager_.allocatePage();
    uint8_t* page = pager_.getPage(page_num);
    page[NODE_TYPE_OFFSET] = NODE_TYPE_INTERNAL;
    writeU16(page, NODE_NUM_KEYS_OFFSET, 0);
    writeU32(page, INTERNAL_RIGHT_CHILD_OFFSET, 0);
    return page_num;
}

// ============================================================
// FIND CHILD — Which child to descend into for a search key
// ============================================================
//
// Internal node: [child0 | key0 | child1 | key1 | child2 | ... | right_child]
//
// For search key K:
//   If K < key0  → go to child0
//   If K < key1  → go to child1
//   ...
//   If K >= last key → go to right_child

uint32_t BTree::findChild(uint32_t page_num, int key) {
    uint16_t num_keys = getNumKeys(page_num);
    for (uint32_t i = 0; i < num_keys; i++) {
        if (key < getInternalKey(page_num, i)) {
            return getInternalChild(page_num, i);
        }
    }
    return getRightChild(page_num);
}

// ============================================================
// SEARCH
// ============================================================
// Start at root. If leaf, scan cells. If internal, descend to correct child.
// Time: O(log n) — at each level we pick one child out of hundreds.

std::optional<Row> BTree::search(int key) {
    uint32_t root = getRootPage();
    if (root == 0) return std::nullopt;  // empty tree
    return searchIn(root, key);
}

std::optional<Row> BTree::searchIn(uint32_t page_num, int key) {
    if (getNodeType(page_num) == NODE_TYPE_LEAF) {
        // Leaf node — scan cells for the key
        uint16_t num_cells = getNumKeys(page_num);
        for (uint32_t i = 0; i < num_cells; i++) {
            if (getLeafKey(page_num, i) == key) {
                return getLeafRow(page_num, i);
            }
        }
        return std::nullopt;  // not found
    } else {
        // Internal node — find the right child and recurse
        uint32_t child = findChild(page_num, key);
        return searchIn(child, key);
    }
}

// ============================================================
// INSERT
// ============================================================
// 1. If tree is empty, create a leaf root and insert there.
// 2. Otherwise, insert into the root recursively.
// 3. If the root splits, create a new root pointing to old and new nodes.

void BTree::insert(int key, const Row& row) {
    // Check for duplicate key
    if (search(key).has_value()) {
        throw std::runtime_error("Duplicate key: " + std::to_string(key));
    }

    uint32_t root = getRootPage();

    if (root == 0) {
        // Empty tree — create first leaf
        uint32_t leaf_page = createLeafPage();
        setLeafCell(leaf_page, 0, key, row);
        setNumKeys(leaf_page, 1);
        setRootPage(leaf_page);
        pager_.flushPage(leaf_page);
    } else {
        // Insert into existing tree
        SplitResult result = insertInto(root, key, row);

        if (result.did_split) {
            // Root split! Create a new root.
            //   new_root → [old_root | separator_key | new_page]
            uint32_t new_root = createInternalPage();
            setInternalCell(new_root, 0, root, result.separator_key);
            setRightChild(new_root, result.new_page_num);
            setNumKeys(new_root, 1);
            setRootPage(new_root);
            pager_.flushPage(new_root);
        }
    }

    setRowCount(getRowCount() + 1);
    pager_.flushPage(0);  // flush metadata
}

// Insert into subtree rooted at page_num. Returns split info.
SplitResult BTree::insertInto(uint32_t page_num, int key, const Row& row) {
    if (getNodeType(page_num) == NODE_TYPE_LEAF) {
        return insertIntoLeaf(page_num, key, row);
    } else {
        return insertIntoInternal(page_num, key, row);
    }
}

// ============================================================
// INSERT INTO LEAF
// ============================================================
// If leaf has room: insert cell in sorted position.
// If leaf is full: split into two leaves, return separator key.

SplitResult BTree::insertIntoLeaf(uint32_t page_num, int key, const Row& row) {
    uint16_t num_cells = getNumKeys(page_num);

    if (num_cells < leaf_max_cells_) {
        // Room available — find the right position (keep sorted)
        uint32_t insert_idx = num_cells;
        for (uint32_t i = 0; i < num_cells; i++) {
            if (key < getLeafKey(page_num, i)) {
                insert_idx = i;
                break;
            }
        }

        // Shift cells right to make room
        // We work backwards to avoid overwriting data
        uint8_t* page = pager_.getPage(page_num);
        for (uint32_t i = num_cells; i > insert_idx; i--) {
            uint32_t src = LEAF_CELLS_OFFSET + (i - 1) * leaf_cell_size_;
            uint32_t dst = LEAF_CELLS_OFFSET + i * leaf_cell_size_;
            memmove(page + dst, page + src, leaf_cell_size_);
        }

        // Write the new cell
        setLeafCell(page_num, insert_idx, key, row);
        setNumKeys(page_num, num_cells + 1);
        pager_.flushPage(page_num);

        return {false, 0, 0};  // no split needed
    }

    // ---- LEAF IS FULL — SPLIT ----
    // Strategy:
    //   1. Create a new (right) leaf
    //   2. Distribute cells: left half stays, right half moves to new leaf
    //   3. Insert the new cell into the correct half
    //   4. Return the first key of the right leaf as the separator

    // Collect all existing cells + the new one
    struct Cell { int32_t key; Row row; };
    std::vector<Cell> all_cells;
    all_cells.reserve(num_cells + 1);

    bool inserted = false;
    for (uint32_t i = 0; i < num_cells; i++) {
        int32_t cell_key = getLeafKey(page_num, i);
        if (!inserted && key < cell_key) {
            all_cells.push_back({key, row});
            inserted = true;
        }
        all_cells.push_back({cell_key, getLeafRow(page_num, i)});
    }
    if (!inserted) {
        all_cells.push_back({key, row});
    }

    // Split point: left gets first half, right gets the rest
    uint32_t split = (all_cells.size() + 1) / 2;

    // Rewrite left leaf (this page)
    uint8_t* page = pager_.getPage(page_num);
    memset(page + NODE_HEADER_SIZE, 0, PAGE_SIZE - NODE_HEADER_SIZE);
    for (uint32_t i = 0; i < split; i++) {
        setLeafCell(page_num, i, all_cells[i].key, all_cells[i].row);
    }
    setNumKeys(page_num, split);
    pager_.flushPage(page_num);

    // Create right leaf
    uint32_t right_page = createLeafPage();
    uint32_t right_count = all_cells.size() - split;
    for (uint32_t i = 0; i < right_count; i++) {
        setLeafCell(right_page, i, all_cells[split + i].key, all_cells[split + i].row);
    }
    setNumKeys(right_page, right_count);
    pager_.flushPage(right_page);

    // Separator = first key of right leaf
    int32_t separator = all_cells[split].key;

    return {true, separator, right_page};
}

// ============================================================
// INSERT INTO INTERNAL NODE
// ============================================================
// 1. Find the correct child.
// 2. Insert into that child recursively.
// 3. If the child split, insert the separator key into this node.
// 4. If this node is now full, split it too.

SplitResult BTree::insertIntoInternal(uint32_t page_num, int key, const Row& row) {
    // Find which child to descend into
    uint32_t child_page = findChild(page_num, key);

    // Recursively insert into child
    SplitResult child_result = insertInto(child_page, key, row);

    if (!child_result.did_split) {
        return {false, 0, 0};  // child didn't split, nothing to do here
    }

    // Child split — we need to insert separator_key + new_page into this node
    uint16_t num_keys = getNumKeys(page_num);

    if (num_keys < internal_max_keys_) {
        // Room available — insert in sorted position
        uint32_t insert_idx = num_keys;
        for (uint32_t i = 0; i < num_keys; i++) {
            if (child_result.separator_key < getInternalKey(page_num, i)) {
                insert_idx = i;
                break;
            }
        }

        // Shift cells right
        uint8_t* page = pager_.getPage(page_num);
        for (uint32_t i = num_keys; i > insert_idx; i--) {
            uint32_t src = INTERNAL_CELLS_OFFSET + (i - 1) * INTERNAL_CELL_SIZE;
            uint32_t dst = INTERNAL_CELLS_OFFSET + i * INTERNAL_CELL_SIZE;
            memmove(page + dst, page + src, INTERNAL_CELL_SIZE);
        }

        // The child that split was either getInternalChild(insert_idx) or right_child.
        // After the split:
        //   - The old child (child_page) keeps its position
        //   - The new child (new_page_num) goes to the right of the separator

        // If child_page was the right_child, the new separator's child is child_page
        // and right_child becomes new_page_num.
        if (child_page == getRightChild(page_num)) {
            setInternalCell(page_num, insert_idx, child_page, child_result.separator_key);
            setRightChild(page_num, child_result.new_page_num);
        } else {
            setInternalCell(page_num, insert_idx, child_page, child_result.separator_key);
            // The child at insert_idx+1 should be the new page
            // But insert_idx+1's child is either already there or needs adjustment
            // Actually: old child stays at its position (which is insert_idx now),
            // and new_page goes into the position previously occupied by old child's next
            // We need to update: cells[insert_idx].child = child_page (old, left side of split)
            // and the pointer that was previously pointing to child_page should now point
            // to child_result.new_page_num

            // Simpler approach: the new page goes where the old pointer was
            // Since we found child_page at some position, that position's child is still child_page
            // The new child goes right after the separator
            // After shifting, cells[insert_idx+1].child should become the new page IF it was
            // pointing to child_page before. But actually after the shift, cells[insert_idx+1]
            // is the old cells[insert_idx] which had child = some other page.

            // Let me think about this differently:
            // Before insert: ... | child_i (=child_page) | key_i | child_{i+1} | ...
            // After child split: child_page has left half, new_page has right half
            // We need: ... | child_page | separator | new_page | key_i | child_{i+1} | ...
            // So the new cell's child is child_page, and we need new_page to replace
            // what was previously child_page's "next" pointer

            // Find which cell had child_page as its child pointer
            // and put new_page_num in its place, while the separator cell gets child_page
            for (uint32_t j = insert_idx + 1; j <= num_keys; j++) {
                if (j < num_keys + 1 && getInternalChild(page_num, j) == child_page) {
                    // This shouldn't happen after shift, but just in case
                    break;
                }
            }
            // After shifting right and inserting at insert_idx:
            // cells[insert_idx] = {child_page, separator}
            // cells[insert_idx+1].child should be new_page_num
            // But cells[insert_idx+1].child currently is whatever was shifted there
            // Actually no — the child at insert_idx+1 was the one that came after child_page
            // We need to update it to be new_page_num

            // Correct approach: After inserting the separator key, the child to its right
            // is the new page. Since cells store (left_child, key), the right child of
            // cells[insert_idx] is cells[insert_idx+1].child. We need that to be new_page_num.
            if (insert_idx + 1 <= num_keys) {
                // There's a cell to the right — update its child pointer
                uint32_t next_offset = INTERNAL_CELLS_OFFSET + (insert_idx + 1) * INTERNAL_CELL_SIZE;
                writeU32(pager_.getPage(page_num), next_offset, child_result.new_page_num);
            }
        }

        setNumKeys(page_num, num_keys + 1);
        pager_.flushPage(page_num);
        return {false, 0, 0};
    }

    // ---- INTERNAL NODE IS FULL — SPLIT ----
    // Collect all keys/children + the new separator, split into two internal nodes

    struct ICell { uint32_t child; int32_t key; };
    std::vector<ICell> all_cells;
    all_cells.reserve(num_keys + 1);

    for (uint32_t i = 0; i < num_keys; i++) {
        all_cells.push_back({getInternalChild(page_num, i), getInternalKey(page_num, i)});
    }
    uint32_t old_right_child = getRightChild(page_num);

    // Insert the new separator in sorted position
    bool inserted = false;
    std::vector<ICell> merged;
    merged.reserve(all_cells.size() + 1);
    for (auto& cell : all_cells) {
        if (!inserted && child_result.separator_key < cell.key) {
            merged.push_back({child_page, child_result.separator_key});
            // Adjust: the cell after this separator should point to new_page
            ICell adjusted_cell = cell;
            adjusted_cell.child = child_result.new_page_num;
            merged.push_back(adjusted_cell);
            inserted = true;
        } else {
            merged.push_back(cell);
        }
    }
    if (!inserted) {
        merged.push_back({child_page, child_result.separator_key});
        old_right_child = child_result.new_page_num;
    }

    // Split: left half stays, middle key goes up, right half to new node
    uint32_t mid = merged.size() / 2;

    // Rewrite this node with left half
    uint8_t* page = pager_.getPage(page_num);
    memset(page + NODE_HEADER_SIZE, 0, PAGE_SIZE - NODE_HEADER_SIZE);
    page[NODE_TYPE_OFFSET] = NODE_TYPE_INTERNAL;
    for (uint32_t i = 0; i < mid; i++) {
        setInternalCell(page_num, i, merged[i].child, merged[i].key);
    }
    // Right child of left node = the left child of the middle key's right side
    if (mid + 1 < merged.size()) {
        setRightChild(page_num, merged[mid + 1].child);
    } else {
        setRightChild(page_num, old_right_child);
    }
    setNumKeys(page_num, mid);
    pager_.flushPage(page_num);

    // Create right node
    uint32_t right_page = createInternalPage();
    uint32_t right_start = mid + 1;
    for (uint32_t i = right_start; i < merged.size(); i++) {
        setInternalCell(right_page, i - right_start, merged[i].child, merged[i].key);
    }
    setRightChild(right_page, old_right_child);
    setNumKeys(right_page, merged.size() - right_start);
    pager_.flushPage(right_page);

    // The middle key goes up to parent
    return {true, merged[mid].key, right_page};
}

// ============================================================
// REMOVE — Simplified: rebuild tree without the deleted key
// ============================================================
bool BTree::remove(int key) {
    if (!search(key).has_value()) return false;

    auto all = getAll();
    std::vector<std::pair<int, Row>> remaining;
    for (auto& [k, v] : all) {
        if (k != key) remaining.push_back({k, v});
    }

    // Reset tree: clear metadata, start fresh
    uint8_t* meta = pager_.getPage(0);
    writeU32(meta, META_ROOT_PAGE_OFFSET, 0);
    writeU32(meta, META_ROW_COUNT_OFFSET, 0);
    writeU32(meta, META_NEXT_PAGE_OFFSET, 1);
    pager_.flushPage(0);

    // Re-insert all remaining rows
    for (auto& [k, v] : remaining) {
        uint32_t root = getRootPage();
        if (root == 0) {
            uint32_t leaf_page = createLeafPage();
            setLeafCell(leaf_page, 0, k, v);
            setNumKeys(leaf_page, 1);
            setRootPage(leaf_page);
            pager_.flushPage(leaf_page);
        } else {
            SplitResult result = insertInto(root, k, v);
            if (result.did_split) {
                uint32_t new_root = createInternalPage();
                setInternalCell(new_root, 0, root, result.separator_key);
                setRightChild(new_root, result.new_page_num);
                setNumKeys(new_root, 1);
                setRootPage(new_root);
                pager_.flushPage(new_root);
            }
        }
        setRowCount(getRowCount() + 1);
    }
    pager_.flushPage(0);

    return true;
}

// ============================================================
// GET ALL — In-order traversal of the B+ Tree
// ============================================================
std::vector<std::pair<int, Row>> BTree::getAll() {
    std::vector<std::pair<int, Row>> result;
    uint32_t root = getRootPage();
    if (root != 0) {
        collectAll(root, result);
    }
    return result;
}

void BTree::collectAll(uint32_t page_num, std::vector<std::pair<int, Row>>& result) {
    if (getNodeType(page_num) == NODE_TYPE_LEAF) {
        uint16_t num_cells = getNumKeys(page_num);
        for (uint32_t i = 0; i < num_cells; i++) {
            result.push_back({getLeafKey(page_num, i), getLeafRow(page_num, i)});
        }
    } else {
        uint16_t num_keys = getNumKeys(page_num);
        for (uint32_t i = 0; i < num_keys; i++) {
            collectAll(getInternalChild(page_num, i), result);
        }
        if (num_keys > 0) {
            collectAll(getRightChild(page_num), result);
        }
    }
}

// ============================================================
// SIZE / FLUSH
// ============================================================
int BTree::size() const {
    return (int)readU32(const_cast<Pager&>(pager_).getPage(0), META_ROW_COUNT_OFFSET);
}

void BTree::flush() {
    pager_.flushAll();
}

} // namespace minidb
