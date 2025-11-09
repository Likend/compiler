enable_testing()
include(GoogleTest)
find_package(GTest REQUIRED)

file(GLOB_RECURSE TEST_FILES *.cpp)

add_executable(test_runner ${TEST_FILES})

target_link_libraries(test_runner
    PRIVATE
        Compiler_lib
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
)
target_include_directories(test_runner PRIVATE ../src/)

# 自动注册测试
gtest_discover_tests(test_runner)