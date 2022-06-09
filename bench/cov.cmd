del -f *.gc*
del -f *.gcna

g++ -I.. -DEMH_FIND_HIT -I../thirdparty -DGCOV=1 -DTKey=0 -DET=0 -fprofile-arcs -ftest-coverage -mavx -march=native -ggdb %1 -o ecov.exe
ecov c10 d13

gcov %1
gcov ecov-ebench.gcno

