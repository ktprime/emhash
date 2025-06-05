cmake_minimum_required(VERSION 3.12)

project(emhash_bench)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message("  CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR} ")
message("  CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
include_directories(${PROJECT_SOURCE_DIR}/bench)

add_definitions(-DHAVE_BOOST=1 -DABSL_HAMP=1 -DEMH_PSL=16)# -DET=1)
find_package(Threads REQUIRED)

if(WIN32)
    set(CMAKE_CXX_FLAGS "/WX- /MP")
    set(CMAKE_CXX_FLAGS_DEBUG "/W3 /Zi /Od /WX- ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "/Ob1 /Ot /Oi /Oy /GL /arch:AVX ${CMAKE_CXX_FLAGS}")
    add_compile_options(/Ob2 /DNDEBUG /O2 /Ot /Oi /Oy /GL /arch:AVX)
elseif(1)
	set(CMAKE_CXX_FLAGS_DEBUG "-g fno-strict-aliasing ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "-flto=auto -march=native  ${CMAKE_CXX_FLAGS}")
endif()

add_executable(ebench ${PROJECT_SOURCE_DIR}/bench/ebench.cpp)
add_executable(sbench ${PROJECT_SOURCE_DIR}/bench/sbench.cpp)
add_executable(mbench ${PROJECT_SOURCE_DIR}/bench/martin_bench.cpp)
add_executable(bs ${PROJECT_SOURCE_DIR}/bench/bstring.cpp)
add_executable(bi ${PROJECT_SOURCE_DIR}/bench/buint64.cpp)
add_executable(fbench ${PROJECT_SOURCE_DIR}/bench/fbench.cpp)
add_executable(hbench ${PROJECT_SOURCE_DIR}/bench/hbench.cpp)
#add_executable(qbench ${PROJECT_SOURCE_DIR}/bench/qbench.cpp)
#add_executable(sibench ${PROJECT_SOURCE_DIR}/bench/simple_bench.cpp)

#target_link_libraries(ebench PRIVATE Threads::Threads)
#target_link_libraries(sbench PRIVATE Threads::Threads)
#target_link_libraries(mbench PRIVATE Threads::Threads)
#target_link_libraries(hbench PRIVATE Threads::Threads)
#target_link_libraries(fbench PRIVATE Threads::Threads)
