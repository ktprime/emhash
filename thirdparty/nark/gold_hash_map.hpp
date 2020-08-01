#ifndef __nark_gold_hash_tab__
#define __nark_gold_hash_tab__

#ifdef _MSC_VER
	#if _MSC_VER < 1600
		#include <hash_map>
		#define DEFAULT_HASH_FUNC stdext::hash
	#else
	 	#include <unordered_map>
	 	#define DEFAULT_HASH_FUNC std::hash
	#endif
#elif defined(__GNUC__) && __GNUC__ * 1000 + __GNUC_MINOR__ < 4001
	#include <ext/hash_map>
	#define DEFAULT_HASH_FUNC __gnu_cxx::hash
#elif defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103 || defined(_LIBCPP_VERSION)
	#include <unordered_map>
	#define DEFAULT_HASH_FUNC std::hash
#else
	#include <tr1/unordered_map>
	#define DEFAULT_HASH_FUNC std::tr1::hash
#endif

//#include <nark/parallel_lib.hpp>
#include "hash_common.hpp"
#include <utility> // for std::identity

#if 0
#include <boost/ref.hpp>
#include <boost/current_function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/type_traits/has_trivial_constructor.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>
#endif

namespace nark {

#if 1
template<class T>
struct nark_identity {
	typedef T value_type;
	const T& operator()(const T& x) const { return x; }
};
#else
	#define nark_identity std::identity
#endif

template<class Key, class Object, size_t Offset>
struct ExtractKeyByOffset {
	const Key& operator()(const Object& x) const {
		return *reinterpret_cast<const Key *>(
				reinterpret_cast<const char*>(&x) + Offset);
	}
	BOOST_STATIC_ASSERT(Offset >= 0);
	BOOST_STATIC_ASSERT(Offset <= sizeof(Object)-sizeof(Key));
};

template< class Key
		, class Elem = Key
		, class HashEqual = hash_and_equal<Key, DEFAULT_HASH_FUNC<Key>, std::equal_to<Key> >
		, class KeyExtractor = nark_identity<Elem>
		, class NodeLayout = node_layout<Elem, unsigned/*LinkTp*/ >
		, class HashTp = size_t
		>
class gold_hash_tab : dummy_bucket<typename NodeLayout::link_t>
{
protected:
	typedef typename NodeLayout::link_t LinkTp;
	typedef typename NodeLayout::copy_strategy CopyStrategy;

//	BOOST_STATIC_ASSERT(sizeof(LinkTp) <= sizeof(Elem));
	using dummy_bucket<LinkTp>::tail;
	using dummy_bucket<LinkTp>::delmark;
	static const intptr_t hash_cache_disabled = 8; // good if compiler aggressive optimize

	struct IsNotFree {
		bool operator()(LinkTp link_val) const { return delmark != link_val; }
	};

public:
	typedef typename NodeLayout::      iterator       fast_iterator;
	typedef typename NodeLayout::const_iterator const_fast_iterator;
	typedef std::reverse_iterator<      fast_iterator>       fast_reverse_iterator;
	typedef std::reverse_iterator<const_fast_iterator> const_fast_reverse_iterator;

#ifdef NARK_GOLD_HASH_MAP_ITERATOR_USE_FAST
	typedef       fast_iterator       iterator;
	typedef const_fast_iterator const_iterator;
	      iterator get_iter(size_t idx)       { return m_nl.begin() + idx; }
	const_iterator get_iter(size_t idx) const { return m_nl.begin() + idx; }
#else
	class iterator; friend class iterator;
	class iterator {
	public:
		typedef Elem  value_type;
		typedef gold_hash_tab* OwnerPtr;
	#define ClassIterator iterator
	#include "gold_hash_map_iterator.hpp"
	};
	class const_iterator; friend class const_iterator;
	class const_iterator {
	public:
		typedef const Elem  value_type;
		const_iterator(const iterator& y)
		  : owner(y.get_owner()), index(y.get_index()) {}
		typedef const gold_hash_tab* OwnerPtr;
	#define ClassIterator const_iterator
	#include "gold_hash_map_iterator.hpp"
	};
	      iterator get_iter(size_t idx)       { return       iterator(this, idx); }
	const_iterator get_iter(size_t idx) const { return const_iterator(this, idx); }
#endif
	typedef std::reverse_iterator<      iterator>       reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	typedef ptrdiff_t difference_type;
	typedef size_t  size_type;
	typedef Elem  value_type;
	typedef Elem& reference;

	typedef const Key key_type;

protected:
	NodeLayout m_nl;
	LinkTp* bucket;  // index to m_nl
	HashTp* pHash;

	double  load_factor;
	size_t  nBucket; // may larger than LinkTp.max if LinkTp is small than size_t
	LinkTp  nElem;
	LinkTp  maxElem;
	LinkTp  maxload;  // cached guard for calling rehash
	LinkTp  freelist_head;
	LinkTp  freelist_size;
    LinkTp  freelist_freq;
	bool    is_sorted;

	HashEqual he;
	KeyExtractor keyExtract;

private:
	void init() {
		new(&m_nl)NodeLayout();
		nElem = 0;
		maxElem = 0;
		maxload = 0;
		if (!std::is_trivial_destructor<Elem>::value || sizeof(Elem) > sizeof(intptr_t))
			pHash = NULL;
		else
			pHash = (HashTp*)(hash_cache_disabled);

		bucket = const_cast<LinkTp*>(&tail); // false start
		nBucket = 1;
		freelist_head = delmark; // don't use freelist
		freelist_size = 0;
        freelist_freq = 0;

		load_factor = 0.8;
		is_sorted = true;
	}

	void relink_impl(bool bFillHash) {
		LinkTp* pb = bucket;
		size_t  nb = nBucket;
		NodeLayout nl = m_nl;
		// (LinkTp)tail is just used for coerce tail be passed by value
		// otherwise, tail is passed by reference, this cause g++ link error
		if (nb > 1) {
			assert(&tail != pb);
			std::fill_n(pb, nb, (LinkTp)tail);
		} else {
			assert(&tail == pb);
			return;
		}
		{ // set delmark
			LinkTp i = freelist_head;
			while (i < delmark) {
				nl.link(i) = delmark;
				i = reinterpret_cast<LinkTp&>(nl.data(i));
			}
		}
		if (intptr_t(pHash) == hash_cache_disabled) {
			if (0 == freelist_size)
				for (size_t j = 0, n = nElem; j < n; ++j) {
					size_t i = he.hash(keyExtract(nl.data(j))) % nb;
					nl.link(j) = pb[i];
					pb[i] = LinkTp(j);
				}
			else
				for (size_t j = 0, n = nElem; j < n; ++j) {
					if (delmark != nl.link(j)) {
						size_t i = he.hash(keyExtract(nl.data(j))) % nb;
						nl.link(j) = pb[i];
						pb[i] = LinkTp(j);
					}
				}
		}
		else if (bFillHash) {
			HashTp* ph = pHash;
			if (0 == freelist_size)
				for (size_t j = 0, n = nElem; j < n; ++j) {
					HashTp h = HashTp(he.hash(keyExtract(nl.data(j))));
					size_t i = h % nb;
					ph[j] = h;
					nl.link(j) = pb[i];
					pb[i] = LinkTp(j);
				}
			else
				for (size_t j = 0, n = nElem; j < n; ++j) {
					if (delmark != nl.link(j)) {
						HashTp h = HashTp(he.hash(keyExtract(nl.data(j))));
						size_t i = h % nb;
						ph[j] = h;
						nl.link(j) = pb[i];
						pb[i] = LinkTp(j);
					}
				}
		}
        else {
			const HashTp* ph = pHash;
			if (0 == freelist_size)
				for (size_t j = 0, n = nElem; j < n; ++j) {
					size_t i = ph[j] % nb;
					nl.link(j) = pb[i];
					pb[i] = LinkTp(j);
				}
			else
				for (size_t j = 0, n = nElem; j < n; ++j) {
					if (delmark != nl.link(j)) {
						size_t i = ph[j] % nb;
						nl.link(j) = pb[i];
						pb[i] = LinkTp(j);
					}
				}
        }
	}

	void relink() { relink_impl(false); }
	void relink_fill() { relink_impl(true); }

	void destroy() {
		NodeLayout nl = m_nl;
		if (!nl.is_null()) {
			if (!std::is_trivial_destructor<Elem>::value) {
                if (freelist_is_empty()) {
                    for (size_t i = nElem; i > 0; --i)
                        nl.data(i-1).~Elem();
                } else {
                    for (size_t i = nElem; i > 0; --i)
                        if (delmark != nl.link(i-1))
                            nl.data(i-1).~Elem();
                }
			}
			m_nl.free();
		}
		if (intptr_t(pHash) != hash_cache_disabled && NULL != pHash)
			free(pHash);
		if (bucket && &tail != bucket)
			free(bucket);
	}

public:
	explicit gold_hash_tab(HashEqual he = HashEqual()
                         , KeyExtractor keyExtr = KeyExtractor())
	  : he(he), keyExtract(keyExtr) {
		init();
	}
	explicit gold_hash_tab(size_t cap
						 , HashEqual he = HashEqual()
						 , KeyExtractor keyExtr = KeyExtractor())
	  : he(he), keyExtract(keyExtr) {
		init();
		rehash(cap);
	}
	/// ensured not calling HashEqual and KeyExtractor
	gold_hash_tab(const gold_hash_tab& y)
	  : he(y.he), keyExtract(y.keyExtract) {
		nElem = y.nElem;
		maxElem = y.nElem;
		maxload = y.maxload;
		nBucket = y.nBucket;
		pHash = NULL;
		freelist_head = y.freelist_head;
		freelist_size = y.freelist_size;
        freelist_freq = y.freelist_freq;

		load_factor = y.load_factor;
		is_sorted = y.is_sorted;

		if (0 == nElem) { // empty
			nBucket = 1;
			maxload = 0;
			bucket  = const_cast<LinkTp*>(&tail);
			return; // not need to do deep copy
		}
		assert(y.bucket != &tail);
		m_nl.reserve(0, nElem);
		bucket = (LinkTp*)malloc(sizeof(LinkTp) * y.nBucket);
		if (NULL == bucket) {
			m_nl.free();
			init();
			throw std::bad_alloc();
		}
		if (intptr_t(y.pHash) == hash_cache_disabled) {
			pHash = (HashTp*)(hash_cache_disabled);
		}
		else {
			pHash = (HashTp*)malloc(sizeof(HashTp) * nElem);
			if (NULL == pHash) {
				free(bucket);
				m_nl.free();
				throw std::bad_alloc();
			}
			memcpy(pHash, y.pHash, sizeof(HashTp) * nElem);
		}
		memcpy(bucket, y.bucket, sizeof(LinkTp) * nBucket);
		if (!std::is_trivial_copy<Elem>::value
				&& freelist_size) {
			node_layout_copy_cons(m_nl, y.m_nl, nElem, IsNotFree());
		} else // freelist is empty, copy all
			node_layout_copy_cons(m_nl, y.m_nl, nElem);
	}
	gold_hash_tab& operator=(const gold_hash_tab& y) {
		if (this != &y) {
		#if 1 // prefer performance, std::map is in this manner
		// this is exception-safe but not transactional
        // non-transactional: lose old content of *this on exception
			this->destroy();
			new(this)gold_hash_tab(y);
		#else // exception-safe, but take more peak memory
			gold_hash_tab(y).swap(*this);
		#endif
		}
		return *this;
	}
	~gold_hash_tab() { destroy(); }

	void swap(gold_hash_tab& y) {
		std::swap(pHash  , y.pHash);
		std::swap(m_nl   , y.m_nl);
		std::swap(nElem  , y.nElem);
		std::swap(maxElem, y.maxElem);
		std::swap(maxload, y.maxload);

		std::swap(nBucket, y.nBucket);
		std::swap(bucket , y.bucket);

		std::swap(freelist_head, y.freelist_head);
		std::swap(freelist_size, y.freelist_size);
		std::swap(freelist_freq, y.freelist_freq);

		std::swap(load_factor, y.load_factor);
		std::swap(is_sorted  , y.is_sorted);
		std::swap(he         , y.he);
		std::swap(keyExtract , y.keyExtract);
	}

//	void setHashEqual(const HashEqual& he) { this->he = he; }
//	void setKeyExtractor(const KeyExtractor& kEx) { this->keyExtract = kEx; }

	const HashEqual& getHashEqual() const { return this->he; }
	const KeyExtractor& getKeyExtractor() const { return this->keyExtract; }

	void clear() {
		destroy();
		init();
	}

	void shrink_to_fit() { if (nElem < maxElem) reserve(nElem); }

	bool is_hash_cached() const {
		return intptr_t(pHash) != hash_cache_disabled;
	}

	void enable_hash_cache() {
		if (intptr_t(pHash) == hash_cache_disabled) {
			if (0 == maxElem) {
				pHash = NULL;
			}
			else {
				HashTp* ph = (HashTp*)malloc(sizeof(HashTp) * maxElem);
				if (NULL == ph) {
					throw std::bad_alloc();
				}
				if (0 == freelist_size) {
					for (size_t i = 0, n = nElem; i < n; ++i)
						ph[i] = he.hash(keyExtract(m_nl.data(i)));
				}
				else {
					for (size_t i = 0, n = nElem; i < n; ++i)
						if (delmark != m_nl.link(i))
							ph[i] = he.hash(keyExtract(m_nl.data(i)));
				}
				pHash = ph;
			}
		}
		assert(NULL != pHash || 0 == maxElem);
	}

	void disable_hash_cache() {
		if (intptr_t(pHash) == hash_cache_disabled)
			return;
		if (pHash)
			free(pHash);
		pHash = (HashTp*)(hash_cache_disabled);
	}

	void rehash(size_t newBucketSize) {
		newBucketSize = __hsm_stl_next_prime(newBucketSize);
		if (newBucketSize != nBucket) {
			// shrink or enlarge
			if (bucket != &tail) {
				assert(NULL != bucket);
				free(bucket);
			}
			bucket = (LinkTp*)malloc(sizeof(LinkTp) * newBucketSize);
			if (NULL == bucket) { // how to handle?
				assert(0); // immediately fail on debug mode
				destroy();
				throw std::runtime_error("rehash failed, unrecoverable");
			}
			nBucket = newBucketSize;
			relink();
			maxload = LinkTp(newBucketSize * load_factor);
		}
	}
//	void resize(size_t n) { rehash(n); }

	void reserve(size_t cap) {
		reserve_nodes(cap);
		rehash(size_t(cap / load_factor + 1));
	}

	void reserve_nodes(size_t cap) {
	//	assert(cap >= maxElem); // could be shrink
		assert(cap >= nElem);
		assert(cap <= delmark);
		if (cap != (size_t)maxElem && cap != nElem) {
			if (intptr_t(pHash) != hash_cache_disabled) {
				HashTp* ph = (HashTp*)realloc(pHash, sizeof(HashTp) * cap);
				if (NULL == ph)
					throw std::bad_alloc();
				pHash = ph;
			}
			if (freelist_size)
				m_nl.reserve(nElem, cap, IsNotFree());
			else
				m_nl.reserve(nElem, cap);
			maxElem = LinkTp(cap);
		}
	}

	void set_load_factor(double fact) {
		if (fact >= 0.999) {
			throw std::logic_error("load factor must <= 0.999");
		}
		load_factor = fact;
		maxload = &tail == bucket ? 0 : LinkTp(nBucket * fact);
	}
	double get_load_factor() const { return load_factor; }

	inline HashTp hash_i(size_t i) const {
		assert(i < nElem);
		if (intptr_t(pHash) == hash_cache_disabled)
			return HashTp(he.hash(keyExtract(m_nl.data(i))));
		else
			return pHash[i];
	}
    inline HashTp hash_v(const Elem& e) const {
        return HashTp(he.hash(keyExtract(e)));
    }
	bool   empty() const { return nElem == freelist_size; }
	size_t  size() const { return nElem -  freelist_size; }
	size_t beg_i() const {
		size_t i;
		if (freelist_size == nElem)
			i = nElem;
		else if (freelist_size && delmark == m_nl.link(0))
			i = next_i(0);
		else
			i = 0;
		return i;
	}
	size_t  end_i() const { return nElem; }
	size_t rbeg_i() const { return freelist_size == nElem ? 0 : nElem; }
	size_t rend_i() const { return 0; }
	size_t next_i(size_t idx) const {
		size_t n = nElem;
	   	assert(idx < n);
		do ++idx; while (idx < n && delmark == m_nl.link(idx));
		return idx;
	}
	size_t prev_i(size_t idx) const {
		assert(idx > 0);
		assert(idx <= nElem);
		do --idx; while (idx > 0 && delmark == m_nl.link(idx));
		return idx;
	}
	size_t capacity() const { return maxElem; }
	size_t delcnt() const { return freelist_size; }

	      iterator  begin()       { return get_iter(beg_i()); }
	const_iterator  begin() const { return get_iter(beg_i()); }
	const_iterator cbegin() const { return get_iter(beg_i()); }

	      iterator  end()       { return get_iter(nElem); }
	const_iterator  end() const { return get_iter(nElem); }
	const_iterator cend() const { return get_iter(nElem); }

	      reverse_iterator  rbegin()       { return       reverse_iterator(end()); }
	const_reverse_iterator  rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }

	      reverse_iterator  rend()       { return       reverse_iterator(begin()); }
	const_reverse_iterator  rend() const { return const_reverse_iterator(begin()); }
	const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

	      fast_iterator  fast_begin()       { return m_nl.begin(); }
	const_fast_iterator  fast_begin() const { return m_nl.begin(); }
	const_fast_iterator fast_cbegin() const { return m_nl.begin(); }

	      fast_iterator  fast_end()       { return m_nl.begin() + nElem; }
	const_fast_iterator  fast_end() const { return m_nl.begin() + nElem; }
	const_fast_iterator fast_cend() const { return m_nl.begin() + nElem; }

	      fast_reverse_iterator  fast_rbegin()       { return       fast_reverse_iterator(fast_end()); }
	const_fast_reverse_iterator  fast_rbegin() const { return const_fast_reverse_iterator(fast_end()); }
	const_fast_reverse_iterator fast_crbegin() const { return const_fast_reverse_iterator(fast_end()); }

	      fast_reverse_iterator  fast_rend()       { return       fast_reverse_iterator(fast_begin()); }
	const_fast_reverse_iterator  fast_rend() const { return const_fast_reverse_iterator(fast_begin()); }
	const_fast_reverse_iterator fast_crend() const { return const_fast_reverse_iterator(fast_begin()); }

	template<class CompatibleObject>
	std::pair<iterator, bool> insert(const CompatibleObject& obj) {
		std::pair<size_t, bool> ib = insert_i(obj);
		return std::pair<iterator, bool>(get_iter(ib.first), ib.second);
	}

	template<class Arg1>
	std::pair<iterator, bool> emplace(const Arg1& a1) {
		std::pair<size_t, bool> ib = insert_i(value_type(a1));
		return std::pair<iterator, bool>(get_iter(ib.first), ib.second);
	}
	template<class Arg1, class Arg2>
	std::pair<iterator, bool> emplace(const Arg1& a1, const Arg2& a2) {
		std::pair<size_t, bool> ib = insert_i(value_type(a1, a2));
		return std::pair<iterator, bool>(get_iter(ib.first), ib.second);
	}
#if defined(HSM_HAS_MOVE)
	template<class... _Valty>
	std::pair<iterator, bool> emplace(_Valty&&... _Val) {
		std::pair<size_t, bool> ib =
			insert_i(value_type(std::forward<_Valty>(_Val)...));
		return std::pair<iterator, bool>(get_iter(ib.first), ib.second);
	}
#endif

	      iterator find(const Key& key)       { return get_iter(find_i(key)); }
	const_iterator find(const Key& key) const { return get_iter(find_i(key)); }

	// keep memory
	void erase_all() {
		NodeLayout nl = m_nl;
		if (!nl.is_null()) {
			if (!std::is_trivial_destructor<Elem>::value) {
                if (freelist_is_empty()) {
                    for (size_t i = nElem; i > 0; --i)
                        nl.data(i-1).~Elem();
                } else {
                    for (size_t i = nElem; i > 0; --i)
                        if (delmark != nl.link(i-1))
                            nl.data(i-1).~Elem();
                }
			}
		}
		if (freelist_head < delmark) {
			assert(freelist_head < nElem);
			freelist_head = tail;
			freelist_size = 0;
			freelist_freq = 0;
		}
		if (nElem) {
			std::fill_n(bucket, nBucket, (LinkTp)tail);
			nElem = 0;
		}
	}

#ifndef NARK_GOLD_HASH_MAP_ITERATOR_USE_FAST
	void erase(iterator iter) {
		assert(iter.get_owner() == this);
		assert(!m_nl.is_null());
		assert(iter.get_index() < nElem);
		erase_i(iter.get_index());
	}
#endif
	void erase(fast_iterator iter) {
		assert(!m_nl.is_null());
		assert(iter >= m_nl.begin());
		assert(iter <  m_nl.begin() + nElem);
		erase_i(iter - m_nl.begin());
	}
private:
	// return erased elements count
	template<class Predictor>
	size_t erase_if_impl(Predictor pred, FastCopy) {
		if (freelist_size)
			// this extra scan should be eliminated, by some way
			revoke_deleted_no_relink();
		NodeLayout nl = m_nl;
		size_t i = 0, n = nElem;
		for (; i < n; ++i)
			if (pred(nl.data(i)))
				goto DoErase;
		return 0;
	DoErase:
		HashTp* ph = pHash;
		nl.data(i).~Elem();
		if (intptr_t(ph) == hash_cache_disabled) {
			for (size_t j = i + 1; j != n; ++j)
				if (pred(nl.data(j)))
					nl.data(j).~Elem();
				else
					memcpy(&nl.data(i), &nl.data(j), sizeof(Elem)), ++i;
		} else {
			for (size_t j = i + 1; j != n; ++j)
				if (pred(nl.data(j)))
					nl.data(j).~Elem();
				else {
					ph[i] = ph[j];
					memcpy(&nl.data(i), &nl.data(j), sizeof(Elem)), ++i;
				}
		}
		nElem = i;
		if (0 == i) { // all elements are erased
			this->destroy();
			this->init();
		}
		return n - i; // erased elements count
	}

	template<class Predictor>
	size_t erase_if_impl(Predictor pred, SafeCopy) {
		if (freelist_size)
			// this extra scan should be eliminated, by some way
			revoke_deleted_no_relink();
		NodeLayout nl = m_nl;
		size_t i = 0, n = nElem;
		for (; i < n; ++i)
			if (pred(nl.data(i)))
				goto DoErase;
		return 0;
	DoErase:
		HashTp* ph = pHash;
		if (intptr_t(ph) == hash_cache_disabled) {
			for (size_t j = i + 1; j != n; ++j)
				if (!pred(nl.data(j)))
					std::swap(nl.data(i), nl.data(j)), ++i;
		} else {
			for (size_t j = i + 1; j != n; ++j)
				if (!pred(nl.data(j))) {
					ph[i] = ph[j];
					std::swap(nl.data(i), nl.data(j)), ++i;
				}
		}
		if (!std::is_trivial_destructor<Elem>::value) {
			for (size_t j = i; j != n; ++j)
				nl.data(j).~Elem();
		}
		nElem = i;
		if (0 == i) { // all elements are erased
			this->destroy();
			this->init();
		}
		return n - i; // erased elements count
	}

public:
	//@{
	//@brief erase_if
	/// will invalidate saved id/index/iterator
	/// can be called even if freelist_is_using
	template<class Predictor>
	size_t erase_if(Predictor pred) {
		if (freelist_is_using())
			return keepid_erase_if(pred);
		else {
			size_t nErased = erase_if_impl(pred, CopyStrategy());
			if (nElem * 2 <= maxElem)
				shrink_to_fit();
			else
				relink();
			return nErased;
		}
	}
	template<class Predictor>
	size_t shrink_after_erase_if(Predictor pred) {
		size_t nErased = erase_if_impl(pred, CopyStrategy());
		shrink_to_fit();
		return nErased;
	}
	template<class Predictor>
	size_t no_shrink_after_erase_if(Predictor pred) {
		size_t nErased = erase_if_impl(pred, CopyStrategy());
		relink();
		return nErased;
	}
	//@}

	template<class Predictor>
	size_t keepid_erase_if(Predictor pred) {
		// should be much slower than erase_if
		assert(freelist_is_using());
		size_t  n = nElem;
		size_t  erased = 0;
		NodeLayout nl = m_nl;
		if (freelist_is_empty()) {
			assert(0 == freelist_size);
			for (size_t i = 0; i != n; ++i)
				if (pred(nl.data(i)))
					erase_to_freelist(i), ++erased;
		}
		else {
			assert(0 != freelist_size);
			for (size_t i = 0; i != n; ++i)
				if (delmark != nl.link(i) && pred(nl.data(i)))
					erase_to_freelist(i), ++erased;
		}
		return erased;
	}
	// if return non-zero, all permanent id/index are invalidated
	size_t revoke_deleted() {
		assert(freelist_is_using()); // must be using/enabled
		if (freelist_size) {
			size_t erased = revoke_deleted_no_relink();
			relink();
			return erased;
		} else
			return 0;
	}
	bool is_deleted(size_t idx) const {
		assert(idx < nElem);
		assert(freelist_is_using());
		return delmark == m_nl.link(idx);
	}
	//@{
	/// default is disabled
	void enable_freelist() {
		if (delmark  == freelist_head) {
			assert(0 == freelist_size);
			freelist_head = tail;
		}
	}
	/// @note this function may take O(n) time
	void disable_freelist() {
		if (delmark != freelist_head) {
			revoke_deleted();
			freelist_head = delmark;
		}
	}
	bool freelist_is_empty() const { return freelist_head >= delmark; }
	bool freelist_is_using() const { return freelist_head != delmark; }
	//@}

	template<class CompatibleObject>
	std::pair<size_t, bool> insert_i(const CompatibleObject& obj) {
		const Key& key = keyExtract(obj);
		const HashTp h = HashTp(he.hash(key));
		size_t i = h % nBucket;
		for (LinkTp p = bucket[i]; tail != p; p = m_nl.link(p)) {
			HSM_SANITY(p < nElem);
			if (he.equal(key, keyExtract(m_nl.data(p))))
				return std::make_pair(p, false);
		}
		if (nark_unlikely(nElem - freelist_size >= maxload)) {
			rehash(nBucket + 1); // will auto find next prime bucket size
			i = h % nBucket;
		}
		size_t slot = risk_slot_alloc(); // here, no risk
		new(&m_nl.data(slot))Elem(obj);
		m_nl.link(slot) = bucket[i]; // newer at head
		bucket[i] = LinkTp(slot); // new head of i'th bucket
		if (intptr_t(pHash) != hash_cache_disabled)
			pHash[slot] = h;
		is_sorted = false;
		return std::make_pair(slot, true);
	}

	///@{ low level operations
	/// the caller should pay the risk for gain
	///
	LinkTp risk_slot_alloc() {
		LinkTp slot;
		if (freelist_is_empty()) {
			assert(0 == freelist_size);
			slot = nElem;
			if (nark_unlikely(nElem == maxElem))
				reserve_nodes(0 == nElem ? 1 : 2*nElem);
			assert(nElem < maxElem);
			nElem++;
		} else {
			assert(freelist_head < nElem);
			assert(freelist_size > 0);
			slot = freelist_head;
			assert(delmark == m_nl.link(slot));
			freelist_size--;
			freelist_head = reinterpret_cast<LinkTp&>(m_nl.data(slot));
		}
		m_nl.link(slot) = tail; // for debug check&verify
		return slot;
	}

	/// precond: # Elem on slot is constructed
	///          # Elem on slot is not in the table:
	///                 it is not reached through 'bucket'
	/// effect: destory the Elem on slot then free the slot
	void risk_slot_free(size_t slot) {
		assert(slot < nElem);
		assert(delmark != m_nl.link(slot));
		if (nark_unlikely(nElem-1 == slot)) {
			m_nl.data(slot).~Elem();
			--nElem;
		}
		else if (freelist_is_using()) {
			fast_slot_free(slot);
			is_sorted = false;
		}
		else {
			assert(!"When freelist is disabled, nElem-1==slot must be true");
			throw std::logic_error(BOOST_CURRENT_FUNCTION);
		}
	}

	/// @note when calling this function:
	//    1. this->begin()[slot] must has been newly constructed by the caller
	//    2. this->begin()[slot] must not existed in the hash index
	size_t risk_insert_on_slot(size_t slot) {
		assert(slot < nElem);
		assert(delmark != m_nl.link(slot));
		const Key& key = keyExtract(m_nl.data(slot));
		const HashTp h = HashTp(he.hash(key));
		size_t i = h % nBucket;
		for (LinkTp p = bucket[i]; tail != p; p = m_nl.link(p)) {
			assert(p != slot); // slot must not be reached
			HSM_SANITY(p < nElem);
			if (he.equal(key, keyExtract(m_nl.data(p))))
				return p;
		}
		if (intptr_t(pHash) != hash_cache_disabled)
			pHash[slot] = h;
		if (nark_unlikely(nElem - freelist_size >= maxload)) { // must be >=
			// rehash will set the bucket&link for 'slot'
			rehash(nBucket + 1); // will auto find next prime bucket size
		} else {
			m_nl.link(slot) = bucket[i]; // newer at head
			bucket[i] = LinkTp(slot); // new head of i'th bucket
		}
		is_sorted = false;
		return slot;
	}

	void risk_size_inc(LinkTp n) {
		if (nark_unlikely(nElem + n > maxElem))
			reserve_nodes(nElem + std::max(nElem, n));
		assert(nElem + n <= maxElem);
		NodeLayout nl = m_nl;
		for (size_t i = n, j = nElem; i; --i, ++j)
			nl.link(j) = (LinkTp)tail;
		nElem += n;
	}

	void risk_size_dec(LinkTp n) {
		assert(nElem >= n);
		nElem -= n;
	}

//	BOOST_CONSTANT(size_t, risk_data_stride = NodeLayout::data_stride);

	//@}

	size_t find_i(const Key& key) const {
		const HashTp h = HashTp(he.hash(key));
		const size_t i = h % nBucket;
		for (LinkTp p = bucket[i]; tail != p; p = m_nl.link(p)) {
			HSM_SANITY(p < nElem);
			if (he.equal(key, keyExtract(m_nl.data(p))))
				return p;
		}
		return nElem; // not found
	}

	template<class CompatibleKey>
	size_t find_i(const CompatibleKey& key) const {
		const HashTp h = HashTp(he.hash(key));
		const size_t i = h % nBucket;
		for (LinkTp p = bucket[i]; tail != p; p = m_nl.link(p)) {
			HSM_SANITY(p < nElem);
			if (he.equal(key, keyExtract(m_nl.data(p))))
				return p;
		}
		return nElem; // not found
	}

	// return erased element count
	size_t erase(const Key& key) {
		const HashTp h = HashTp(he.hash(key));
		const HashTp i = h % nBucket;
		for (LinkTp p = bucket[i]; tail != p; p = m_nl.link(p)) {
			HSM_SANITY(p < nElem);
			if (he.equal(key, keyExtract(m_nl.data(p)))) {
				erase_i_impl(p, i);
				return 1;
			}
		}
		return 0;
	}

	size_t count(const Key& key) const {
		return find_i(key) == nElem ? 0 : 1;
	}
	// return value is same with count(key), but type is different
	// this function is not stl standard, but more meaningful for app
	bool exists(const Key& key) const {
		return find_i(key) != nElem;
	}

private:
	// if return non-zero, all permanent id/index are invalidated
	size_t revoke_deleted_no_relink() {
		assert(freelist_is_using()); // must be using/enabled
		assert(freelist_size > 0);
		NodeLayout nl = m_nl;
		size_t i = 0, n = nElem;
		for (; i < n; ++i)
			if (delmark == nl.link(i))
				goto DoErase;
		assert(0); // would not go here
	DoErase:
		HashTp* ph = pHash;
		if (intptr_t(ph) == hash_cache_disabled) {
			for (size_t j = i + 1; j < n; ++j)
				if (delmark != nl.link(j))
					CopyStrategy::move_cons(&nl.data(i), nl.data(j)), ++i;
		} else {
			for (size_t j = i + 1; j < n; ++j)
				if (delmark != nl.link(j)) {
					ph[i] = ph[j]; // copy cached hash value
					CopyStrategy::move_cons(&nl.data(i), nl.data(j)), ++i;
				}
		}
		nElem = i;
		freelist_head = tail;
		freelist_size = 0;
		return n - i; // erased elements count
	}

	// use erased elem as free list link
	HSM_FORCE_INLINE void fast_slot_free(size_t slot) {
		m_nl.link(slot) = delmark;
		m_nl.data(slot).~Elem();
		reinterpret_cast<LinkTp&>(m_nl.data(slot)) = freelist_head;
		freelist_size++;
        freelist_freq++; // never decrease
		freelist_head = slot;
	}
	HSM_FORCE_INLINE void
	erase_to_freelist(const size_t slot) {
		HashTp const h = hash_i(slot);
		size_t const i = h % nBucket;
		HSM_SANITY(tail != bucket[i]);
		LinkTp* curp = &bucket[i];
        LinkTp  curr; // delete from collision list ...
		while (slot != (curr = *curp)) {
			curp = &m_nl.link(curr);
			HSM_SANITY(*curp < nElem);
		}
		*curp = m_nl.link(slot); // delete the idx'th node from collision list
		fast_slot_free(slot);
	}
	// bucket_idx == hash % nBucket
	void erase_i_impl(const size_t idx, const HashTp bucket_idx) {
		size_t  n = nElem;
		HashTp*ph = pHash;
		HSM_SANITY(n >= 1);
	#if defined(_DEBUG) || !defined(NDEBUG)
		assert(idx < n);
	#else
		if (idx >= n) {
			throw std::invalid_argument(BOOST_CURRENT_FUNCTION);
		}
	#endif
		HSM_SANITY(tail != bucket[bucket_idx]);
		LinkTp* curp = &bucket[bucket_idx];
        LinkTp  curr;
		while (idx != (curr = *curp)) {
			curp = &m_nl.link(curr);
			HSM_SANITY(*curp < n);
		}
		*curp = m_nl.link(idx); // delete the idx'th node from collision list

		if (nark_unlikely(n-1 == idx)) {
			m_nl.data(idx).~Elem();
			--nElem;
		}
		else if (freelist_is_using()) {
			assert(!is_deleted(idx));
			fast_slot_free(idx);
			is_sorted = false;
		}
		else {
            // Move last Elem to slot idx
            //
            // delete last Elem from it's collision list
			const HashTp lastHash = hash_i(n-1);
			const size_t j = lastHash % nBucket;
			HSM_SANITY(tail != bucket[j]);
			curp = &bucket[j];
			while (n-1 != (curr = *curp)) {
				curp = &m_nl.link(curr);
				HSM_SANITY(*curp < n);
			}
			*curp = m_nl.link(n-1); // delete the last node from link list

            // put last Elem to the slot idx and sync the link
			CopyStrategy::move_assign(&m_nl.data(idx), m_nl.data(n-1));
			m_nl.link(idx) = bucket[j];
			if (intptr_t(ph) != hash_cache_disabled)
				ph[idx] = lastHash;
			bucket[j] = LinkTp(idx);
			is_sorted = false;
			--nElem;
		}
	}

public:
	void erase_i(const size_t idx) {
		assert(nElem >= 1);
		assert(idx < nElem);
		assert(delmark != m_nl.link(idx));
		erase_i_impl(idx, hash_i(idx) % nBucket);
	}

	const Key& key(size_t idx) const {
		assert(nElem >= 1);
		assert(idx < nElem);
		assert(delmark != m_nl.link(idx));
		return keyExtract(m_nl.data(idx));
	}

	const Key& end_key(size_t idxEnd) const {
		assert(nElem >= 1);
		assert(idxEnd >= 1);
		assert(idxEnd <= nElem);
		assert(delmark != m_nl.link(nElem - idxEnd));
		return keyExtract(m_nl.data(nElem - idxEnd));
	}

	const Elem& elem_at(size_t idx) const {
		assert(nElem >= 1);
		assert(idx < nElem);
		assert(delmark != m_nl.link(idx));
		return m_nl.data(idx);
	}
	Elem& elem_at(size_t idx) {
		assert(nElem >= 1);
		assert(idx < nElem);
		assert(delmark != m_nl.link(idx));
		return m_nl.data(idx);
	}

	const Elem& end_elem(size_t idxEnd) const {
		assert(nElem >= 1);
		assert(idxEnd >= 1);
		assert(idxEnd <= nElem);
		assert(delmark != m_nl.link(nElem - idxEnd));
		return m_nl.data(nElem - idxEnd);
	}
	Elem& end_elem(size_t idxEnd) {
		assert(nElem >= 1);
		assert(idxEnd >= 1);
		assert(idxEnd <= nElem);
		assert(delmark != m_nl.link(nElem - idxEnd));
		return m_nl.data(nElem - idxEnd);
	}

	template<class OP>
	void for_each(OP op) {
		size_t n = nElem;
		NodeLayout nl = m_nl;
		if (freelist_size) {
			assert(freelist_is_using());
			for (size_t i = 0; i != n; ++i)
				if (delmark != nl.link(i))
					op(nl.data(i));
		} else {
			for (size_t i = 0; i != n; ++i)
				op(nl.data(i));
		}
	}
	template<class OP>
	void for_each(OP op) const {
		const size_t n = nElem;
		const NodeLayout nl = m_nl;
		if (freelist_size) {
			assert(freelist_is_using());
			for (size_t i = 0; i != n; ++i)
				if (delmark != nl.link(i))
					op(nl.data(i));
		} else {
			for (size_t i = 0; i != n; ++i)
				op(nl.data(i));
		}
	}

	template<class Compare>	void sort(Compare comp) {
		if (freelist_size)
			revoke_deleted_no_relink();
		fast_iterator beg_iter = m_nl.begin();
		fast_iterator end_iter = beg_iter + nElem;
		nark_parallel_sort(beg_iter, end_iter, Compare(comp));
		relink_fill();
		is_sorted = true;
	}
	void sort() { sort(std::less<Elem>()); }

	size_t bucket_size() const { return nBucket; }

	template<class IntVec>
	void bucket_histogram(IntVec& hist) const {
		for (size_t i = 0, n = nBucket; i < n; ++i) {
			size_t listlen = 0;
			for (LinkTp j = bucket[i]; j != tail; j = m_nl.link(j))
				++listlen;
			if (hist.size() <= listlen)
				hist.resize(listlen+1);
			hist[listlen]++;
		}
	}

protected:
// DataIO support
	template<class DataIO> void dio_load_fast(DataIO& dio) {
		unsigned char cacheHash;
		clear();
		typename DataIO::my_var_uint64_t size;
		dio >> size;
		dio >> load_factor;
		dio >> cacheHash;
		pHash = cacheHash ? NULL : (size_t*)(hash_cache_disabled);
		if (0 == size.t)
			return;
		if (NULL == pHash) {
			pHash = (HashTp*)malloc(sizeof(HashTp)*size.t);
			if (NULL == pHash)
				throw std::bad_alloc();
		}
		m_nl.reserve(0, size.t);
		size_t i = 0, n = size.t;
		try {
			NodeLayout nl = m_nl;
			for (; i < n; ++i) {
				new(&nl.data(i))Elem();
				dio >> nl.data(i);
			}
			if ((size_t*)(hash_cache_disabled) != pHash) {
				assert(NULL != pHash);
				for (size_t j = 0; j < n; ++j)
					pHash[j] = he.hash(keyExtract(nl.data(i)));
			}
			nElem = LinkTp(size.t);
			maxElem = nElem;
			maxload = nElem; //LinkTp(load_factor * nBucket);
			rehash(maxElem / load_factor);
		}
		catch (...) {
			nElem = i; // valid elem count
			clear();
			throw;
		}
	}
	template<class DataIO> void dio_save_fast(DataIO& dio) const {
		dio << typename DataIO::my_var_uint64_t(nElem);
		dio << load_factor;
		dio << (unsigned char)(intptr_t(pHash) != hash_cache_disabled);
		const NodeLayout nl = m_nl;
		for (size_t i = 0, n = nElem; i < n; ++i)
			dio << nl.data(i);
	}

	// compatible format
	template<class DataIO> void dio_load(DataIO& dio) {
		typename DataIO::my_var_uint64_t Size;
		dio >> Size;
		this->clear();
		this->reserve(Size.t);
		Elem e;
		for (size_t i = 0, n = Size.t; i < n; ++i) {
			this->insert_i(e);
		}
	}
	template<class DataIO> void dio_save(DataIO& dio) const {
		dio << typename DataIO::my_var_uint64_t(this->size());
		if (this->freelist_size) {
			for (size_t i = 0, n = this->end_i(); i < n; ++i)
				if (this->is_deleted())
					dio << m_nl.data(i);
		} else {
			for (size_t i = 0, n = this->end_i(); i < n; ++i)
				dio << m_nl.data(i);
		}
	}

	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, gold_hash_tab& x) {
		x.dio_load(dio);
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const gold_hash_tab& x) {
		x.dio_save(dio);
	}
};

template<class First>
struct nark_get_first {
	template<class T>
	const First& operator()(const T& x) const { return x.first; }
};

template< class Key
		, class Value
		, class HashFunc = DEFAULT_HASH_FUNC<Key>
		, class KeyEqual = std::equal_to<Key>
		, class NodeLayout = node_layout<std::pair<Key, Value>, unsigned>
		, class HashTp = size_t
		>
class gold_hash_map : public
	gold_hash_tab<Key, std::pair<Key, Value>
		, hash_and_equal<Key, HashFunc, KeyEqual>, nark_get_first<Key>
		, NodeLayout
		, HashTp
		>
{
	typedef
	gold_hash_tab<Key, std::pair<Key, Value>
		, hash_and_equal<Key, HashFunc, KeyEqual>, nark_get_first<Key>
		, NodeLayout
		, HashTp
		>
	super;
public:
//	typedef std::pair<const Key, Value> value_type;
	typedef Value mapped_type;
	using super::insert;
	using super::insert_i;
	std::pair<size_t, bool>
	insert_i(const Key& key, const Value& val = Value()) {
		return this->insert_i(std::make_pair(key, val));
	}
	Value& operator[](const Key& key) {
//		typedef boost::reference_wrapper<const Key> key_cref;
		typedef std::pair<key_cref , Value> key_by_ref;
		typedef std::pair<const Key, Value> key_by_val;
		std::pair<size_t, bool> ib =
			this->insert_i(key_by_val(         key , Value()));
		return this->m_nl.data(ib.first).second;
	}
	Value& val(size_t idx) {
		assert(idx < this->nElem);
		assert(this->delmark != this->m_nl.link(idx));
		return this->m_nl.data(idx).second;
	}
	const Value& val(size_t idx) const {
		assert(idx < this->nElem);
		assert(this->delmark != this->m_nl.link(idx));
		return this->m_nl.data(idx).second;
	}
	const Value& end_val(size_t idxEnd) const {
		assert(this->nElem >= 1);
		assert(idxEnd >= 1);
		assert(idxEnd <= this->nElem);
		assert(this->delmark != this->m_nl.link(this->nElem - idxEnd));
		return this->m_nl.data(this->nElem - idxEnd).second;
	}
	Value& end_val(size_t idxEnd) {
		assert(this->nElem >= 1);
		assert(idxEnd >= 1);
		assert(idxEnd <= this->nElem);
		assert(this->delmark != this->m_nl.link(this->nElem - idxEnd));
		return this->m_nl.data(this->nElem - idxEnd).second;
	}

// DataIO support
	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, gold_hash_map& x) {
		x.dio_load(dio);
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const gold_hash_map& x) {
		x.dio_save(dio);
	}
};

template< class Key
		, class Value
		, class HashFunc = DEFAULT_HASH_FUNC<Key>
		, class KeyEqual = std::equal_to<Key>
		, class Deleter = HSM_DefaultDeleter
		>
class gold_hash_map_p : public nark_ptr_hash_map
    < gold_hash_map<Key, Value*, HashFunc, KeyEqual>, Value*, Deleter>
{};

///////////////////////////////////////////////////////////////////////////////

template< class Key
		, class HashFunc = DEFAULT_HASH_FUNC<Key>
		, class KeyEqual = std::equal_to<Key>
		, class NodeLayout = node_layout<Key, unsigned>
		, class HashTp = size_t
		>
class gold_hash_set : public
	gold_hash_tab<Key, Key
		, hash_and_equal<Key, HashFunc, KeyEqual>, nark_identity<Key>
		, NodeLayout, HashTp
		>
{
public:
// DataIO support
	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, gold_hash_set& x) {
		x.dio_load(dio);
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const gold_hash_set& x) {
		x.dio_save(dio);
	}
};

} // namespace nark

namespace std { // for std::swap

template< class Key
		, class Elem
		, class HashEqual
		, class KeyExtractor
		, class NodeLayout
		, class HashTp
		>
void
swap(nark::gold_hash_tab<Key, Elem, HashEqual, KeyExtractor, NodeLayout, HashTp>& x,
	 nark::gold_hash_tab<Key, Elem, HashEqual, KeyExtractor, NodeLayout, HashTp>& y)
{
	x.swap(y);
}

template< class Key
		, class Value
		, class HashFunc
		, class KeyEqual
		, class NodeLayout
		, class HashTp
		>
void
swap(nark::gold_hash_map<Key, Value, HashFunc, KeyEqual, NodeLayout, HashTp>& x,
	 nark::gold_hash_map<Key, Value, HashFunc, KeyEqual, NodeLayout, HashTp>& y)
{
	x.swap(y);
}

template< class Key
		, class HashFunc
		, class KeyEqual
		, class NodeLayout
		, class HashTp
		>
void
swap(nark::gold_hash_set<Key, HashFunc, KeyEqual, NodeLayout, HashTp>& x,
	 nark::gold_hash_set<Key, HashFunc, KeyEqual, NodeLayout, HashTp>& y)
{
	x.swap(y);
}

} // namespace std

#endif // __nark_gold_hash_tab__

