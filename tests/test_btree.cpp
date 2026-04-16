#include "test_framework.h"
#include "storage/btree.h"
using namespace minidb;

TEST(btree_insert_and_search) {
    BTree tree;
    Row r1 = {1, std::string("Alice"), 20};
    Row r2 = {2, std::string("Bob"), 21};
    Row r3 = {3, std::string("Charlie"), 22};

    tree.insert(1, r1);
    tree.insert(2, r2);
    tree.insert(3, r3);

    ASSERT_EQ(tree.size(), 3);

    Row* found = tree.search(2);
    ASSERT_TRUE(found != nullptr);
    ASSERT_EQ(std::get<std::string>((*found)[1]), std::string("Bob"));
}

TEST(btree_search_not_found) {
    BTree tree;
    tree.insert(1, {1, std::string("Alice")});
    ASSERT_TRUE(tree.search(99) == nullptr);
}

TEST(btree_remove) {
    BTree tree;
    tree.insert(1, {1, std::string("Alice")});
    tree.insert(2, {2, std::string("Bob")});
    tree.insert(3, {3, std::string("Charlie")});

    ASSERT_TRUE(tree.remove(2));
    ASSERT_EQ(tree.size(), 2);
    ASSERT_TRUE(tree.search(2) == nullptr);
    ASSERT_TRUE(tree.search(1) != nullptr);
    ASSERT_TRUE(tree.search(3) != nullptr);
}

TEST(btree_get_all_sorted) {
    BTree tree;
    tree.insert(30, {30});
    tree.insert(10, {10});
    tree.insert(20, {20});
    tree.insert(50, {50});
    tree.insert(40, {40});

    auto all = tree.getAll();
    ASSERT_EQ((int)all.size(), 5);
    ASSERT_EQ(all[0].first, 10);
    ASSERT_EQ(all[1].first, 20);
    ASSERT_EQ(all[2].first, 30);
    ASSERT_EQ(all[3].first, 40);
    ASSERT_EQ(all[4].first, 50);
}

TEST(btree_duplicate_key_updates) {
    BTree tree;
    tree.insert(1, {1, std::string("Alice")});
    tree.insert(1, {1, std::string("Updated")});

    ASSERT_EQ(tree.size(), 1);
    Row* found = tree.search(1);
    ASSERT_EQ(std::get<std::string>((*found)[1]), std::string("Updated"));
}

TEST(btree_many_inserts_causes_splits) {
    BTree tree;
    // Insert enough to trigger multiple splits (ORDER=4, split at 3 keys)
    for (int i = 1; i <= 20; i++) {
        tree.insert(i, {i});
    }
    ASSERT_EQ(tree.size(), 20);

    // All should be findable
    for (int i = 1; i <= 20; i++) {
        ASSERT_TRUE(tree.search(i) != nullptr);
    }

    // Should be sorted
    auto all = tree.getAll();
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(all[i].first, i + 1);
    }
}
