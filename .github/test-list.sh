#!/usr/bin/env bash
# =============================================================================
# Shared test list for CI jobs (Single Source of Truth).
#
# This file defines all test targets that should be built and run across CI
# jobs (build, asan, coverage, cmake, windows-msvc). Each job may subset the
# list but should not add tests that are absent here.
#
# Usage in CI:
#   source .github/test-list.sh
#   for src in "${UNIT_TESTS[@]}"; do
#       $CXX $FLAGS unit/${src}.cpp -o ${src}
#   done
#
# Special flags: tests in ATTACK_TESTS require -DEMH_SAFE_PSL.
# =============================================================================

# Unit tests — functional correctness (doctest, fast)
UNIT_TESTS=(
    test_crud
    test_full_api
    test_copy_move
    test_iterators
    test_iterator_invalidation
    test_edge_cases
    test_string_keys
    test_special_keys
    test_reserve_clear
    test_allocator
    test_hashset
    test_stress_correctness
    test_small_data
    test_api_coverage
)

# Memory tests — sanitizer + leak + lifecycle (doctest)
MEMORY_TESTS=(
    test_string_key_leak
    test_lifecycle_audit
    test_sanitizer
)

# Stress tests — high-load + million-scale correctness
STRESS_TESTS=(
    test_stress_all
    test_highload
    test_bad_hash
    test_reserve_fix
    test_million_stress
    test_complex_conditions
)

# Attack tests — hash collision hardening (require -DEMH_SAFE_PSL)
ATTACK_TESTS=(
    test_hash_attack
    test_collision_hardening
)

# Debug test — only in build job (depends on uncommitted scratch files)
DEBUG_TESTS=(
    debug_all_maps
)
