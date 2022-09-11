ifeq ($(shell expr `g++ -dumpversion` '>=' 5.), 1)
CXX?=g++ -fwhole-program -flto
endif

ifeq ($(shell expr `clang++ -dumpversion` '>=' 1.), 1)
CXX?=clang++ -stdlib=libc++ -flto
#sudo apt install libc++abi-12-dev libstdc++-dev-12
endif

CXXFLAGS = -O3 -march=native -mtune=native -I.. -I../thirdparty -DNDEBUG=1
#-I/usr/local/include

ifneq ($(FOLLY),)
CXXFLAGS += -DFOLLY -lfolly
endif

ifneq ($(SW),)
CXXFLAGS += -DABSL=1
endif

ifneq ($(MH),)
CXXFLAGS += -DHOOD_HASH=1
endif

ifneq ($(FH),)
CXXFLAGS += -DFIB_HASH=$(FH)
endif

ifneq ($(EH),)
CXXFLAGS += -DEMH_INT_HASH=$(EH)
endif

ifneq ($(EMH),)
CXXFLAGS += -DEMH=$(EMH)
else
#CXXFLAGS += -DEMH=7
endif

ifneq ($(AH),)
CXXFLAGS += -DABSL_HASH=1
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

ifneq ($(HL),)
CXXFLAGS += -DEMH_HIGH_LOAD=$(HL)
endif

ifneq ($(AVX2),)
CXXFLAGS += -DAVX2_EHASH=$(AVX2) -mavx2
endif

ifneq ($(H2),)
CXXFLAGS += -DEMH_H2=1
endif

ifneq ($(MAP),)
CXXFLAGS += -DSMAP=1
endif

ifneq ($(QC),)
CXXFLAGS += -DQC_HASH=$(QC) -std=c++20
endif

ifneq ($(CK),)
CXXFLAGS += -std=c++20 -I../thirdparty/ck -I../thirdparty/ck/base
endif

ifneq ($(std),)
CXXFLAGS += -std=c++$(std)
else
CXXFLAGS += -std=c++17
endif

ifneq ($(check),)
CXXFLAGS += -fsanitize=undefined -g
endif

ifneq ($(addr),)
CXXFLAGS += -fsanitize=address
endif

ifneq ($(HIT),)
CXXFLAGS += -DEMH_FIND_HIT=1
endif

ifneq ($(MAR),)
CXXFLAGS += -D$(MAR)
endif

ifneq ($(MF),)
CXXFLAGS += $(MF)
endif

all:
ifneq ($(QB),)
	$(CXX) $(CXXFLAGS) -I. qbench.cpp -o qbench
endif

	$(CXX) $(CXXFLAGS) sbench.cpp -o sb
	$(CXX) $(CXXFLAGS) ebench.cpp -o ebench
	$(CXX) $(CXXFLAGS) martinus_bench.cpp -o mbench
	$(CXX) $(CXXFLAGS) btest.cpp -o btest
	$(CXX) $(CXXFLAGS) tbench.cpp -o tbench
	$(CXX) $(CXXFLAGS) app.cpp -o app
	$(CXX) $(CXXFLAGS) zhash_bench.cc -o zbench
	$(CXX) $(CXXFLAGS) hbench.cpp -o hbench
	$(CXX) $(CXXFLAGS) simple_bench.cpp -o simbench
	$(CXX) $(CXXFLAGS) fbench.cpp -o fbench
ifneq ($(EMH),)
	$(CXX) $(CXXFLAGS) -DEMH_HASH2=1 template.cc -o template
	$(CXX) $(CXXFLAGS) patch_bench.cpp -o pabench
	$(CXX) $(CXXFLAGS) ph_bench.cpp -o phbench
endif

eb:
	./ebench
mb:
	./mbench
qb:
	./qbench

clean:
	rm -rf ebench sb mbench hbench simbench pabench phbench fbench app zbench qbench

