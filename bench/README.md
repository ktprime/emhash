# about bench
  each cpp bench file(collected from github) can be complied and benched.

# how to build (compile options in makefile)
set c++ compiler and absl hash/some hash map(ET=1 : martin/phmap/ska/tsl. ET=2.. more hash maps)
 ###  make CXX=clang++ ET=1 SW=1

set key=string,val=int (0-uint32_t 1-int64_t 2-std::string/(view) 4-struct)
 ###  make Key=2 Val=0

use martinus's hash and rand function (RT=1-5)
 ###  make RH=1 RT=3

set interger hash function and c++20 (FH=1-6, if FH > 100 means bad hash function ex: hash(n) = n / FH or n * FH)
 ###  make FH=2 std=20

other compile options
 ### make AVX2=1 AH=1 QB=1

 # compile ebench
 ### g++ -I.. -I../thirdparty -O3 -march=native -DET=1 -DHOOD_HASH=1 -DABSL=1 ebench.cpp -o eb

# compile martinus bench
 ### g++ -I.. -I../thirdparty -O3 -march=native -DET=1 -DABSL=1 martin_bench.cpp -o mb

# compile qc bench
 ### g++ -I.. -I../thirdparty -std=c++20 -O3 -march=native -DET=1 -DABSL=1 qbench.cpp -o qb
 
