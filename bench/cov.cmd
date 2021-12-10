del -f *.gcov
del -f *.gcno
del -f *.gcna

g++ -I.. -I../thirdparty -DGCOV=1 -DFIB_HASH=1 -DTKey=2 -DET=0 -fprofile-arcs -ftest-coverage -mavx -march=native -ggdb %1 -o ecov.exe
ecov c10
gcov %1
gcov ecov-ebench.gcno
gvim emilib.hpp.gcov

