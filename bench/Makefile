CXX=g++
CXXFLAGS=-O3 -march=native -fwhole-program -mavx2 -I.. -I../thirdparty


K=INT64
V=INT32
K_LEN=0
V_LEN=0
ISIZE=0
LEN=1234567
INIT_SIZE=10000
QR=1
RR=0.5
IFR=0.5
QFR=0.4
RFR=0.6

EMH=5

all:
	$(CXX) $(CXXFLAGS) fht_bench.cpp -DK_$(K) -DV_$(V) -DK_LEN=$(K_LEN) -DV_LEN=$(V_LEN) -DINIT_SIZE=$(ISIZE) -DTEST_LEN=$(LEN) -DQUERY_RATE=$(QR) -DREMOVE_RATE=$(RR) -DINSERT_FAILURE_RATE=$(IFR) -DQUERY_FAILURE_RATE=$(QFR) -DREMOVE_FAILURE_RATE=$(RFR) -c
	$(CXX) $(CXXFLAGS) fht_bench.o -o fht_bench
	$(CXX) $(CXXFLAGS) ebench.cpp  -o ebench
	$(CXX) $(CXXFLAGS) martin_bench.cpp -lpthread -o  mbench
	$(CXX) $(CXXFLAGS) hbench.cpp -o hbench
	$(CXX) $(CXXFLAGS) -DEMH=$(EMH) ph_bench.cpp -o phbench
	$(CXX) $(CXXFLAGS) -DEMH=$(EMH) patch_bench.cpp -o pabench
	$(CXX) $(CXXFLAGS) simple_bench.cpp -o simbench

fb:fht_bench.o
	./fht_bench

hb:
	./hbench

eb:
	./ebench

mb:
	./mbench

clean:
	rm -rf ebench mbench hbench simble_bench pabench phbench fht_bench *~ *#* *.o driver-K=*-V=* my_table/*~
