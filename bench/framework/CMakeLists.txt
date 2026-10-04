cmake_minimum_required(VERSION 3.16)
project(emhash_bench_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(EMH_BENCH_WITH_BOOST "Compile the boost reference baseline" ON)
option(EMH_BENCH_WITH_ABSL  "Compile the absl reference baseline"  ON)
set(EMH_BENCH_SIMD_LEVEL "SSE2" CACHE STRING "SIMD level for x86 (NONE/SSE/SSE2/AVX/AVX2/AVX512)")

set(EMH_BENCH_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(EMH_BENCH_INC   "${EMH_BENCH_ROOT}/include")
set(EMH_BENCH_TP    "${EMH_BENCH_ROOT}/thirdparty")

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_executable(emhash_bench bench_main.cpp)

target_include_directories(emhash_bench PRIVATE
    "${EMH_BENCH_INC}"
    "${EMH_BENCH_TP}"
    "${CMAKE_CURRENT_SOURCE_DIR}"      # bench_framework.hpp
    "${CMAKE_CURRENT_SOURCE_DIR}/.."   # nanobench.h
)

# ---------------------------------------------------------------------------
# Optional reference baselines — enabled only when the headers are present,
# so a partial third-party tree degrades to "skipped" rather than a hard error.
# ---------------------------------------------------------------------------
if(EMH_BENCH_WITH_BOOST)
    if(EXISTS "${EMH_BENCH_TP}/boost/unordered/unordered_flat_map.hpp")
        target_compile_definitions(emhash_bench PRIVATE EMH_BENCH_HAVE_BOOST)
        message(STATUS "emhash_bench: boost reference baseline enabled")
    else()
        message(STATUS "emhash_bench: boost headers not found, baseline skipped")
    endif()
endif()

if(EMH_BENCH_WITH_ABSL)
    # absl's flat_hash_map is NOT header-only. These sources supply the out-of-line
    # symbols it needs (e.g. GetHashRefForEmptyHasher, MixingHashState::kSeed).
    # All are required; if any is missing the baseline is skipped rather than
    # producing a confusing link failure.
    set(_absl_srcs
        "${EMH_BENCH_TP}/absl/container/internal/raw_hash_set.cc"
        "${EMH_BENCH_TP}/absl/hash/internal/hash.cc"
        "${EMH_BENCH_TP}/absl/hash/internal/city.cc"
        "${EMH_BENCH_TP}/absl/hash/internal/low_level_hash.cc"
    )
    set(_absl_ok TRUE)
    foreach(_s ${_absl_srcs})
        if(NOT EXISTS "${_s}")
            set(_absl_ok FALSE)
        endif()
    endforeach()

    if(_absl_ok AND EXISTS "${EMH_BENCH_TP}/absl/container/flat_hash_map.h")
        target_compile_definitions(emhash_bench PRIVATE EMH_BENCH_HAVE_ABSL)
        target_sources(emhash_bench PRIVATE ${_absl_srcs})
        message(STATUS "emhash_bench: absl reference baseline enabled")
    else()
        message(STATUS "emhash_bench: absl headers/sources not found, baseline skipped")
    endif()
    unset(_absl_srcs)
    unset(_absl_ok)
    unset(_s)
endif()

# ---------------------------------------------------------------------------
# SIMD flags: the emilib headers use SSE2 intrinsics on x86.
# Note: no -march=native here — A/B comparisons are only valid when both sides
# are built with identical flags, and pinning the level keeps builds portable.
# ---------------------------------------------------------------------------
if(NOT EMH_BENCH_SIMD_LEVEL STREQUAL "NONE")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
       AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86|AMD64|amd64|x86_64|i[3-6]86")
        string(TOLOWER "${EMH_BENCH_SIMD_LEVEL}" _simd)
        if(_simd STREQUAL "avx512")
            set(_simd "avx512f")
        endif()
        target_compile_options(emhash_bench PRIVATE -m${_simd})
    endif()
endif()
unset(_simd)

if(MSVC)
    target_compile_options(emhash_bench PRIVATE /W4 /permissive- /Zc:__cplusplus)
    if(EMH_BENCH_SIMD_LEVEL MATCHES "AVX2")
        target_compile_options(emhash_bench PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(emhash_bench PRIVATE -Wall -Wextra -Wno-unused-parameter)
    if(UNIX)
        target_link_libraries(emhash_bench PRIVATE pthread)
    endif()
endif()

message(STATUS "")
message(STATUS "=== emhash_bench configuration ===")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  SIMD:     ${EMH_BENCH_SIMD_LEVEL}")
message(STATUS "")
