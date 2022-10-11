del -f *.gc*
del -f *.gcna

g++ -I.. -I../thirdparty -DGCOV=1 -DTKey=1 -DFIB_HASH=2 -DEMH_H2=1 -DABSL=1 -fprofile-arcs -ftest-coverage -mavx -march=native -ggdb ebench.cpp -o ecov.exe
ecov c10 d23

gcov ebench.cpp
gcov ecov-ebench.gcno

