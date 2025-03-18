#ifeq ($(shell expr `g++ -dumpversion` '>=' 1.), 1)
#CXX?=g++ -fwhole-program
#endif

#ifeq ($(shell expr `clang++ -dumpversion` '>=' 1.), 1)
#CXX?=clang++ -stdlib=libc++
#sudo apt install libc++abi-12-dev libstdc++-dev-12
#endif

CXXFLAGS =-O3 -DNDEBUG=1 -march=native -I.. -I../thirdparty
#-march=armv8.6-a -fpermissive
#-I/usr/local/include

ifneq ($(FOLLY),)
CXXFLAGS += -DFOLLY_F14 -lfolly
endif

ifneq ($(SW),)
CXXFLAGS += -DABSL_HMAP=1
endif

ifneq ($(BF),)
CXXFLAGS += -DHAVE_BOOST=1
endif

ifneq ($(MH),)
CXXFLAGS += -DHOOD_HASH=1
endif

ifneq ($(FH),)
CXXFLAGS += -DFIB_HASH=$(FH)
endif

ifneq ($(WH),)
CXXFLAGS += -DWY_HASH=1
endif

ifneq ($(EH),)
CXXFLAGS += -DEMH_INT_HASH=$(EH)
endif

ifneq ($(EMH),)
CXXFLAGS += -DEMH=$(EMH)
else
#CXXFLAGS += -DEMH=7
endif

ifneq ($(SWH),)
CXXFLAGS += -DABSL_HASH=1
endif

ifneq ($(AH),)
CXXFLAGS += -DANKERL_HASH=1
endif

ifneq ($(SH),)
CXXFLAGS += -DSTD_HASH=1
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
CXXFLAGS += -DEMH_H2=$(H2)
endif

ifneq ($(SMAP),)
CXXFLAGS += -DSMAP=1
endif

ifneq ($(QC),)
CXXFLAGS += -DQC_HASH=$(QC) -std=c++20
endif

ifneq ($(std),)
CXXFLAGS += -std=c++$(std)
else
CXXFLAGS += -std=c++17
endif

ifneq ($(check),)
CXXFLAGS += -fsanitize=undefined -g
endif

ifneq ($(santi),)
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

ifneq ($(CK),)
CXXFLAGS += -std=c++20 -I../thirdparty/ck -I../thirdparty/ck/base -DCK_HMAP=1
endif

CXXFLAGS += -flto=auto # -static clang libc++ -static-libstdc++?

ifneq ($(PSL),)
CXXFLAGS += -DPSL= (PSL)
endif

all:
ifneq ($(QB),)
	$(CXX) $(CXXFLAGS) -std=c++20 -I. qbench.cpp -o qbench
endif

#	$(CXX) $(CXXFLAGS) -Werror=conversion -Wno-sign-conversion -Wextra -Wpedantic -Wconversion martin_bench.cpp -o mbench
	$(CXX) $(CXXFLAGS) -Werror=conversion martin_bench.cpp -o mbench
	$(CXX) $(CXXFLAGS) ebench.cpp -o ebench
	$(CXX) $(CXXFLAGS) hash_join.cpp -o join_hash
	$(CXX) $(CXXFLAGS) buint64.cpp -o bi
	$(CXX) $(CXXFLAGS) bstring.cpp -o bs
	$(CXX) $(CXXFLAGS) word_count.cpp -o wc
	$(CXX) $(CXXFLAGS) tbench.cpp -o tbench
	$(CXX) $(CXXFLAGS) sbench.cpp -o sb
	$(CXX) $(CXXFLAGS) hbench.cpp -o hbench
	$(CXX) $(CXXFLAGS) simple_bench.cpp -o simbench
	$(CXX) $(CXXFLAGS) fbench.cpp -o fbench
	$(CXX) $(CXXFLAGS) app.cpp -o app
	$(CXX) $(CXXFLAGS) zhash_bench.cc -o zbench
	$(CXX) $(CXXFLAGS) -fopenmp hash_join2.cpp -o join_hash2
ifneq ($(EMH),)
	$(CXX) $(CXXFLAGS) template.cc -o template
	$(CXX) $(CXXFLAGS) patch_bench.cpp -o pabench
	$(CXX) $(CXXFLAGS) ph_bench.cpp -o phbench
endif

eb:
	./ebench
mb:
	./mbench
qb:
	./qbench
ww:
	./wc

clean:
	rm -rf ebench sb mbench hbench simbench pabench phbench fbench app zbench qbench wc bi bs

