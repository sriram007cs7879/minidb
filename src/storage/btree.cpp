#include "storage/btree.h"
#include <algorithm>

namespace minidb {

BTree::BTree() : root_(nullptr), size_(0) {}

BTree::~BTree() {
    destroyTree(root_);
}

// ============================================================
// INSERT
// ============================================================
// B-Tree insert works in 2 steps:
// 1. If root is full, split it first (tree grows taller from the top)
// 2. Insert into the correct leaf node (splitting along the way if needed)

void BTree::insert(int key, const Row& value) {
    // First check if key already exists — if so, update
    Row* existing = search(key);
    if (existing) {
        *existing = value;
        return;
    }

    if (root_ == nullptr) {
        // Empty tree — create a leaf node with this one key
        root_ = new Node(true);
        root_->keys.push_back(key);
        root_->values.push_back(value);
        size_++;
        return;
    }

    // If root is full (has ORDER-1 = 3 keys), split it
    if ((int)root_->keys.size() == ORDER - 1) {
        // Create a new root
        Node* new_root = new Node(false);
        new_root->children.push_back(root_);
        // Split the old root (which is now child 0 of new_root)
        splitChild(new_root, 0);
        root_ = new_root;
    }

    // Now insert into the (non-full) tree
    insertNonFull(root_, key, value);
    size_++;
}

// Split child[idx] of parent into two nodes.
// Before: parent has child[idx] with 3 keys [A, B, C]
// After:  B moves up to parent, child[idx] has [A], new child has [C]
//
// Example: splitting [10, 20, 30]
//   Before:  parent: [...]
//                       |
//                  [10, 20, 30]  ← full!
//
//   After:   parent: [..., 20, ...]
//                     /        \
//                  [10]        [30]

void BTree::splitChild(Node* parent, int idx) {
    Node* full_child = parent->children[idx];
    int mid = (ORDER - 1) / 2;  // middle index = 1 for ORDER=4

    // Create new node with the right half of full_child
    Node* new_child = new Node(full_child->is_leaf);

    // Move keys and values after the middle to new_child
    for (int i = mid + 1; i < (int)full_child->keys.size(); i++) {
        new_child->keys.push_back(full_child->keys[i]);
        new_child->values.push_back(full_child->values[i]);
    }

    // Move children after the middle to new_child (if not a leaf)
    if (!full_child->is_leaf) {
        for (int i = mid + 1; i < (int)full_child->children.size(); i++) {
            new_child->children.push_back(full_child->children[i]);
        }
    }

    // The middle key moves up to parent
    int mid_key = full_child->keys[mid];
    Row mid_value = full_child->values[mid];

    // Truncate full_child to only keep the left half
    full_child->keys.resize(mid);
    full_child->values.resize(mid);
    if (!full_child->is_leaf) {
        full_child->children.resize(mid + 1);
    }

    // Insert middle key into parent at position idx
    parent->keys.insert(parent->keys.begin() + idx, mid_key);
    parent->values.insert(parent->values.begin() + idx, mid_value);
    // Insert new_child as parent's child at idx+1
    parent->children.insert(parent->children.begin() + idx + 1, new_child);
}

// Insert key into a node that is guaranteed to be non-full.
// If node is a leaf, insert directly.
// If node is internal, find the right child and recurse.

void BTree::insertNonFull(Node* node, int key, const Row& value) {
    int i = (int)node->keys.size() - 1;

    if (node->is_leaf) {
        // Find the right position and insert
        // Keys are sorted, so find where this key belongs
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        node->keys.insert(node->keys.begin() + i + 1, key);
        node->values.insert(node->values.begin() + i + 1, value);
    } else {
        // Find which child to go to
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        i++;  // i is now the index of the child to descend into

        // If that child is full, split it first
        if ((int)node->children[i]->keys.size() == ORDER - 1) {
            splitChild(node, i);
            // After split, the middle key is at node->keys[i]
            // Decide which of the two children to go to
            if (key > node->keys[i]) {
                i++;
            }
        }

        insertNonFull(node->children[i], key, value);
    }
}

// ============================================================
// SEARCH
// ============================================================
// Start at root, at each node:
//   - Check if key is in this node's keys
//   - If not, go to the appropriate child
// Time complexity: O(log n)

Row* BTree::search(int key) {
    return searchNode(root_, key);
}

Row* BTree::searchNode(Node* node, int key) {
    if (node == nullptr) return nullptr;

    // Search through keys in this node
    for (int i = 0; i < (int)node->keys.size(); i++) {
        if (key == node->keys[i]) {
            return &node->values[i];  // Found!
        }
        if (key < node->keys[i]) {
            // Key would be in the left child
            if (node->is_leaf) return nullptr;  // No children — not found
            return searchNode(node->children[i], key);
        }
    }

    // Key is greater than all keys in this node
    if (node->is_leaf) return nullptr;
    return searchNode(node->children[node->keys.size()], key);
}

// ============================================================
// REMOVE
// ============================================================
// Simplified approach: collect all entries except the one to delete,
// clear the tree, and re-insert everything.
// Not the most efficient, but correct and simple.

bool BTree::remove(int key) {
    if (search(key) == nullptr) return false;

    // Collect all entries except the one with this key
    auto all = getAll();
    std::vector<std::pair<int, Row>> remaining;
    for (auto& [k, v] : all) {
        if (k != key) {
            remaining.push_back({k, v});
        }
    }

    // Clear the tree
    destroyTree(root_);
    root_ = nullptr;
    size_ = 0;

    // Re-insert everything
    for (auto& [k, v] : remaining) {
        insert(k, v);
    }

    return true;
}

// ============================================================
// GET ALL — In-order traversal
// ============================================================
// Returns all key-value pairs sorted by key.
// This is what SELECT * uses.

std::vector<std::pair<int, Row>> BTree::getAll() {
    std::vector<std::pair<int, Row>> result;
    collectAll(root_, result);
    return result;
}

void BTree::collectAll(Node* node, std::vector<std::pair<int, Row>>& result) {
    if (node == nullptr) return;

    for (int i = 0; i < (int)node->keys.size(); i++) {
        // Visit left child first (if internal node)
        if (!node->is_leaf && i < (int)node->children.size()) {
            collectAll(node->children[i], result);
        }
        // Visit this key
        result.push_back({node->keys[i], node->values[i]});
    }

    // Visit the rightmost child
    if (!node->is_leaf && !node->children.empty()) {
        collectAll(node->children[node->keys.size()], result);
    }
}

// ============================================================
// CLEANUP
// ============================================================
void BTree::destroyTree(Node* node) {
    if (node == nullptr) return;
    for (auto* child : node->children) {
        destroyTree(child);
    }
    delete node;
}

} // namespace minidb
