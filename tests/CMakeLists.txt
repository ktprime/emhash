# emhash Test Suite CMakeLists.txt
# Build all tests with cmake

cmake_minimum_required(VERSION 3.16)
project(emhash_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler options (GCC/Clang are applied globally; MSVC options are set
# per-target via emhash_add_test so that /WX only catches warnings in test
# source files, not in the library headers which we intentionally do not modify.)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
    # Enable SSE2 for SIMD intrinsics (required for emilib hash maps)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86|AMD64|amd64|x86_64")
        add_compile_options(-msse2)
    endif()
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto=auto")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    # Optional sanitizer build (Linux/Clang only). Enable with
    #   cmake -DEMHASH_SANITIZER=address  (or undefined, thread, memory)
    if(DEFINED EMHASH_SANITIZER AND NOT EMHASH_SANITIZER STREQUAL "")
        add_compile_options(-fsanitize=${EMHASH_SANITIZER} -fno-omit-frame-pointer)
        add_link_options(-fsanitize=${EMHASH_SANITIZER})
        message(STATUS "emhash: sanitizer build enabled (-fsanitize=${EMHASH_SANITIZER})")
    endif()
else()
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
    # MSVC supports AddressSanitizer on x64 since VS 2019 16.9.
    # Enable with: cmake -DEMHASH_SANITIZER=address
    if(DEFINED EMHASH_SANITIZER AND EMHASH_SANITIZER STREQUAL "address")
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/fsanitize=address)
        message(STATUS "emhash: MSVC AddressSanitizer build enabled")
    endif()
endif()

# Helper to register an emhash test executable
function(emhash_add_test name source)
    add_executable(${name} ${source})
    target_compile_features(${name} PRIVATE cxx_std_17)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    if(MSVC)
        # /W4 enables the highest standard warning level. /WX is intentionally
        # not used because emhash library headers (which are intentionally not
        # modified) produce level-4 warnings when instantiated from test code.
        target_compile_options(${name} PRIVATE /W4)
    endif()
endfunction()

# ============================================================================
# Verification Tests
# ============================================================================

emhash_add_test(test_verify verify/test_all_maps.cpp)
emhash_add_test(quick_test8 verify/quick_test8.cpp)
emhash_add_test(test_emhash58 verify/test_emhash58.cpp)
emhash_add_test(test_hidden_bugs8 verify/test_hidden_bugs8.cpp)
emhash_add_test(test_extreme verify/test_extreme.cpp)
emhash_add_test(test_interface_combo verify/test_interface_combo.cpp)
emhash_add_test(test_emhash5_verify verify/test_emhash5_verify.cpp)
emhash_add_test(test_emhash_allocator verify/test_emhash_allocator.cpp)
emhash_add_test(test_functional_edge verify/test_functional_edge.cpp)
emhash_add_test(test_sanitizer_aggregate verify/test_sanitizer_aggregate.cpp)

# Merged from test/ directory
emhash_add_test(test_hashset_allocator verify/test_hashset_allocator.cpp)

emhash_add_test(test_emilib_comprehensive verify/test_emilib_comprehensive.cpp)
find_package(Threads REQUIRED)
target_link_libraries(test_emilib_comprehensive PRIVATE Threads::Threads)

emhash_add_test(test_size_sweep verify/test_size_sweep.cpp)

# Full API coverage tests
emhash_add_test(test_hashmap_full_api verify/test_hashmap_full_api.cpp)
emhash_add_test(test_hashset_full_api verify/test_hashset_full_api.cpp)

emhash_add_test(test_special_key_types verify/test_special_key_types.cpp)
emhash_add_test(test_merge_correctness verify/test_merge_correctness.cpp)
emhash_add_test(edge_test verify/edge_test.cpp)
emhash_add_test(test_find_slot_bucket verify/test_find_slot_bucket.cpp)
target_link_libraries(test_find_slot_bucket PRIVATE Threads::Threads)

emhash_add_test(test_string_key_leak verify/test_string_key_leak.cpp)

# ============================================================================
# Stress Tests
# ============================================================================

emhash_add_test(test_stress stress/stress_all_maps.cpp)
emhash_add_test(stress_fix stress/stress_fix.cpp)
emhash_add_test(highload_test stress/highload_test.cpp)
emhash_add_test(test_emhash5_stress stress/test_emhash5_stress.cpp)
emhash_add_test(test_emhash5_hifi stress/test_emhash5_hifi.cpp)

# Merged from test/ directory
emhash_add_test(test_bad_hash_stress stress/test_bad_hash_stress.cpp)

# ============================================================================
# Hash Attack Tests (requires EMH_SAFE_PSL)
# ============================================================================

emhash_add_test(test_attack attack/hash_attack_all.cpp)
emhash_add_test(test_collision_hardening attack/test_collision_hardening.cpp)
target_compile_definitions(test_attack PRIVATE EMH_SAFE_PSL)

emhash_add_test(hash_attack5 attack/hash_attack.cpp)
target_compile_definitions(hash_attack5 PRIVATE EMH_SAFE_PSL)

emhash_add_test(hash_attack7 attack/hash_attack7.cpp)
target_compile_definitions(hash_attack7 PRIVATE EMH_SAFE_PSL)

# ============================================================================
# Debug Tests
# ============================================================================

emhash_add_test(test_debug debug/debug_all_maps.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_debug PRIVATE -g -O0)
elseif(MSVC)
    target_compile_options(test_debug PRIVATE /Od /Zi)
endif()

emhash_add_test(debug_chain debug/debug_chain.cpp)
emhash_add_test(debug_set_erase debug/debug_set_erase.cpp)
emhash_add_test(min_repro debug/min_repro.cpp)
emhash_add_test(reproduce_crash debug/reproduce_crash.cpp)
emhash_add_test(fuzz_reproduce debug/fuzz_reproduce.cpp)

# Merged from test/ directory
emhash_add_test(test_probe_bug debug/test_probe_bug.cpp)
emhash_add_test(test_probe_coverage debug/test_probe_coverage.cpp)
emhash_add_test(test_debug_find debug/test_debug_find.cpp)
emhash_add_test(test_repro_collision debug/test_repro_collision.cpp)

# ============================================================================
# Fuzz Tests
# ============================================================================

emhash_add_test(reproduce_emhash8_bug fuzz/reproduce_emhash8_bug.cpp)

# ============================================================================
# CTest Integration
# ============================================================================

include(CTest)

function(emhash_register_test name)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "emhash")
endfunction()

emhash_register_test(reproduce_emhash8_bug)

# Verification tests
emhash_register_test(test_verify)
emhash_register_test(quick_test8)
emhash_register_test(test_emhash58)
emhash_register_test(test_hidden_bugs8)
emhash_register_test(test_extreme)
emhash_register_test(test_interface_combo)
emhash_register_test(test_emhash5_verify)
emhash_register_test(test_emhash_allocator)
emhash_register_test(test_functional_edge)
emhash_register_test(test_hashset_allocator)
emhash_register_test(test_emilib_comprehensive)
emhash_register_test(test_size_sweep)
emhash_register_test(test_hashmap_full_api)
emhash_register_test(test_hashset_full_api)
emhash_register_test(test_special_key_types)
emhash_register_test(test_merge_correctness)
emhash_register_test(edge_test)
emhash_register_test(test_find_slot_bucket)
emhash_register_test(test_string_key_leak)

# Stress tests
emhash_register_test(test_stress)
emhash_register_test(test_bad_hash_stress)
emhash_register_test(stress_fix)
emhash_register_test(highload_test)
emhash_register_test(test_emhash5_stress)
emhash_register_test(test_emhash5_hifi)

# Hash attack tests
emhash_register_test(test_attack)
emhash_register_test(hash_attack5)
emhash_register_test(hash_attack7)

# Debug tests
emhash_register_test(test_debug)

# ============================================================================
# Test Suites (custom targets)
# ============================================================================

# Quick test suite (runs in ~20 seconds)
add_custom_target(quick_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Quick Tests ==="
    COMMAND test_verify
    COMMAND test_debug
    COMMAND ${CMAKE_COMMAND} -E echo "=== All Quick Tests Passed ==="
    DEPENDS test_verify test_debug
)

# Stress test suite
add_custom_target(stress_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Stress Tests ==="
    COMMAND test_stress
    COMMAND highload_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== All Stress Tests Passed ==="
    DEPENDS test_stress highload_test
)

# Attack test suite
add_custom_target(attack_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Hash Attack Tests ==="
    COMMAND test_attack
    COMMAND ${CMAKE_COMMAND} -E echo "=== All Attack Tests Passed ==="
    DEPENDS test_attack
)

# All tests
add_custom_target(all_tests
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running All Tests ==="
    COMMAND test_verify
    COMMAND test_debug
    COMMAND test_stress
    COMMAND test_attack
    COMMAND ${CMAKE_COMMAND} -E echo "=== All Tests Passed ==="
    DEPENDS test_verify test_debug test_stress test_attack
)

# Print help
add_custom_target(emhash_test_help
    COMMAND ${CMAKE_COMMAND} -E echo "Available targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  quick_test    - Quick validation tests (~20s)"
    COMMAND ${CMAKE_COMMAND} -E echo "  stress_test   - Stress tests (~1min)"
    COMMAND ${CMAKE_COMMAND} -E echo "  attack_test   - Hash attack tests (~2min)"
    COMMAND ${CMAKE_COMMAND} -E echo "  all_tests     - Run all tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  emhash_test_help - Show this help"
)

# ============================================================================
# Google Benchmark (optional, off by default)
# ============================================================================
option(EMHASH_ENABLE_BENCH "Enable Google Benchmark performance tests" OFF)

if(EMHASH_ENABLE_BENCH)
    include(FetchContent)
    FetchContent_Declare(
        googlebench
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        31c66f47f60d06790a4885a261d682ba339bd100 # v1.9.4
        GIT_SHALLOW    TRUE
    )
    # Prevent Google Benchmark from building tests
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
