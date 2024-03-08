del -f *.gc*
del -f *.gcna

g++ -I.. -I../thirdparty -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb ebench.cpp -o ecov.exe
ecov c10 dfba5678dm
gcov ebench.cpp
gcov ecov-ebench.gcno

#g++ -I.. -I../thirdparty -DEMH_PSL=32 -ggdb -DGCOV=1 -fprofile-arcs -ftest-coverage -march=native -ggdb martin_bench.cpp -o mcov.exe
#mcov 56378mbb4e4
#gcov martin_bench.cpp
#gcov mcov-martin_bench.gcno

