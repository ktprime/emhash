/*
 * Fast open addressing linked hash table with tombstone bit map.
 *
 * Copyright (c) 2020 Michael Clark <michaeljclark@mac.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cassert>

#include <utility>
#include <functional>

namespace zedland {

/*
 * This open addressing linkedhashmap uses a 2-bit entry per slot bitmap
 * that eliminates the need for empty and deleted key sentinels.
 * The hashmap has a simple array of key and value pairs and the
 * tombstone bitmap, which are allocated in a single call to malloc.
 *
 * The linkedhashmap entries contains a bidirectional linked list for
 * predictable iteration order based on order of insertion.
 */

template <class Key, class Value, class Offset = int32_t,
          class Hash = std::hash<Key>,
          class Pred = std::equal_to<Key>>
struct linkedhashmap
{
    static const size_t default_size =    (2<<3);  /* 16 */
    static const size_t load_factor =     (2<<15); /* 0.5 */
    static const size_t load_multiplier = (2<<16); /* 1.0 */

    static inline Hash _hasher;
    static inline Pred _compare;

    struct data_type {
        Key first;
        Value second;
        Offset prev;
        Offset next;
    };

    typedef Key key_type;
    typedef Value mapped_type;
    typedef std::pair<Key, Value> value_type;
    typedef Hash hasher;
    typedef Pred key_equal;
    typedef Offset offset_type;
    typedef data_type& reference;
    typedef const data_type& const_reference;

    enum : offset_type { empty_offset = offset_type(-1) };

    size_t used;
    size_t tombs;
    size_t limit;
    offset_type head;
    offset_type tail;
    data_type *data;
    uint64_t *bitmap;

    /*
     * scanning iterator
     */

    struct iterator
    {
        linkedhashmap *h;
        size_t i;

        iterator& operator++() { i = h->data[i].next; return *this; }
        iterator operator++(int) { iterator r = *this; ++(*this); return r; }
        data_type& operator*() { return h->data[i]; }
        data_type* operator->() { return &h->data[i]; }
        bool operator==(const iterator &o) const { return h == o.h && i == o.i; }
        bool operator!=(const iterator &o) const { return h != o.h || i != o.i; }
    };

    /*
     * constructors and destructor
     */

    inline linkedhashmap() : linkedhashmap(default_size) {}
    inline linkedhashmap(size_t initial_size) :
        used(0), tombs(0), limit(initial_size),
        head(empty_offset), tail(empty_offset)
    {
        size_t data_size = sizeof(data_type) * initial_size;
        size_t bitmap_size = initial_size >> 2;
        size_t total_size = data_size + bitmap_size;

        assert(is_pow2(initial_size));

        data = (data_type*)malloc(total_size);
        bitmap = (uint64_t*)((char*)data + data_size);
        memset(data, 0, total_size);
    }
    inline ~linkedhashmap() { free(data); }

    /*
     * copy constructor and assignment operator
     */

    inline linkedhashmap(const linkedhashmap &o) :
        used(o.used), tombs(o.tombs), limit(o.limit),
        head(o.head), tail(o.tail)
    {
        size_t data_size = sizeof(data_type) * limit;
        size_t bitmap_size = limit >> 2;
        size_t total_size = data_size + bitmap_size;

        data = (data_type*)malloc(total_size);
        bitmap = (uint64_t*)((char*)data + data_size);
        memcpy(data, o.data, total_size);
    }

    inline linkedhashmap(linkedhashmap &&o) :
        used(o.used), tombs(o.tombs), limit(o.limit),
        head(o.head), tail(o.tail),
        data(o.data), bitmap(o.bitmap)
    {
        o.data = nullptr;
        o.bitmap = nullptr;
    }

    inline linkedhashmap& operator=(const linkedhashmap &o)
    {
        free(data);

        used = o.used;
        tombs = o.tombs;
        limit = o.limit;
        head = o.head;
        tail = o.tail;

        size_t data_size = sizeof(data_type) * limit;
        size_t bitmap_size = limit >> 2;
        size_t total_size = data_size + bitmap_size;

        data = (data_type*)malloc(total_size);
        bitmap = (uint64_t*)((char*)data + data_size);
        memcpy(data, o.data, total_size);

        return *this;
    }

    inline linkedhashmap& operator=(linkedhashmap &&o)
    {
        data = o.data;
        bitmap = o.bitmap;
        used = o.used;
        tombs = o.tombs;
        limit = o.limit;
        head = o.head;
        tail = o.tail;

        o.data = nullptr;
        o.bitmap = nullptr;

        return *this;
    }

    /*
     * member functions
     */

    inline size_t size() { return used; }
    inline size_t capacity() { return limit; }
    inline size_t load() { return (used + tombs) * load_multiplier / limit; }
    inline size_t index_mask() { return limit - 1; }
    inline size_t hash_index(uint64_t h) { return h & index_mask(); }
    inline size_t key_index(Key key) { return hash_index(_hasher(key)); }
    inline hasher hash_function() const { return _hasher; }
    inline iterator begin() { return iterator{ this, size_t(head) }; }
    inline iterator end() { return iterator{ this, size_t(empty_offset) }; }

    /*
     * bit manipulation helpers
     */

    enum bitmap_state {
        available = 0, occupied = 1, deleted = 2, recycled = 3
    };
    static inline size_t bitmap_idx(size_t i) { return i >> 5; }
    static inline size_t bitmap_shift(size_t i) { return ((i << 1) & 63); }
    static inline bitmap_state bitmap_get(uint64_t *bitmap, size_t i)
    {
        return (bitmap_state)((bitmap[bitmap_idx(i)] >> bitmap_shift(i)) & 3);
    }
    static inline void bitmap_set(uint64_t *bitmap, size_t i, uint64_t value)
    {
        bitmap[bitmap_idx(i)] |= (value << bitmap_shift(i));
    }
    static inline void bitmap_clear(uint64_t *bitmap, size_t i, uint64_t value)
    {
        bitmap[bitmap_idx(i)] &= ~(value << bitmap_shift(i));
    }
    static inline bool is_pow2(intptr_t n) { return  ((n & -n) == n); }

    /**
     * the implementation
     */

    void resize_internal(data_type *old_data, uint64_t *old_bitmap,
                         size_t old_size, size_t new_size)
    {
        size_t data_size = sizeof(data_type) * new_size;
        size_t bitmap_size = new_size >> 2;
        size_t total_size = data_size + bitmap_size;

        assert(is_pow2(new_size));

        data = (data_type*)malloc(total_size);
        bitmap = (uint64_t*)((char*)data + data_size);
        limit = new_size;
        memset(data, 0, total_size);

        offset_type k = empty_offset;
        for (size_t i = head; i != empty_offset; i = old_data[i].next) {
            data_type *v = old_data + i;
            for (size_t j = key_index(v->first); ; j = (j+1) & index_mask()) {
                if ((bitmap_get(bitmap, j) & occupied) != occupied) {
                    bitmap_set(bitmap, j, occupied);
                    if (i == head) head = j;
                    if (i == tail) tail = j;
                    data[j].first = v->first;
                    data[j].second = v->second;
                    data[j].next = empty_offset;
                    if (k == empty_offset) {
                        data[j].prev = empty_offset;
                    } else {
                        data[j].prev = k;
                        data[k].next = j;
                    }
                    k = j;
                    break;
                }
            }
        }

        tombs = 0;
        free(old_data);
    }

    /* inserts indice link before specified position */
    void insert_link_internal(size_t pos, size_t i)
    {
        if (head == tail && head == empty_offset) {
            head = tail = i;
            data[i].next = data[i].prev = empty_offset;
        } else if (pos == empty_offset) {
            data[i].next = empty_offset;
            data[i].prev = tail;
            data[tail].next = i;
            tail = i;
        } else {
            data[i].next = pos;
            data[i].prev = data[pos].prev;
            if (data[pos].prev != empty_offset) {
                data[data[pos].prev].next = i;
            }
            data[pos].prev = i;
            if (head == pos) head = i;
        }
    }

    /* remove indice link at the specified index */
    void erase_link_internal(size_t i)
    {
        assert(head != empty_offset && tail != empty_offset);
        if (head == tail && i == head) {
            head = tail = empty_offset;
        } else {
            if (head == i) head = data[i].next;
            if (tail == i) tail = data[i].prev;
            if (data[i].prev != empty_offset) {
                data[data[i].prev].next = data[i].next;
            }
            if (data[i].next != empty_offset) {
                data[data[i].next].prev = data[i].prev;
            }
        }
    }

    void clear()
    {
        size_t data_size = sizeof(data_type) * limit;
        size_t bitmap_size = limit >> 2;
        size_t total_size = data_size + bitmap_size;
        memset(data, 0, total_size);
        head = tail = empty_offset;
        used = tombs = 0;
    }

    iterator insert(const value_type& val) { return insert(end(), val); }
    iterator insert(Key key, Value val) { return insert(end(), value_type(key, val)); }
    iterator emplace(const Key key, const Value& val) { return insert(value_type(key, val)); }
    iterator emplace(const Key key, Value&& val) { return insert(std::move(value_type(key, val))); }

    iterator insert(iterator h, const value_type& v)
    {
        for (size_t i = key_index(v.first); ; i = (i+1) & index_mask()) {
            bitmap_state state = bitmap_get(bitmap, i);
            if ((state & occupied) != occupied) {
                bitmap_set(bitmap, i, occupied);
                data[i] = data_type{v.first, v.second};
                insert_link_internal(h.i, i);
                used++;
                if ((state & deleted) == deleted) tombs--;
                if (load() > load_factor) {
                    resize_internal(data, bitmap, limit, limit << 1);
                    for (i = key_index(v.first); ; i = (i+1) & index_mask()) {
                        bitmap_state state = bitmap_get(bitmap, i);
                             if (state == available) abort();
                        else if (state == deleted); /* skip */
                        else if (_compare(data[i].first, v.first)) {
                            return iterator{this, i};
                        }
                    }
                } else {
                    return iterator{this, i};
                }
            } else if (_compare(data[i].first, v.first)) {
                data[i].second = v.second;
                return iterator{this, i};
            }
        }
    }

    Value& operator[](const Key &key)
    {
        for (size_t i = key_index(key); ; i = (i+1) & index_mask()) {
            bitmap_state state = bitmap_get(bitmap, i);
            if ((state & occupied) != occupied) {
                bitmap_set(bitmap, i, occupied);
                data[i].first = key;
                insert_link_internal(empty_offset, i);
                used++;
                if ((state & deleted) == deleted) tombs--;
                if (load() > load_factor) {
                    resize_internal(data, bitmap, limit, limit << 1);
                    for (i = key_index(key); ; i = (i+1) & index_mask()) {
                        bitmap_state state = bitmap_get(bitmap, i);
                             if (state == available) abort();
                        else if (state == deleted); /* skip */
                        else if (_compare(data[i].first, key)) {
                            return data[i].second;
                        }
                    }
                }
                return data[i].second;
            } else if (_compare(data[i].first, key)) {
                return data[i].second;
            }
        }
    }

    iterator find(const Key &key)
    {
        for (size_t i = key_index(key); ; i = (i+1) & index_mask()) {
            bitmap_state state = bitmap_get(bitmap, i);
                 if (state == available)           /* notfound */ break;
            else if (state == deleted);            /* skip */
            else if (_compare(data[i].first, key)) return iterator{this, i};
        }
        return end();
    }

    void erase(Key key)
    {
        for (size_t i = key_index(key); ; i = (i+1) & index_mask()) {
            bitmap_state state = bitmap_get(bitmap, i);
                 if (state == available)           /* notfound */ break;
            else if (state == deleted);            /* skip */
            else if (_compare(data[i].first, key)) {
                bitmap_set(bitmap, i, deleted);
                data[i].second = Value(0);
                bitmap_clear(bitmap, i, occupied);
                erase_link_internal(i);
                used--;
                tombs++;
                return;
            }
        }
    }
};

};
