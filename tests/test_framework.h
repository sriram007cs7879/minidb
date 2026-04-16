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

static std::vector<TestCase>& getTests() {
    static std::vector<TestCase> tests;
    return tests;
}

static int gFailCount = 0;
static int gPassCount = 0;

#define TEST(name) \
    void test_##name(); \
    static bool reg_##name = (getTests().push_back({#name, test_##name}), true); \
    void test_##name()

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << #expr << " (line " << __LINE__ << ")\n"; \
        gFailCount++; \
        return; \
    }

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "  FAIL: " << #a << " == " << #b << " (line " << __LINE__ << ")\n"; \
        std::cerr << "    Got: " << (a) << " vs " << (b) << "\n"; \
        gFailCount++; \
        return; \
    }

#define ASSERT_THROW(expr) \
    { bool caught = false; \
      try { expr; } catch (...) { caught = true; } \
      if (!caught) { \
          std::cerr << "  FAIL: expected exception from " << #expr << " (line " << __LINE__ << ")\n"; \
          gFailCount++; \
          return; \
      } \
    }

static int runAllTests() {
    std::cout << "Running " << getTests().size() << " tests...\n\n";
    for (auto& test : getTests()) {
        int before = gFailCount;
        test.func();
        if (gFailCount == before) {
            std::cout << "  PASS: " << test.name << "\n";
            gPassCount++;
        } else {
            std::cout << "  FAIL: " << test.name << "\n";
        }
    }
    std::cout << "\n" << gPassCount << " passed, " << gFailCount << " failed.\n";
    return gFailCount > 0 ? 1 : 0;
}
