#include "test_framework.h"

// Test files are included via the build system (CMakeLists.txt links them)
// Each test file registers its tests using the TEST() macro

int main() {
    return runAllTests();
}
