del -f *.gc*
del -f *.gcna

rem g++ -I.. -I../thirdparty -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb ebench.cpp -o ecov.exe
rem ecov c10 dfba5678dm
rem gcov ebench.cpp
rem gcov ecov-ebench.gcno

g++ -I.. -flto -I../thirdparty -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb martin_bench.cpp -o mcov.exe
mcov 5678mbb6e6
gcov martin_bench.cpp
gcov mcov-martin_bench.gcno

