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
    include/opt_cache.h
    include/opt_cache.tpp
    include/cache_system.h
    include/cache_system.tpp
)


