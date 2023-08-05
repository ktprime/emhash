del -f *.gc*
del -f *.gcna

#g++ -I.. -I../thirdparty -DEMH_PSL=16 -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb ebench.cpp -o ecov.exe
#ecov c10 dfba6dm
#gcov ebench.cpp
#gcov ecov-ebench.gcno

g++ -I.. -I../thirdparty -DEMH_PSL=32 -ggdb -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb martin_bench.cpp -o mcov.exe
mcov 5678mb6e6
gcov martin_bench.cpp
gcov mcov-martin_bench.gcno


