del -f *.gc*
del -f *.gcna

rem g++ -I.. -I../thirdparty -std=c++17 -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -g ebench.cpp -o ecov.exe
rem ecov c10 dfba5678dm
rem gcov ebench.cpp
rem gcov ecov-ebench.gcno

g++ -I.. -flto=auto -I../thirdparty -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb martin_bench.cpp -o mcov.exe
mcov 7b8e8
gcov martin_bench.cpp
gcov mcov-martin_bench.gcno
