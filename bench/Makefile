ifeq ($(shell expr `g++ -dumpversion` '>=' 5.), 1)
CXX=g++  -fwhole-program
endif

ifneq ($(CL),)
ifeq ($(shell expr `clang++ -dumpversion` '>=' 1.), 1)
CXX=clang++
endif
endif

CXXFLAGS = -Ofast -mavx -march=native -flto -mtune=native -std=c++17 -I.. -I../thirdparty -I/usr/local/include


ifneq ($(FOLLY),)
CXXFLAGS += -DFOLLY -lfolly
endif

ifneq ($(ABSL),)
CXXFLAGS += -DABSL=1
endif

ifneq ($(HH),)
CXXFLAGS += -DHOOD_HASH=1
endif

ifneq ($(FH),)
CXXFLAGS += -DFIB_HASH=$(FH)
endif

ifneq ($(EH),)
CXXFLAGS += -DEMH_FIBONACCI_HASH=$(EH)
endif

ifneq ($(RT),)
CXXFLAGS += -DRT=$(RT)
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
else
CXXFLAGS += -DEMH=7
endif

ifneq ($(AH),)
CXXFLAGS += -DABSL_HASH=1
#CXXFLAGS += ../thirdparty/absl/hash/internal/low_level_hash.cc
#CXXFLAGS += ../thirdparty/absl/hash/internal/city.cc
endif

ifneq ($(HL),)
CXXFLAGS += -DEMH_HIGH_LOAD=$(HL)
endif

ifneq ($(AVX2),)
CXXFLAGS += -DAVX2_EHASH=$(AVX2) -mavx2
endif


all:
	$(CXX) $(CXXFLAGS) app.cpp -o app
	$(CXX) $(CXXFLAGS) zhash_bench.cc -o zbench
	$(CXX) $(CXXFLAGS) ebench.cpp -o ebench
	$(CXX) $(CXXFLAGS) patch_bench.cpp -o pabench
	$(CXX) $(CXXFLAGS) sbench.cpp -o sb
	$(CXX) $(CXXFLAGS) martin_bench.cpp -lpthread -o mbench
	$(CXX) $(CXXFLAGS) hbench.cpp -o hbench
	$(CXX) $(CXXFLAGS) simple_bench.cpp -o simbench
	$(CXX) $(CXXFLAGS) ph_bench.cpp -o phbench
	$(CXX) $(CXXFLAGS) -DK_$(K) -DV_$(V) fbench.cpp -o fbench

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
