# emhash Test Suite CMakeLists.txt
# Build all tests with cmake

cmake_minimum_required(VERSION 3.14)
project(emhash_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
elseif(MSVC)
    add_compile_options(/W4)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bench)

# ============================================================================
# Verification Tests
# ============================================================================

add_executable(test_verify verify/test_all_maps.cpp)
target_compile_features(test_verify PUBLIC cxx_std_17)

add_executable(quick_test8 verify/quick_test8.cpp)
target_compile_features(quick_test8 PUBLIC cxx_std_17)

add_executable(test_emhash58 verify/test_emhash58.cpp)
target_compile_features(test_emhash58 PUBLIC cxx_std_17)

add_executable(test_hidden_bugs8 verify/test_hidden_bugs8.cpp)
target_compile_features(test_hidden_bugs8 PUBLIC cxx_std_17)

add_executable(test_extreme verify/test_extreme.cpp)
target_compile_features(test_extreme PUBLIC cxx_std_17)

add_executable(test_interface_combo verify/test_interface_combo.cpp)
target_compile_features(test_interface_combo PUBLIC cxx_std_17)

add_executable(test_emhash5_verify verify/test_emhash5_verify.cpp)
target_compile_features(test_emhash5_verify PUBLIC cxx_std_17)

add_executable(test_emhash_allocator verify/test_emhash_allocator.cpp)
target_compile_features(test_emhash_allocator PUBLIC cxx_std_17)

add_executable(test_functional_edge verify/test_functional_edge.cpp)
target_compile_features(test_functional_edge PUBLIC cxx_std_17)

# Merged from test/ directory
add_executable(test_hashset_allocator verify/test_hashset_allocator.cpp)
target_compile_features(test_hashset_allocator PUBLIC cxx_std_17)

add_executable(test_emilib_comprehensive verify/test_emilib_comprehensive.cpp)
target_compile_features(test_emilib_comprehensive PUBLIC cxx_std_17)

add_executable(test_main verify/test_main.cpp)
target_compile_features(test_main PUBLIC cxx_std_17)

add_executable(test_size_sweep verify/test_size_sweep.cpp)
target_compile_features(test_size_sweep PUBLIC cxx_std_17)

# Boost.Test based comprehensive unit tests (requires Boost)
# add_executable(hash_map_tests verify/hash_map_tests.cpp)
# target_compile_features(hash_map_tests PUBLIC cxx_std_17)
# target_link_libraries(hash_map_tests PRIVATE Boost::unit_test_framework)

# ============================================================================
# Stress Tests
# ============================================================================

add_executable(test_stress stress/stress_all_maps.cpp)
target_compile_features(test_stress PUBLIC cxx_std_17)

add_executable(stress_fix stress/stress_fix.cpp)
target_compile_features(stress_fix PUBLIC cxx_std_17)

add_executable(highload_test stress/highload_test.cpp)
target_compile_features(highload_test PUBLIC cxx_std_17)

add_executable(test_emhash5_stress stress/test_emhash5_stress.cpp)
target_compile_features(test_emhash5_stress PUBLIC cxx_std_17)

add_executable(test_emhash5_hifi stress/test_emhash5_hifi.cpp)
target_compile_features(test_emhash5_hifi PUBLIC cxx_std_17)

# Merged from test/ directory
add_executable(test_bad_hash_stress stress/test_bad_hash_stress.cpp)
target_compile_features(test_bad_hash_stress PUBLIC cxx_std_17)

# ============================================================================
# Hash Attack Tests (requires EMH_SAFE_PSL)
# ============================================================================

add_executable(test_attack attack/hash_attack_all.cpp)
target_compile_features(test_attack PUBLIC cxx_std_17)
target_compile_definitions(test_attack PRIVATE EMH_SAFE_PSL)

add_executable(hash_attack5 attack/hash_attack.cpp)
target_compile_features(hash_attack5 PUBLIC cxx_std_17)
target_compile_definitions(hash_attack5 PRIVATE EMH_SAFE_PSL)

add_executable(hash_attack7 attack/hash_attack7.cpp)
target_compile_features(hash_attack7 PUBLIC cxx_std_17)
target_compile_definitions(hash_attack7 PRIVATE EMH_SAFE_PSL)

# ============================================================================
# Debug Tests
# ============================================================================

add_executable(test_debug debug/debug_all_maps.cpp)
target_compile_features(test_debug PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_debug PRIVATE -g -O0)
elseif(MSVC)
    target_compile_options(test_debug PRIVATE /Od /Zi)
endif()

add_executable(debug_chain debug/debug_chain.cpp)
target_compile_features(debug_chain PUBLIC cxx_std_17)

add_executable(debug_set_erase debug/debug_set_erase.cpp)
target_compile_features(debug_set_erase PUBLIC cxx_std_17)

add_executable(min_repro debug/min_repro.cpp)
target_compile_features(min_repro PUBLIC cxx_std_17)

add_executable(reproduce_crash debug/reproduce_crash.cpp)
target_compile_features(reproduce_crash PUBLIC cxx_std_17)

add_executable(fuzz_reproduce debug/fuzz_reproduce.cpp)
target_compile_features(fuzz_reproduce PUBLIC cxx_std_17)

# Merged from test/ directory
add_executable(test_probe_bug debug/test_probe_bug.cpp)
target_compile_features(test_probe_bug PUBLIC cxx_std_17)

add_executable(test_probe_coverage debug/test_probe_coverage.cpp)
target_compile_features(test_probe_coverage PUBLIC cxx_std_17)

add_executable(test_debug_find debug/test_debug_find.cpp)
target_compile_features(test_debug_find PUBLIC cxx_std_17)

add_executable(test_repro_collision debug/test_repro_collision.cpp)
target_compile_features(test_repro_collision PUBLIC cxx_std_17)

# ============================================================================
# Benchmarks
# ============================================================================

add_executable(ebench bench/ebench.cpp)
target_compile_features(ebench PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ebench PRIVATE -O3 -march=native)
elseif(MSVC)
    target_compile_options(ebench PRIVATE /O2)
endif()

add_executable(martin_bench bench/martin_bench.cpp)
target_compile_features(martin_bench PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(martin_bench PRIVATE -O3 -march=native)
elseif(MSVC)
    target_compile_options(martin_bench PRIVATE /O2)
endif()

add_executable(highload_bench bench/highload_bench.cpp)
target_compile_features(highload_bench PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(highload_bench PRIVATE -O3)
elseif(MSVC)
    target_compile_options(highload_bench PRIVATE /O2)
endif()
target_compile_definitions(highload_bench PRIVATE EMH_HIGH_LOAD=123456)

# ============================================================================
# Fuzz Tests (optional, requires clang)
# ============================================================================

option(ENABLE_FUZZ_TESTS "Enable fuzz tests (requires clang)" OFF)

if(ENABLE_FUZZ_TESTS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_emhash_all fuzz/fuzz_emhash_all.cpp)
    target_compile_features(fuzz_emhash_all PUBLIC cxx_std_17)
    target_compile_options(fuzz_emhash_all PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_emhash_all PRIVATE -fsanitize=fuzzer,address)

    add_executable(fuzz_emhash8 fuzz/fuzz_emhash8.cpp)
    target_compile_features(fuzz_emhash8 PUBLIC cxx_std_17)
    target_compile_options(fuzz_emhash8 PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_emhash8 PRIVATE -fsanitize=fuzzer,address)

    add_executable(fuzz_emhash8_advanced fuzz/fuzz_emhash8_advanced.cpp)
    target_compile_features(fuzz_emhash8_advanced PUBLIC cxx_std_17)
    target_compile_options(fuzz_emhash8_advanced PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_emhash8_advanced PRIVATE -fsanitize=fuzzer,address)

    add_executable(fuzz_emilib_all fuzz/fuzz_emilib_all.cpp)
    target_compile_features(fuzz_emilib_all PUBLIC cxx_std_17)
    target_compile_options(fuzz_emilib_all PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_emilib_all PRIVATE -fsanitize=fuzzer,address)
endif()

# Non-fuzzer versions (can be built with gcc on Linux/macOS; skip on MinGW/MSVC)
if(NOT WIN32 OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_extreme fuzz/fuzz_extreme.cpp fuzz/fuzz_main.cpp)
    target_compile_features(fuzz_extreme PUBLIC cxx_std_17)
    target_compile_options(fuzz_extreme PRIVATE -fsanitize=address,undefined)
    target_link_options(fuzz_extreme PRIVATE -fsanitize=address,undefined)

    add_executable(fuzz_nocoll fuzz/fuzz_nocoll.cpp fuzz/fuzz_main.cpp)
    target_compile_features(fuzz_nocoll PUBLIC cxx_std_17)
    target_compile_options(fuzz_nocoll PRIVATE -fsanitize=address,undefined)
    target_link_options(fuzz_nocoll PRIVATE -fsanitize=address,undefined)
endif()

add_executable(reproduce_emhash8_bug fuzz/reproduce_emhash8_bug.cpp)
target_compile_features(reproduce_emhash8_bug PUBLIC cxx_std_17)

# ============================================================================
# Test Targets
# ============================================================================

# Quick test suite (runs in ~20 seconds)
add_custom_target(quick_test
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Quick Tests ==="
    COMMAND test_verify
    COMMAND test_debug
    COMMAND ${CMAKE_COMMAND} -E echo "=== All Quick Tests Passed ==="
    DEPENDS test_verify test_debug
)

# Aggregate fuzz target (ASan variants work with gcc/clang; LibFuzzer variants need clang)
add_custom_target(fuzz
    COMMAND ${CMAKE_COMMAND} -E echo "=== Building ASan Fuzz Variants ==="
    DEPENDS fuzz_extreme fuzz_nocoll
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
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz          - Build fuzz variants"
    COMMAND ${CMAKE_COMMAND} -E echo "  emhash_test_help - Show this help"
)