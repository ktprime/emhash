cmake_minimum_required(VERSION 3.8)

project(emhash_map_tests)

add_executable(emhash_map_tests "main.cpp" "hash_map_tests.cpp")

target_compile_features(emhash_map_tests PRIVATE cxx_std_17)
                                    
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#    target_compile_options(emhash_map_tests PRIVATE -Werror -Wall -Wextra -Wold-style-cast -DTSL_DEBUG -UNDEBUG)
    target_compile_options(emhash_map_tests PRIVATE -Wall -Wextra -DTSL_DEBUG -UNDEBUG)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(emhash_map_tests PRIVATE /bigobj /WX /W3 /DTSL_DEBUG /UNDEBUG)
endif()

# Boost::unit_test_framework
find_package(Boost 1.54.0 REQUIRED COMPONENTS unit_test_framework)
target_link_libraries(emhash_map_tests PRIVATE Boost::unit_test_framework)   

# tsl::robin_maphint(../ ${CMAKE_CURRENT_BINARY_DIR}/emhash)
#target_link_libraries(emhash_map_tests PRIVATE emhash5::HashMap)  
