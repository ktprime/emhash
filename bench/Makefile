CXX=g++-10
CXX=clang++
CXXFLAGS = -O3 -march=native -flto -mtune=native -fwhole-program -std=c++17 -I.. -I../thirdparty -I/usr/local/include
#CXXFLAGS += -DFOLLY -lfolly
#CXXFLAGS += -DABSL=1

EMH=5

all:
	$(CXX) $(CXXFLAGS) -DET=1 -DTKey=1 -DTVal=1 ebench.cpp -o ebench
	$(CXX) $(CXXFLAGS) -DET=1 -DTKey=1 sbench.cpp -o sb
	$(CXX) $(CXXFLAGS) -DET=1 -DABSL -DABSL_HASH martin_bench.cpp -lpthread -o mbench
	$(CXX) $(CXXFLAGS) -DABSL hbench.cpp -o hbench
	$(CXX) $(CXXFLAGS) -DK_$(K) -DV_$(V) -DABSL fbench.cpp -o fbench
	$(CXX) $(CXXFLAGS) simple_bench.cpp -o simbench
	$(CXX) $(CXXFLAGS) -DEMH=$(EMH) ph_bench.cpp -o phbench
	$(CXX) $(CXXFLAGS)  patch_bench.cpp -o pabench

fb:
	./fbench

hb:
	./hbench

eb:
	./ebench

mb:
	./mbench

sib:
	./simbench
	
sb:
	./sb

clean:
	rm -rf ebench sb mbench hbench setbench simbench pabench phbench fbench
