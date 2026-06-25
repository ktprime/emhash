# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.6)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(googlebench-populate NONE)


# Pass through things we've already detected in the main project to avoid
# paying the cost of redetecting them again in ExternalProject_Add()
set(GIT_EXECUTABLE [==[D:/Program Files/Git/cmd/git.exe]==])
set(GIT_VERSION_STRING [==[2.51.0.windows.1]==])
set_property(GLOBAL PROPERTY _CMAKE_FindGit_GIT_EXECUTABLE_VERSION
  [==[D:/Program Files/Git/cmd/git.exe;2.51.0.windows.1]==]
)


include(ExternalProject)
ExternalProject_Add(googlebench-populate
                     "UPDATE_DISCONNECTED" "False" "GIT_REPOSITORY" "https://github.com/google/benchmark.git" "GIT_TAG" "31c66f47f60d06790a4885a261d682ba339bd100" "GIT_SHALLOW" "TRUE"
                    SOURCE_DIR          "D:/emhash/tests/build_bench/_deps/googlebench-src"
                    BINARY_DIR          "D:/emhash/tests/build_bench/_deps/googlebench-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


