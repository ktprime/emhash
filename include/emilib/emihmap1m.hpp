// LICENSE:
// version 1.2.0
// https://github.com/ktprime/emhash/blob/master/include/emilib/emihmap1.hpp
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

// Opt-in variant of emihmap1 (EMILIB1_META32) with a 32B metadata word per 16-slot group:
// all 16 slots are usable (no in-group probe-bound byte) and the overflow bookkeeping lives
// next to the states instead of in separate arrays.
//
//   states[16] | offsets[8] | cnt[8]
//   offsets: 8 bytes, one per slot pair -- the search distance of
//           the pair: how many groups a lookup for a key homed there must scan (1 = all
//           its keys live in the home group; emihmap2-style monotonic max, but 8 bounds
//           per group instead of 1 per 16 main buckets)
//   cnt:    8 bytes, one per slot pair -- the number of live keys homed at that pair that
//           live OUTSIDE the home group (keys still in the home group are not counted)
//
// A lookup reads the search distance once -- offset[home_slot/2] of the home group, where
// home_slot = hash & mask (the exact slot, not aligned to the group start). Distance is
// recorded lazily: pairs whose keys never left the home group (and were placed in their
// own two slots) keep distance 0, and a find for such a pair just compares the pair's two
// state bytes -- no SIMD group scan. Otherwise the walk scans the jump-probe
// sequence home, home+steps 1..dist-1 (triangular offsets, then a fixed odd stride;
// insert and lookup share get_next_bucket so a placement at step k is found at step k).
// The walk is read-only: no per-group counters are touched, so misses and duplicate hits
// never need bookkeeping or undo. Inserts only write on actual overflow placement (cnt++
// for placements beyond the home group and search-distance extension) or when a lazy pair
// places into a foreign slot; erases do cnt--, and a counter falling back to 0 collapses
// the chain to the home group (what emihmap2 never does). erase(iterator)
// rehashes the key once to find its home pair, then does an O(1) update. Counters and
// distances saturate (255) and stay sticky there, so the search distance can
// only ever be too large (perf loss), never wrongly reset (correctness is safe). Freed
// slots always return to EEMPTY: probe termination is decided by the search distance and
// the tail ESENTINEL, never by a slot state, so no tombstones are needed.
//
// It lives in its own namespace (emilib1m) so benchmarks can instantiate it next to the
// default emilib::HashMap in one translation unit; emihmap1.hpp only pulls it in (and
// aliases emilib::HashMap to it) when EMILIB1_META32 is defined.

#include "config.h"

namespace emilib1m {

// The insert chain (emplace -> do_insert -> find_or_allocate) is force-inlined for
// both compilers. clang leaves those calls out of line in this TU shape and loses
// ~11-14% on a fixed-size erase+insert loop without the hint; gcc already inlines
// them in the bench/framework TUs, but in the big ebench TU it leaves find_or_allocate
// out of line too and loses ~25% on insert_hit / insert_l3_cache / insert_no_reserve
// (measured 2026-11-26), so the hint is unconditional. Re-verify the framework
// numbers if gcc regresses again.
#ifndef EMILIB1_INSERT_INLINE
#define EMILIB1_INSERT_INLINE EMH_INLINE
#endif

enum State : int8_t {
    EEMPTY = -128,
    EDELETE = EEMPTY + 1,
    EFILLED = EDELETE + 1,
    ESENTINEL = 127,
    STATE_BITS = 1,
};

#ifndef AVX2_EHASH
const static auto simd_empty = _mm_set1_epi8(EEMPTY);
const static auto simd_delete = _mm_set1_epi8(EDELETE);
const static auto simd_filled = _mm_set1_epi8(EFILLED);

#define SET1_EPI8 _mm_set1_epi8
#define SET1_EPI32 _mm_set1_epi32
#define LOAD_EPI8 _mm_loadu_si128
#define MOVEMASK_EPI8 _mm_movemask_epi8
#define CMPEQ_EPI8 _mm_cmpeq_epi8
#define CMPGT_EPI8 _mm_cmpgt_epi8
#else
const static auto simd_empty = _mm256_set1_epi8(EEMPTY);
const static auto simd_delete = _mm256_set1_epi8(EDELETE);
const static auto simd_filled = _mm256_set1_epi8(EFILLED);

#define SET1_EPI8 _mm256_set1_epi8
#define LOAD_EPI8 _mm256_loadu_si256
#define MOVEMASK_EPI8 _mm256_movemask_epi8
#define CMPEQ_EPI8 _mm256_cmpeq_epi8
#define CMPGT_EPI8 _mm256_cmpgt_epi8
#endif

// find filled or empty
constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);
// A group holds 16 slots. The old 15+1 layout kept a per-home probe bound in byte 15; the
// 32B meta below replaces it with per-class overflow counters, so all 16 bytes are slots.
constexpr static uint32_t group_bmask = (1u << simd_bytes) - 1;

// The fingerprint vector: one aligned 16-byte load from the shared 4KB broadcast
// table (config.h) on the SSE2 width; SET1_EPI8 for wider vectors.
static EMH_INLINE decltype(simd_empty) fp_vector(int8_t fp) noexcept {
    if constexpr (sizeof(simd_empty) == 16) {
        return *reinterpret_cast<const __m128i*>(fp_broadcast.bytes[static_cast<unsigned char>(fp)]);
    } else {
        return SET1_EPI8(fp);
    }
}

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>,
          typename AllocT = std::allocator<std::pair<KeyT, ValueT>>>
class HashMap {
private:
    using htype = HashMap<KeyT, ValueT, HashT, EqT, AllocT>;

    // One 32B metadata word per group: states | per-pair probe bounds | per-pair counters.
    // 32B alignment keeps every group inside a single cache line half.
    // 32B metadata word per group: 16 state bytes, one byte-sized probe bound per slot
    // pair and one 8bit overflow counter per slot pair.
    // 2B/slot of metadata. The bound byte saturates at 255 and stays sticky there;
    // a 255 bound means "255 or more steps": lookups then walk the whole jump cycle
    // instead of trusting it. cnt == 255 is sticky ("255 or more"): neither side moves
    // it, so after saturation the bound can only ever be too large, never wrongly reset
    // while elements still overflow. The states word needs no alignment (the SIMD scans
    // use unaligned loads); alignas(16) just pairs it with the counters in one 32B line.
    struct alignas(16) GroupMeta {
        int8_t states[simd_bytes];       // fingerprint/state per slot
        uint8_t offsets[simd_bytes / 2]; // probe bound per slot pair, one byte each
        uint8_t cnt[simd_bytes / 2];     // overflow counter per slot pair, one byte each
    };

    using PairT = std::pair<const KeyT, ValueT>;
    using PairAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<PairT>;
    using PairAllocTraits = std::allocator_traits<PairAlloc>;
    using StateAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<GroupMeta>;
    using StateAllocTraits = std::allocator_traits<StateAlloc>;
    constexpr static uint8_t MXLOAD_FACTOR = 5; // max_load = LOAD_FACTOR / (LOAD_FACTOR + 1)

public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
    using size_t = uint32_t;
#pragma GCC diagnostic pop
    using value_type = PairT;
    using reference = PairT&;
    using const_reference = const PairT&;

    using mapped_type = ValueT;
    using val_type = ValueT;
    using key_type = KeyT;
    using hasher = HashT;
    using key_equal = EqT;
    using allocator_type = AllocT;

    template <typename UType, typename std::enable_if<!std::is_integral<UType>::value, int8_t>::type = 0>
    EMH_INLINE int8_t hash_key2(size_t& main_bucket, const UType& key) const {
        const auto key_hash = emilib_hash_of(_hasher, key);
        // The exact home slot: the slot pair (main_bucket & 15) / 2 selects the probe
        // bound in the home group's offset[] array. Scan sites align down to the group.
        main_bucket = static_cast<size_t>(key_hash) & _mask;
        return static_cast<int8_t>(static_cast<int>(key_hash % 253) + static_cast<int>(EFILLED));
    }

    template <typename UType, typename std::enable_if<std::is_integral<UType>::value, int8_t>::type = 0>
    EMH_INLINE int8_t hash_key2(size_t& main_bucket, const UType& key) const {
        const auto key_hash = emilib_hash_of(_hasher, key);
        main_bucket = static_cast<size_t>(key_hash & _mask);
        // Integral keys take the fingerprint from the raw key (emilib_fp_from_key,
        // config.h): identity-like hashes collapse a top-bit fold to one fingerprint for
        // dense key ranges, while the Fibonacci multiply keeps 253 distinct fingerprints.
        return emilib_fp_from_key(key, static_cast<int>(EFILLED));
    }

#define bucket_to_slot(bucket) bucket

    // ── per-slot-pair overflow bookkeeping, keyed by the home group ──
    static EMH_INLINE size_t home_group(size_t main_bucket) noexcept {
        return main_bucket & ~static_cast<size_t>(simd_bytes - 1);
    }

    // Two adjacent home slots (2i, 2i+1) share one offset/cnt pair.
    static EMH_INLINE uint8_t pair_of(size_t main_bucket) noexcept {
        return static_cast<uint8_t>((main_bucket & (simd_bytes - 1)) >> 1);
    }

    // +1 group holds the ESENTINEL tail that terminates the SIMD scans.
    static size_t meta_count(size_t num_buckets) noexcept { return num_buckets / simd_bytes + 1; }
    static size_t meta_size(size_t num_buckets) noexcept { return meta_count(num_buckets) * sizeof(GroupMeta); }

    EMH_INLINE GroupMeta& group_of(size_t bucket) noexcept { return _groups[bucket / simd_bytes]; }
    EMH_INLINE const GroupMeta& group_of(size_t bucket) const noexcept { return _groups[bucket / simd_bytes]; }

    // bucket is group-aligned at every SIMD-scan site, so this is the scan base.
    EMH_INLINE int8_t* group_states(size_t bucket) noexcept { return group_of(bucket).states; }
    EMH_INLINE const int8_t* group_states(size_t bucket) const noexcept { return group_of(bucket).states; }

    EMH_INLINE int8_t state_of(size_t bucket) const noexcept { return group_of(bucket).states[bucket % simd_bytes]; }

    EMH_INLINE void set_states(size_t bucket, int8_t key_h2) noexcept {
        group_of(bucket).states[bucket % simd_bytes] = key_h2;
    }

    // Search distance of one slot pair: how many groups a lookup for a key homed at this
    // pair must scan -- 1 means everything lives in the home group, k means the chain
    // reaches k-1 groups beyond it. 0 marks a pair that never had a live key: finds skip
    // the scan entirely, inserts can place directly without a duplicate check, and a pair
    // whose last key is erased returns to 0. One byte, saturating at 255, which lookups
    // treat as "walk the whole cycle". A plain byte read/write: no nibble packing or
    // odd/even split, so the lookup path has no shift/mask work at all.
    EMH_INLINE uint8_t get_offset(size_t gbucket, uint8_t pair) const noexcept {
        return group_of(gbucket).offsets[pair];
    }

    EMH_INLINE void set_offset(size_t gbucket, uint8_t pair, size_t distance) noexcept {
        auto& byte = group_of(gbucket).offsets[pair];
        const auto nv = static_cast<uint8_t>(distance < 255 ? distance : 255);
        if (nv > byte)
            byte = nv;
    }

    // Lower the distance when the pair's last overflowed key is erased. This has to
    // write the byte directly: set_offset() only ever raises it, so routing the
    // collapse through it would leave a stale -- and after churn, permanently
    // saturated -- bound behind (find/find_miss would scan the whole cycle forever).
    EMH_INLINE void collapse_offset(size_t gbucket, uint8_t pair) noexcept { group_of(gbucket).offsets[pair] = 1; }

    // Overflow count of one slot pair, one full byte: the number of live keys homed at
    // this pair that live OUTSIDE the home group (keys in the home group are not counted).
    // cnt == 255 is sticky ("255 or more"): neither side moves it, so after saturation
    // the search distance can only stay too large, never be reset while elements overflow.
    EMH_INLINE void inc_cnt(size_t gbucket, uint8_t pair) noexcept {
        auto& byte = group_of(gbucket).cnt[pair];
        if (byte != 255)
            byte += 1;
    }

    EMH_INLINE bool dec_cnt(size_t gbucket, uint8_t pair) noexcept {
        auto& byte = group_of(gbucket).cnt[pair];
        if (byte == 255)
            return false; // sticky: overflowed keys remain, keep the search distance
        return --byte == 0;
    }

    EMH_INLINE void release_slot(size_t bucket) noexcept {
        if (need_explicit_dtor())
            _pairs[bucket_to_slot(bucket)].~PairT();
        // Freed slots go back to EEMPTY: probe termination is decided by the search
        // distance and the tail sentinel, never by a slot state, so no tombstones are
        // needed -- and a tombstone-only map (tested) keeps groups "occupied", which made
        // every lookup walk longer and measured ~10% slower overall.
        set_states(bucket, State::EEMPTY);
        _num_filled -= 1;
    }

    class const_iterator;
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = PairT;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() {}
        iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { _bmask = _from = 0; }

        void init() {
            _from = (_bucket / simd_bytes) * simd_bytes;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= static_cast<size_t>(~((1ull << (_bucket % simd_bytes)) - 1));
            } else {
                _bmask = 0;
            }
        }

        size_t operator-(const iterator& r) const { return _bucket - r._bucket; }

        size_t bucket() const { return _bucket; }

        iterator& operator++() {
            goto_next_element();
            return *this;
        }

        iterator operator++(int) {
            iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->_pairs[bucket_to_slot(_bucket)]; }
        pointer operator->() const { return _map->_pairs + bucket_to_slot(_bucket); }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

    private:
        void goto_next_element() {
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = _map->filled_mask(_from += simd_bytes);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype* _map;
        size_t _bmask = 0;
        size_t _bucket;
        size_t _from;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const PairT;
        using pointer = value_type*;
        using reference = value_type&;

        const_iterator(const iterator& it) // implicit: std containers require iterator -> const_iterator
            : _map(it._map), _bmask(it._bmask), _bucket(it._bucket), _from(it._from) {}
        const_iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        const_iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) {
            _bmask = _from = 0;
        }

        void init() {
            _from = (_bucket / simd_bytes) * simd_bytes;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= static_cast<size_t>(~((1ull << (_bucket % simd_bytes)) - 1));
            } else {
                _bmask = 0;
            }
        }

        size_t bucket() const { return _bucket; }

        size_t operator-(const const_iterator& r) const { return _bucket - r._bucket; }

        const_iterator& operator++() {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->_pairs[bucket_to_slot(_bucket)]; }
        pointer operator->() const { return _map->_pairs + bucket_to_slot(_bucket); }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

    private:
        void goto_next_element() {
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = _map->filled_mask(_from += simd_bytes);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype* _map;
        size_t _bmask = 0;
        size_t _bucket;
        size_t _from;
    };

    // ------------------------------------------------------------------------

    explicit HashMap(size_t n = 4) noexcept { rehash(n); }

    explicit HashMap(const AllocT& alloc) noexcept : _alloc(alloc), _state_alloc(alloc) { rehash(4); }

    HashMap(size_t n, const AllocT& alloc) noexcept : _alloc(alloc), _state_alloc(alloc) { rehash(n); }

    HashMap(const HashMap& other)
        : _alloc(PairAllocTraits::select_on_container_copy_construction(other._alloc)),
          _state_alloc(StateAllocTraits::select_on_container_copy_construction(other._state_alloc)) {
        rehash(1);
        clone(other);
    }

    HashMap(const HashMap& other, const AllocT& alloc) : _alloc(alloc), _state_alloc(alloc) {
        rehash(1);
        clone(other);
    }

    HashMap(HashMap&& other) noexcept {
        rehash(1);
        if (this != &other) {
            swap(other);
        }
    }

    HashMap(HashMap&& other, const AllocT& alloc) noexcept : _alloc(alloc), _state_alloc(alloc) {
        rehash(1);
        if (this != &other) {
            swap(other);
        }
    }

    HashMap(std::initializer_list<value_type> il) noexcept {
        rehash(static_cast<size_t>(il.size()));
        for (auto it = il.begin(); it != il.end(); ++it)
            insert(*it);
    }

    template <class InputIt> HashMap(InputIt first, InputIt last, size_t bucket_count = 4) noexcept {
        rehash(static_cast<size_t>(std::distance(first, last)) + bucket_count);
        for (; first != last; ++first)
            insert(*first);
    }

    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            if constexpr (PairAllocTraits::propagate_on_container_copy_assignment::value) {
                _alloc = other._alloc;
                _state_alloc = other._state_alloc;
            }
            clone(other);
        }
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap() noexcept {
        clear_data();
        _num_filled = 0;
        if (_groups != nullptr) {
            StateAllocTraits::deallocate(_state_alloc, _groups, meta_count(_num_buckets));
            PairAllocTraits::deallocate(_alloc, _pairs, 1 + bucket_to_slot(_num_buckets));
        }
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator_type(_alloc); }

    void clone(const HashMap& other) noexcept {
        if (other.size() == 0) {
            clear();
            return;
        }

        clear_data();

        if (other._num_buckets != _num_buckets) {
            // Release the old buffers BEFORE zeroing _num_buckets: deallocation element
            // counts are derived from it, so it must still hold the old capacity here.
            if (_groups != nullptr) {
                StateAllocTraits::deallocate(_state_alloc, _groups, meta_count(_num_buckets));
                _groups = nullptr;
                PairAllocTraits::deallocate(_alloc, _pairs, 1 + bucket_to_slot(_num_buckets));
                _pairs = nullptr;
            }
            _num_filled = _num_buckets = 0;
            rehash(other._num_buckets);
        }

        if (is_trivially_copyable()) {
            const auto pairs_size = (1 + bucket_to_slot(_num_buckets)) * sizeof(PairT);
            memcpy(reinterpret_cast<char*>(_pairs), reinterpret_cast<const char*>(other._pairs), pairs_size);
        } else {
            for (auto it = other.cbegin(); it.bucket() != _num_buckets; ++it)
                new (_pairs + bucket_to_slot(it.bucket())) PairT(*it);
        }

        _num_filled = other._num_filled;
        memcpy(_groups, other._groups, meta_size(_num_buckets));
    }

    void swap(HashMap& other) noexcept {
        std::swap(_hasher, other._hasher);
        std::swap(_eq, other._eq);
        std::swap(_alloc, other._alloc);
        std::swap(_state_alloc, other._state_alloc);
        std::swap(_groups, other._groups);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
    }

    // -------------------------------------------------------------

    iterator begin() noexcept { return {this, find_filled_slot(0)}; }

    const_iterator cbegin() const noexcept { return {this, find_filled_slot(0)}; }

    const_iterator begin() const noexcept { return cbegin(); }

    iterator end() noexcept { return {this, _num_buckets, false}; }

    const_iterator cend() const noexcept { return {this, _num_buckets, false}; }

    const_iterator end() const noexcept { return cend(); }

    size_t size() const noexcept { return _num_filled; }

    bool empty() const noexcept { return _num_filled == 0; }

    // Returns the number of buckets.
    size_t bucket_count() const noexcept { return _num_buckets; }

    /// Returns average number of elements per bucket.
    float load_factor() const noexcept {
        return _num_buckets ? static_cast<float>(_num_filled) / static_cast<float>(bucket_to_slot(_num_buckets)) : 0.0f;
    }

    float max_load_factor() const noexcept { return static_cast<float>(MXLOAD_FACTOR) / (MXLOAD_FACTOR + 1); }
    void max_load_factor(float) noexcept {}

    hasher hash_function() const noexcept { return _hasher; }
    key_equal key_eq() const noexcept { return _eq; }

    constexpr uint64_t max_size() const { return 1ull << (sizeof(_num_buckets) * 8 - 1); }
    constexpr uint64_t max_bucket_count() const { return max_size(); }

    // ------------------------------------------------------------

    template <typename K = KeyT> EMH_INLINE iterator find(const K& key) noexcept {
        return {this, find_filled_bucket(key)};
    }

    template <typename K = KeyT> EMH_INLINE const_iterator find(const K& key) const noexcept {
        return {this, find_filled_bucket(key)};
    }

    template <typename K = KeyT> EMH_INLINE bool contains(const K& key) const noexcept {
        return find_filled_bucket(key) != _num_buckets;
    }

    template <typename K = KeyT> size_t count(const K& key) const noexcept {
        return find_filled_bucket(key) != _num_buckets;
    }

    template <typename K = KeyT> ValueT& at(const K& key) {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            throw std::out_of_range("emilib1m::HashMap::at(): key not found");
        return _pairs[bucket].second;
    }

    template <typename K = KeyT> const ValueT& at(const K& key) const {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            throw std::out_of_range("emilib1m::HashMap::at(): key not found");
        return _pairs[bucket].second;
    }

    /// Returns the matching ValueT* or nullptr if k isn't found.
    template <typename K = KeyT> ValueT* try_get(const K& key) noexcept {
        auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &_pairs[bucket_to_slot(bucket)].second;
    }

    /// Const version of the above
    template <typename K = KeyT> const ValueT* try_get(const K& key) const noexcept {
        auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &_pairs[bucket_to_slot(bucket)].second;
    }

    /// set value if key exists
    template <typename K = KeyT>
    bool try_set(const K& key, const ValueT& val) noexcept(std::is_nothrow_copy_assignable<ValueT>::value) {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            return false;
        _pairs[bucket_to_slot(bucket)].second = val;
        return true;
    }

    /// set value if key exists (move)
    template <typename K = KeyT>
    bool try_set(const K& key, ValueT&& val) noexcept(std::is_nothrow_move_assignable<ValueT>::value) {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            return false;
        _pairs[bucket_to_slot(bucket)].second = std::move(val);
        return true;
    }

    template <typename Con> bool operator==(const Con& rhs) const noexcept {
        if (size() != rhs.size())
            return false;

        for (auto it = begin(), last = end(); it != last; ++it) {
            auto oi = rhs.find(it->first);
            if (oi == rhs.end() || it->second != oi->second)
                return false;
        }
        return true;
    }

    template <typename Con> bool operator!=(const Con& rhs) const noexcept { return !(*this == rhs); }

    void merge(HashMap& rhs) noexcept {
        if (this == &rhs)
            return;
        if (empty()) {
            *this = std::move(rhs);
            return;
        }

        for (auto rit = rhs.begin(); rit != rhs.end();) {
            auto fit = find(rit->first);
            if (fit == end()) {
                insert_unique(rit->first, std::move(rit->second));
                rhs.erase(rit++);
            } else {
                ++rit;
            }
        }
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    template <typename K, typename V>
    EMILIB1_INSERT_INLINE std::pair<iterator, bool> do_insert(K&& key, V&& val) noexcept {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new (_pairs + slot) PairT(std::forward<K>(key), std::forward<V>(val));
            _num_filled++;
        }
        return {{this, bucket, false}, bempty};
    }

    EMILIB1_INSERT_INLINE std::pair<iterator, bool> do_insert(const value_type& value) noexcept {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new (_pairs + slot) PairT(value);
            _num_filled++;
        }
        return {{this, bucket, false}, bempty};
    }

    EMILIB1_INSERT_INLINE std::pair<iterator, bool> do_insert(value_type&& value) noexcept {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new (_pairs + slot) PairT(std::move(value));
            _num_filled++;
        }
        return {{this, bucket, false}, bempty};
    }

    template <class... Args> EMILIB1_INSERT_INLINE std::pair<iterator, bool> emplace(Args&&... args) noexcept {
        return do_insert(std::forward<Args>(args)...);
    }

    // hint is ignored (the underlying open-addressing layout cannot use it); kept for API parity
    template <class... Args> iterator emplace_hint(const_iterator, Args&&... args) noexcept {
        return emplace(std::forward<Args>(args)...).first;
    }

    EMILIB1_INSERT_INLINE std::pair<iterator, bool> insert(value_type&& value) noexcept {
        return do_insert(std::move(value));
    }

    EMILIB1_INSERT_INLINE std::pair<iterator, bool> insert(const value_type& value) noexcept {
        return do_insert(value);
    }

#if 0
    iterator insert(iterator hint, const value_type& value) noexcept
    {
        (void)hint;
        return do_insert(value).first;
    }
#endif

    // SFINAE-constrained to real iterators: a bare insert(3, 4) must be a
    // compile error instead of dereferencing ints as range iterators
    template <typename Iter, typename = decltype(*std::declval<Iter&>())> void insert(Iter beginc, Iter endc) noexcept {
        rehash(static_cast<size_t>(endc - beginc) + _num_filled);
        for (; beginc != endc; ++beginc)
            do_insert(beginc->first, beginc->second);
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) noexcept {
        return do_insert(key, std::forward<Args>(args)...);
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) noexcept {
        return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    void insert(std::initializer_list<value_type> ilist) noexcept {
        rehash(static_cast<size_t>(ilist.size()) + _num_filled);
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            (void)do_insert(*it);
    }

    template <typename K, typename V> size_t insert_unique(K&& key, V&& val) noexcept {
        check_expand_need();

        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        const auto gbucket = home_group(main_bucket);
        const auto bucket = find_empty_slot(gbucket, 0, gbucket, pair_of(main_bucket));
        set_states(bucket, key_h2);

        new (_pairs + bucket_to_slot(bucket)) PairT(std::forward<K>(key), std::forward<V>(val));
        _num_filled++;
        return bucket;
    }

    size_t insert_unique(const value_type& value) noexcept { return insert_unique(value.first, value.second); }

    template <class M> std::pair<iterator, bool> insert_or_assign(const KeyT& key, M&& val) {
        return do_assign(key, std::forward<M>(val));
    }
    template <class M> std::pair<iterator, bool> insert_or_assign(KeyT&& key, M&& val) {
        return do_assign(std::move(key), std::forward<M>(val));
    }

    template <typename K, typename V> std::pair<iterator, bool> do_assign(K&& key, V&& val) {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        // Check if inserting a new val rather than overwriting an old entry
        if (bempty) {
            new (_pairs + slot) PairT(std::forward<K>(key), std::forward<V>(val));
            _num_filled++;
        } else {
            _pairs[slot].second = std::forward<V>(val);
        }

        return {{this, bucket, false}, bempty};
    }

    bool set_get(const KeyT& key, const ValueT& val, ValueT& oldv) {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bempty) {
            new (_pairs + slot) PairT(key, val);
            _num_filled++;
        } else {
            oldv = _pairs[slot].second;
        }
        return bempty;
    }

    EMH_INLINE ValueT& operator[](const KeyT& key) noexcept {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new (_pairs + slot) PairT(key, std::move(ValueT()));
            _num_filled++;
        }

        return _pairs[slot].second;
    }

    ValueT& operator[](KeyT&& key) noexcept {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new (_pairs + slot) PairT(std::move(key), std::move(ValueT()));
            _num_filled++;
        }

        return _pairs[slot].second;
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    // Force-inlined: left out of line it costs ~80 Ir/call in the erase+insert churn
    // (bench/framework insert_erase, gcc) where the default map's erase inlines for free.
    EMH_INLINE size_t erase(const KeyT& key) noexcept {
        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        auto next_bucket = home_group(main_bucket);
        const auto pair = pair_of(main_bucket);
        const auto dist = get_offset(next_bucket, pair);
        const auto filled = fp_vector(key_h2);

        // Read-only walk; only a real hit touches the counters, so a miss needs no undo.
        {
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(next_bucket)));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & group_bmask;
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(_pairs[bucket_to_slot(fbucket)].first, key)) {
                    erase_update(main_bucket, fbucket);
                    return 1;
                }
                maskf &= maskf - 1;
            }
        }
        if (dist <= 1)
            return 0;
        size_t max_offset = static_cast<size_t>(dist - 1);
        if (dist == 255) // saturated search distance: bound unknown, walk the whole cycle
            max_offset = _num_buckets / simd_bytes + 6;

        for (size_t offset = 1; offset <= max_offset; ++offset) {
            next_bucket = get_next_bucket(next_bucket, offset);
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(next_bucket)));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & group_bmask;
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(_pairs[bucket_to_slot(fbucket)].first, key)) {
                    erase_update(main_bucket, fbucket);
                    return 1;
                }
                maskf &= maskf - 1;
            }
        }
        return 0;
    }

    // erase returning the iterator to the next element (advancing before the erase
    // keeps the walk on the same code path as a normal ++ traversal)
    iterator erase(const const_iterator& cit) noexcept {
        iterator next(this, cit._bucket);
        ++next;
        _erase(cit._bucket);
        return next;
    }

    iterator erase(iterator it) noexcept {
        iterator next = it;
        ++next;
        _erase(it._bucket);
        return next;
    }

    // iterator-shaped fast erase; the raw bucket form is private (an int key
    // would silently convert to a bucket index and erase the wrong element)
    void _erase(const const_iterator& cit) noexcept { _erase(cit._bucket); }

    iterator erase(const_iterator first, const_iterator last) {
        auto iend = cend();
        auto next = first;
        for (; next != last && next != iend;)
            erase(next++);

        return {this, next.bucket()};
    }

    template <typename Pred> size_t erase_if(Pred pred) noexcept {
        auto old_size = size();
        for (auto it = begin(), last = end(); it != last;) {
            if (pred(*it))
                erase(it++);
            else
                ++it;
        }
        return old_size - size();
    }

    static constexpr bool need_explicit_dtor() {
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
    }

    static constexpr bool is_trivially_copyable() {
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
    }

    void clear_meta() noexcept {
        const auto groups = _num_buckets / simd_bytes;
        assert(_num_buckets >= simd_bytes);
        for (size_t g = 0; g <= groups; ++g) {
            std::memset(_groups[g].states, State::EEMPTY, simd_bytes);
            std::memset(_groups[g].offsets, 0, sizeof(_groups[g].offsets));
            std::memset(_groups[g].cnt, 0, sizeof(_groups[g].cnt));
        }
        // tail sentinel: the SIMD scans stop at the end of the table
        std::memset(_groups[groups].states, State::ESENTINEL, simd_bytes);
        _num_filled = 0;
    }

    void clear_data() noexcept {
        if (need_explicit_dtor()) {
            for (auto it = begin(); _num_filled; ++it) {
                const auto bucket = it.bucket();
                _pairs[bucket_to_slot(bucket)].~PairT();
                _num_filled -= 1;
            }
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear() noexcept {
        if (_num_filled) {
            clear_data();
            clear_meta();
        }
        _num_filled = 0;
    }

    void shrink_to_fit() { rehash(_num_filled + _num_filled / MXLOAD_FACTOR + 1); }

    bool reserve(size_t num_elems) noexcept {
        size_t required_buckets = num_elems + num_elems / MXLOAD_FACTOR;
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    void dump_statics() const {
        size_t off[16] = {0};
        const auto num_groups = _num_buckets / simd_bytes;
        for (size_t g = 0; g < num_groups; g++)
            for (size_t p = 0; p < simd_bytes / 2; p++)
                off[get_offset(g * simd_bytes, static_cast<uint8_t>(p))]++;

        size_t total = 0, sums = 0;
        for (size_t i = 0; i < 16; i++) {
            if (off[i] != 0) {
                total += off[i];
                sums += static_cast<size_t>(off[i]) * i; // i is the search distance
                printf("\n%3d %8d %.5lf %3.3lf%%", static_cast<int>(i), static_cast<int>(off[i]),
                       1.0 * off[i] / (_num_buckets / simd_bytes), 100.0 * total / (_num_buckets / simd_bytes));
            }
        }
        printf(", load_factor = %.3f average probe group length PGL = %.4lf\n", load_factor(), 1.0 * sums / total);
    }

    /// Make room for this many elements
    void rehash(size_t num_elems) noexcept {
        const size_t required_buckets = num_elems;
        if (required_buckets < _num_filled)
            return;

#if EMH_STATIS
        if (_num_filled > EMH_STATIS)
            dump_statics();
#endif

        auto num_buckets = _num_filled > (1u << 16) ? (1u << 16) : simd_bytes;
        while (num_buckets < required_buckets) {
            num_buckets *= 2;
        }

        assert(num_buckets < max_size() && num_buckets > _num_filled);

        const auto pairs_size = (1 + bucket_to_slot(num_buckets)) * sizeof(PairT);

        auto* new_pairs = PairAllocTraits::allocate(_alloc, 1 + bucket_to_slot(num_buckets));
        auto* new_groups = StateAllocTraits::allocate(_state_alloc, meta_count(num_buckets));

        auto old_num_filled = _num_filled;
        auto old_groups = _groups;
        auto old_pairs = _pairs;
        auto old_buckets = _num_buckets;

        _num_filled = 0;
        _num_buckets = num_buckets;
        _mask = num_buckets - 1;
        _groups = new_groups;
        _pairs = new_pairs;

        // fill last packet zero (tail sentinel for SIMD scan termination)
        // Only the _groups ESENTINEL marker is needed; key/value of the sentinel
        // are never accessed, so no placement-new is required for non-trivial types.
        if (is_trivially_copyable())
            memset(reinterpret_cast<char*>(_pairs + pairs_size / sizeof(PairT) - 1), 0, sizeof(_pairs[0]));

        clear_meta();

        for (size_t src_bucket = old_buckets - 1; _num_filled < old_num_filled; --src_bucket) {
            if (old_groups[src_bucket / simd_bytes].states[src_bucket % simd_bytes] >= State::EFILLED) {
                auto& src_pair = old_pairs[bucket_to_slot(src_bucket)];

                size_t main_bucket;
                const auto key_h2 = hash_key2(main_bucket, src_pair.first);
                const auto gbucket = home_group(main_bucket);
                const auto pair = pair_of(main_bucket);

                // Fast path: the rebuilt table sits at LF ~0.42, so the home group has a
                // free slot in >99.7% of cases (measured). Scanning it inline keeps the
                // generic find_empty_slot call -- 64 Ir/call, and 58% of the whole insert
                // workload -- off this 109k-times-per-100k-inserts reinsertion loop.
                size_t bucket;
                const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(gbucket)));
                const auto maske = static_cast<uint32_t>(MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec))) & group_bmask;
                if (EMH_LIKELY(maske != 0)) {
                    bucket = gbucket + CTZ(maske);
                    // A foreign slot of the home group means this key is not in its own
                    // pair, so the pair must be searched as far as the home group.
                    if (((bucket & (simd_bytes - 1)) >> 1) != pair)
                        set_offset(gbucket, pair, 1);
                } else {
                    bucket = find_empty_slot(get_next_bucket(gbucket, 1), 1, gbucket, pair);
                }
                prefetch_write(reinterpret_cast<char*>(&_pairs[bucket_to_slot(bucket)]));
                set_states(bucket, key_h2);
                const auto slot = bucket_to_slot(bucket);
                new (_pairs + slot) PairT(std::move(src_pair));
                _num_filled++;
                if (need_explicit_dtor())
                    src_pair.~PairT();
            }
        }

        if (old_groups != nullptr)
            StateAllocTraits::deallocate(_state_alloc, old_groups, meta_count(old_buckets));
        if (old_pairs != nullptr)
            PairAllocTraits::deallocate(_alloc, old_pairs, 1 + bucket_to_slot(old_buckets));
    }

private:
    // raw-bucket fast erase (private: an int key argument would silently
    // convert to a bucket index and erase the wrong element)
    // Rehash the key once to find its home pair, then do an O(1) bookkeeping update.
    void _erase(size_t bucket) noexcept {
        size_t main_bucket;
        hash_key2(main_bucket, _pairs[bucket_to_slot(bucket)].first);
        erase_update(main_bucket, bucket);
    }

    // Shared bookkeeping after a live key's slot is freed: a key living beyond the home
    // group drops the pair's overflow count, and a count reaching 0 collapses the chain
    // back to the home group (distance 1). An erase inside the home group writes nothing:
    // lazy pairs (distance 0) keep 0 -- finds disambiguate via the pair's own slots --
    // and eager pairs keep a distance that still covers any remaining keys.
    EMH_INLINE void erase_update(size_t main_bucket, size_t bucket) noexcept {
        const auto gbucket = home_group(main_bucket);
        const auto pair = pair_of(main_bucket);
        release_slot(bucket);
        if (home_group(bucket) != gbucket) {
            if (dec_cnt(gbucket, pair))
                collapse_offset(gbucket, pair);
        }
    }

    // Can we fit another element?
    inline void check_expand_need() { reserve(_num_filled); }

    // Prefetch for read operations (find). Active by default: emilib1 has a 15/16-slot
    // group and a comparatively long per-group probe step, so overlapping the pair-line
    // fetch with the SIMD metadata scan measurably helps the find path. Define
    // EMILIB1_NO_FIND_PREFETCH to disable.
    inline static void prefetch_read(char* ctrl) {
#if !defined(EMILIB1_NO_FIND_PREFETCH)
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_prefetch(reinterpret_cast<const char*>(ctrl), _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(static_cast<const void*>(ctrl));
#endif
#else
        (void)ctrl;
#endif
    }

    // Prefetch for write operations (insert/erase)
    inline static void prefetch_write(char* ctrl) {
#ifndef EMH_NO_WRITE_PREFETCH
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_prefetch(reinterpret_cast<const char*>(ctrl), _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(static_cast<const void*>(ctrl), 1, 1);
#endif
#else
        (void)ctrl;
#endif
    }

    // Legacy function for backward compatibility
    inline static void prefetch_heap_block(char* ctrl) {
#ifndef EMH_NO_PREFETCH
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_prefetch(reinterpret_cast<const char*>(ctrl), _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(static_cast<const void*>(ctrl));
#endif
#else
        (void)ctrl;
#endif
    }

    // Jump-probe step k: triangular offsets for the first steps, then a fixed odd stride
    // that cycles through every group. Insert and lookup must walk the same sequence, so
    // an element placed at step k is always found at step k; offset[] counts these steps.
    EMH_INLINE size_t get_next_bucket(size_t next_bucket, size_t k) const noexcept {
        const size_t step = (k < 7) ? simd_bytes * k : ((_num_buckets / 8 / simd_bytes) | 1) * simd_bytes;
        return (next_bucket + step) & _mask;
    }

    // Find the bucket with this key, or return _num_buckets
    template <typename K> EMH_INLINE size_t find_filled_bucket(const K& key) const noexcept {
        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        auto next_bucket = home_group(main_bucket);
        const auto pair = pair_of(main_bucket);
        const auto dist = get_offset(next_bucket, pair);
        const auto filled = fp_vector(key_h2);

        // Home group scan: the full 16-slot compare, so the match mask never depends on
        // the metadata load (a pair mask computed from `dist` puts an L1/L2 load on the
        // critical path and measured 18 points slower). `dist` only gates continuation;
        // scanning a lazy pair's whole group is still correct because its keys live in
        // slots that belong to that same group.
        {
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(next_bucket)));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & group_bmask;
            if (maskf) {
                prefetch_read(reinterpret_cast<char*>(&_pairs[bucket_to_slot(next_bucket)]));
                do {
                    const auto fbucket = next_bucket + CTZ(maskf);
                    if (EMH_LIKELY(_eq(_pairs[bucket_to_slot(fbucket)].first, key)))
                        return fbucket;
                } while (maskf &= maskf - 1);
            }
        }
        if (dist <= 1)
            return _num_buckets;
        size_t max_offset = static_cast<size_t>(dist) - 1;
        if (dist == 255) // saturated search distance: bound unknown, walk the whole cycle
            max_offset = _num_buckets / simd_bytes + 6;

        for (size_t offset = 1; offset <= max_offset; ++offset) {
            next_bucket = get_next_bucket(next_bucket, offset);
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(next_bucket)));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & group_bmask;
            if (maskf) {
                prefetch_read(reinterpret_cast<char*>(&_pairs[bucket_to_slot(next_bucket)]));
                do {
                    const auto fbucket = next_bucket + CTZ(maskf);
                    if (EMH_LIKELY(_eq(_pairs[bucket_to_slot(fbucket)].first, key)))
                        return fbucket;
                } while (maskf &= maskf - 1);
            }
        }
        return _num_buckets;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    template <typename K> EMILIB1_INSERT_INLINE size_t find_or_allocate(const K& key, bool& bnew) noexcept {
        size_t required_buckets = _num_filled + _num_filled / MXLOAD_FACTOR;
        if (EMH_LIKELY(required_buckets >= _num_buckets))
            rehash(required_buckets + 2);

        constexpr size_t chole = static_cast<size_t>(-1);
        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        prefetch_write(reinterpret_cast<char*>(&_pairs[bucket_to_slot(main_bucket)]));
        const auto filled = fp_vector(key_h2);
        const auto gbucket = home_group(main_bucket);
        const auto pair = pair_of(main_bucket);
        const auto dist = get_offset(gbucket, pair);
        const auto num_groups = _num_buckets / simd_bytes;

        size_t max_offset = dist == 0 ? 0 : dist - 1;
        if (dist == 255) // saturated search distance: rule out duplicates everywhere
            max_offset = num_groups + 6;

        // Home group: one load serves all three uses -- duplicate check, first free slot,
        // and (for a lazy pair) its own two slots. Rescanning the same word for each use
        // was 25% of the insert cost.
        size_t hole = chole;
        auto next_bucket = gbucket;
        {
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(gbucket)));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & group_bmask;
            while (maskf != 0) {
                const auto b = gbucket + CTZ(maskf);
                if (_eq(_pairs[bucket_to_slot(b)].first, key)) {
                    bnew = false;
                    return b;
                }
                maskf &= maskf - 1;
            }
            const auto maske = static_cast<uint32_t>(MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec))) & group_bmask;
            if (dist == 0) {
                if (maske != 0) {
                    // Lazy pair: prefer its own two slots; a foreign slot of the home
                    // group switches the pair to eager distance 1.
                    const auto own = maske & (static_cast<uint32_t>(3u << (pair * 2)) & group_bmask);
                    const auto ebucket = gbucket + CTZ(own != 0 ? own : maske);
                    prefetch_write(reinterpret_cast<char*>(&_pairs[bucket_to_slot(ebucket)]));
                    if (own == 0)
                        set_offset(gbucket, pair, 1);
                    set_states(ebucket, key_h2);
                    bnew = true;
                    return ebucket;
                }
                // home group full: extend the chain one group out
                const auto ebucket = find_empty_slot(get_next_bucket(gbucket, 1), 1, gbucket, pair);
                set_states(ebucket, key_h2);
                bnew = true;
                return ebucket;
            }
            if (maske != 0)
                hole = gbucket + CTZ(maske);
        }

        if (max_offset == 0) {
            // dist == 1: nothing exists beyond the home group, so the home-group scan has
            // already ruled out duplicates and its first free slot is where the key goes.
            if (hole != chole) {
                set_states(hole, key_h2);
                bnew = true;
                return hole;
            }
            const auto ebucket = find_empty_slot(get_next_bucket(gbucket, 1), 1, gbucket, pair);
            set_states(ebucket, key_h2);
            bnew = true;
            return ebucket;
        }

        // Walk the remaining steps for duplicates, remembering the first free slot (chains
        // can span past holes after erases, so the whole distance is walked before placing).
        for (size_t offset = 1; offset <= max_offset; ++offset) {
            next_bucket = get_next_bucket(next_bucket, offset);
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(next_bucket)));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & group_bmask;
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(_pairs[bucket_to_slot(fbucket)].first, key)) {
                    bnew = false;
                    return fbucket;
                }
                maskf &= maskf - 1;
            }
            if (hole == chole) {
                const auto maske = static_cast<uint32_t>(MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec))) & group_bmask;
                if (maske != 0)
                    hole = next_bucket + CTZ(maske);
            }
        }

        if (hole != chole) {
            // Inside the known distance: the search bound already covers this step count.
            if (home_group(hole) != gbucket)
                inc_cnt(gbucket, pair);
            set_states(hole, key_h2);
            bnew = true;
            return hole;
        }

        // No free slot within the bound: extend the chain to the first free slot along the
        // same sequence and widen the probe bound to the new step count.
        next_bucket = get_next_bucket(next_bucket, max_offset + 1);
        const auto ebucket = find_empty_slot(next_bucket, max_offset + 1, gbucket, pair);
        set_states(ebucket, key_h2);
        bnew = true;
        return ebucket;
    }

    inline size_t filled_mask(size_t gbucket) const noexcept {
        const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(gbucket)));
        return static_cast<size_t>(MOVEMASK_EPI8(CMPGT_EPI8(vec, simd_delete))) & group_bmask;
    }

    // Walk along the jump sequence from (next_bucket, already at step `offset`) until a
    // free slot shows up, then record the placement: the pair's overflow count goes up
    // and the search distance widens to the new step count + 1 (saturating at 255, which
    // lookups treat as "walk the whole cycle"). Steps 0..6 are triangular and from step 7
    // the stride cycles through every group within num_groups steps, so under LF < 1 a
    // free slot is always found by step num_groups + 6 -- placements never go past it.
    // Does not set the slot state -- the caller stores the fingerprint.
    size_t find_empty_slot(size_t next_bucket, size_t offset, size_t gbucket, uint8_t pair) noexcept {
        const auto num_groups = _num_buckets / simd_bytes;
        for (const auto last = num_groups + 7; offset < last; ++offset) {
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(group_states(next_bucket)));
            const auto maske = static_cast<uint32_t>(MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec))) & group_bmask;
            if (maske != 0) {
                const auto probe = next_bucket + CTZ(maske);
                prefetch_write(reinterpret_cast<char*>(&_pairs[probe]));
                if (offset != 0 || (probe & (simd_bytes - 1)) >> 1 != pair) {
                    // Placed beyond the home group (overflowed), or into a foreign slot
                    // of the home group: record eagerly. A placement into the pair's own
                    // slot at step 0 stays lazy -- no metadata write.
                    if (offset != 0)
                        inc_cnt(gbucket, pair);
                    set_offset(gbucket, pair, offset + 1);
                }
                return probe;
            }
            next_bucket = get_next_bucket(next_bucket, offset + 1);
        }
        assert(false); // unreachable below max load factor
        return gbucket;
    }

    size_t find_filled_slot(size_t next_bucket) const noexcept {
        if (EMH_UNLIKELY(_num_filled == 0))
            return _num_buckets;
        // next_bucket -= next_bucket % simd_bytes;
        while (true) {
            const auto maske = filled_mask(next_bucket);
            if (maske != 0)
                return next_bucket + CTZ(maske);
            next_bucket += simd_bytes;
        }
        return 0;
    }

private:
    HashT _hasher;
    EqT _eq;
    PairAlloc _alloc;
    StateAlloc _state_alloc;
    GroupMeta* _groups = nullptr; // 32B per group: states + per-slot distances + per-class counts
    PairT* _pairs = nullptr;
    size_t _num_buckets = 0;
    size_t _mask = 0; // _num_buckets minus one
    size_t _num_filled = 0;
};

} // namespace emilib1m
