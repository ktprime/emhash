cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)
project(emhash_test)
if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

add_executable(emhash_test "main.cpp")

target_compile_features(emhash_test PRIVATE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections -march=native -Wconversion")
#    target_compile_options(emhash_test PRIVATE -Werror -Wall -Wextra -Wold-style-cast -DTSL_DEBUG -UNDEBUG)
	target_compile_options(emhash_test PRIVATE -O3 -march=native -Wall -Wextra -DTSL_DEBUG -DNDEBUG)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(emhash_test PRIVATE /bigobj /WX /W3 /DTSL_DEBUG /NDEBUG)
endif()

#include_directories(${PROJECT_SOURCE_DIR}/..)
include_directories(${PROJECT_SOURCE_DIR}/../thirdparty/)

# Boost::unit_test_framework
#find_package(Boost 1.74.0 REQUIRED COMPONENTS unit_test_framework)
#target_link_libraries(emhash_test PRIVATE Boost::unit_test_framework)

# tsl::robin_maphint(../ ${CMAKE_CURRENT_BINARY_DIR}/emhash)
#target_link_libraries(emhash_test PRIVATE emhash5::HashMap)
