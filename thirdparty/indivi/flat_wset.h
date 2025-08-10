/**
 * Copyright 2025 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

#ifndef INDIVI_FLAT_WSET_H
#define INDIVI_FLAT_WSET_H

#include "indivi/hash.h"
#include "indivi/detail/flat_wtable.h"

#include <functional> // for std::equal_to

namespace indivi
{
/*
 * Flat_wset is a fast associative container that stores (unaligned) unordered unique keys.
 * Similar to `std::unordered_set` but using an open-addressing schema,
 * with a dynamically allocated, consolidated array of values and metadata (capacity grows based on power of 2).
 * It is optimized for small sizes (starting at 2, container sizeof is 48 Bytes on 64-bits).
 *
 * Each entry uses 1 additional byte of metadata (to store hash fragments or empty/tombstone markers).
 * While trying to greatly minimize tombstone usage on erase.
 * It doesn't group buckets but still relies on SIMD operations for speed (SSE2 or NEON are mandatory).
 *
 * Come with an optimized 64-bits hash function by default (see `hash.h`).
 * Use a fixed max load factor of 0.8 (a bit lower, to keep find-miss fast on high loads).
 * Iterators are invalidated on usual open-addressing operations (except the end iterator), but never on erase.
 * Search, insertion, and removal of elements have average constant time 𝓞(1) complexity.
 * Best for find hit/miss scenarios, a bit slower for re-inserting and iterating.
 */
template<
  class Key,
  class Hash = indivi::hash<Key>,
  class KeyEqual = std::equal_to<Key> >
class flat_wset
{
public:
  using key_type = Key;
  using value_type = Key;
  using size_type = std::size_t;
  using difference_type = std::make_signed<size_type>::type;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

private:
  using nc_key_type = typename std::remove_const<Key>::type;
  using item_type = nc_key_type;
  using flat_wtable = detail::flat_wtable<key_type, void*, value_type, item_type, size_type, hasher, key_equal>;

  // Members
  flat_wtable mTable;

  size_type group_capa() const noexcept { return mTable.group_capa(); }
  Hash& hash() noexcept { return mTable.hash(); }
  const Hash& hash() const noexcept { return mTable.hash(); }
  KeyEqual& equal() noexcept { return mTable.equal(); }
  const KeyEqual& equal() const noexcept { return mTable.equal(); }

public:
  using iterator = typename flat_wtable::iterator;
  using const_iterator = typename flat_wtable::const_iterator;

  // Ctr/Dtr
  flat_wset() : flat_wset(0)
  {}

  explicit flat_wset(size_type bucket_count, const Hash& hash = Hash(), const key_equal& equal = key_equal())
    : mTable(bucket_count, hash, equal)
  {}

  template< class InputIt >
  flat_wset(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(), const key_equal& equal = key_equal())
    : mTable(first, last, bucket_count, hash, equal)
  {}

  flat_wset(std::initializer_list<value_type> init, size_type bucket_count = 0, const Hash& hash = Hash(), const key_equal& equal = key_equal())
    : mTable(init.begin(), init.end(), bucket_count, hash, equal)
  {}

  flat_wset(const flat_wset& other)
    : mTable(other.mTable)
  {}

  flat_wset(flat_wset&& other)
    noexcept(std::is_nothrow_move_constructible<flat_wtable>::value)
    : mTable(std::move(other.mTable))
  {}

  ~flat_wset() = default;

  // Assignment
  flat_wset& operator=(const flat_wset& other)
  {
    mTable = other.mTable;
    return *this;
  }
  flat_wset& operator=(flat_wset&& other) noexcept(std::is_nothrow_move_assignable<flat_wtable>::value)
  {
    mTable = std::move(other.mTable);
    return *this;
  }
  flat_wset& operator=(std::initializer_list<value_type> ilist)
  {
    clear();
    insert(ilist.begin(), ilist.end());
    return *this;
  }

  // Iterators
  iterator begin() noexcept { return mTable.begin(); }
  const_iterator begin() const noexcept { return mTable.begin(); }
  const_iterator cbegin() const noexcept { return mTable.cbegin(); }

  iterator end() noexcept { return mTable.end(); }
  const_iterator end() const noexcept { return mTable.cend(); }
  const_iterator cend() const noexcept { return mTable.cend(); }

  // Capacity
  bool empty() const noexcept { return mTable.empty(); }
  size_type size() const noexcept { return mTable.size(); }
  size_type max_size() const noexcept { return mTable.max_size(); }

  // Bucket interface
  size_type bucket_count() const noexcept { return mTable.bucket_count(); }
  size_type max_bucket_count() const noexcept { return mTable.max_bucket_count(); }

  // Hash policy
  float load_factor() const noexcept { return mTable.load_factor(); }
  float max_load_factor() const noexcept { return mTable.max_load_factor(); }
  void max_load_factor(float) noexcept { /*for compatibility*/ }

  void rehash(size_type count) { mTable.rehash(count); }
  void reserve(size_type count) { mTable.reserve(count); }

  // Observers
  hasher hash_function() const { return mTable.hash_function(); }
  key_equal key_eq() const { return mTable.key_eq(); }

  // Lookup
  size_type count(const Key& key) const { return mTable.count(key); }
  bool contains(const Key& key) const { return mTable.contains(key); }

  iterator find(const Key& key) { return mTable.find(key); }
  const_iterator find(const Key& key) const { return mTable.find(key); }

  // Modifiers
  void clear() noexcept { mTable.clear(); }

  template< class P >
  std::pair<iterator, bool> insert(P&& value) { return mTable.insert(std::forward<P>(value)); }

  std::pair<iterator, bool> insert(Key&& value) { return mTable.insert(std::move(value)); }

  template< class InputIt >
  void insert(InputIt first, InputIt last) { mTable.insert(first, last); }

  void insert(std::initializer_list<value_type> ilist) { mTable.insert_list(ilist); }

  template< class... Args >
  std::pair<iterator, bool> emplace(Args&&... args) { return mTable.emplace(std::forward<Args>(args)...); }

  iterator erase_(iterator pos) { return mTable.erase_(pos); }
  iterator erase_(const_iterator pos) { return mTable.erase_(pos); }
  // non-standard, see `erase_()`
  void erase(iterator pos) { mTable.erase(pos); }
  void erase(const_iterator pos) { mTable.erase(pos); }

  size_type erase(const Key& key) { return mTable.erase(key); }

  void swap(flat_wset& other) noexcept(noexcept(mTable.swap(other.mTable))) { mTable.swap(other.mTable); }

  // Non-member
  friend void swap(flat_wset& lhs, flat_wset& rhs) noexcept(noexcept(lhs.swap(rhs))) { lhs.swap(rhs); }

  friend bool operator==(const flat_wset& lhs, const flat_wset& rhs) { return lhs.mTable.equal(rhs.mTable); }

  friend bool operator!=(const flat_wset& lhs, const flat_wset& rhs) { return !(lhs.mTable.equal(rhs.mTable)); }

  template< class Pred >
  friend size_type erase_if(flat_wset& set, Pred pred) { return set.mTable.erase_if(pred); }

#ifdef INDIVI_FLAT_W_DEBUG
  bool is_cleared() const noexcept { return mTable.is_cleared(); }
#endif
#ifdef INDIVI_FLAT_W_STATS
  typename flat_wtable::GroupStats get_group_stats() const noexcept { return mTable.get_group_stats(); }
  typename flat_wtable::FindStats get_find_stats() const noexcept { return mTable.get_find_stats(); }
  void reset_find_stats() noexcept { return mTable.reset_find_stats(); }
#endif
};

} // namespace indivi

#endif // INDIVI_FLAT_WSET_H
