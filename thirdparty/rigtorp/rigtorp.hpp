// © 2017-2020 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: MIT

/*
HashMap

A high performance hash map. Uses open addressing with linear
probing.

Advantages:
  - Predictable performance. Doesn't use the allocator unless load factor
    grows beyond 50%. Linear probing ensures cash efficency.
  - Deletes items by rearranging items and marking slots as empty instead of
    marking items as deleted. This is keeps performance high when there
    is a high rate of churn (many paired inserts and deletes) since otherwise
    most slots would be marked deleted and probing would end up scanning
    most of the table.

Disadvantages:
  - Significant performance degradation at high load factors.
  - Maximum load factor hard coded to 50%, memory inefficient.
  - Memory is not reclaimed on erase.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rigtorp {

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<void>,
          typename Allocator = std::allocator<std::pair<Key, T>>>
class HashMap {
public:
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<Key, T>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using allocator_type = Allocator;
  using reference = value_type &;
  using const_reference = const value_type &;
  using buckets = std::vector<value_type, allocator_type>;

  template <typename ContT, typename IterVal> struct hm_iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = IterVal;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::forward_iterator_tag;

    bool operator==(const hm_iterator &other) const {
      return other.hm_ == hm_ && other.idx_ == idx_;
    }
    bool operator!=(const hm_iterator &other) const {
      return !(other == *this);
    }

    hm_iterator &operator++() {
      ++idx_;
      advance_past_empty();
      return *this;
    }

    hm_iterator operator++(int) {
      advance_past_empty();
      ++idx_;
      return *this;
    }

    reference operator*() const { return hm_->buckets_[idx_]; }
    pointer operator->() const { return &hm_->buckets_[idx_]; }

  private:
    explicit hm_iterator(ContT *hm) : hm_(hm) { advance_past_empty(); }
    explicit hm_iterator(ContT *hm, size_type idx) : hm_(hm), idx_(idx) {}
    template <typename OtherContT, typename OtherIterVal>
    hm_iterator(const hm_iterator<OtherContT, OtherIterVal> &other)
        : hm_(other.hm_), idx_(other.idx_) {}

    void advance_past_empty() {
      while (idx_ < hm_->buckets_.size() &&
             key_equal()(hm_->buckets_[idx_].first, hm_->empty_key_)) {
        ++idx_;
      }
    }

    ContT *hm_ = nullptr;
    typename ContT::size_type idx_ = 0;
    friend ContT;
  };

  using iterator = hm_iterator<HashMap, value_type>;
  using const_iterator = hm_iterator<const HashMap, const value_type>;

public:

  HashMap(size_type bucket_count = 4, const allocator_type &alloc = allocator_type())
      : empty_key_(Key{}), buckets_(alloc) {
    size_t pow2 = 1;
    while (pow2 < bucket_count) {
      pow2 <<= 1;
    }
    buckets_.resize(pow2, std::make_pair(empty_key_, T()));
    mask_ = uint32_t(buckets_.size() - 1);
  }

  HashMap(size_type bucket_count, key_type empty_key,
          const allocator_type &alloc = allocator_type())
      : empty_key_(empty_key), buckets_(alloc) {
    size_t pow2 = 1;
    while (pow2 < bucket_count) {
      pow2 <<= 1;
    }
    buckets_.resize(pow2, std::make_pair(empty_key_, T()));
    mask_ = uint32_t(buckets_.size() - 1);
  }

  HashMap(const HashMap &other, size_type bucket_count)
      : HashMap(bucket_count, other.empty_key_, other.get_allocator()) {
    for (auto it = other.begin(); it != other.end(); ++it) {
      insert(*it);
    }
  }

  HashMap(const HashMap& other)
      : HashMap(other.size(), other.empty_key_, other.get_allocator()) {
    for (auto it = other.begin(); it != other.end(); ++it) {
      insert(*it);
    }
  }

  HashMap(HashMap&& other)
  {
    if (this != &other) {
      swap(other);
      other.clear();
    }
  }

  HashMap& operator=(const HashMap& other)  {
    if (this != &other) {
      clear();
      empty_key_ = other.empty_key_;
      for (auto it = other.begin(); it != other.end(); ++it) {
        insert(*it);
      }
    }
    return *this;
  }

  HashMap& operator=(HashMap&& other)
  {
    if (this != &other) {
      swap(other);
      other.clear();
    }
    return *this;
  }

  allocator_type get_allocator() const noexcept {
      return buckets_.get_allocator();
  }

  // Iterators
  iterator begin() noexcept { return iterator(this); }

  const_iterator begin() const noexcept { return const_iterator(this); }

  const_iterator cbegin() const noexcept { return const_iterator(this); }

  iterator end() noexcept { return iterator(this, buckets_.size()); }

  const_iterator end() const noexcept {
    return const_iterator(this, buckets_.size());
  }

  const_iterator cend() const noexcept {
    return const_iterator(this, buckets_.size());
  }

  // Capacity
  bool empty() const noexcept { return size() == 0; }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return buckets_.max_size() / 2; }

  float load_factor() const noexcept { return (float)size_ / buckets_.size(); }
  float max_load_factor(float lf = 0.5f) const noexcept { return 0.5f; }

  // Modifiers
  void clear() noexcept {
    for (auto &b : buckets_) {
      if (b.first != empty_key_) {
        b.first = empty_key_;
      }
    }
    size_ = 0;
  }

  std::pair<iterator, bool> insert(const value_type &value) {
    return emplace_impl(value.first, value.second);
  }

  std::pair<iterator, bool> insert(value_type &&value) {
    return emplace_impl(value.first, std::move(value.second));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&... args) {
    return emplace_impl(std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> insert_or_assign(Args &&... args) {
    return emplace_impl(std::forward<Args>(args)...);
  }

  //void erase(iterator it) { erase_impl(it); }
  iterator erase(iterator it) {
      erase_impl(it);
      return ++it;
  }

  size_type erase(const key_type &key) { return erase_impl(key); }

  template <typename K> size_type erase(const K &x) { return erase_impl(x); }

  void swap(HashMap &other) noexcept {
    std::swap(buckets_, other.buckets_);
    std::swap(size_, other.size_);
    std::swap(mask_, other.mask_);
    std::swap(empty_key_, other.empty_key_);
  }

  // Lookup
  mapped_type &at(const key_type &key) { return at_impl(key); }

  template <typename K> mapped_type &at(const K &x) { return at_impl(x); }

  const mapped_type &at(const key_type &key) const { return at_impl(key); }

  template <typename K> const mapped_type &at(const K &x) const {
    return at_impl(x);
  }

  mapped_type &operator[](const key_type &key) {
    return emplace_impl(key).first->second;
  }

  size_type count(const key_type &key) const { return count_impl(key); }

  template <typename K> size_type count(const K &x) const {
    return count_impl(x);
  }

  iterator find(const key_type &key) { return find_impl(key); }

  template <typename K> iterator find(const K &x) { return find_impl(x); }

  const_iterator find(const key_type &key) const { return find_impl(key); }

  template <typename K> const_iterator find(const K &x) const {
    return find_impl(x);
  }

  // Bucket interface
  size_type bucket_count() const noexcept { return buckets_.size(); }

  size_type max_bucket_count() const noexcept { return buckets_.max_size(); }

  // Hash policy
  void rehash(size_type count) {
    count = std::max(count, size() * 2);
    HashMap other(*this, count);
    swap(other);
  }

  void reserve(size_type count) {
    if (count * 2 > buckets_.size() * 1) {
      rehash(count * 2);
    }
  }

  // Observers
  hasher hash_function() const { return hasher(); }

  key_equal key_eq() const { return key_equal(); }

private:
  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace_impl(const K &key, Args &&... args) {
    //assert(!key_equal()(empty_key_, key) && "empty key shouldn't be used");
    reserve(size_ + 1);
    for (size_t idx = key_to_idx(key);; idx = probe_next(idx)) {
      if (key_equal()(buckets_[idx].first, empty_key_)) {
        buckets_[idx].second = mapped_type(std::forward<Args>(args)...);
        buckets_[idx].first = key;
        size_++;
        return {iterator(this, idx), true};
      } else if (key_equal()(buckets_[idx].first, key)) {
        return {iterator(this, idx), false};
      }
    }
  }

  void erase_impl(iterator it) {
    size_t bucket = it.idx_;
    for (size_t idx = probe_next(bucket);; idx = probe_next(idx)) {
      if (key_equal()(buckets_[idx].first, empty_key_)) {
        buckets_[bucket].first = empty_key_;
        size_--;
        return;
      }
      size_t ideal = key_to_idx(buckets_[idx].first);
      if (diff(bucket, ideal) < diff(idx, ideal)) {
        // swap, bucket is closer to ideal than idx
        buckets_[bucket] = buckets_[idx];
        bucket = idx;
      }
    }
  }

  template <typename K> size_type erase_impl(const K &key) {
    auto it = find_impl(key);
    if (it != end()) {
      erase_impl(it);
      return 1;
    }
    return 0;
  }

  template <typename K> mapped_type &at_impl(const K &key) {
    iterator it = find_impl(key);
    if (it != end()) {
      return it->second;
    }
    throw std::out_of_range("HashMap::at");
  }

  template <typename K> const mapped_type &at_impl(const K &key) const {
    return const_cast<HashMap *>(this)->at_impl(key);
  }

  template <typename K> size_t count_impl(const K &key) const {
    return find_impl(key) == end() ? 0 : 1;
  }

  template <typename K> iterator find_impl(const K &key) {
    //assert(!key_equal()(empty_key_, key) && "empty key shouldn't be used");
    for (size_t idx = key_to_idx(key);; idx = probe_next(idx)) {
      if (key_equal()(buckets_[idx].first, key)) {
        return iterator(this, idx);
      }
      if (key_equal()(buckets_[idx].first, empty_key_)) {
        return end();
      }
    }
  }

  template <typename K> const_iterator find_impl(const K &key) const {
    return const_cast<HashMap *>(this)->find_impl(key);
  }

  template <typename K>
  size_t key_to_idx(const K &key) const noexcept(noexcept(hasher()(key))) {
    return hasher()(key) & mask_;
  }

  size_t probe_next(size_t idx) const noexcept {
    return (idx + 1) & mask_;
  }

  size_t diff(size_t a, size_t b) const noexcept {
    return (mask_ + 1 + (a - b)) & mask_;
  }

private:
  key_type empty_key_;
  uint32_t size_ = 0;
  uint32_t mask_ = 0;
  buckets buckets_;
};
} // namespace rigtorp
