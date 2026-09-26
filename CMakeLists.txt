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
    include/arc_cache.h
    include/arc_cache.tpp
    include/twoq_cache.h
    include/twoq_cache.tpp
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

add_executable(cache_test
    tests/test_cache.cpp
)
target_include_directories(cache_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(cache_test PRIVATE GTest::gtest)

target_compile_definitions(cache_test PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

add_test(NAME cache_test COMMAND cache_test)



add_executable(cache_system_test
    tests/test_system.cpp
)
target_include_directories(cache_system_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(cache_system_test PRIVATE GTest::gtest)

target_compile_definitions(cache_system_test PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

add_test(NAME cache_system_test COMMAND cache_system_test)



add_executable(optimal_cache_test
    tests/test_optimal_cache.cpp
)
target_include_directories(optimal_cache_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(optimal_cache_test PRIVATE GTest::gtest)

target_compile_definitions(optimal_cache_test PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

add_test(NAME optimal_cache_test COMMAND optimal_cache_test)





include(GoogleTest)


