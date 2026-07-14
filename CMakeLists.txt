cmake_minimum_required(VERSION 3.14)

project(Orderbook LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(orderbook_lib
    src/orderbook.cpp
    src/imbalance.cpp
    src/vwap.cpp
    src/bsm_engine.cpp
    src/snapshot.cpp
    src/option_pricer.cpp
)
target_include_directories(orderbook_lib PUBLIC include)

# --- Main Executable ---
add_executable(orderbook_main src/main.cpp)
target_link_libraries(orderbook_main PRIVATE orderbook_lib)

# --- Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.16.0
)
# Prevent Google Test from overriding compiler settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Google Benchmark
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# Enable CTest
enable_testing()

# --- Test Executables ---
# Each test file becomes its own executable that links against:
# 1. orderbook_lib 
# 2. GTest::gtest_main (Google Test + automatic main() function)

add_executable(test_order tests/test_order.cpp)
target_link_libraries(test_order PRIVATE orderbook_lib GTest::gtest_main)
target_include_directories(test_order PRIVATE include)

add_executable(test_orderbook_basic tests/test_orderbook_basic.cpp)
target_link_libraries(test_orderbook_basic PRIVATE orderbook_lib GTest::gtest_main)
target_include_directories(test_orderbook_basic PRIVATE include)

add_executable(test_orderbook_types tests/test_orderbook_types.cpp)
target_link_libraries(test_orderbook_types PRIVATE orderbook_lib GTest::gtest_main)
target_include_directories(test_orderbook_types PRIVATE include)

add_executable(test_invariants tests/test_invariants.cpp)
target_link_libraries(test_invariants PRIVATE orderbook_lib GTest::gtest_main)
target_include_directories(test_invariants PRIVATE include)

add_executable(test_bsm_engine tests/test_bsm_engine.cpp)
target_link_libraries(test_bsm_engine PRIVATE orderbook_lib GTest::gtest_main)
target_include_directories(test_bsm_engine PRIVATE include)

add_executable(test_option_pricer tests/test_option_pricer.cpp)
target_link_libraries(test_option_pricer PRIVATE orderbook_lib GTest::gtest_main)
target_include_directories(test_option_pricer PRIVATE include)


# Register tests with CTest so "ctest" discovers them
include(GoogleTest)
gtest_discover_tests(test_order)
gtest_discover_tests(test_orderbook_basic)
gtest_discover_tests(test_orderbook_types)
gtest_discover_tests(test_invariants)
gtest_discover_tests(test_bsm_engine)
gtest_discover_tests(test_option_pricer)

# --- Benchmarks ---
add_executable(benchmark_engine tests/benchmark_engine.cpp)
target_link_libraries(benchmark_engine PRIVATE orderbook_lib benchmark::benchmark benchmark::benchmark_main)
target_include_directories(benchmark_engine PRIVATE include)
