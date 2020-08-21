#include <bitset>
#include <numeric>
#include <sstream>

#include <random>
#include <map>
#include <ctime>
#include <cassert>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <array>

//#define NOMINMAX
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
# include <windows.h>
#else
#include <sched.h>
#include <pthread.h>
//#include <cpuid.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#endif

#include "thirdparty/wyhash.h"
using namespace std;

//#define EMH_STATIS 1
//#define ET            1
//#define HOOD_HASH     1
//#define EMH_HIGH_LOAD 123456

#include "hash_table2.hpp"
#include "hash_table3.hpp"
#include "hash_table4.hpp"
#include "hash_table6.hpp"
#include "hash_table7.hpp"
#include "hash_table5.hpp"
#include "hash_set4.hpp"

#if __cplusplus >= 201103L || _MSC_VER > 1600
#include "tsl/robin_map.h"         //https://github.com/tessil/robin-map
#include "tsl/hopscotch_map.h"     //https://github.com/tessil/hopscotch-map
#include "martin/robin_hood.h"     //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h

//#include "emilib/hash_emilib32.hpp"
//#include "hrd/hash_set7.h"         //https://github.com/tessil/robin-map

#include "ska/flat_hash_map.hpp"   //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
#include "ska/bytell_hash_map.hpp" //https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#include "phmap/phmap.h"           //https://github.com/tessil/robin-map
#endif

static int RND = time(0);


std::unordered_map<std::string, std::string> show_name =
{
#if EM3
	//    {"emhash2", "emhash2"},
	{"emhash3", "emhash3"},
	{"emhash4", "emhash4"},
#endif

	{"emhash5", "emhash5"},
	{"emhash6", "emhash6"},
	{"emhash7", "emhash7"},
//	{"emilib3", "emilib3"},

#if ET
	{"phmap", "phmap flat"},
	{"robin_hood", "martin flat"},
	//    {"hrd7", "hrd7map"},

#if ET > 1
	{"robin_map", "tessil robin"},
	{"ska", "skarupk flat"},
#endif
#endif
};

static const char* find(const std::string& map_name)
{
	for (const auto& kv : show_name)
	{
		if (map_name.find(kv.first) < 10)
		{
			return kv.second.c_str();
		}
	}

	return "";
}

// this is probably the fastest high quality 64bit random number generator that exists.
// Implements Small Fast Counting v4 RNG from PractRand.
class sfc64 {
	public:
		using result_type = uint64_t;

		// no copy ctors so we don't accidentally get the same random again
		sfc64(sfc64 const&) = delete;
		sfc64& operator=(sfc64 const&) = delete;

		sfc64(sfc64&&) = default;
		sfc64& operator=(sfc64&&) = default;

		sfc64(std::array<uint64_t, 4> const& state)
			: m_a(state[0])
			  , m_b(state[1])
			  , m_c(state[2])
			  , m_counter(state[3]) {}

		static constexpr uint64_t(min)() {
			return (std::numeric_limits<uint64_t>::min)();
		}
		static constexpr uint64_t(max)() {
			return (std::numeric_limits<uint64_t>::max)();
		}

		sfc64()
			: sfc64(UINT64_C(0x853c49e6748fea9b)) {}

		sfc64(uint64_t seed)
			: m_a(seed), m_b(seed), m_c(seed), m_counter(1) {
				for (int i = 0; i < 12; ++i) {
					operator()();
				}
			}

		void seed() {
			*this = sfc64{std::random_device{}()};
		}

		uint64_t operator()() noexcept {
			auto const tmp = m_a + m_b + m_counter++;
			m_a = m_b ^ (m_b >> right_shift);
			m_b = m_c + (m_c << left_shift);
			m_c = rotl(m_c, rotation) + tmp;
			return tmp;
		}

		// this is a bit biased, but for our use case that's not important.
		uint64_t operator()(uint64_t boundExcluded) noexcept {
#ifdef __SIZEOF_INT128__
			return static_cast<uint64_t>((static_cast<unsigned __int128>(operator()()) * static_cast<unsigned __int128>(boundExcluded)) >> 64u);
#elif _MSC_VER
			uint64_t high;
			uint64_t a = operator()();
			_umul128(a, boundExcluded, &high);
			return high;
#endif
		}

		std::array<uint64_t, 4> state() const {
			return {m_a, m_b, m_c, m_counter};
		}

		void state(std::array<uint64_t, 4> const& s) {
			m_a = s[0];
			m_b = s[1];
			m_c = s[2];
			m_counter = s[3];
		}

	private:
	template <typename T>
			T rotl(T const x, int k) {
				return (x << k) | (x >> (8 * sizeof(T) - k));
			}

		static constexpr int rotation = 24;
		static constexpr int right_shift = 11;
		static constexpr int left_shift = 3;
		uint64_t m_a;
		uint64_t m_b;
		uint64_t m_c;
		uint64_t m_counter;
};

template<class RandomIt>
void myrandom_shuffle(RandomIt first, RandomIt last)
{
	std::random_device rd;
	std::mt19937 g(rd());
	typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;
	typedef std::uniform_int_distribution<diff_t> distr_t;
	typedef typename distr_t::param_type param_t;

	distr_t D;
	diff_t n = last - first;
	for (diff_t i = n-1; i > 0; --i) {
		using std::swap;
		swap(first[i], first[D(g, param_t(0, i))]);
	}
}

static int64_t now2ms()
{
#if _WIN32
#if 0
	FILETIME ptime[4] = { 0 };
	GetThreadTimes(GetCurrentThread(), &ptime[0], &ptime[1], &ptime[2], &ptime[3]);
	return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10000;
#elif 0
	return GetTickCount64() / 1000;
#else
	static LARGE_INTEGER freq = {0};
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);

	LARGE_INTEGER nowus;
	QueryPerformanceCounter(&nowus);
	return (nowus.QuadPart * 1000) / (freq.QuadPart);
#endif
#elif __linux__ || __unix__
	struct rusage rup;
	getrusage(RUSAGE_SELF, &rup);
	uint64_t sec = rup.ru_utime.tv_sec + rup.ru_stime.tv_sec;
	uint64_t usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
	return sec * 1000 + usec / 1000;
#else
	auto tp = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
#endif
}

template<class MAP> void bench_insert(MAP& map)
{
	auto map_name = find(typeid(MAP).name());
	if (map_name == "")
		return;
	printf("%s map = %s\n", __FUNCTION__, map_name);

	//    map.max_load_factor(7.0 / 8);
	auto nowms = now2ms();
	sfc64 rng(RND);
	{
		{
			auto ts = now2ms();
			for (size_t n = 0; n < 100'000'000; ++n) {
				map[static_cast<int>(rng())];
			}
			printf("    insert 100M int %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
		}

		{
			auto ts = now2ms();
			map.clear();
			printf("    clear 100M int %.4lf\n", ((int)(now2ms() - ts)) / 1000.0);
		}

		{
			auto ts = now2ms();
			for (size_t n = 0; n < 100'000'000; ++n) {
				map[static_cast<int>(rng())];
			}
			printf("    reinsert 100M int %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
		}

		{
			auto ts = now2ms();
			for (size_t n = 0; n < 100'000'000; ++n) {
				map.erase(static_cast<int>(rng()));
			}
			printf("    remove 100M int %.4lf sec, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, (int)map.size());
		}
	}

	printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
}

template <typename T>
struct as_bits_t {
	T value;
};

template <typename T>
as_bits_t<T> as_bits(T value) {
	return as_bits_t<T>{value};
}

template <typename T>
std::ostream& operator<<(std::ostream& os, as_bits_t<T> const& t) {
	os << std::bitset<sizeof(t.value) * 8>(t.value);
	return os;
}

template<class MAP> void bench_randomInsertErase(MAP& map)
{
	// random bits to set for the mask
	std::vector<int> bits(64);
	std::iota(bits.begin(), bits.end(), 0);
	sfc64 rng(999);

#if 0
	for (auto &v : bits) v = rng();
#else
	std::shuffle(bits.begin(), bits.end(), rng);
#endif

	uint64_t bitMask = 0;
	auto bitsIt = bits.begin();

	size_t const expectedFinalSizes[] = {7, 127, 2084, 32722, 524149, 8367491};
	size_t const max_n = 50'000'000;

	//    map.max_load_factor(7.0 / 8);
	auto map_name = find(typeid(MAP).name());
	if (map_name == "")
		return;
	printf("%s map = %s\n", __FUNCTION__, map_name);

	auto nowms = now2ms();
	for (int i = 0; i < 6; ++i) {
		// each iteration, set 4 new random bits.
		for (int b = 0; b < 4; ++b) {
			bitMask |= UINT64_C(1) << *bitsIt++;
		}

		// std::cout << (i + 1) << ". " << as_bits(bitMask) << std::endl;

		auto ts = now2ms();
		// benchmark randomly inserting & erasing
		for (size_t i = 0; i < max_n; ++i) {
			map.emplace(rng() & bitMask, i);
			map.erase(rng() & bitMask);
		}

		printf("    %02ld bits %2zd M cycles time use %.4lf sec map %d size\n",
				std::bitset<64>(bitMask).count(), (max_n / 1000'000), ((int)(now2ms() - ts)) / 1000.0, (int)map.size());

#ifndef _MSC_VER
		//assert(RND != 123 || map.size() == expectedFinalSizes[i]);
#endif
	}
	printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
}

template<class MAP> void bench_randomDistinct2(MAP& map)
{
	auto map_name = find(typeid(MAP).name());
	if (map_name == "")
		return ;
	printf("%s map = %s\n", __FUNCTION__, map_name);

	constexpr size_t const n = 50'000'000;

	auto nowms = now2ms();
	sfc64 rng(RND);

	//    map.max_load_factor(7.0 / 8);
	int checksum;
	{
		auto ts = now2ms();
		checksum = 0;
		size_t const max_rng = n / 20;
		for (size_t i = 0; i < n; ++i) {
			checksum += ++map[static_cast<int>(rng(max_rng))];
		}
		printf("     05%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
		assert(RND != 123 || 549985352 == checksum);
	}

	{
		map.clear();
		auto ts = now2ms();
		checksum = 0;
		size_t const max_rng = n / 4;
		for (size_t i = 0; i < n; ++i) {
			checksum += ++map[static_cast<int>(rng(max_rng))];
		}
		printf("     25%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
		assert(RND != 123 || 149979034 == checksum);
	}

	{
		map.clear();
		auto ts = now2ms();
		size_t const max_rng = n / 2;
		for (size_t i = 0; i < n; ++i) {
			checksum += ++map[static_cast<int>(rng(max_rng))];
		}
		printf("     50%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
		assert(RND != 123 || 249981806 == checksum);
	}

	{
		map.clear();
		auto ts = now2ms();
		checksum = 0;
		for (size_t i = 0; i < n; ++i) {
			checksum += ++map[static_cast<int>(rng())];
		}
		printf("    100%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
		assert(RND != 123 || 50291811 == checksum);
	}
	//#endif

	printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
}

template<class MAP>
size_t runRandomString(size_t max_n, size_t string_length, uint32_t bitMask )
{
	//printf("%s map = %s\n", __FUNCTION__, typeid(MAP).name());
	sfc64 rng(RND);

	// time measured part
	size_t verifier = 0;
	std::stringstream ss;
	ss << string_length << " bytes" << std::dec;

	std::string str(string_length, 'x');
	// point to the back of the string (32bit aligned), so comparison takes a while
	auto const idx32 = (string_length / 4) - 1;
	auto const strData32 = reinterpret_cast<uint32_t*>(&str[0]) + idx32;

	MAP map;
	auto ts = now2ms();
	for (size_t i = 0; i < max_n; ++i) {
		*strData32 = rng() & bitMask;

		// create an entry.
		map[str] = 0;

		*strData32 = rng() & bitMask;
		auto it = map.find(str);
		if (it != map.end()) {
			++verifier;
			map.erase(it);
		}
	}

	printf("    %016x time = %.2lf, loadf = %.2lf %d\n", bitMask, ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
	return verifier;
}

template<class MAP>
uint64_t randomFindInternalString(size_t numRandom, size_t const length, size_t numInserts, size_t numFindsPerInsert)
{
	size_t constexpr NumTotal = 4;
	size_t const numSequential = NumTotal - numRandom;

	size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

	std::stringstream ss;
	ss << (numSequential * 100 / NumTotal) << "% " << length << " byte";
	auto title = ss.str();

	sfc64 rng(RND);

	size_t num_found = 0;

	std::array<bool, NumTotal> insertRandom = {false};
	for (size_t i = 0; i < numRandom; ++i) {
		insertRandom[i] = true;
	}

	sfc64 anotherUnrelatedRng(987654321);
	auto const anotherUnrelatedRngInitialState = anotherUnrelatedRng.state();
	sfc64 findRng(anotherUnrelatedRngInitialState);

	std::string str(length, 'y');
	// point to the back of the string (32bit aligned), so comparison takes a while
	auto const idx32 = (length / 4) - 1;
	auto const strData32 = reinterpret_cast<uint32_t*>(&str[0]) + idx32;

	auto ts = now2ms();
	MAP map;
	{
		size_t i = 0;
		size_t findCount = 0;

		do {

			// insert NumTotal entries: some random, some sequential.
			std::shuffle(insertRandom.begin(), insertRandom.end(), rng);
			for (bool isRandomToInsert : insertRandom) {
				auto val = anotherUnrelatedRng();
				if (isRandomToInsert) {
					*strData32 = rng();
				} else {
					*strData32 = val;
				}
				map[str] = static_cast<size_t>(1);
				++i;
			}

			// the actual benchmark code which sohould be as fast as possible
			for (size_t j = 0; j < numFindsPerIter; ++j) {
				if (++findCount > i) {
					findCount = 0;
					findRng.state(anotherUnrelatedRngInitialState);
				}
				*strData32 = findRng();
				auto it = map.find(str);
				if (it != map.end()) {
					num_found += it->second;
				}
			}
		} while (i < numInserts);
	}

	printf("    %s success time = %.2lf s %d loadf = %.2lf\n", title.c_str(), ((int)(now2ms() - ts)) / 1000.0, (int)num_found, map.load_factor());
	return num_found;
}

template<class MAP>
void bench_randomFindString(MAP& map)
{
	auto map_name = find(typeid(MAP).name());
	if (map_name == "")
		return ;
	printf("%s map = %s\n", __FUNCTION__, map_name);

	static constexpr size_t numInserts = 100'000;
	static constexpr size_t numFindsPerInsert = 1000;
	auto nowms = now2ms();

	randomFindInternalString<MAP>(4, 100, numInserts, numFindsPerInsert);
	randomFindInternalString<MAP>(3, 100, numInserts, numFindsPerInsert);
	randomFindInternalString<MAP>(2, 100, numInserts, numFindsPerInsert);
	randomFindInternalString<MAP>(1, 100, numInserts, numFindsPerInsert);
	randomFindInternalString<MAP>(0, 100, numInserts, numFindsPerInsert);
	printf("total time = %.2lf\n\n", ((int)(now2ms() - nowms))/1000.0);
}

template<class MAP>
void bench_randomEraseString(MAP& map)
{
	auto map_name = find(typeid(MAP).name());
	if (map_name == "")
		return ;
	printf("%s map = %s\n", __FUNCTION__, map_name);

	auto nowms = now2ms();
	{ assert(RND != 123 || 9734165 == runRandomString<MAP> (20'000'000, 7, 0xfffff)); }
	{ assert(RND != 123 || 2966452 == runRandomString<MAP>(6'000'000, 1000, 0x1ffff)); }
	{ assert(RND != 123 || 9734165 == runRandomString<MAP>(20'000'000, 8, 0xfffff)); }
	{ assert(RND != 123 || 9734165 == runRandomString<MAP>(20'000'000, 13, 0xfffff)); }
	{ assert(RND != 123 || 5867228 == runRandomString<MAP>(12'000'000, 100, 0x7ffff)); }
	printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
}

template<class MAP>
uint64_t randomFindInternal(size_t numRandom, uint64_t bitMask, size_t numInserts, size_t numFindsPerInsert) {
	size_t constexpr NumTotal = 4;
	size_t const numSequential = NumTotal - numRandom;

	size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

	sfc64 rng(RND);

	size_t num_found = 0;
	MAP map;
	//map.max_load_factor(7.0 / 8);
	std::array<bool, NumTotal> insertRandom = {false};
	for (size_t i = 0; i < numRandom; ++i) {
		insertRandom[i] = true;
	}

	sfc64 anotherUnrelatedRng(987654321);
	auto const anotherUnrelatedRngInitialState = anotherUnrelatedRng.state();
	sfc64 findRng(anotherUnrelatedRngInitialState);
	auto ts = now2ms();

	{
		size_t i = 0;
		size_t findCount = 0;

		//bench.beginMeasure(title.c_str());
		do {
			// insert NumTotal entries: some random, some sequential.
			std::shuffle(insertRandom.begin(), insertRandom.end(), rng);
			for (bool isRandomToInsert : insertRandom) {
				auto val = anotherUnrelatedRng();
				if (isRandomToInsert) {
					map[rng() & bitMask] = static_cast<size_t>(1);
				} else {
					map[val & bitMask] = static_cast<size_t>(1);
				}
				++i;
			}

			// the actual benchmark code which sohould be as fast as possible
			for (size_t j = 0; j < numFindsPerIter; ++j) {
				if (++findCount > i) {
					findCount = 0;
					findRng.state(anotherUnrelatedRngInitialState);
				}
				auto it = map.find(findRng() & bitMask);
				if (it != map.end()) {
					num_found += it->second;
				}
			}
		} while (i < numInserts);
	}

	printf("    %3lu%% %016lx success time = %.2lf s, %8d loadf = %.2lf\n", numSequential * 100 / NumTotal, bitMask, ((int)(now2ms() - ts)) / 1000.0, (int)num_found, map.load_factor());
	return num_found;
}

template<class MAP>
void bench_IterateIntegers(MAP& map)
{
	auto map_name = find(typeid(MAP).name());
	if (map_name == "")
		return ;
	printf("%s map = %s\n", __FUNCTION__, map_name);

	sfc64 rng(RND);

	size_t const num_iters = 50000;
	uint64_t result = 0;
	//Map<uint64_t, uint64_t> map;

	auto ts = now2ms();
	for (size_t n = 0; n < num_iters; ++n) {
		map[rng()] = n;
		for (auto const& keyVal : map) {
			result += keyVal.second;
		}
	}

	auto ts1 = now2ms();
	for (size_t n = 0; n < num_iters; ++n) {
		map.erase(rng());
		for (auto const& keyVal : map) {
			result += keyVal.second;
		}
	}
	printf("    total iterate/removing time = %.2lf, %.2lf|%ld\n\n", (ts1 - ts) / 1000.0, (now2ms() - ts) /1000.0, result);
}

template<class MAP>
void bench_randomFind(MAP& bench)
{
	auto map_name = find(typeid(MAP).name());
	if (map_name == "")
		return;
	printf("\n%s map = %s\n", __FUNCTION__, map_name);

	static constexpr auto lower32bit = UINT64_C(0x00000000FFFFFFFF);
	static constexpr auto upper32bit = UINT64_C(0xFFFFFFFF00000000);
	static constexpr size_t numInserts = 2000;
	static constexpr size_t numFindsPerInsert = 500'000;

	auto ts = now2ms();

	randomFindInternal<MAP>(4, lower32bit, numInserts, numFindsPerInsert);
	randomFindInternal<MAP>(4, upper32bit, numInserts, numFindsPerInsert);

	randomFindInternal<MAP>(3, lower32bit, numInserts, numFindsPerInsert);
	randomFindInternal<MAP>(3, upper32bit, numInserts, numFindsPerInsert);

	randomFindInternal<MAP>(2, lower32bit, numInserts, numFindsPerInsert);
	randomFindInternal<MAP>(2, upper32bit, numInserts, numFindsPerInsert);

	randomFindInternal<MAP>(1, lower32bit, numInserts, numFindsPerInsert);
	randomFindInternal<MAP>(1, upper32bit, numInserts, numFindsPerInsert);

	randomFindInternal<MAP>(0, lower32bit, numInserts, numFindsPerInsert);
	randomFindInternal<MAP>(0, upper32bit, numInserts, numFindsPerInsert);

	printf("total time = %.2lf\n", ((int)(now2ms() - ts))/1000.0);
}

std::vector<size_t> integerKeys()
{
	std::mt19937_64 rd(0);

	std::vector<size_t> numbers;
	for (size_t i = 0; i < 1000000; ++i) {
		numbers.push_back(rd());
	}

	return numbers;
}

std::vector<std::string> generateKeys(const std::string& prefix, const std::vector<size_t>& integer_keys, const std::string& suffix)
{
	std::vector<std::string> results;
	results.reserve(integer_keys.size());
	for (const auto& int_key : integer_keys) {
		results.emplace_back(prefix + std::to_string(int_key) + suffix);
		// results.emplace_back(prefix + std::string((const char*)&int_key, sizeof(int_key)) + suffix);
	}
	return results;
}

template<typename HashSet, typename Keys>
double time_once(const Keys& keys)
{
	auto ns = now2ms();
	HashSet set;

	// Add keys individually to force rehashing every now and then:
	for (const auto& key : keys) {
		set.emplace(key);
	}

	return (now2ms() - ns) / 1000.0;
}

static void basic_test(int n)
{
	printf("1. add 1 - %d one by one\n", n);
	{
		{
			auto ts = now2ms();
			emhash7::HashMap<int, int> emap(n);
			for (int i = 0; i < n; i++)
				emap.insert_unique(i, i);
			printf("emap unique time = %d ms loadf = %.2lf\n", (int)(now2ms() - ts), emap.load_factor());
		}

		{
			auto ts = now2ms();
			emhash2::HashMap<int, int> emap(n);
			for (int i = 0; i < n; i++)
				emap.insert(i, i);
			printf("emap insert time = %d ms loadf = %.2lf\n", (int)(now2ms() - ts), emap.load_factor());
		}

		{
			auto ts = now2ms();
			std::unordered_map<int, int> umap(n);
			for (int i = 0; i < n; i++)
				umap.emplace(i, i);
			printf("umap insert time = %d ms loadf = %.2lf\n", (int)(now2ms() - ts), umap.load_factor());
		}

		if (1)
		{
			auto ts = now2ms();
			std::map<int, int> mmap;
			for (int i = 0; i < n; i++)
				mmap.emplace(i, i);
			printf("map time = %d ms\n", (int)(now2ms() - ts));
		}

		{
			auto ts = now2ms();
			std::vector<int> v;
			v.reserve(n);
			for (int i = 0; i < n; i++)
				v.emplace_back(i);
			printf("vec reserve time = %d ms\n", (int)(now2ms() - ts));
		}

		{
			auto ts = now2ms();
			std::vector<int> v;
			for (int i = 0; i < n; i++)
				v.emplace_back(i);
			printf("vec emplace time = %d ms\n", (int)(now2ms() - ts));
		}
		{
			auto ts = now2ms();
			std::vector<int> v(n);
			for (int i = 0; i < n; i++)
				v[i] = i;
			printf("vec resize time = %d ms\n", (int)(now2ms() - ts));
		}
		{
			auto ts = now2ms();
			std::vector<int> v;
			for (int i = 0; i < n; i++)
				v.push_back(i);
			printf("vec push time = %d ms\n", (int)(now2ms() - ts));
		}
		{
			auto ts = now2ms();
			emhash9::HashSet<int> eset(n);
			for (int i = 0; i < n; i++)
				eset.insert_unique(i);
			printf("eset time = %d ms\n", (int)(now2ms() - ts));
		}

		{
			auto ts = now2ms();
			std::unordered_set<int> uset(n);
			for (int i = 0; i < n; i++)
				uset.insert(i);
			printf("uset time = %d ms\n", (int)(now2ms() - ts));
		}

		puts("\n");
	}

	printf("2. random_shuffle 1 - %d\n", n);
	{
		std::vector <int> data(n);
		for (int i = 0; i < n; i ++)
			data[i] = i;

		myrandom_shuffle(data.begin(), data.end());
		for (int i = 0; i < n; i ++)
			data[0] += 0;

		{
			auto ts = now2ms();
			std::unordered_map<int, int> umap(n);
			for (auto v: data)
				umap.emplace(v, 0);
			printf("umap insert time = %d ms loadf = %.2lf\n", (int)(now2ms() - ts), umap.load_factor());
		}

		{
			auto ts = now2ms();
			emhash6::HashMap<int, int> emap(n);
			for (auto v: data)
				emap.insert_unique(v, 0);
			printf("emap uniqe time = %d ms loadf = %.2lf\n", (int)(now2ms() - ts), emap.load_factor());
		}

		{
			auto ts = now2ms();
			emhash7::HashMap<int, int> emap(n);
			for (auto v: data)
				emap.insert(v, 0);
			printf("emap insert time = %d ms\n", (int)(now2ms() - ts));
		}

		{
			auto ts = now2ms();
			std::vector<int> v(n);
			for (auto d: data)
				v[d] = 0;
			printf("vec    time = %d ms\n", (int)(now2ms() - ts));
		}

		{
			auto ts = now2ms();
			emhash9::HashSet<int> eset(n);
			for (auto v: data)
				eset.insert_unique(v);
			printf("eset unique time = %d ms\n", (int)(now2ms() - ts));
		}

		{
			auto ts = now2ms();
			std::unordered_set<int> eset(n);
			for (auto v: data)
				eset.insert(v);
			printf("uset insert time = %d ms\n", (int)(now2ms() - ts));
		}
		puts("\n");
	}

	puts("3. random data");
	{
		std::vector <int> data(n);
		for (int i = 0; i < n; i ++)
			data[i] = (uint32_t)(rand() * rand()) % n + rand();
		myrandom_shuffle(data.begin(), data.end());

		{
			auto ts = now2ms();
			emhash2::HashMap<int, int> emap(n);
			for (auto v: data)
				emap.insert(v, 0);
			printf("emap5 insert time = %d ms loadf = %.2lf/size %d\n", (int)(now2ms() - ts), emap.load_factor(), emap.size());
		}

		{
			auto ts = now2ms();
			emhash9::HashSet<int> eset;
			// for (auto v: data)
			//     eset.insert(v);

			eset.insert(data.begin(), data.end());
			printf("eset insert range time = %d ms loadf = %.2lf/size %zd\n", (int)(now2ms() - ts), eset.load_factor(), eset.size());
		}

		{
			auto ts = now2ms();
			std::unordered_map<int, int> umap(n);
			for (auto v: data)
				umap.emplace(v, 0);
			printf("umap insert time = %d ms loadf = %.2lf\n", (int)(now2ms() - ts), umap.load_factor());
		}

		if (0)
		{
			auto ts = now2ms();
			std::vector<int> vec(n);
			for (auto d: data)
				vec[(unsigned int)d % n] = 0;
			printf("vec time = %d ms\n", (int)(now2ms() - ts));
		}

		{
			auto ts = now2ms();
			std::unordered_set<int> uset(n);
			uset.insert(data.begin(), data.end());
			printf("uset insert time = %d ms loadf = %.2lf/size %zd\n", (int)(now2ms() - ts), uset.load_factor(), uset.size());
		}

		puts("\n");
	}
}

int main(int argc, char* argv[])
{
	puts("./test [23456mpts] n");
	srand(time(0));

	for (auto& m : show_name)
		printf("%10s %20s\n", m.first.c_str(), m.second.c_str());

#if _WIN32
	SetThreadAffinityMask(GetCurrentThread(), 0x2);
#elif __linux__
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0x2, &cpuset);
	pthread_t current_thread = pthread_self();
	pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#endif

	if (argc > 1) {
		for (char c = argv[1][0], i = 0; c != '\0'; c = argv[1][i ++ ]) {
			if (c >= '2' && c < '8') {
				string map_name("emhash");
				map_name += c;
				show_name.erase(map_name);
			} else if (c == 'm')
				show_name.erase("robin_hood");
			else if (c == 'p')
				show_name.erase("phmap");
			else if (c == 't')
				show_name.erase("robin_map");
			else if (c == 's')
				show_name.erase("ska");
			else if (c == 'h')
				show_name.erase("hrd7");
			else if (c == 'e')
				show_name.erase("emilib");
		}
	}
	if (argc > 2) {
		int n = argc > 1 ? atoi(argv[2]) : 12345678;
		basic_test(n);
		return 0;
	}

	puts("test hash");
	for (auto& m : show_name)
		printf("%10s %20s\n", m.first.c_str(), m.second.c_str());

	if (1)
	{
#if 1
		typedef robin_hood::hash<uint64_t> hash_func;
#else
		typedef std::hash<uint64_t> hash_func;
#endif
#if EM3
		{ emhash2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
		{ emhash3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); /*            emap.dump_statis(); */ }
		{ emhash4::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); /*            emap.dump_statis(); */ }
#endif
		{ emhash5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
		{ emhash7::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
		{ emhash6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
#if ET
//		{ hrd7::hash_map <uint64_t, uint64_t, hash_func> hmap;  bench_IterateIntegers(hmap); }
		{ tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_IterateIntegers(rmap); }
		{ robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_IterateIntegers(martin); }
		{ ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_IterateIntegers(fmap); }
		{ phmap::flat_hash_map<uint64_t, uint64_t, hash_func> pmap;bench_IterateIntegers(pmap); }
#endif
		putchar('\n');
	}

	if (1)
	{
#ifdef HOOD_HASH
		typedef robin_hood::hash<std::string> hash_func;
#else
		typedef std::hash<std::string> hash_func;
#endif

#if EM3
		{emhash7::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
		{emhash2::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
		{emhash3::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
		{emhash4::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
#endif
		{emhash6::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
		{emhash5::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}

#if ET
//		{emilib3::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
//		{hrd7::hash_map <std::string, size_t, hash_func> hmap;   bench_randomFindString(hmap); }
		{tsl::robin_map  <std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
		{robin_hood::unordered_map <std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
		{ska::flat_hash_map<std::string, size_t, hash_func> bench;   bench_randomFindString(bench);}
		{phmap::flat_hash_map<std::string, size_t, hash_func> bench; bench_randomFindString(bench);}
#endif
		putchar('\n');
	}

	if (1)
	{
#ifdef HOOD_HASH
		typedef robin_hood::hash<std::string> hash_func;
#else
		typedef std::hash<std::string> hash_func;
#endif

#if EM3
		{emhash4::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
		{emhash2::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
		{emhash3::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
		{emhash7::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
#endif
		{emhash6::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
		{emhash5::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}

#if ET
//		{emilib3::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
//		{hrd7::hash_map <std::string, int, hash_func> hmap;   bench_randomEraseString(hmap); }
		{tsl::robin_map  <std::string, int, hash_func> bench; bench_randomEraseString(bench);}
		{robin_hood::unordered_map <std::string, int, hash_func> bench; bench_randomEraseString(bench);}
		{ska::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
		{phmap::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
#endif
	}

	if (1)
	{
#if 1
		typedef robin_hood::hash<size_t> hash_func;
#else
		typedef std::hash<size_t> hash_func;
#endif

#if ET
		{ tsl::robin_map  <size_t, size_t, hash_func> rmap;  bench_randomFind(rmap); }
		{ robin_hood::unordered_map <size_t, size_t, hash_func> martin; bench_randomFind(martin); }
		{ ska::flat_hash_map <size_t, size_t, hash_func> fmap; bench_randomFind(fmap); }
		{ phmap::flat_hash_map <size_t, size_t, hash_func> pmap; bench_randomFind(pmap); }
//		{ hrd7::hash_map <size_t, size_t, hash_func> hmap;  bench_randomFind(hmap); }
//		{ emilib3::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }

#endif
		{ emhash5::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
		{ emhash6::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
#if EM3
		{ emhash4::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
		{ emhash7::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
		{ emhash2::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
		{ emhash3::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
#endif
		putchar('\n');
	}

	if (1)
	{
#ifdef HOOD_HASH
		typedef robin_hood::hash<int> hash_func;
#else
		typedef std::hash<int> hash_func;
#endif
		{ emhash6::HashMap<int, int, hash_func> emap; bench_insert(emap); }
		{ emhash5::HashMap<int, int, hash_func> emap; bench_insert(emap); /*            emap.dump_statis(); **/ }
#if EM3
		{ emhash7::HashMap<int, int, hash_func> emap; bench_insert(emap); }
		{ emhash2::HashMap<int, int, hash_func> emap; bench_insert(emap); }
		{ emhash4::HashMap<int, int, hash_func> emap; bench_insert(emap); }
		{ emhash3::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#endif
#if ET
//		{ emilib3::HashMap<int, int, hash_func> emap; bench_insert(emap); /*            emap.dump_statis(); **/ }
//		{ hrd7::hash_map <int, int, hash_func> hmap;  bench_insert(hmap); }
		{ tsl::robin_map  <int, int, hash_func> rmap; bench_insert(rmap); }
		{ robin_hood::unordered_map <int, int, hash_func> martin; bench_insert(martin); }
		{ ska::flat_hash_map <int, int, hash_func> fmap; bench_insert(fmap); }
		{ phmap::flat_hash_map <int, int, hash_func> pmap; bench_insert(pmap); }
#endif
		putchar('\n');
	}

	if (1)
	{
#if 1
		typedef robin_hood::hash<uint64_t> hash_func;
#else
		typedef std::hash<uint64_t> hash_func;
#endif

		{ emhash5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
		{ emhash6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
#if EM3
		{ emhash7::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); /*            emap.dump_statis(); */ }
		{ emhash2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
		{ emhash3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); /*            emap.dump_statis(); */ }
		{ emhash4::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); /*            emap.dump_statis(); */ }
#endif

#if ET
//		{ emilib3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
//		{ hrd7::hash_map <size_t, size_t, hash_func> hmap; bench_randomInsertErase(hmap); }
		{ tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_randomInsertErase(rmap); }
		{ robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_randomInsertErase(martin); }
		{ ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_randomInsertErase(fmap); }
		{ phmap::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
#endif
		putchar('\n');
	}

	if (1)
	{
#if 1
		typedef robin_hood::hash<int> hash_func;
#else
		typedef std::hash<int> hash_func;
#endif
		{ emhash6::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
		{ emhash5::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#if EM3
		{ emhash2::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
		{ emhash7::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
		{ emhash4::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); /**           emap.dump_statis(); */ }
		{ emhash3::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); /**           emap.dump_statis(); */ }
#endif

#if ET
//		{ emilib3::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
//		{ hrd7::hash_map <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
		{ tsl::robin_map     <int, int, hash_func> rmap; bench_randomDistinct2(rmap); }
		{ robin_hood::unordered_map <int, int, hash_func> martin; bench_randomDistinct2(martin); }
		{ ska::flat_hash_map <int, int, hash_func> fmap; bench_randomDistinct2(fmap); }
		{ phmap::flat_hash_map <int, int, hash_func> pmap; bench_randomDistinct2(pmap); }
#endif
		putchar('\n');
	}

	return 0;
}

