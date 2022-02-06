# about bench
  each cpp bnech file(collected from github) can be complied and benched.
 
# how to build (many compile option in makefile)
 //set compiler and some hash map
 ##  make CXX=clang++ ET=1            
 //set absl open and key=string,val=int
 ##  make CXX=clang++ ABSL=1 Key=2 Val=0
 //use hood hash and rand function
 ##  make HH=1   RT=3
 //compile my bench file
 ## g++ -I.. -I../thirdparty -O3 -march=native -DET=1 -DHOOD_HASH=1 -DABSL=1 ebench.cpp -o eb
 //compile martin bench file
 ## g++ -I.. -I../thirdparty -O3 -march=native -DET=1  -DABSL=1 martin_bench.cpp -o mb
 
 
