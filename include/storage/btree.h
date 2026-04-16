#pragma once

#include "core/types.h"
#include <vector>
#include <utility>

namespace minidb {

// B-Tree of order 4: each node holds up to 3 keys and 4 children.
// Keys are integers (row IDs), values are Rows.
// The tree is always balanced — all leaves are at the same depth.
//
// Why order 4?
//   - Small enough to understand and debug
//   - Big enough to show the B-Tree concept (splits, multiple keys per node)
//   - Real databases use order ~100-1000 for disk efficiency

class BTree {
public:
    static const int ORDER = 4;  // max children per node
    // max keys per node = ORDER - 1 = 3
    // when a node has ORDER-1 keys and we insert, it splits

    struct Node {
        bool is_leaf;
        std::vector<int> keys;
        std::vector<Row> values;        // values[i] corresponds to keys[i]
        std::vector<Node*> children;    // only used if is_leaf == false

        Node(bool leaf) : is_leaf(leaf) {}
    };

    BTree();
    ~BTree();

    // Insert a key-value pair. If key exists, updates the value.
    void insert(int key, const Row& value);

    // Search for a key. Returns pointer to Row if found, nullptr otherwise.
    Row* search(int key);

    // Remove a key. Returns true if found and removed.
    bool remove(int key);

    // Get all key-value pairs in sorted order (in-order traversal)
    std::vector<std::pair<int, Row>> getAll();

    int size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // Get the root (for testing/debugging)
    Node* getRoot() const { return root_; }

private:
    Node* root_;
    int size_;

    void splitChild(Node* parent, int idx);
    void insertNonFull(Node* node, int key, const Row& value);
    Row* searchNode(Node* node, int key);
    void collectAll(Node* node, std::vector<std::pair<int, Row>>& result);
    void destroyTree(Node* node);
};

} // namespace minidb
