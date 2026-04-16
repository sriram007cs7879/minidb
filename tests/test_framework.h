// Minimal test framework — no external dependencies needed
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>

struct TestCase {
    std::string name;
    std::function<void()> func;
};

// Use inline to ensure ONE shared instance across all .cpp files
// (requires C++17)
inline std::vector<TestCase>& getTests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& getFailCount() {
    static int count = 0;
    return count;
}

inline int& getPassCount() {
    static int count = 0;
    return count;
}

#define TEST(name) \
    void test_##name(); \
    static bool reg_##name = (getTests().push_back({#name, test_##name}), true); \
    void test_##name()

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << #expr << " (line " << __LINE__ << ")\n"; \
        getFailCount()++; \
        return; \
    }

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "  FAIL: " << #a << " == " << #b << " (line " << __LINE__ << ")\n"; \
        getFailCount()++; \
        return; \
    }

#define ASSERT_THROW(expr) \
    { bool caught = false; \
      try { expr; } catch (...) { caught = true; } \
      if (!caught) { \
          std::cerr << "  FAIL: expected exception from " << #expr << " (line " << __LINE__ << ")\n"; \
          getFailCount()++; \
          return; \
      } \
    }

inline int runAllTests() {
    std::cout << "Running " << getTests().size() << " tests...\n\n";
    for (auto& test : getTests()) {
        int before = getFailCount();
        test.func();
        if (getFailCount() == before) {
            std::cout << "  PASS: " << test.name << "\n";
            getPassCount()++;
        } else {
            std::cout << "  FAIL: " << test.name << "\n";
        }
    }
    std::cout << "\n" << getPassCount() << " passed, " << getFailCount() << " failed.\n";
    return getFailCount() > 0 ? 1 : 0;
}
