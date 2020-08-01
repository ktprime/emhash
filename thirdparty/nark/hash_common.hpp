#ifndef __nark_fast_hash_common__
#define __nark_fast_hash_common__

#include <nark/config.hpp>
#include <nark/node_layout.hpp>
#include <nark/stdtypes.hpp>
#include <nark/bits_rotate.hpp>
#include <algorithm>

// HSM_ : Hash String Map
#define HSM_SANITY assert

namespace nark {

template<class UintHash, class UintVal>
inline UintHash nark_warn_unused_result
FaboHashCombine(UintHash h0, UintVal val) {
	BOOST_STATIC_ASSERT(sizeof(UintVal) <= sizeof(UintHash));
	return BitsRotateLeft(h0, 5) + val;
}

inline static size_t
__hsm_stl_next_prime(size_t __n)
{
	static const size_t primes[] =
	{
		5,11,19,37,   53ul,         97ul,         193ul,       389ul,
		769ul,        1543ul,       3079ul,       6151ul,      12289ul,
		24593ul,      49157ul,      98317ul,      196613ul,    393241ul,
		786433ul,     1572869ul,    3145739ul,    6291469ul,   12582917ul,
		25165843ul,   50331653ul,   100663319ul,  201326611ul, 402653189ul,
		805306457ul,  1610612741ul, 3221225473ul, 4294967291ul,
#ifndef NARK_WORD_BITS
	#error "NARK_WORD_BITS is not defined"
#endif
#if	NARK_WORD_BITS == 64
      /* 30    */ (size_t)8589934583ull,
      /* 31    */ (size_t)17179869143ull,
      /* 32    */ (size_t)34359738337ull,
      /* 33    */ (size_t)68719476731ull,
      /* 34    */ (size_t)137438953447ull,
      /* 35    */ (size_t)274877906899ull,
      /* 36    */ (size_t)549755813881ull,
      /* 37    */ (size_t)1099511627689ull,
      /* 38    */ (size_t)2199023255531ull,
      /* 39    */ (size_t)4398046511093ull,
      /* 40    */ (size_t)8796093022151ull,
      /* 41    */ (size_t)17592186044399ull,
      /* 42    */ (size_t)35184372088777ull,
      /* 43    */ (size_t)70368744177643ull,
      /* 44    */ (size_t)140737488355213ull,
      /* 45    */ (size_t)281474976710597ull,
      /* 46    */ (size_t)562949953421231ull,
      /* 47    */ (size_t)1125899906842597ull,
      /* 48    */ (size_t)2251799813685119ull,
      /* 49    */ (size_t)4503599627370449ull,
      /* 50    */ (size_t)9007199254740881ull,
      /* 51    */ (size_t)18014398509481951ull,
      /* 52    */ (size_t)36028797018963913ull,
      /* 53    */ (size_t)72057594037927931ull,
      /* 54    */ (size_t)144115188075855859ull,
      /* 55    */ (size_t)288230376151711717ull,
      /* 56    */ (size_t)576460752303423433ull,
      /* 57    */ (size_t)1152921504606846883ull,
      /* 58    */ (size_t)2305843009213693951ull,
      /* 59    */ (size_t)4611686018427387847ull,
      /* 60    */ (size_t)9223372036854775783ull,
      /* 61    */ (size_t)18446744073709551557ull,
#endif // NARK_WORD_BITS == 64
	};
	const size_t* __first = primes;
	const size_t* __last = primes + sizeof(primes)/sizeof(primes[0]);
	const size_t* pos = std::lower_bound(__first, __last, __n);
	return pos == __last ? __last[-1] : *pos;
}

inline size_t __hsm_align_pow2(size_t x) {
	assert(x > 0);
	size_t p = 0, y = x;
	while (y)
		y >>= 1, ++p;
	if ((size_t(1) << (p-1)) == x)
		return x;
	else
		return size_t(1) << p;
}

namespace hash_common {

	template<bool Test, class Then, class Else>
	struct IF { typedef Then type; };
	template<class Then, class Else>
	struct IF<false, Then, Else> { typedef Else type; };

	template<int Bits> struct AllOne {
		typedef typename IF<(Bits <= 32), uint32_t, uint64_t>::type Uint;
		static const Uint value = (Uint(1) << Bits) - 1;
	};
	template<> struct AllOne<32> {
		typedef uint32_t Uint;
		static const Uint value = Uint(-1);
	};
	template<> struct AllOne<64> {
		typedef uint64_t Uint;
		static const Uint value = Uint(-1);
	};
} // hash_common

template<class Uint, int Bits = sizeof(Uint) * 8>
struct dummy_bucket {
	typedef Uint link_t;
	static const Uint tail = nark::hash_common::AllOne<Bits>::value;
	static const Uint delmark = tail-1;
	static const Uint maxlink = tail-2;
};
template<class Uint, int Bits> const Uint dummy_bucket<Uint, Bits>::tail;
template<class Uint, int Bits> const Uint dummy_bucket<Uint, Bits>::delmark;
template<class Uint, int Bits> const Uint dummy_bucket<Uint, Bits>::maxlink;

template<class Key, class Hash, class Equal>
struct hash_and_equal : private Hash, private Equal {
	size_t hash(const Key& x) const { return Hash::operator()(x); }
	bool  equal(const Key& x, const Key& y) const {
	   	return Equal::operator()(x, y);
   	}

	template<class OtherKey>
	size_t hash(const OtherKey& x) const { return Hash::operator()(x); }

	template<class Key1, class Key2>
	bool  equal(const Key1& x, const Key2& y) const {
	   	return Equal::operator()(x, y);
   	}

	hash_and_equal() {}
	hash_and_equal(const Hash& h, const Equal& eq) : Hash(h), Equal(eq) {}
};

struct HSM_DefaultDeleter { // delete NULL is OK
	template<class T> void operator()(T* p) const { delete p; }
};

template<class HashMap, class ValuePtr, class Deleter>
class nark_ptr_hash_map : private Deleter {
	HashMap map;
//	typedef void (type_safe_bool*)(nark_ptr_hash_map**, Deleter**);
//	static void type_safe_bool_true(nark_ptr_hash_map**, Deleter**) {}
public:
	typedef typename HashMap::  key_type   key_type;
	typedef ValuePtr value_type;
//	BOOST_STATIC_ASSERT(boost::mpl::is_pointer<value_type>);

	~nark_ptr_hash_map() { del_all(); }

	value_type operator[](const key_type& key) const {
		size_t idx = map.find_i(key);
		return idx == map.end_i() ? NULL : map.val(idx);
	}

	bool is_null(const key_type& key) const { return (*this)[key] == NULL; }

	void replace(const key_type& key, value_type pval) {
		std::pair<size_t, bool> ib = map.insert_i(key, pval);
		if (!ib.second) {
			value_type& old = map.val(ib.first);
			if (old) static_cast<Deleter&>(*this)(old);
			old = pval;
		}
	}
	std::pair<value_type*, bool> insert(const key_type& key, value_type pval) {
		std::pair<size_t, bool> ib = map.insert_i(key, pval);
		return std::make_pair(&map.val(ib.first), ib.second);
	}
	void clear() {
		del_all();
		map.clear();
	}
	void erase_all() {
		del_all();
	   	map.erase_all();
	}
	size_t erase(const key_type& key) {
		size_t idx = map.find_i(key);
		if (map.end_i() == idx) {
			return 0;
		} else {
			value_type& pval = map.val(idx);
			if (pval) {
				static_cast<Deleter&>(*this)(pval);
				pval = NULL;
			}
			map.erase_i(idx);
			return 1;
		}
	}
	size_t size() const { return map.size(); }
	bool  empty() const { return map.empty(); }

	template<class OP>
	void for_each(OP op) const { map.template for_each<OP>(op); }
	template<class OP>
	void for_each(OP op)       { map.template for_each<OP>(op); }

	HashMap& get_map() { return map; }
	const HashMap& get_map() const { return map; }

private:
	void del_all() {
		if (map.delcnt() == 0) {
			for (size_t i = 0; i < map.end_i(); ++i) {
				value_type& p = map.val(i);
				if (p) { static_cast<Deleter&>(*this)(p); p = NULL; }
			}
		} else {
			for (size_t i = map.beg_i(); i < map.end_i(); i = map.next_i(i)) {
				value_type& p = map.val(i);
				if (p) { static_cast<Deleter&>(*this)(p); p = NULL; }
			}
		}
	}
};

} // namespace nark


#endif // __nark_fast_hash_common__

