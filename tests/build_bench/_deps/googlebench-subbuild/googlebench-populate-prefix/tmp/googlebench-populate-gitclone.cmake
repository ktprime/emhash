
if(NOT "D:/emhash/tests/build_bench/_deps/googlebench-subbuild/googlebench-populate-prefix/src/googlebench-populate-stamp/googlebench-populate-gitinfo.txt" IS_NEWER_THAN "D:/emhash/tests/build_bench/_deps/googlebench-subbuild/googlebench-populate-prefix/src/googlebench-populate-stamp/googlebench-populate-gitclone-lastrun.txt")
  message(STATUS "Avoiding repeated git clone, stamp file is up to date: 'D:/emhash/tests/build_bench/_deps/googlebench-subbuild/googlebench-populate-prefix/src/googlebench-populate-stamp/googlebench-populate-gitclone-lastrun.txt'")
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "D:/emhash/tests/build_bench/_deps/googlebench-src"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: 'D:/emhash/tests/build_bench/_deps/googlebench-src'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "D:/Program Files/Git/cmd/git.exe"  clone --no-checkout --depth 1 --no-single-branch --config "advice.detachedHead=false" "https://github.com/google/benchmark.git" "googlebench-src"
    WORKING_DIRECTORY "D:/emhash/tests/build_bench/_deps"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once:
          ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/google/benchmark.git'")
endif()

execute_process(
  COMMAND "D:/Program Files/Git/cmd/git.exe"  checkout 31c66f47f60d06790a4885a261d682ba339bd100 --
  WORKING_DIRECTORY "D:/emhash/tests/build_bench/_deps/googlebench-src"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: '31c66f47f60d06790a4885a261d682ba339bd100'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "D:/Program Files/Git/cmd/git.exe"  submodule update --recursive --init 
    WORKING_DIRECTORY "D:/emhash/tests/build_bench/_deps/googlebench-src"
    RESULT_VARIABLE error_code
    )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: 'D:/emhash/tests/build_bench/_deps/googlebench-src'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
    "D:/emhash/tests/build_bench/_deps/googlebench-subbuild/googlebench-populate-prefix/src/googlebench-populate-stamp/googlebench-populate-gitinfo.txt"
    "D:/emhash/tests/build_bench/_deps/googlebench-subbuild/googlebench-populate-prefix/src/googlebench-populate-stamp/googlebench-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: 'D:/emhash/tests/build_bench/_deps/googlebench-subbuild/googlebench-populate-prefix/src/googlebench-populate-stamp/googlebench-populate-gitclone-lastrun.txt'")
endif()

