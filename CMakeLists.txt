cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" EMHASH_VERSION LIMIT_COUNT 1)
project(emhash VERSION ${EMHASH_VERSION} LANGUAGES CXX
    DESCRIPTION "High-performance, memory-efficient C++ open addressing flat hash table"
    HOMEPAGE_URL "https://github.com/ktprime/emhash"
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WITH_BENCHMARKS "Build benchmarks." OFF)
option(WITH_EXAMPLES "Build examples." ON)
option(WITH_TESTS "Build tests." OFF)

# SIMD optimization level for x86 (emilib headers use SSE2 intrinsics;
# emhash8 uses _mm_prefetch). Setting NONE disables intrinsics entirely.
# Options: NONE, SSE, SSE2, AVX, AVX2, AVX512 (default: SSE2)
set(EMH_SIMD_LEVEL "SSE2" CACHE STRING "SIMD level for x86 (NONE/SSE/SSE2/AVX/AVX2/AVX512)")
set_property(CACHE EMH_SIMD_LEVEL PROPERTY STRINGS NONE SSE SSE2 AVX AVX2 AVX512)

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message("  CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR} ")
message("  CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message("  EMH_SIMD_LEVEL: ${EMH_SIMD_LEVEL}")

add_library(emhash INTERFACE)
# ALIAS target so in-tree consumers (examples, tests) can use the same
# emhash::emhash name that find_package consumers use after install.
add_library(emhash::emhash ALIAS emhash)

target_include_directories(
     emhash INTERFACE
     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
     $<INSTALL_INTERFACE:include>
)

target_compile_features(emhash INTERFACE cxx_std_17)

# Propagate SIMD flag to consumers on x86/x86_64 so the intrinsics in
# the headers compile out-of-the-box after find_package(emhash).
if(NOT EMH_SIMD_LEVEL STREQUAL "NONE")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
       AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86|AMD64|amd64|x86_64|i[3-6]86")
        string(TOLOWER "${EMH_SIMD_LEVEL}" _emh_simd_flag)
        if(_emh_simd_flag STREQUAL "avx512")
            set(_emh_simd_flag "avx512f")
        endif()
        target_compile_options(emhash INTERFACE -m${_emh_simd_flag})
    endif()
endif()
unset(_emh_simd_flag)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS emhash
    EXPORT emhash-targets
)

# Install headers preserving directory structure (emhash/ and emilib/)
# Exclude sse2neon.h from install — it is a third-party ARM compatibility shim
# that should not pollute the user's include directory root.
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PATTERN "sse2neon.h" EXCLUDE)

install(EXPORT emhash-targets
        FILE emhashTargets.cmake
        NAMESPACE emhash::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emhash
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/emhashConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/emhashConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emhash
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/emhashConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/emhashConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emhash
)

if(WITH_BENCHMARKS)
    find_package(Threads REQUIRED)
    find_package(OpenMP REQUIRED COMPONENTS CXX)

    set(BENCH_INCLUDES ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/thirdparty ${PROJECT_SOURCE_DIR}/bench)
    set(BENCH_DEFS -DHAVE_BOOST=1 -DABSL_HMAP=1 -DEMH_PSL=16)

    if(WIN32)
        add_compile_options(/WX- /MP /Ob2 /DNDEBUG /O2 /Ot /Oi /Oy /Gy /GF /GS- /GL /arch:AVX2)
        set(CMAKE_SUPPRESS_REGENERATION ON)
        if(CMAKE_GENERATOR MATCHES "Visual Studio")
          set_property(GLOBAL PROPERTY USE_FOLDERS ON)
        endif()
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-strict-aliasing")
        set(CMAKE_CXX_FLAGS_RELEASE "-flto=auto -DNDEBUG -march=native -O2")
        set(CMAKE_CXX_FLAGS "-DNDEBUG -march=native ${CMAKE_CXX_FLAGS}")
    endif()

    # Helper function to reduce boilerplate for benchmark targets
    function(emhash_add_bench name source)
        add_executable(${name} ${PROJECT_SOURCE_DIR}/bench/${source})
        target_include_directories(${name} PRIVATE ${BENCH_INCLUDES})
        target_compile_definitions(${name} PRIVATE ${BENCH_DEFS})
    endfunction()

    emhash_add_bench(ebench  ebench.cpp)
    emhash_add_bench(sbench  sbench.cpp)
    emhash_add_bench(mbench  martin_bench.cpp)
    emhash_add_bench(cbench  comprehensive_bench.cpp)
    emhash_add_bench(bs      bstring.cpp)
    emhash_add_bench(bi      buint64.cpp)
    emhash_add_bench(fbench  fbench.cpp)
    emhash_add_bench(hbench  hbench.cpp)
    emhash_add_bench(zbench  zhash_bench.cc)
    emhash_add_bench(jbench  hash_join2.cpp)
    target_link_libraries(jbench PRIVATE OpenMP::OpenMP_CXX)
endif()

if(WITH_EXAMPLES)
    add_subdirectory(docs/examples)
endif()

if(WITH_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

