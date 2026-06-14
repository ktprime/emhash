cmake_minimum_required(VERSION 3.16)
project(emhash_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the emhash headers from parent directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Examples
add_executable(basic_map basic_map.cpp)
add_executable(basic_set basic_set.cpp)
add_executable(custom_hash custom_hash.cpp)
add_executable(custom_allocator custom_allocator.cpp)

# Quick benchmark
add_executable(quick_bench ../quick_bench.cpp)
