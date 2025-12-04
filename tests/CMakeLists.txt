# Create test executable
add_executable(TaskScheduler_Tests
    test_main.cpp
    test_task_lifecycle.cpp
    test_state_machine.cpp
    test_config_updates.cpp
    test_concurrency.cpp
    test_scheduler.cpp
    test_lazy_deletion.cpp
    test_config_module.cpp
    test_edge_cases.cpp
    test_error_handling.cpp
)

# Link against TaskScheduler library
target_link_libraries(TaskScheduler_Tests PRIVATE
    TaskScheduler
    GTest::gtest
    GTest::gtest_main
)

# Include directories
target_include_directories(TaskScheduler_Tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(TaskScheduler_Tests)
