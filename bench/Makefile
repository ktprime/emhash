CXX=g++-10
CXX=g++
CXXFLAGS=-O3 -march=native -flto -mtune=native -fwhole-program -std=c++17 -I.. -I../thirdparty

K=INT64
V=INT32
K_LEN=0
V_LEN=0
ISIZE=100
LEN=1234567
QR=1
RR=0.3
IFR=0.2
QFR=0.2
RFR=0.3

EMH=5

all:
#	$(CXX) $(CXXFLAGS) fbench.cpp -DK_$(K) -DV_$(V) -DK_LEN=$(K_LEN) -DV_LEN=$(V_LEN) -DINIT_SIZE=$(ISIZE) -DTEST_LEN=$(LEN) -DQUERY_RATE=$(QR) -DREMOVE_RATE=$(RR) -DINSERT_FAILURE_RATE=$(IFR) -DQUERY_FAILURE_RATE=$(QFR) -DREMOVE_FAILURE_RATE=$(RFR) -c
	$(CXX) $(CXXFLAGS) -DET=1 ebench.cpp -o ebench
	$(CXX) $(CXXFLAGS) -DET=1 sbench2.cpp -o sb2
	$(CXX) $(CXXFLAGS)  hbench.cpp -o hbench
	$(CXX) $(CXXFLAGS) -DET=1 sbench.cpp -o setbench
	$(CXX) $(CXXFLAGS) -DET martin_bench.cpp -lpthread -o mbench
	$(CXX) $(CXXFLAGS) -DK_$(K) -DV_$(V) fbench.cpp -o fbench
	$(CXX) $(CXXFLAGS) simple_bench.cpp -o simbench
	$(CXX) $(CXXFLAGS) -DEMH=$(EMH) patch_bench.cpp -o pabench
	$(CXX) $(CXXFLAGS) -DEMH=$(EMH) ph_bench.cpp -o phbench

fb:
	./fbench

hb:
	./hbench

eb:
	./ebench

mb:
	./mbench

sb:
	./simbench

clean:
	rm -rf ebench mbench hbench setbench simbench pabench phbench fbench *~ *#* *.o driver-K=*-V=* my_table/*~
