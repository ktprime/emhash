#ifndef __nark_gold_hash_idx_hpp__
#define __nark_gold_hash_idx_hpp__

#include "hash_common.hpp"

namespace nark {

template<class LinkTp>
class gold_hash_idx_default_bucket : public dummy_bucket<LinkTp> {
	LinkTp* m_bucket;
	size_t  m_bsize;
public:
	typedef LinkTp link_t;
	using dummy_bucket<LinkTp>::tail;
	gold_hash_idx_default_bucket() {
	   	m_bucket = const_cast<LinkTp*>(&tail);
	   	m_bsize = 1;
   	}
	~gold_hash_idx_default_bucket() {
		if (m_bucket && &tail != m_bucket)
			::free(m_bucket);
	}
	void resize_fill_bucket(size_t bsize) {
		assert(NULL != m_bucket);
		if (m_bucket && &tail != m_bucket)
		   	::free(m_bucket);
		m_bucket = (LinkTp*)::malloc(sizeof(LinkTp) * bsize);
		if (NULL == m_bucket) {
			assert(0); // immediately fail on debug mode
			throw std::runtime_error("malloc failed in rehash, unrecoverable");
		}
		std::fill_n(m_bucket, bsize, tail);
		m_bsize = bsize;
	}
	void reset_bucket() {
		assert(&tail != m_bucket);
		std::fill_n(m_bucket, m_bsize, tail);
	}
	void setbucket(size_t pos, size_t val) { m_bucket[pos] = val; }
	LinkTp bucket(size_t pos) const { return m_bucket[pos]; }
	size_t bucket_size() const { return m_bsize; }
};

/*
/// concept KeyFromIndex
template<class Elem, class LinkTp = unsigned>
struct key_from_idx : valvec<Elem>, gold_hash_idx_default_bucket<LinkTp> {
	typedef LinkTp link_t;
//	size_t  size() const; // derived from valvec
	link_t  link(size_t idx) const { return (*this)[idx].link; }
	void setlink(size_t idx, LinkTp val) { (*this)[idx].link = val; }
};
*/
template< class LinkStore
		, class HashEqual
		, class HashTp = size_t
		>
class gold_hash_idx1 : boost::noncopyable, public HashEqual {
	typedef typename LinkStore::link_t link_t;

	static const link_t tail = LinkStore::tail;
	static const link_t delmark = LinkStore::delmark;
	static const intptr_t hash_cache_disabled = 8; // good if compiler aggressive optimize

	double       load_factor;
	size_t       maxload;
	HashTp*      pHash;
	LinkStore	 m_link_store;
	size_t       n_used_slots; // deducable, but redundant here

	void init() {
		load_factor = 0.8;
		maxload = 0;
		pHash = (HashTp*)(hash_cache_disabled);
		n_used_slots = 0;
	}

public:
	gold_hash_idx1() { init(); }
	gold_hash_idx1(const LinkStore& link_store, const HashEqual& he)
	  : HashEqual(he), m_link_store(link_store)
   	{ init(); }

	~gold_hash_idx1() { clear(); }

	void clear() {
		if (pHash && intptr_t(pHash) != hash_cache_disabled)
			::free(pHash);
		init();
	}

	LinkStore& get_link_store() { return m_link_store; }
	const LinkStore& get_link_store() const { return m_link_store; }

	bool is_deleted(size_t idx) const {
		const size_t nElem = m_link_store.size();
		assert(idx < nElem); (void)nElem;
		return delmark == m_link_store.link(idx);
	}

	size_t insert_at(size_t idx) {
	#if defined(_DEBUG) || !defined(NDEBUG)
		const size_t nElem = m_link_store.size();
		assert(idx < nElem);
		assert(n_used_slots < nElem);
		assert(delmark == m_link_store.link(idx));
	#endif
		HashTp hash = HashEqual::hash(idx);
		size_t hMod = hash % m_link_store.bucket_size();
	//	printf("insert(idx=%zd) hash=%zd\n", idx, hash);
		link_t p = m_link_store.bucket(hMod);
		for (; tail != p; p = m_link_store.link(p)) {
			HSM_SANITY(p < nElem);
			if (HashEqual::equal(idx, p))
				return p;
		}
		if (n_used_slots >= maxload) {
			rehash(size_t(n_used_slots/load_factor)+1);
			hMod = hash % m_link_store.bucket_size();
		}
		m_link_store.setlink(idx, m_link_store.bucket(hMod));
		m_link_store.setbucket(hMod, link_t(idx));
		if (intptr_t(pHash) != hash_cache_disabled)
			pHash[idx] = hash;
		n_used_slots++;
		return idx;
	}

	// For calling this function, the following function must exists:
	//   HashTp HashEqual::hash(const CompatibleKey&);
	//   bool   HashEqual::equal(const CompatibleKey& x, link_t y);
	template<class CompatibleKey>
	size_t find(const CompatibleKey& key) const {
		const size_t nElem = m_link_store.size();
		HashTp hash = HashEqual::hash(key);
		size_t hMod = hash % m_link_store.bucket_size();
		link_t p = m_link_store.bucket(hMod);
		for (; tail != p; p = m_link_store.link(p)) {
			HSM_SANITY(p < nElem);
			if (HashEqual::equal(key, p))
				return p;
		}
		return nElem; // not found
	}

	size_t erase_i(size_t idx) {
		const size_t nElem = m_link_store.size();
		HSM_SANITY(nElem >= 1);
	#if defined(_DEBUG) || !defined(NDEBUG)
		assert(idx < nElem);
	#else
		if (idx >= nElem) {
			throw std::invalid_argument(BOOST_CURRENT_FUNCTION);
		}
	#endif
		if (delmark == m_link_store.link(idx))
			return 0;
		HashTp hash = HashEqual::hash(idx);
		size_t hMod = hash % m_link_store.bucket_size();
	//	printf("erase(idx=%zd) hash=%zd\n", idx, hash);
        link_t curr = m_link_store.bucket(hMod);
		HSM_SANITY(tail != curr);
		if (curr == idx) {
			m_link_store.setbucket(hMod, m_link_store.link(curr));
		} else {
			link_t prev;
			do {
				prev = curr;
				curr = m_link_store.link(curr);
				HSM_SANITY(curr < nElem);
			} while (idx != curr);
			m_link_store.setlink(prev, m_link_store.link(curr));
		}
		n_used_slots--;
		m_link_store.setlink(idx, delmark);
		return 1;
	}

	void erase_all() {
		if (m_link_store.bucket_size() == 1)
			return;
		const size_t nElem = m_link_store.size();
		for (size_t i = 0; i < nElem; ++i) {
			m_link_store.setlink(i, delmark);
		}
		m_link_store.reset_bucket();
		n_used_slots = 0;
	}

	void rehash(size_t newBucketSize) {
		newBucketSize = __hsm_stl_next_prime(newBucketSize);
		if (m_link_store.bucket_size() != newBucketSize) {
			m_link_store.resize_fill_bucket(newBucketSize);
			relink();
			maxload = link_t(newBucketSize * load_factor);
		}
	}

	void resize(size_t newsize) { m_link_store.resize(newsize); }

	typename LinkStore::value_type& node_at(size_t i) {
		assert(i < m_link_store.size());
	//	assert(m_link_store.link(i) != delmark);
		return m_link_store[i];
	}
	const
	typename LinkStore::value_type& node_at(size_t i) const {
		assert(i < m_link_store.size());
	//	assert(m_link_store.link(i) != delmark);
		return m_link_store[i];
	}

protected:
	template<bool IsHashCached>
	void relink_impl() {
		size_t nb = m_link_store.bucket_size();
		const HashTp* ph = pHash;
		for (size_t i = 0, n = m_link_store.size(); i < n; ++i) {
			if (delmark == m_link_store.link(i))
				continue;
			HashTp hash = IsHashCached ? ph[i] : HashEqual::hash(i);
			size_t hMod = hash % nb;
			m_link_store.setlink(i, m_link_store.bucket(hMod));
			m_link_store.setbucket(hMod, i);
		}
	}

	void relink() {
		if (intptr_t(pHash) == hash_cache_disabled)
			relink_impl<false>();
		else
			relink_impl<true >();
	}
};

#pragma pack(push,1)
template<int LinkBits>
struct CompactBucketLink {
	static const size_t tail = dummy_bucket<size_t, LinkBits>::tail;
	size_t head : LinkBits; // required unsigned integer field
	size_t next : LinkBits; // required unsigned integer field
};
#pragma pack(pop)

// load factor is 1
// so the bucket array and next-link array are compacted into one
// HashEqual::hash and HashEqual::equal take integer id params
// key extraction is composed into HashEqual, so it is not possible
// to insert a node that is not in the external DataArray.
// an example HashEqual:
//   struct HashEqual {
//     vector<DataElement>* vec;
//     size_t hash(size_t idx) const {
//         return DataElement::HashFunc((*vec)[idx].key);
//     }
//     bool equal(size_t x, size_t y) const {
//		   const DataElement* p = &*vec->begin();
//         return DataElement::equal(p[x].key, p[y].key);
//     }
//   };
// (gold_hash_idx2::m_size >= vector<DataElement>::size) must be true
template< class HashEqual
		, class Node = CompactBucketLink<32>
		, class HashTp = size_t
		>
class gold_hash_idx2 : boost::noncopyable {
	BOOST_STATIC_ASSERT(boost::has_trivial_destructor<Node>::value);
	typedef size_t link_t;
	static const link_t tail    = Node::tail;
	static const link_t delmark = Node::tail - 1;
	static const size_t hash_cache_disabled = 8;

	Node*	  m_node;
	size_t	  m_size;
	size_t	  m_used;
	HashTp*   m_hash_cache;
	HashEqual m_func;

public:
	gold_hash_idx2(const HashEqual& func = HashEqual()) : m_func(func) {
		m_node = NULL;
		m_size = 0;
		m_used = 0;
		m_hash_cache = (HashTp*)(hash_cache_disabled);
	}

	Node& node_at(size_t idx) {
		assert(idx < m_size);
		assert(idx < delmark);
		return m_node[idx];
	}
	const Node& node_at(size_t idx) const {
		assert(idx < m_size);
		assert(idx < delmark);
		return m_node[idx];
	}
	size_t end_i()const { return m_size; }
	size_t size() const { return m_size; }
	bool  empty() const { return m_size == 0; }

	bool is_deleted(size_t idx) const {
		assert(idx < m_size);
		assert(idx < delmark);
		return delmark == m_node[idx].next;
	}

	size_t insert_at(size_t idx) {
	#if defined(_DEBUG) || !defined(NDEBUG)
		assert(idx < m_size);
		assert(idx < delmark);
		assert(m_used < m_size);
		assert(delmark == m_node[idx].next);
	#endif
		HashTp hash = m_func.hash(idx);
		size_t hMod = hash % m_size;
	//	printf("insert(idx=%zd) hash=%zd\n", idx, hash);
		link_t p = m_node[hMod].head;
		while (tail != p) {
			HSM_SANITY(p < m_size);
			if (m_func.equal(idx, p))
				return p;
			p = m_node[p].next;
		}
		m_node[idx].next = m_node[hMod].head;
		m_node[hMod].head = idx; // new head of hMod'th bucket
		m_used++;
		if (size_t(m_hash_cache) != hash_cache_disabled)
			m_hash_cache[idx] = hash;
		return idx;
	}

	size_t find(size_t idx) const {
		assert(idx < m_size);
		assert(idx < delmark);
		HashTp hash = m_func.hash(idx);
		size_t hMod = hash % m_size;
		link_t p = m_node[hMod].head;
		while (tail != p) {
			HSM_SANITY(p < m_size);
			if (m_func.equal(idx, p))
				return p;
			p = m_node[p].next;
		}
		return m_size; // not found
	}

	///@returns number of erased elements
	size_t erase_i(size_t idx) {
	#if defined(_DEBUG) || !defined(NDEBUG)
		assert(idx < m_size);
		assert(idx < delmark);
	#else
		if (idx >= std::min(m_size, delmark)) {
			throw std::invalid_argument(BOOST_CURRENT_FUNCTION);
		}
	#endif
		if (delmark == m_node[idx].next)
			return 0;
		HashTp hash = m_func.hash(idx);
		size_t hMod = hash % m_size;
	//	printf("erase(idx=%zd) hash=%zd\n", idx, hash);
		HSM_SANITY(tail != m_node[hMod].head);
        link_t curr = m_node[hMod].head;
		if (curr == idx) {
			m_node[hMod].head = m_node[curr].next;
		} else {
			link_t prev;
			do {
				prev = curr;
				curr = m_node[curr].next;
				HSM_SANITY(curr < m_size);
			} while (idx != curr);
			m_node[prev].next = m_node[curr].next;
		}
		m_used--;
		m_node[idx].next = delmark;
		return 1;
	}

	void erase_all() {
		Node* node = m_node;
		for (size_t i = 0, n = m_size; i < n; ++i) {
			node[i].head = tail;
			node[i].next = delmark;
		}
		m_used = 0;
	}

	void rehash(size_t newsize) {
		if (newsize >= m_size/2 && newsize <= m_size)
			return;
		newsize = __hsm_stl_next_prime(newsize);
		if (newsize == m_size)
			return;
	//	fprintf(stderr, "gold_hash_idx2::rehash: old=%zd new=%zd\n", m_size, newsize);
		Node* q = (Node*)realloc(m_node, sizeof(Node) * newsize);
		if (NULL == q) { // how to handle?
			assert(0); // immediately fail on debug mode
			throw std::runtime_error("rehash: realloc failed");
		}
		for (size_t i = m_size; i < newsize; ++i) {
			if (!boost::has_trivial_constructor<Node>::value)
				new (q+i) Node(); // calling default constructor
			q[i].next = delmark;
		}
		m_node = q;
		m_size = newsize;
		relink();
	}
	void resize(size_t newsize) { rehash(newsize); }

protected:
	template<bool IsHashCached>
	void relink_impl() {
		size_t size = m_size;
		Node*  node = m_node;
		const HashTp* ph = m_hash_cache;
		for (size_t i = 0; i < size; ++i) node[i].head = tail;
		for (size_t i = 0; i < size; ++i) {
			if (delmark == node[i].next)
				continue;
			HashTp hash = IsHashCached ? ph[i] : m_func.hash(i);
			size_t hMod = hash % size;
			node[i].next = node[hMod].head;
			node[hMod].head = i;
		}
	}

	void relink() {
		if (size_t(m_hash_cache) == hash_cache_disabled)
			relink_impl<false>();
		else
			relink_impl<true >();
	}
};
template<class HashEqual, class Node, class HashTp>
const size_t gold_hash_idx2<HashEqual, Node, HashTp>::tail;
template<class HashEqual, class Node, class HashTp>
const size_t gold_hash_idx2<HashEqual, Node, HashTp>::delmark;

} // namespace nark

#endif // __nark_gold_hash_idx_hpp__

