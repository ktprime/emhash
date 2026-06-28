# emhash Test Suite CMakeLists.txt (rewritten for doctest framework)
#
# New structure:
#   common/  - shared headers (doctest.h, maps.hpp, hashers.hpp, trackers.hpp, utilities.hpp)
#   unit/    - functional correctness (doctest, TEST_CASE_TEMPLATE)
#   memory/  - sanitizer + leak + lifecycle (doctest)
#   stress/  - stress tests (doctest)
#   attack/  - hash attack tests (doctest, EMH_SAFE_PSL)
#   fuzz/    - libfuzzer targets (optional) + regression reproducer
#   debug/   - unchanged debug tools (not part of doctest suite)
#   bench/   - unchanged benchmarks (optional, Google Benchmark)
#   archive/ - old scratch files (not built)

cmake_minimum_required(VERSION 3.16)
project(emhash_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----------------------------------------------------------------------------
# Compiler options
# ----------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
    # SSE2 for SIMD intrinsics (required for emilib hash maps)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86|AMD64|amd64|x86_64")
        add_compile_options(-msse2)
    endif()
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto=auto")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    # Optional sanitizer build: cmake -DEMHASH_SANITIZER=address|undefined|thread|memory
    if(DEFINED EMHASH_SANITIZER AND NOT EMHASH_SANITIZER STREQUAL "")
        add_compile_options(-fsanitize=${EMHASH_SANITIZER} -fno-omit-frame-pointer)
        add_link_options(-fsanitize=${EMHASH_SANITIZER})
        message(STATUS "emhash: sanitizer build enabled (-fsanitize=${EMHASH_SANITIZER})")
    endif()
else()
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
    # MSVC AddressSanitizer (x64): cmake -DEMHASH_SANITIZER=address
    if(DEFINED EMHASH_SANITIZER AND EMHASH_SANITIZER STREQUAL "address")
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/fsanitize=address)
        message(STATUS "emhash: MSVC AddressSanitizer build enabled")
    endif()
endif()

# ----------------------------------------------------------------------------
# Shared include paths: ../include for emhash headers, common/ for doctest + utils
# ----------------------------------------------------------------------------
set(EMHASH_TEST_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/common)

# ----------------------------------------------------------------------------
# Helper: register a doctest-based test executable + CTest entry
# ----------------------------------------------------------------------------
function(emhash_add_test name source)
    add_executable(${name} ${source})
    target_compile_features(${name} PRIVATE cxx_std_17)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}           # for #include "common/maps.hpp"
        ${EMHASH_TEST_COMMON})                # for #include "doctest.h"
    if(MSVC)
        # /W4 highest warning level; /WX intentionally not used (library headers emit L4 warnings)
        target_compile_options(${name} PRIVATE /W4)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "emhash")
endfunction()

# Helper: register a test needing EMH_SAFE_PSL (attack tests)
function(emhash_add_attack_test name source)
    emhash_add_test(${name} ${source})
    target_compile_definitions(${name} PRIVATE EMH_SAFE_PSL)
endfunction()

# ----------------------------------------------------------------------------
# unit/ — functional correctness (auto-discovered)
# ----------------------------------------------------------------------------
file(GLOB UNIT_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")
foreach(src ${UNIT_TESTS})
    get_filename_component(name ${src} NAME_WE)
    emhash_add_test(${name} ${src})
endforeach()

# ----------------------------------------------------------------------------
# memory/ — sanitizer + leak + lifecycle (auto-discovered)
# ----------------------------------------------------------------------------
file(GLOB MEMORY_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/memory/*.cpp")
foreach(src ${MEMORY_TESTS})
    get_filename_component(name ${src} NAME_WE)
    emhash_add_test(${name} ${src})
endforeach()

# ----------------------------------------------------------------------------
# stress/ — stress tests (explicit list of new doctest files; old files
# removed once transition complete)
# ----------------------------------------------------------------------------
set(EMHASH_NEW_STRESS_TESTS
    test_stress_all
    test_highload
    test_bad_hash
    test_reserve_fix
)
foreach(name ${EMHASH_NEW_STRESS_TESTS})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/stress/${name}.cpp")
        emhash_add_test(${name} "stress/${name}.cpp")
    endif()
endforeach()

# ----------------------------------------------------------------------------
# attack/ — hash attack tests (explicit list of new doctest files)
# ----------------------------------------------------------------------------
set(EMHASH_NEW_ATTACK_TESTS
    test_hash_attack
    test_collision_hardening
)
foreach(name ${EMHASH_NEW_ATTACK_TESTS})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/attack/${name}.cpp")
        emhash_add_attack_test(${name} "attack/${name}.cpp")
    endif()
endforeach()

# ----------------------------------------------------------------------------
# fuzz/ — regression reproducer (always built; libfuzzer targets optional)
# ----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/reproduce_emhash8_bug.cpp")
    emhash_add_test(reproduce_emhash8_bug "fuzz/reproduce_emhash8_bug.cpp")
    set_tests_properties(reproduce_emhash8_bug PROPERTIES WILL_FAIL TRUE)
endif()

option(EMHASH_ENABLE_FUZZ "Enable libfuzzer targets (requires clang)" OFF)
if(EMHASH_ENABLE_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "EMHASH_ENABLE_FUZZ requires clang (libfuzzer)")
    endif()
    set(EMHASH_FUZZ_TARGETS
        fuzz_emhash_all
        fuzz_emilib_all
        fuzz_extreme
    )
    foreach(name ${EMHASH_FUZZ_TARGETS})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/${name}.cpp")
            add_executable(${name} "fuzz/${name}.cpp")
            target_compile_features(${name} PRIVATE cxx_std_17)
            target_include_directories(${name} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/../include
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${EMHASH_TEST_COMMON})
            target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address -fno-omit-frame-pointer)
            target_link_options(${name} PRIVATE -fsanitize=fuzzer,address)
        endif()
    endforeach()
endif()

# ----------------------------------------------------------------------------
# CTest integration
# ----------------------------------------------------------------------------
include(CTest)

# ----------------------------------------------------------------------------
# Custom targets: quick / stress / attack / all
# ----------------------------------------------------------------------------
add_custom_target(quick_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Quick Tests (unit + memory) ==="
    COMMAND ctest -L emhash -E "stress|attack|fuzz" --output-on-failure
    COMMENT "Quick test suite"
)

add_custom_target(stress_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Stress Tests ==="
    COMMAND ctest -L emhash -R "stress|highload|bad_hash|reserve_fix" --output-on-failure
    COMMENT "Stress test suite"
)

add_custom_target(attack_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Attack Tests ==="
    COMMAND ctest -L emhash -R "attack|collision" --output-on-failure
    COMMENT "Attack test suite"
)

add_custom_target(all_tests
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running All Tests ==="
    COMMAND ctest -L emhash --output-on-failure
    COMMENT "All tests"
)

add_custom_target(emhash_test_help
    COMMAND ${CMAKE_COMMAND} -E echo "Available targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  quick_test    - unit + memory tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  stress_test   - stress tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  attack_test   - hash attack tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  all_tests     - all tests"
)

# ----------------------------------------------------------------------------
# Google Benchmark (optional, off by default) — unchanged
# ----------------------------------------------------------------------------
option(EMHASH_ENABLE_BENCH "Enable Google Benchmark performance tests" OFF)
if(EMHASH_ENABLE_BENCH)
    include(FetchContent)
    FetchContent_Declare(
        googlebench
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        31c66f47f60d06790a4885a261d682ba339bd100 # v1.9.4
        GIT_SHALLOW    TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Populate(googlebench)
    add_subdirectory(${googlebench_SOURCE_DIR} ${googlebench_BINARY_DIR} EXCLUDE_FROM_ALL)

    add_executable(emhash_bench bench/emhash_bench.cpp)
    target_compile_features(emhash_bench PRIVATE cxx_std_17)
    target_include_directories(emhash_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    target_link_libraries(emhash_bench PRIVATE benchmark::benchmark)

    add_custom_target(bench
        COMMAND emhash_bench --benchmark_format=console
        DEPENDS emhash_bench
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()
