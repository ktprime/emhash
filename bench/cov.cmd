del -f *.gc*
del -f *.gcna

g++ -I.. -I../thirdparty -DGCOV=1 -DEMH_H2=1 -fprofile-arcs -ftest-coverage -mavx -march=native -ggdb ebench.cpp -o ecov.exe
ecov c10 d23

gcov ebench.cpp
gcov ecov-ebench.gcno

