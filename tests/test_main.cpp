#include <gtest/gtest.h>

// GTest main entry point
// This is provided by gtest_main, but we define it explicitly for clarity
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
