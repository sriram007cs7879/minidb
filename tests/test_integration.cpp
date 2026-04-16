#include "test_framework.h"
#include "sql/tokenizer.h"
#include "sql/parser.h"
#include "core/catalog.h"
#include "core/executor.h"
#include <filesystem>

using namespace minidb;

TEST(integration_create_and_insert) {
    std::string test_dir = "/tmp/minidb_test_int1";
    std::filesystem::remove_all(test_dir);

    Catalog catalog(test_dir);

    auto tokens = Tokenizer::tokenize("CREATE TABLE students (id INT, name TEXT, age INT);");
    auto stmt = Parser::parse(tokens);
    std::string result = Executor::execute(stmt.get(), catalog);
    ASSERT_EQ(result, std::string("Table 'students' created."));

    tokens = Tokenizer::tokenize("INSERT INTO students VALUES (1, 'Alice', 20);");
    stmt = Parser::parse(tokens);
    result = Executor::execute(stmt.get(), catalog);
    ASSERT_EQ(result, std::string("Inserted 1 row."));

    tokens = Tokenizer::tokenize("SELECT * FROM students;");
    stmt = Parser::parse(tokens);
    result = Executor::execute(stmt.get(), catalog);
    ASSERT_TRUE(result.find("Alice") != std::string::npos);
    ASSERT_TRUE(result.find("1 row") != std::string::npos);

    std::filesystem::remove_all(test_dir);
}

TEST(integration_select_where) {
    std::string test_dir = "/tmp/minidb_test_int2";
    std::filesystem::remove_all(test_dir);

    Catalog catalog(test_dir);

    auto exec = [&](const std::string& sql) {
        auto tokens = Tokenizer::tokenize(sql);
        auto stmt = Parser::parse(tokens);
        return Executor::execute(stmt.get(), catalog);
    };

    exec("CREATE TABLE people (id INT, name TEXT, age INT)");
    exec("INSERT INTO people VALUES (1, 'Alice', 20)");
    exec("INSERT INTO people VALUES (2, 'Bob', 25)");
    exec("INSERT INTO people VALUES (3, 'Charlie', 20)");

    std::string result = exec("SELECT * FROM people WHERE age = 20");
    ASSERT_TRUE(result.find("Alice") != std::string::npos);
    ASSERT_TRUE(result.find("Charlie") != std::string::npos);
    ASSERT_TRUE(result.find("2 rows") != std::string::npos);
    ASSERT_TRUE(result.find("Bob") == std::string::npos);

    result = exec("SELECT name FROM people WHERE id = 2");
    ASSERT_TRUE(result.find("Bob") != std::string::npos);

    std::filesystem::remove_all(test_dir);
}

TEST(integration_delete) {
    std::string test_dir = "/tmp/minidb_test_int3";
    std::filesystem::remove_all(test_dir);

    Catalog catalog(test_dir);

    auto exec = [&](const std::string& sql) {
        auto tokens = Tokenizer::tokenize(sql);
        auto stmt = Parser::parse(tokens);
        return Executor::execute(stmt.get(), catalog);
    };

    exec("CREATE TABLE items (id INT, name TEXT)");
    exec("INSERT INTO items VALUES (1, 'Pen')");
    exec("INSERT INTO items VALUES (2, 'Book')");
    exec("INSERT INTO items VALUES (3, 'Eraser')");

    std::string result = exec("DELETE FROM items WHERE id = 2");
    ASSERT_TRUE(result.find("Deleted 1 row") != std::string::npos);

    result = exec("SELECT * FROM items");
    ASSERT_TRUE(result.find("Pen") != std::string::npos);
    ASSERT_TRUE(result.find("Eraser") != std::string::npos);
    ASSERT_TRUE(result.find("Book") == std::string::npos);
    ASSERT_TRUE(result.find("2 rows") != std::string::npos);

    std::filesystem::remove_all(test_dir);
}

TEST(integration_persistence) {
    std::string test_dir = "/tmp/minidb_test_int4";
    std::filesystem::remove_all(test_dir);

    // Session 1: create and insert
    {
        Catalog catalog(test_dir);
        auto exec = [&](const std::string& sql) {
            auto tokens = Tokenizer::tokenize(sql);
            auto stmt = Parser::parse(tokens);
            return Executor::execute(stmt.get(), catalog);
        };
        exec("CREATE TABLE notes (id INT, text TEXT)");
        exec("INSERT INTO notes VALUES (1, 'Hello World')");
        exec("INSERT INTO notes VALUES (2, 'Goodbye')");
        catalog.saveAll();
    }

    // Session 2: data persists on disk via the Pager
    {
        Catalog catalog(test_dir);
        catalog.loadAll();

        auto tokens = Tokenizer::tokenize("SELECT * FROM notes");
        auto stmt = Parser::parse(tokens);
        std::string result = Executor::execute(stmt.get(), catalog);

        ASSERT_TRUE(result.find("Hello World") != std::string::npos);
        ASSERT_TRUE(result.find("Goodbye") != std::string::npos);
        ASSERT_TRUE(result.find("2 rows") != std::string::npos);
    }

    std::filesystem::remove_all(test_dir);
}
