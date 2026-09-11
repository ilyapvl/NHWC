cmake_minimum_required(VERSION 3.10)
project(cache_system)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main src/main.cpp)

target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_sources(main PRIVATE
    include/cache.h
    include/cache.tpp
    include/lru_cache.h
    include/lru_cache.tpp
    include/lfu_cache.h
    include/lfu_cache.tpp
    include/lirs_cache.h
    include/lirs_cache.tpp
    include/opt_cache.h
    include/opt_cache.tpp
    include/cache_system.h
    include/cache_system.tpp
)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(lru_cache_tests
    tests/test_lru.cpp
)
target_include_directories(lru_cache_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(lru_cache_tests PRIVATE GTest::gtest_main)

target_compile_definitions(lru_cache_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

include(GoogleTest)
gtest_discover_tests(lru_cache_tests)

