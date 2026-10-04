cmake_minimum_required(VERSION 3.14)
project(emilib_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(EMILIB_BENCH_AVX2 "Enable AVX2 variants (requires -mavx2)" OFF)

# ---------------------------------------------------------------------------
# Source files — collect all .cpp in this directory
# ---------------------------------------------------------------------------
file(GLOB EMILIB_BENCH_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(emilib_bench ${EMILIB_BENCH_SOURCES})

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(emilib_bench PRIVATE
    # emilib / emhash public headers
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    # third-party headers (boost, absl, etc.)
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty
    # current directory (for local bench headers)
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(emilib_bench PRIVATE
    _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
    _SILENCE_CXX20_CISO646_REMOVED_WARNING
    NOMINMAX
)

# ---------------------------------------------------------------------------
# Compiler-specific flags
# ---------------------------------------------------------------------------

# GCC / Clang on x86
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(emilib_bench PRIVATE -msse2)

    if(EMILIB_BENCH_AVX2)
        target_compile_options(emilib_bench PRIVATE -mavx2)
    endif()

    # Link pthread on Linux / macOS
    if(UNIX)
        target_link_libraries(emilib_bench PRIVATE pthread)
    endif()
endif()

# MSVC
if(MSVC)
    target_compile_options(emilib_bench PRIVATE
        /permissive-
        /Zc:__cplusplus
        /O2   # optimize for speed (overridden by CMAKE_BUILD_TYPE if set)
    )

    if(EMILIB_BENCH_AVX2)
        target_compile_options(emilib_bench PRIVATE /arch:AVX2)
    endif()
endif()

# ---------------------------------------------------------------------------
# Boost — header-only at thirdparty/boost (single-header style)
# The include path above already covers it. No linking needed.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Abseil — header-only stubs at thirdparty/absl (no linking needed)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Print configuration summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== emilib_bench configuration ===")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  AVX2:         ${EMILIB_BENCH_AVX2}")
message(STATUS "  Sources:      ${EMILIB_BENCH_SOURCES}")
message(STATUS "")
