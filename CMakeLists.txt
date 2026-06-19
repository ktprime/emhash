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

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message("  CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR} ")
message("  CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

add_library(emhash INTERFACE)

target_include_directories(
     emhash INTERFACE
     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
     $<INSTALL_INTERFACE:include>
)

target_compile_features(emhash INTERFACE cxx_std_17)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS emhash
    EXPORT emhash-targets
)

# Install headers preserving directory structure (emhash/ and emilib/)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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

    add_executable(ebench ${PROJECT_SOURCE_DIR}/bench/ebench.cpp)
    target_include_directories(ebench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(ebench PRIVATE ${BENCH_DEFS})

    add_executable(sbench ${PROJECT_SOURCE_DIR}/bench/sbench.cpp)
    target_include_directories(sbench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(sbench PRIVATE ${BENCH_DEFS})

    add_executable(mbench ${PROJECT_SOURCE_DIR}/bench/martin_bench.cpp)
    target_include_directories(mbench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(mbench PRIVATE ${BENCH_DEFS})

    add_executable(cbench ${PROJECT_SOURCE_DIR}/bench/comprehensive_bench.cpp)
    target_include_directories(cbench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(cbench PRIVATE ${BENCH_DEFS})

    add_executable(bs ${PROJECT_SOURCE_DIR}/bench/bstring.cpp)
    target_include_directories(bs PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(bs PRIVATE ${BENCH_DEFS})

    add_executable(bi ${PROJECT_SOURCE_DIR}/bench/buint64.cpp)
    target_include_directories(bi PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(bi PRIVATE ${BENCH_DEFS})

    add_executable(fbench ${PROJECT_SOURCE_DIR}/bench/fbench.cpp)
    target_include_directories(fbench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(fbench PRIVATE ${BENCH_DEFS})

    add_executable(hbench ${PROJECT_SOURCE_DIR}/bench/hbench.cpp)
    target_include_directories(hbench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(hbench PRIVATE ${BENCH_DEFS})

    add_executable(zbench ${PROJECT_SOURCE_DIR}/bench/zhash_bench.cc)
    target_include_directories(zbench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(zbench PRIVATE ${BENCH_DEFS})

    add_executable(jbench ${PROJECT_SOURCE_DIR}/bench/hash_join2.cpp)
    target_include_directories(jbench PRIVATE ${BENCH_INCLUDES})
    target_compile_definitions(jbench PRIVATE ${BENCH_DEFS})
    target_link_libraries(jbench PRIVATE OpenMP::OpenMP_CXX)
endif()

if(WITH_EXAMPLES)
    add_subdirectory(docs/examples)
endif()

if(WITH_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

