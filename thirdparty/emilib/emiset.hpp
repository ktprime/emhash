// version 1.0.2
// https://github.com/ktprime/emhash/blob/master/thirdparty/emilib/emiset.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2025 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

#pragma once

#include <cstdlib> // malloc
#include <iterator>
#include <utility>

namespace emilib {

/// like std::equal_to but no need to `#include <functional>`


/// A cache-friendly hash set with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashSet
{
private:
	using htype = HashSet<KeyT, HashT, EqT>;

public:
	using size_type       = size_t;
	using value_type      = KeyT;
	using reference       = KeyT&;
	using const_reference = const KeyT&;

	class iterator
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type   = size_t;
		using distance_type     = size_t;
		using value_type        = KeyT;
		using pointer           = value_type*;
		using reference         = value_type&;

		iterator() { }

		iterator(htype* hash_set, size_t bucket) : _set(hash_set), _bucket(bucket)
		{
		}

		iterator& operator++()
		{
			goto_next_element();
			return *this;
		}

		iterator operator++(int)
		{
			size_t old_index = _bucket;
			goto_next_element();
			return iterator(_set, old_index);
		}

		reference operator*() const
		{
			return _set->_keys[_bucket];
		}

		pointer operator->() const
		{
			return _set->_keys + _bucket;
		}

		bool operator==(const iterator& rhs) const
		{
			return _bucket == rhs._bucket;
		}

		bool operator!=(const iterator& rhs) const
		{
			return _bucket != rhs._bucket;
		}

	private:
		void goto_next_element()
		{
			do {
				_bucket++;
			} while (_bucket < _set->_num_buckets && _set->_states[_bucket] != State::FILLED);
		}
	public:
		htype* _set;
		int    _bucket;
	};

	class const_iterator
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type   = size_t;
		using distance_type     = size_t;
		using value_type        = const KeyT;
		using pointer           = value_type*;
		using reference         = value_type&;

		const_iterator() { }

		const_iterator(iterator proto) : _set(proto._set), _bucket(proto._bucket)
		{
		}

		const_iterator(const htype* hash_set, size_t bucket) : _set(hash_set), _bucket(bucket)
		{
		}

		const_iterator& operator++()
		{
			goto_next_element();
			return *this;
		}

		const_iterator operator++(int)
		{
			size_t old_index = _bucket;
			goto_next_element();
			return const_iterator(_set, old_index);
		}

		reference operator*() const
		{
			return _set->_keys[_bucket];
		}

		pointer operator->() const
		{
			return _set->_keys + _bucket;
		}

		bool operator==(const const_iterator& rhs) const
		{
			return _bucket == rhs._bucket;
		}

		bool operator!=(const const_iterator& rhs) const
		{
			return _bucket != rhs._bucket;
		}

	private:
		void goto_next_element()
		{
			do {
				_bucket++;
			} while (_bucket < _set->_num_buckets && _set->_states[_bucket] != State::FILLED);
		}

	//private:
	//	friend class htype;
	public:
		const htype* _set;
		int          _bucket;
	};

	// ------------------------------------------------------------------------

	HashSet() = default;

	HashSet(const HashSet& other)
	{
		reserve(other.size());
		insert(other.cbegin(), other.cend());
	}

	HashSet(HashSet&& other)
	{
		*this = std::move(other);
	}

	HashSet& operator=(const HashSet& other)
	{
		clear();
		reserve(other.size());
		insert(other.cbegin(), other.cend());
		return *this;
	}

	void operator=(HashSet&& other)
	{
		swap(other);
	}

	void max_load_factor(float lf)
	{

	}

	~HashSet()
	{
		for (int bucket = 0; bucket < _num_buckets; ++bucket) {
			if (_states[bucket] == State::FILLED) {
				_keys[bucket].~KeyT();
			}
		}
		free(_states);
		free(_keys);
	}

	void swap(HashSet& other)
	{
		std::swap(_hasher,           other._hasher);
		std::swap(_eq,               other._eq);
		std::swap(_states,           other._states);
		std::swap(_keys,             other._keys);
		std::swap(_num_buckets,      other._num_buckets);
		std::swap(_num_filled,       other._num_filled);
		std::swap(_max_probe_length, other._max_probe_length);
		std::swap(_mask,             other._mask);
	}

	// -------------------------------------------------------------

	iterator begin()
	{
		int bucket = 0;
		while (bucket < _num_buckets && _states[bucket] != State::FILLED) {
			++bucket;
		}
		return iterator(this, bucket);
	}

	const_iterator cbegin() const
	{
		int bucket = 0;
		while (bucket < _num_buckets && _states[bucket] != State::FILLED) {
			++bucket;
		}
		return const_iterator(this, bucket);
	}

	const_iterator begin() const
	{
		return cbegin();
	}

	iterator end()
	{
		return iterator(this, _num_buckets);
	}

	const_iterator cend() const
	{
		return const_iterator(this, _num_buckets);
	}

	const_iterator end() const
	{
		return cend();
	}

	size_t size() const
	{
		return _num_filled;
	}

	bool empty() const
	{
		return _num_filled==0;
	}

	// Returns the number of buckets.
	size_t bucket_count() const
	{
		return _num_buckets;
	}

	/// Returns average number of elements per bucket.
	float load_factor() const
	{
		return static_cast<float>(_num_filled) / static_cast<float>(_num_buckets);
	}

	// ------------------------------------------------------------

	iterator find(const KeyT& key)
	{
		auto bucket = find_filled_bucket(key);
		if (bucket == (size_t)-1) {
			return end();
		}
		return iterator(this, bucket);
	}

	const_iterator find(const KeyT& key) const
	{
		auto bucket = find_filled_bucket(key);
		if (bucket == (size_t)-1) {
			return end();
		}
		return const_iterator(this, bucket);
	}

	bool contains(const KeyT& k) const
	{
		return find_filled_bucket(k) != (size_t)-1;
	}

	size_t count(const KeyT& k) const
	{
		return find_filled_bucket(k) != (size_t)-1 ? 1 : 0;
	}

	// -----------------------------------------------------

	/// Insert an element, unless it already exists.
	/// Returns a pair consisting of an iterator to the inserted element
	/// (or to the element that prevented the insertion)
	/// and a bool denoting whether the insertion took place.
	std::pair<iterator, bool> insert(const KeyT& key)
	{
		check_expand_need();

		auto bucket = find_or_allocate(key);

		if (_states[bucket] == State::FILLED) {
			return { iterator(this, bucket), false };
		} else {
			_states[bucket] = State::FILLED;
			new(_keys + bucket) KeyT(key);
			_num_filled++;
			return { iterator(this, bucket), true };
		}
	}

	/// Insert an element, unless it already exists.
	/// Returns a pair consisting of an iterator to the inserted element
	/// (or to the element that prevented the insertion)
	/// and a bool denoting whether the insertion took place.
	std::pair<iterator, bool> insert(KeyT&& key)
	{
		check_expand_need();

		auto bucket = find_or_allocate(key);

		if (_states[bucket] == State::FILLED) {
			return { iterator(this, bucket), false };
		} else {
			_states[bucket] = State::FILLED;
			new(_keys + bucket) KeyT(std::move(key));
			_num_filled++;
			return { iterator(this, bucket), true };
		}
	}

	template<class... Args>
	std::pair<iterator, bool> emplace(Args&&... args)
	{
		return insert(KeyT(std::forward<Args>(args)...));
	}

	void insert(const_iterator begin, const_iterator end)
	{
		// TODO: reserve space exactly once.
		for (; begin != end; ++begin) {
			insert(*begin);
		}
	}

	/// Same as above, but contains(key) MUST be false
	void insert_unique(KeyT key)
	{
		check_expand_need();
		auto bucket = find_empty_bucket(key);
		_states[bucket] = State::FILLED;
		new(_keys + bucket) KeyT(std::move(key));
		_num_filled++;
	}

	// -------------------------------------------------------

	/// Erase an element from the hash set.
	/// return false if element was not found.
	bool erase(const KeyT& key)
	{
		auto bucket = find_filled_bucket(key);
		if (bucket != (size_t)-1) {
			_states[bucket] = State::ACTIVE;
			_keys[bucket].~KeyT();
			_num_filled -= 1;
			return true;
		} else {
			return false;
		}
	}

	/// Erase an element using an iterator.
	/// Returns an iterator to the next element (or end()).
	iterator erase(iterator it)
	{
		_states[it._bucket] = State::ACTIVE;
		_keys[it._bucket].~KeyT();
		_num_filled -= 1;
		return ++it;
	}

	/// Remove all elements, keeping full capacity.
	void clear()
	{
		for (size_t bucket = 0; bucket < _num_buckets; ++bucket) {
			if (_states[bucket] == State::FILLED) {
				_states[bucket] = State::INACTIVE;
				_keys[bucket].~KeyT();
			}
		}
		_num_filled = 0;
		_max_probe_length = -1;
	}

	/// Make room for this many elements
	void reserve(size_t num_elems)
	{
		size_t required_buckets = num_elems + num_elems / 4 + 1;
		if (required_buckets <= (size_t)_num_buckets) {
			return;
		}
		size_t num_buckets = 4;
		while (num_buckets < required_buckets) { num_buckets *= 2; }

		auto new_states = (State*)malloc(num_buckets * sizeof(State));
		auto new_keys  = (KeyT*)malloc(num_buckets * sizeof(KeyT));

		if (!new_states || !new_keys) {
			free(new_states);
			free(new_keys);
			//throw std::bad_alloc();
		}

		// auto old_num_filled  = _num_filled;
		auto old_num_buckets = _num_buckets;
		auto old_states      = _states;
		auto old_keys        = _keys;

		_num_filled  = 0;
		_num_buckets = num_buckets;
		_mask        = _num_buckets - 1;
		_states      = new_states;
		_keys        = new_keys;

		std::fill_n(_states, num_buckets, State::INACTIVE);

		_max_probe_length = -1;

		for (int src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
			if (old_states[src_bucket] == State::FILLED) {
				auto& src = old_keys[src_bucket];

				auto dst_bucket = find_empty_bucket(src);
				_states[dst_bucket] = State::FILLED;
				new(_keys + dst_bucket) KeyT(std::move(src));
				_num_filled += 1;

				src.~KeyT();
			}
		}

		free(old_states);
		free(old_keys);
	}

private:
	// Can we fit another element?
	void check_expand_need()
	{
		reserve(_num_filled + 1);
	}

	// Find the bucket with this key, or return (size_t)-1
	size_t find_filled_bucket(const KeyT& key) const
	{
		if (empty()) { return (size_t)-1; } // Optimization

		auto hash_value = _hasher(key);
		for (int offset=0; offset<=_max_probe_length; ++offset) {
			auto bucket = (hash_value + offset) & _mask;
			if (_states[bucket] == State::FILLED) {
				if (_eq(_keys[bucket], key)) {
					return bucket;
				}
			} else if (_states[bucket] == State::INACTIVE) {
				return (size_t)-1; // End of the chain!
			}
		}
		return (size_t)-1;
	}

	// Find the bucket with this key, or return a good empty bucket to place the key in.
	// In the latter case, the bucket is expected to be filled.
	size_t find_or_allocate(const KeyT& key)
	{
		auto hash_value = _hasher(key);
		size_t hole = (size_t)-1;
		int offset=0;
		for (; offset<=_max_probe_length; ++offset) {
			auto bucket = (hash_value + offset) & _mask;

			if (_states[bucket] == State::FILLED) {
				if (_eq(_keys[bucket], key)) {
					return bucket;
				}
			} else if (_states[bucket] == State::INACTIVE) {
				return bucket;
			} else {
				// ACTIVE: keep searching
				if (hole == (size_t)-1) {
					hole = bucket;
				}
			}
		}

		// No key found - but maybe a hole for it
		if (hole != (size_t)-1) {
			return hole;
		}

		// No hole found within _max_probe_length
		for (; ; ++offset) {
			auto bucket = (hash_value + offset) & _mask;

			if (_states[bucket] != State::FILLED) {
				_max_probe_length = offset;
				return bucket;
			}
		}
	}

	// key is not in this map. Find a place to put it.
	size_t find_empty_bucket(const KeyT& key)
	{
		auto hash_value = _hasher(key);
		for (int offset=0; ; ++offset) {
			auto bucket = (hash_value + offset) & _mask;
			if (_states[bucket] != State::FILLED) {
				if (offset > _max_probe_length) {
					_max_probe_length = offset;
				}
				return bucket;
			}
		}
	}

private:
	enum class State : uint8_t
	{
		INACTIVE, // Never been touched
		ACTIVE,   // Is inside a search-chain, but is empty
		FILLED    // Is set with key/value
	};

	HashT   _hasher;
	EqT     _eq;
	State*  _states           = nullptr;
	KeyT*   _keys             = nullptr;
	int  _num_buckets      =  0;
	int  _num_filled       =  0;
	int  _max_probe_length = -1; // Our longest bucket-brigade is this long. ONLY when we have zero elements is this ever negative (-1).
	int  _mask             =  0;  // _num_buckets minus one
};

} // namespace emhash