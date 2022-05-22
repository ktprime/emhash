del -f *.gc*
del -f *.gcna

g++ -I.. -DEMH_H2=1 -DEMH_FIND_HIT -I../thirdparty -DGCOV=1 -DET=0 -fprofile-arcs -ftest-coverage -mavx -march=native -ggdb %1 -o ecov.exe
ecov c10 d123

gcov %1
gcov ecov-ebench.gcno

