ifeq ($(shell expr `g++ -dumpversion` '>=' 5.), 1)
CXX=g++  -fwhole-program
endif

ifeq ($(shell expr `clang++ -dumpversion` '>=' 1.), 1)
CXX=clang++
endif

CXXFLAGS = -Ofast -march=native -flto -mtune=native -std=c++17 -I.. -I../thirdparty -I/usr/local/include

ifneq ($(FOLLY),)
CXXFLAGS += -DFOLLY -lfolly
endif

ifneq ($(ABSL),)
CXXFLAGS += -DABSL=1
endif

ifneq ($(HH),)
CXXFLAGS += -DHOOD_HASH=1
endif

ifneq ($(Key),)
CXXFLAGS += -DTKey=$(Key)
endif

ifneq ($(Val),)
CXXFLAGS += -DTVal=$(Val)
endif

ifneq ($(ET),)
CXXFLAGS += -DET=$(ET)
endif

ifneq ($(EA),)
CXXFLAGS += -DEA=$(EA)
endif

ifneq ($(EMH),)
CXXFLAGS += -DEMH=$(EMH)
endif

ifneq ($(AH),)
CXXFLAGS += -DABSL_HASH=1 ../thirdparty/absl/hash/internal/wyhash.cc
endif

all:
	$(CXX) $(CXXFLAGS) ebench.cpp -o ebench
	$(CXX) $(CXXFLAGS) sbench.cpp -o sb
	$(CXX) $(CXXFLAGS) patch_bench.cpp -o pabench
	$(CXX) $(CXXFLAGS) martin_bench.cpp -lpthread -o mbench
	$(CXX) $(CXXFLAGS) hbench.cpp -o hbench
	$(CXX) $(CXXFLAGS) -DK_$(K) -DV_$(V) fbench.cpp -o fbench
	$(CXX) $(CXXFLAGS) simple_bench.cpp -o simbench
	$(CXX) $(CXXFLAGS) ph_bench.cpp -o phbench

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
