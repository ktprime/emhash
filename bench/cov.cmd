del -f *.gcov
del -f *.gcno
del -f *.gcna

g++ -I.. -DEMH_H2=1 -I../thirdparty -DGCOV=1 -DET=0 -fprofile-arcs -ftest-coverage -mavx -march=native -ggdb %1 -o ecov.exe
ecov c10 d57
gcov %1
gcov ecov-ebench.gcno
gvim emilib.hpp.gcov

