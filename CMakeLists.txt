cmake_minimum_required(VERSION 3.14)
project(minidb VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Main library
add_library(minidb_lib
    src/storage/pager.cpp
    src/storage/btree.cpp
    src/sql/tokenizer.cpp
    src/sql/parser.cpp
    src/core/table.cpp
    src/core/catalog.cpp
    src/core/executor.cpp
)

target_include_directories(minidb_lib PUBLIC include)

# Main executable
add_executable(minidb src/main.cpp)
target_link_libraries(minidb PRIVATE minidb_lib)

# Tests
enable_testing()
add_executable(minidb_tests
    tests/test_main.cpp
    tests/test_btree.cpp
    tests/test_tokenizer.cpp
    tests/test_integration.cpp
)
target_include_directories(minidb_tests PRIVATE tests)
target_link_libraries(minidb_tests PRIVATE minidb_lib)
add_test(NAME minidb_tests COMMAND minidb_tests)
