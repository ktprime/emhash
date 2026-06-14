@echo off
rem Code coverage build script for emhash
rem Usage: cov.cmd (run from bench/ directory)
rem
rem Build:
rem   g++ -I../include -I../thirdparty -std=c++17 -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -g ebench.cpp -o ecov.exe
rem
rem Run:
rem   ecov 10 0 dfba5678dm
rem
rem Generate coverage:
rem   gcov ebench.cpp
rem   gcov ecov-ebench.gcno

del /Q *.gc* 2>nul
del /Q *.gcna 2>nul

rem Uncomment for ebench coverage
rem g++ -I../include -flto=auto -I../thirdparty -std=c++17 -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb ebench.cpp -o ecov.exe
rem ecov 10 0 dfba5678dm
rem gcov ebench.cpp
rem gcov ecov-ebench.gcno

rem Build with coverage
g++ -I../include -flto=auto -I../thirdparty -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb martin_bench.cpp -o mcov.exe
mcov 7b8e8
gcov martin_bench.cpp
gcov mcov-martin_bench.gcno
