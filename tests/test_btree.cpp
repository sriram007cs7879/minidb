#include "test_framework.h"
#include "storage/btree.h"
#include <filesystem>

using namespace minidb;

static Schema makeTestSchema() {
    Schema s;
    s.table_name = "test";
    s.columns = {{"id", ColumnType::INT}, {"name", ColumnType::TEXT}, {"age", ColumnType::INT}};
    return s;
}

TEST(btree_insert_and_search) {
    std::string path = "/tmp/minidb_test_btree1.db";
    std::filesystem::remove(path);

    Schema schema = makeTestSchema();
    BTree tree(path, schema);

    tree.insert(1, {1, std::string("Alice"), 20});
    tree.insert(2, {2, std::string("Bob"), 21});
    tree.insert(3, {3, std::string("Charlie"), 22});

    ASSERT_EQ(tree.size(), 3);

    auto found = tree.search(2);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(std::get<std::string>((*found)[1]), std::string("Bob"));

    std::filesystem::remove(path);
}

TEST(btree_search_not_found) {
    std::string path = "/tmp/minidb_test_btree2.db";
    std::filesystem::remove(path);

    Schema schema = makeTestSchema();
    BTree tree(path, schema);
    tree.insert(1, {1, std::string("Alice"), 20});

    ASSERT_FALSE(tree.search(99).has_value());

    std::filesystem::remove(path);
}

TEST(btree_remove) {
    std::string path = "/tmp/minidb_test_btree3.db";
    std::filesystem::remove(path);

    Schema schema = makeTestSchema();
    BTree tree(path, schema);
    tree.insert(1, {1, std::string("Alice"), 20});
    tree.insert(2, {2, std::string("Bob"), 21});
    tree.insert(3, {3, std::string("Charlie"), 22});

    ASSERT_TRUE(tree.remove(2));
    ASSERT_EQ(tree.size(), 2);
    ASSERT_FALSE(tree.search(2).has_value());
    ASSERT_TRUE(tree.search(1).has_value());
    ASSERT_TRUE(tree.search(3).has_value());

    std::filesystem::remove(path);
}

TEST(btree_get_all_sorted) {
    std::string path = "/tmp/minidb_test_btree4.db";
    std::filesystem::remove(path);

    Schema s;
    s.table_name = "nums";
    s.columns = {{"val", ColumnType::INT}};
    BTree tree(path, s);

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

    std::filesystem::remove(path);
}

TEST(btree_many_inserts_causes_splits) {
    std::string path = "/tmp/minidb_test_btree5.db";
    std::filesystem::remove(path);

    Schema schema = makeTestSchema();
    BTree tree(path, schema);

    // Insert enough rows to trigger page splits
    // With row_size = 4+256+4 = 264, leaf_cell = 268 bytes
    // leaf_max_cells = (4096-3)/268 = 15
    // So 20 rows will definitely trigger at least one split
    for (int i = 1; i <= 20; i++) {
        tree.insert(i, {i, std::string("Person_") + std::to_string(i), 20 + i});
    }
    ASSERT_EQ(tree.size(), 20);

    // All should be findable
    for (int i = 1; i <= 20; i++) {
        auto found = tree.search(i);
        ASSERT_TRUE(found.has_value());
    }

    // Should be sorted
    auto all = tree.getAll();
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(all[i].first, i + 1);
    }

    std::filesystem::remove(path);
}

TEST(btree_persistence) {
    std::string path = "/tmp/minidb_test_btree6.db";
    std::filesystem::remove(path);

    Schema schema = makeTestSchema();

    // Session 1: insert data
    {
        BTree tree(path, schema);
        tree.insert(1, {1, std::string("Alice"), 20});
        tree.insert(2, {2, std::string("Bob"), 21});
        tree.flush();
    }

    // Session 2: data should still be there
    {
        BTree tree(path, schema);
        ASSERT_EQ(tree.size(), 2);
        auto found = tree.search(1);
        ASSERT_TRUE(found.has_value());
        ASSERT_EQ(std::get<std::string>((*found)[1]), std::string("Alice"));
    }

    std::filesystem::remove(path);
}
