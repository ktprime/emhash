#ifeq ($(shell expr `g++ -dumpversion` '>=' 1.), 1)
#CXX?=g++ -fwhole-program
#endif

#ifeq ($(shell expr `clang++ -dumpversion` '>=' 1.), 1)
#CXX?=clang++ -stdlib=libc++
#sudo apt install libc++abi-12-dev libstdc++-dev-12
#endif

CXXFLAGS =-O3 -DNDEBUG=1 -Wextra -Wno-unused-parameter -march=native -I../include -I../thirdparty
#-march=armv8.6-a -fpermissive
#-I/usr/local/include

ifneq ($(SW),)
CXXFLAGS += -DABSL_HMAP=1
endif

ifneq ($(BF),)
CXXFLAGS += -DHAVE_BOOST=1
CXXFLAGS += -DHAVE_INDIVI=1
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
CXXFLAGS += -DTTKey=$(Key)
endif

ifneq ($(Val),)
CXXFLAGS += -DTTVal=$(Val)
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

ifneq ($(std),)
CXXFLAGS += -std=c++$(std)
else
CXXFLAGS += -std=c++23
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

CXXFLAGS += -flto=auto  -Wall # -static -static-libstdc++?

# Core benchmarks: ebench/mbench (full suites), cbench (scenario bench),
# sb (type matrix), bi/bs (Peter Dimov micro-benchmarks, int/string keys)
all:
	$(CXX) $(CXXFLAGS) -fno-rtti -fpermissive ebench.cpp -o ebench
	$(CXX) $(CXXFLAGS) martin_bench.cpp -o mbench
	$(CXX) $(CXXFLAGS) comprehensive_bench.cpp -o cbench
	$(CXX) $(CXXFLAGS) -fno-rtti sbench.cpp -o sb
	$(CXX) $(CXXFLAGS) -fno-rtti buint64.cpp -o bi
	$(CXX) $(CXXFLAGS) -fno-rtti bstring.cpp -o bs

eb:
	./ebench
mb:
	./mbench
cb:
	./cbench

cbench:
	$(CXX) $(CXXFLAGS) comprehensive_bench.cpp -o cbench

clean:
	rm -rf ebench mbench cbench sb bi bs
