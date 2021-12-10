//Copyright Nathan Ward 2019.

/* This class is inspired by google's flash_hash_map and skarupke's hash maps
 * see: github.com/abseil/abseil-cpp/absl/container
 *      github.com/skarupke/flat_hash_map
 */ 

#ifndef SIMD_HASH_MAP_HPP
#define SIMD_HASH_MAP_HPP

#include "simd_metadata.hpp"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

static constexpr std::size_t get_next_cap(std::size_t cap_minus_one) noexcept {
  if (cap_minus_one) { 
    if (cap_minus_one < std::numeric_limits<std::uint32_t>::max())
      return (++cap_minus_one << 2);
    else
      return (++cap_minus_one << 1);
  }
  return 128;
}

template <std::size_t Size>
static constexpr std::enable_if_t<(Size % 2) == 0, std::size_t>
next_multiple_of(std::size_t n) noexcept {
  return (n + Size - 1) & (0-Size);
}

template <typename T, std::size_t Size>
struct bucket_group {
  metadata md_[Size];
  T kv_[Size];

  static auto empty_group() noexcept {
    static metadata empty_md[]{
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty,
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty,
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty,
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty,
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty,
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty,
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty,
        mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty, mdEmpty};
    return reinterpret_cast<bucket_group*>(&empty_md);
  }

  constexpr void reset_metadata() noexcept {
    for (auto i{0}; i < Size; ++i) md_[i] = md_states::mdEmpty;
  }
};

template <typename Key, typename T, typename Hash, typename KeyEqual,
          std::size_t GroupSize, std::size_t InitNumGroups>
class simd_hash_base : private Hash,
                       private KeyEqual  //, private Allocator
{
 public:
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<const Key, T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  // using allocator_type = Allocator;
  // using allocator_traits = std::allocator_traits<allocator_type>;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;

 protected:
  using group_t = bucket_group<value_type, GroupSize>;
  using group_pointer = group_t *;
  // using group_allocator = typename allocator_traits::template
  // rebind_alloc<group_t>; using group_pointer = typename
  // group_allocator::pointer;
 protected:
  template <bool Const>
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;

    explicit constexpr Iterator(group_pointer group, size_type index) noexcept
        : index_{index}, group_{group} {}

    constexpr Iterator operator++() noexcept {
      do {
        if ((index_ % GroupSize) == 0) --group_;
        if (index_-- == 0) break;
      } while (isEmpty((group_->md_[index_ % GroupSize])));
      return *this;
    }

    constexpr bool operator!=(const Iterator &rhs) const noexcept {
      return (index_ != rhs.index_);
    }

    constexpr bool operator==(const Iterator &rhs) const noexcept {
      return (index_ != rhs.index_);
    }

    constexpr const value_type &operator*() const noexcept {
      return group_->kv_[index_ % GroupSize];
    }

   #if 0
    constexpr std::enable_if<!Const, value_type &>::type operator*() noexcept {
      return group_->kv_[index_ % GroupSize];
    }

    constexpr std::enable_if<!Const, value_type *>::type operator->() noexcept {
      return group_->kv_ + index_ % GroupSize;
    }
   #endif

    constexpr const value_type *operator->() const noexcept {
      return group_->kv_ + index_ % GroupSize;
    }



    constexpr explicit operator bool() const noexcept {
      return isFull(group_->md_[index_ % GroupSize]);
    }

//   protected:
    friend class Iterator;
    group_pointer group_{nullptr};
    size_type index_{std::numeric_limits<size_type>::max()};
  };

 public:
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

  [[nodiscard]] constexpr iterator begin() noexcept {
    size_type cap = cap_minus_one_ ? cap_minus_one_ + 1 : 0;
    if (cap) return ++iterator{table_ + (cap / GroupSize), cap};
    return end();
  }

  [[nodiscard]] constexpr iterator begin() const noexcept {
    size_type cap = cap_minus_one_ ? cap_minus_one_ + 1 : 0;
    if (cap) return ++iterator{table_ + (cap / GroupSize), cap};
    return end();
  }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
      auto it = begin();
      return const_iterator{it.group_, it.index_};
  }

  [[nodiscard]] constexpr iterator end() noexcept {
    return iterator{nullptr, std::numeric_limits<size_type>::max()};
  }

  [[nodiscard]] constexpr iterator end() const noexcept {
    return iterator{nullptr, std::numeric_limits<size_type>::max()};
  }

  [[nodiscard]] constexpr const_iterator cend() const noexcept { 
      auto it = end();
      return const_iterator{it.group_, it.index_};
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }

  [[nodiscard]] constexpr size_type max_size() const noexcept {
    return cap_minus_one_ ? cap_minus_one_ + 1 : 0;
  }

  [[nodiscard]] constexpr double load_factor() const noexcept {
    return size_ ? ((double)size_ / (cap_minus_one_ + 1)) : 0;
  }

  [[nodiscard]] constexpr double max_load_factor(float lf) const noexcept {
    return size_ ? ((double)size_ / (cap_minus_one_ + 1)) : 0;
  }
  // constexpr const allocator_type& get_allocator() const noexcept
  //{
  //  return static_cast<const allocator_type&>(my_alloc());
  //}

  constexpr const key_equal &key_eq() const noexcept {
    return static_cast<const key_equal &>(my_key_eq());
  }
  constexpr const hasher &hash_function() const noexcept {
    return static_cast<const hasher &>(my_hasher());
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(const key_type &key, Args &&... args) {
      return try_emplace(key, std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(key_type &&key, Args &&...args) {
    return try_emplace(key, std::forward<Args>(args)...);
  }


  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const key_type &key, Args &&... args) {
    if (is_full()) {
      grow();
      return try_emplace(key, std::forward<Args>(args)...);
    }

    size_type hash{hash_key(key)};
    size_type table_index{calc_table_index(hash)};
    size_type group{calc_group(table_index)};
    size_type group_index{calc_group_index(table_index, group)};
    metadata partial_hash{calc_partial_hash(hash)};

    group_t &group_ref = table_[group];

    if (isEmpty(group_ref.md_[group_index])) {
      group_ref.md_[group_index] = partial_hash;
      new (std::addressof(group_ref.kv_[group_index]))
          value_type{std::piecewise_construct, std::forward_as_tuple(key),
                     std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {iterator{std::addressof(group_ref), table_index}, true};
    }

    simd_metadata simd(group_ref.md_);
    auto bit_mask = simd.Match(partial_hash);

    if (bit_mask) {
      for (const auto &i : bit_mask) {
        if (compare_keys(group_ref.kv_[i].first, key)) {
          return {iterator{std::addressof(group_ref), group * GroupSize + i},
                  false};
        }
      }
    }

    auto bucket_index = simd.getFirstOpenBucket();
    if (bucket_index != -1) {
      group_ref.md_[bucket_index] = partial_hash;
      new (std::addressof(group_ref.kv_[bucket_index]))
          value_type{std::piecewise_construct, std::forward_as_tuple(key),
                     std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {
          iterator{std::addressof(group_ref), group * GroupSize + bucket_index},
          true};
    }

    grow();
    return try_emplace(key, std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(key_type &&key, Args &&... args) {
    if (is_full()) {
      grow();
      return try_emplace(std::move(key), std::forward<Args>(args)...);
    }

    size_type hash{hash_key(key)};
    size_type table_index{calc_table_index(hash)};
    size_type group{calc_group(table_index)};
    size_type group_index{calc_group_index(table_index, group)};
    metadata partial_hash{calc_partial_hash(hash)};

    group_t &group_ref = table_[group];

    if (isEmpty(group_ref.md_[group_index])) {
      group_ref.md_[group_index] = partial_hash;
      new (std::addressof(group_ref.kv_[group_index])) value_type{
          std::piecewise_construct, std::forward_as_tuple(std::move(key)),
          std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {iterator{std::addressof(group_ref), table_index}, true};
    }

    simd_metadata simd(group_ref.md_);
    auto bit_mask = simd.Match(partial_hash);

    if (bit_mask) {
      for (const auto &i : bit_mask) {
        if (compare_keys(group_ref.kv_[i].first, key)) {
          return {iterator{std::addressof(group_ref), group * GroupSize + i},
                  false};
        }
      }
    }

    auto bucket_index = simd.getFirstOpenBucket();
    if (bucket_index != -1) {
      group_ref.md_[bucket_index] = partial_hash;
      new (std::addressof(group_ref.kv_[bucket_index])) value_type{
          std::piecewise_construct, std::forward_as_tuple(std::move(key)),
          std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {
          iterator{std::addressof(group_ref), group * GroupSize + bucket_index},
          true};
    }

    grow();
    return try_emplace(std::move(key), std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace_or_assign(const key_type &key,
                                              Args &&... args) {
    if (is_full()) {
      grow();
      return emplace_or_assign(key, std::forward<Args>(args)...);
    }

    size_type hash{hash_key(key)};
    size_type table_index{calc_table_index(hash)};
    size_type group{calc_group(table_index)};
    size_type group_index{calc_group_index(table_index, group)};
    metadata partial_hash{calc_partial_hash(hash)};

    group_t &group_ref = table_[group];

    if (isEmpty(group_ref.md_[group_index])) {
      group_ref.md_[group_index] = partial_hash;
      new (std::addressof(group_ref.kv_[group_index]))
          value_type{std::piecewise_construct, std::forward_as_tuple(key),
                     std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {iterator{std::addressof(group_ref), table_index}, true};
    }

    simd_metadata simd(group_ref.md_);
    auto bit_mask = simd.Match(partial_hash);

    if (bit_mask) {
      for (const auto &i : bit_mask) {
        if (compare_keys(group_ref.kv_[i].first, key)) {
          group_ref.kv_[i] =
              std::move(mapped_type{std::forward<Args>(args)...});
          return {iterator{std::addressof(group_ref), group * GroupSize + i},
                  false};
        }
      }
    }

    auto bucket_index = simd.getFirstOpenBucket();
    if (bucket_index != -1) {
      group_ref.md_[bucket_index] = partial_hash;
      new (std::addressof(group_ref.kv_[bucket_index]))
          value_type{std::piecewise_construct, std::forward_as_tuple(key),
                     std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {
          iterator{std::addressof(group_ref), group * GroupSize + bucket_index},
          true};
    }

    grow();
    return emplace_or_assign(key, std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace_or_assign(key_type &&key, Args &&... args) {
    if (is_full()) {
      grow();
      return emplace_or_assign(std::move(key), std::forward<Args>(args)...);
    }

    size_type hash{hash_key(key)};
    size_type table_index{calc_table_index(hash)};
    size_type group{calc_group(table_index)};
    size_type group_index{calc_group_index(table_index, group)};
    metadata partial_hash{calc_partial_hash(hash)};

    group_t &group_ref = table_[group];

    if (isEmpty(group_ref.md_[group_index])) {
      group_ref.md_[group_index] = partial_hash;
      new (std::addressof(group_ref.kv_[group_index]))
          value_type{std::piecewise_construct, std::forward_as_tuple(key),
                     std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {iterator{std::addressof(group_ref), table_index}, true};
    }

    simd_metadata simd(group_ref.md_);
    auto bit_mask = simd.Match(partial_hash);

    if (bit_mask) {
      for (const auto &i : bit_mask) {
        if (compare_keys(group_ref.kv_[i].first, key)) {
          group_ref.kv_[i] =
              std::move(mapped_type{std::forward<Args>(args)...});
          return {iterator{std::addressof(group_ref), group * GroupSize + i},
                  false};
        }
      }
    }

    auto bucket_index = simd.getFirstOpenBucket();
    if (bucket_index != -1) {
      group_ref.md_[bucket_index] = partial_hash;
      new (std::addressof(group_ref.kv_[bucket_index]))
          value_type{std::piecewise_construct, std::forward_as_tuple(key),
                     std::forward_as_tuple(std::forward<Args>(args)...)};
      ++size_;
      return {
          iterator{std::addressof(group_ref), group * GroupSize + bucket_index},
          true};
    }

    grow();
    return emplace_or_assign(std::move(key), std::forward<Args>(args)...);
  }

    T &operator[](const key_type &key) { return try_emplace(key).first->second; }
    T& operator[](key_type &&key)      { return emplace(std::move(key)).first->second; }

  [[nodiscard]] bool contains(const key_type &key) const noexcept {
    if (empty()) return false;

    size_type hash{hash_key(key)};
    size_type table_index{calc_table_index(hash)};
    size_type group{calc_group(table_index)};
    metadata partial_hash{calc_partial_hash(hash)};

    group_t &group_ref = table_[group];
    simd_metadata simd(group_ref.md_);
    auto bit_mask = simd.Match(partial_hash);

    if (!bit_mask) return false;

    for (const auto &i : bit_mask) {
      if (compare_keys(group_ref.kv_[i].first, key)) return true;
    }
    return false;
  }

  size_t count(const key_type &key) { return contains(key) ? 1 : 0;}

  [[nodiscard]] std::optional<iterator> find(const key_type &key) {
    if (empty()) return std::nullopt;

    size_type hash{hash_key(key)};
    metadata partial_hash{calc_partial_hash(hash)};
    size_type group{calc_group(calc_table_index(hash))};

    group_t &group_ref = table_[group];
    simd_metadata simd(group_ref.md_);
    auto bit_mask = simd.Match(partial_hash);

    if (!bit_mask) return std::nullopt;

    for (const auto &i : bit_mask) {
      if (compare_keys(group_ref.kv_[i].first, key))
        return {iterator{std::addressof(group_ref), group * GroupSize + i}};
    }

    return std::nullopt;
  }

  [[nodiscard]] std::optional<const_iterator> find(const key_type &key) const {
    if (empty()) return std::nullopt;

    size_type hash{hash_key(key)};
    metadata partial_hash{calc_partial_hash(hash)};
    size_type group{calc_group(calc_table_index(hash))};

    group_t &group_ref = table_[group];
    simd_metadata simd(group_ref.md_);
    auto bit_mask = simd.Match(partial_hash);

    if (!bit_mask) return std::nullopt;

    for (const auto &i : bit_mask) {
      if (compare_keys(group_ref.kv_[i].first, key))
        return {
            const_iterator{std::addressof(group_ref), group * GroupSize + i}};
    }

    return std::nullopt;
  }

  constexpr void reserve(size_type size) {
    if (size <= cap_minus_one_ + 1) return;
    rehash(next_multiple_of<GroupSize>(size));
  }

  bool erase(const key_type &key) noexcept {
    if (empty()) return false;

    size_type hash{hash_key(key)};
    size_type group{calc_group(calc_table_index(hash))};
    metadata partial_hash{calc_partial_hash(hash)};

    group_t &group_ref = table_[group];

    simd_metadata simd(group_ref.md_);
    auto bit_mask{simd.Match(partial_hash)};
    if (!bit_mask) return false;

    for (const auto &i : bit_mask) {
      if (compare_keys(group_ref.kv_[i].first, key)) {
        group_ref.md_[i] = md_states::mdEmpty;
        group_ref.kv_[i].~value_type();
        --size_;
        return true;
      }
    }
    return false;
  }

  void clear() noexcept {
    if (table_ == group_t::empty_group()) return;

    auto num_groups{(cap_minus_one_ + 1) / GroupSize};
    for (auto ptr{table_}, end{table_ + num_groups}; ptr != end; ++ptr) {
      for (auto i{0}; i < GroupSize; ++i) {
        if (isFull(ptr->md_[i])) ptr->kv_[i].~value_type();
      }
    }
    deallocate_groups(table_);
    size_ = 0;
    cap_minus_one_ = 0;
    table_ = group_t::empty_group();
  }

 protected:
  constexpr void grow() { rehash(get_next_cap(cap_minus_one_)); }

  constexpr void rehash(size_type num_items) {
    if (num_items == 0) {
      clear();
      return;
    }

    if (num_items == cap_minus_one_ + 1) return;

    auto num_groups{num_items / GroupSize};
    if (num_groups % GroupSize) ++num_groups;

    auto memory = std::malloc(sizeof(group_t) * num_groups);
    auto table_temp{reinterpret_cast<group_pointer>(memory)};

    for (auto ptr{table_temp}, end{table_temp + num_groups}; ptr != end; ++ptr)
      ptr->reset_metadata();

    std::swap(table_, table_temp);
    std::swap(cap_minus_one_, num_items);
    --cap_minus_one_;
    size_ = 0;

    if (table_temp != group_t::empty_group()) {
      if (num_items) ++num_items;
      size_type old_num_groups = num_items / GroupSize;
      if (num_items % GroupSize) ++old_num_groups;

      for (auto ptr{table_temp}, end{table_temp + old_num_groups}; ptr != end;
           ++ptr) {
        for (int i{0}; i < GroupSize; ++i) {
          if (isFull(ptr->md_[i])) {
            auto key_temp = ptr->kv_[i].first;
            try_emplace(std::move(ptr->kv_[i].first),
                        std::move(ptr->kv_[i].second));
            ptr->kv_[i].~value_type();
          }
        }
      }
    }
    deallocate_groups(table_temp);
  }

  constexpr void deallocate_groups(group_pointer ptr) {
    if (ptr == group_t::empty_group()) return;
    std::free(ptr);
  }

  constexpr bool is_full() const noexcept {
    if (cap_minus_one_) return size_ == cap_minus_one_ + 1;
    return true;
  }

  template <typename K1, typename K2>
  constexpr bool compare_keys(const K1 &key1, const K2 &key2) const noexcept {
    return my_key_eq()(key1, key2);
  }

  template <typename K>
  constexpr size_type hash_key(const K &key) const noexcept {
    return my_hasher()(key);
  }

  constexpr metadata calc_partial_hash(const size_type hash) const noexcept {
    return (metadata)(hash & 0x7F);
  }

  constexpr size_type calc_group(const size_type index) const noexcept {
    return index / GroupSize;
  }

  constexpr size_type calc_table_index(const size_type hash) const noexcept {
    return hash & cap_minus_one_;
  }

  constexpr size_type calc_group_index(const size_type index,
                                       const size_type group) const noexcept {
    return (index - group * GroupSize);
  }

  constexpr const hasher &my_hasher() const noexcept {
    return std::get<0>(settings_);
  }

  constexpr const key_equal &my_key_eq() const noexcept {
    return std::get<1>(settings_);
  }
  // constexpr allocator_type& my_alloc() noexcept
  //{
  //  return std::get<2>(settings_);
  //}
 public:
  ~simd_hash_base() noexcept { clear(); }

 protected:
  size_type size_{0};
  size_type cap_minus_one_{0};
  group_pointer table_{group_t::empty_group()};
  std::tuple<hasher, key_equal> settings_{hasher{}, key_equal{}};

}; //simd_hash_base

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class simd_hash_map
    : public simd_hash_base<Key, T, Hash, KeyEqual, simd_metadata::size, 0> {};

#endif //SIMD_HASH_MAP_HPP
