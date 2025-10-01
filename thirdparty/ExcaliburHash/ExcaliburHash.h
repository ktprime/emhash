#pragma once

#include <algorithm>
#include <stdint.h>
#include <type_traits>
#include <utility>

#if !defined(EXLBR_ALLOC) || !defined(EXLBR_FREE)
    #if defined(_WIN32)
        // Windows
        #include <xmmintrin.h>
        #define EXLBR_ALLOC(sizeInBytes, alignment) _mm_malloc(sizeInBytes, alignment)
        #define EXLBR_FREE(ptr) _mm_free(ptr)
    #else
    // Posix
        #include <stdlib.h>
        #define EXLBR_ALLOC(sizeInBytes, alignment) aligned_alloc(alignment, sizeInBytes)
        #define EXLBR_FREE(ptr) free(ptr)
    #endif
#endif

#if !defined(EXLBR_ASSERT)
    #include <assert.h>
    #define EXLBR_ASSERT(expression) assert(expression)
/*
    #define EXLBR_ASSERT(expr)                                                                                                             \
        do                                                                                                                                 \
        {                                                                                                                                  \
            if (!(expr))                                                                                                                   \
                _CrtDbgBreak();                                                                                                            \
        } while (0)
*/
#endif

#if !defined(EXLBR_RESTRICT)
    #define EXLBR_RESTRICT __restrict
#endif

#if defined(__GNUC__)
    #if !defined(EXLBR_LIKELY)
        #define EXLBR_LIKELY(x) __builtin_expect(!!(x), 1)
    #endif
    #if !defined(EXLBR_UNLIKELY)
        #define EXLBR_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #endif
#else
    #if !defined(EXLBR_LIKELY)
        #define EXLBR_LIKELY(x) (x)
    #endif
    #if !defined(EXLBR_UNLIKELY)
        #define EXLBR_UNLIKELY(x) (x)
    #endif
#endif

#include "ExcaliburKeyInfo.h"

namespace Excalibur
{

/*

TODO: Description

TODO: Design descisions/principles

TODO: Memory layout

*/
template <typename TKey, typename TValue, unsigned kNumInlineItems = 1, typename TKeyInfo = KeyInfo<TKey>> class HashTable
{
    struct has_values : std::bool_constant<!std::is_same<std::nullptr_t, typename std::remove_reference<TValue>::type>::value>
    {
    };

    static inline constexpr uint32_t k_MinNumberOfBuckets = 16;

    template <typename T, class... Args> static T* construct(void* EXLBR_RESTRICT ptr, Args&&... args)
    {
        return new (ptr) T(std::forward<Args>(args)...);
    }
    template <typename T> static void destruct(T* EXLBR_RESTRICT ptr) { ptr->~T(); }

    template <bool hasValue, typename dummy = void> struct Storage
    {
    };

    template <typename dummy> struct Storage<true, dummy>
    {
        struct TItem
        {
            using TValueStorage = typename std::aligned_storage<sizeof(TValue), alignof(TValue)>::type;
            TKey m_key;
            TValueStorage m_value;

            inline TItem(TKey&& other) noexcept
                : m_key(std::move(other))
            {
            }

            [[nodiscard]] inline bool isValid() const noexcept { return TKeyInfo::isValid(m_key); }
            [[nodiscard]] inline bool isEmpty() const noexcept { return TKeyInfo::isEqual(TKeyInfo::getEmpty(), m_key); }
            [[nodiscard]] inline bool isTombstone() const noexcept { return TKeyInfo::isEqual(TKeyInfo::getTombstone(), m_key); }
            [[nodiscard]] inline bool isEqual(const TKey& key) const noexcept { return TKeyInfo::isEqual(key, m_key); }

            [[nodiscard]] inline TKey* key() noexcept { return &m_key; }
            [[nodiscard]] inline TValue* value() noexcept
            {
                TValue* value = reinterpret_cast<TValue*>(&m_value);
                return std::launder(value);
            }
        };
    };

    template <typename dummy> struct Storage<false, dummy>
    {
        struct TItem
        {
            TKey m_key;

            inline TItem(TKey&& other) noexcept
                : m_key(std::move(other))
            {
            }
            [[nodiscard]] inline bool isValid() const noexcept { return TKeyInfo::isValid(m_key); }
            [[nodiscard]] inline bool isEmpty() const noexcept { return TKeyInfo::isEqual(TKeyInfo::getEmpty(), m_key); }
            [[nodiscard]] inline bool isTombstone() const noexcept { return TKeyInfo::isEqual(TKeyInfo::getTombstone(), m_key); }
            [[nodiscard]] inline bool isEqual(const TKey& key) const noexcept { return TKeyInfo::isEqual(key, m_key); }

            [[nodiscard]] inline TKey* key() noexcept { return &m_key; }
            // inline TValue* value() noexcept{ return nullptr; }
        };
    };

    using TItem = typename Storage<has_values::value>::TItem;

    template <typename T> static inline T shr(T v, T shift) noexcept
    {
        static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value, "Type T should be an integral unsigned type.");
        return (v >> shift);
    }

    [[nodiscard]] inline uint32_t nextPow2(uint32_t v) noexcept
    {
        EXLBR_ASSERT(v != 0);
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return v;
    }

    [[nodiscard]] static inline size_t align(size_t cursor, size_t alignment) noexcept
    {
        return (cursor + (alignment - 1)) & ~(alignment - 1);
    }
    [[nodiscard]] static inline bool isPointerAligned(void* cursor, size_t alignment) noexcept
    {
        return (uintptr_t(cursor) & (alignment - 1)) == 0;
    }

    template <typename TIterator> struct IteratorHelper
    {
        [[nodiscard]] static TIterator begin(const HashTable& ht) noexcept
        {
            if (ht.empty())
            {
                return end(ht);
            }

            TItem* EXLBR_RESTRICT item = ht.m_storage;
            while (true)
            {
                if (item->isValid())
                {
                    return TIterator(&ht, item);
                }
                item++;
            }
        }

        [[nodiscard]] static TIterator end(const HashTable& ht) noexcept
        {
            const uint32_t numBuckets = ht.m_numBuckets;
            TItem* const endItem = ht.m_storage + numBuckets;
            return TIterator(&ht, endItem);
        }
    };

    inline void moveFrom(HashTable&& other)
    {
        // note: the current hash table is supposed to be destroyed/non-initialized
        if (!other.isUsingInlineStorage())
        {
            // if we are not using inline storage than it's a simple pointer swap
            constructInline(TKeyInfo::getEmpty());
            m_storage = other.m_storage;
            m_numBuckets = other.m_numBuckets;
            m_numElements = other.m_numElements;
            m_numTombstones = other.m_numTombstones;
            other.m_storage = nullptr;
            // don't need to zero rest of the members because dtor doesn't use them
            // other.m_numBuckets = 0;
            // other.m_numElements = 0;
            // other.m_numTombstones = 0;
        }
        else
        {
            // if using inline storage than let's move items from one inline storage into another
            TItem* otherInlineItems = reinterpret_cast<TItem*>(&other.m_inlineStorage);
            TItem* inlineItems = moveInline(otherInlineItems);

            m_storage = inlineItems;
            m_numBuckets = other.m_numBuckets;
            m_numElements = other.m_numElements;
            m_numTombstones = other.m_numTombstones;
            // note: other's online items will be destroyed automatically when its dtor called
            // other.m_storage = nullptr;
            // other.m_numBuckets = 0;
            // other.m_numElements = 0;
            // other.m_numTombstones = 0;
        }
    }

    inline void copyFrom(const HashTable& other)
    {
        if (other.empty())
        {
            return;
        }

        const uint32_t numBuckets = other.m_numBuckets;
        TItem* EXLBR_RESTRICT item = other.m_storage;
        TItem* const enditem = item + numBuckets;
        for (; item != enditem; item++)
        {
            if (item->isValid())
            {
                if constexpr (has_values::value)
                {
                    emplace(*item->key(), *item->value());
                }
                else
                {
                    emplace(*item->key());
                }
            }
        }
    }

    [[nodiscard]] inline bool isUsingInlineStorage() const noexcept
    {
        const TItem* inlineStorage = reinterpret_cast<const TItem*>(&m_inlineStorage);
        return (inlineStorage == m_storage);
    }

    template <class... Args> inline TItem* constructInline(Args&&... args)
    {
        TItem* inlineItems = reinterpret_cast<TItem*>(&m_inlineStorage);
        for (unsigned i = 0; i < kNumInlineItems; i++)
        {
            construct<TItem>((inlineItems + i), std::forward<Args>(args)...);
        }
        return inlineItems;
    }

    inline TItem* moveInline(TItem* otherInlineItems)
    {
        TItem* inlineItems = reinterpret_cast<TItem*>(&m_inlineStorage);

        if constexpr (has_values::value)
        {
            // move all keys and valid values
            for (unsigned i = 0; i < kNumInlineItems; i++)
            {
                TItem* inlineItem = (inlineItems + i);
                TItem* otherInlineItem = (otherInlineItems + i);
                const bool hasValidValue = otherInlineItem->isValid();
                // move construct key
                inlineItem = construct<TItem>(inlineItem, std::move(*otherInlineItem->key()));

                // move inline storage value (if any)
                if (hasValidValue)
                {
                    TValue* value = inlineItem->value();
                    TValue* otherValue = otherInlineItem->value();
                    // move construct value
                    construct<TValue>(value, std::move(*otherValue));
                }
            }
        }
        else
        {
            // move only keys
            for (unsigned i = 0; i < kNumInlineItems; i++)
            {
                // move construct key
                construct<TItem>((inlineItems + i), std::move(*(otherInlineItems + i)->key()));
            }
        }

        return inlineItems;
    }

    inline uint32_t create(uint32_t numBuckets)
    {
        numBuckets = (numBuckets < k_MinNumberOfBuckets) ? k_MinNumberOfBuckets : numBuckets;

        // numBuckets has to be power-of-two
        EXLBR_ASSERT(numBuckets > 0);
        EXLBR_ASSERT((numBuckets & (numBuckets - 1)) == 0);

        size_t numBytes = sizeof(TItem) * numBuckets;
        // note: 64 to match CPU cache line size
        size_t alignment = std::max(alignof(TItem), size_t(64));
        numBytes = align(numBytes, alignment);
        EXLBR_ASSERT((numBytes % alignment) == 0);

        void* raw = EXLBR_ALLOC(numBytes, alignment);
        EXLBR_ASSERT(raw);
        m_storage = reinterpret_cast<TItem*>(raw);
        EXLBR_ASSERT(raw == m_storage);
        EXLBR_ASSERT(isPointerAligned(m_storage, alignof(TItem)));

        m_numBuckets = numBuckets;
        m_numElements = 0;
        m_numTombstones = 0;

        TItem* EXLBR_RESTRICT item = m_storage;
        TItem* const endItem = item + numBuckets;
        for (; item != endItem; item++)
        {
            construct<TItem>(item, TKeyInfo::getEmpty());
        }

        return numBuckets;
    }

    inline void destroy()
    {
        const uint32_t numBuckets = m_numBuckets;
        TItem* EXLBR_RESTRICT item = m_storage;
        TItem* const endItem = item + numBuckets;
        for (; item != endItem; item++)
        {
            // destroy value if need
            if constexpr (!std::is_trivially_destructible<TValue>::value)
            {
                if (item->isValid())
                {
                    destruct(item->value());
                }
            }

            // note: this won't automatically call value dtors!
            destruct(item);
        }
    }

    inline void destroyAndFreeMemory()
    {
        if constexpr (!std::is_trivially_destructible<TValue>::value || !std::is_trivially_destructible<TKey>::value)
        {
            destroy();
        }

        if (!isUsingInlineStorage())
        {
            EXLBR_FREE(m_storage);
        }
    }

    [[nodiscard]] inline TItem* findImpl(const TKey& key) const noexcept
    {
        EXLBR_ASSERT(!TKeyInfo::isEqual(TKeyInfo::getTombstone(), key));
        EXLBR_ASSERT(!TKeyInfo::isEqual(TKeyInfo::getEmpty(), key));
        const size_t numBuckets = m_numBuckets;
        TItem* const firstItem = m_storage;
        TItem* const endItem = firstItem + numBuckets;
        const size_t hashValue = TKeyInfo::hash(key);
        const size_t bucketIndex = hashValue & (numBuckets - 1);
        TItem* startItem = firstItem + bucketIndex;
        TItem* EXLBR_RESTRICT currentItem = startItem;
        do
        {
            if (EXLBR_LIKELY(currentItem->isEqual(key)))
            {
                return currentItem;
            }

            if (currentItem->isEmpty())
            {
                return endItem;
            }

            currentItem++;
            currentItem = (currentItem == endItem) ? firstItem : currentItem;
        } while (currentItem != startItem);
        return endItem;
    }

  public:
    class IteratorBase
    {
      protected:
        [[nodiscard]] inline const TKey* getKey() const noexcept
        {
            EXLBR_ASSERT(m_item->isValid());
            return m_item->key();
        }
        [[nodiscard]] inline const TValue* getValue() const noexcept
        {
            EXLBR_ASSERT(m_item->isValid());
            return m_item->value();
        }

        static TItem* getNextValidItem(TItem* item, TItem* endItem) noexcept
        {
            do
            {
                item++;
            } while (item < endItem && !item->isValid());
            return item;
        }

        void copyFrom(const IteratorBase& other)
        {
            m_ht = other.m_ht;
            m_item = other.m_item;
        }

      public:
        IteratorBase() = delete;

        IteratorBase(const IteratorBase& other) noexcept
            : m_ht(other.m_ht)
            , m_item(other.m_item)
        {
        }

        IteratorBase(const HashTable* ht, TItem* item) noexcept
            : m_ht(ht)
            , m_item(item)
        {
        }

        bool operator==(const IteratorBase& other) const noexcept
        {
            // note: m_ht comparison is redundant and hence skipped
            return m_item == other.m_item;
        }
        bool operator!=(const IteratorBase& other) const noexcept
        {
            // note: m_ht comparison is redundant and hence skipped
            return m_item != other.m_item;
        }

        IteratorBase& operator++() noexcept
        {
            TItem* endItem = m_ht->m_storage + m_ht->m_numBuckets;
            m_item = getNextValidItem(m_item, endItem);
            return *this;
        }

        IteratorBase operator++(int) noexcept
        {
            IteratorBase res = *this;
            ++*this;
            return res;
        }

      protected:
        const HashTable* m_ht;
        TItem* m_item;
        friend class HashTable<TKey, TValue, kNumInlineItems, TKeyInfo>;
    };

    class IteratorK : public IteratorBase
    {
      public:
        IteratorK() = delete;

        IteratorK(const HashTable* ht, TItem* item) noexcept
            : IteratorBase(ht, item)
        {
        }

        [[nodiscard]] inline const TKey& operator*() const noexcept { return *IteratorBase::getKey(); }
        [[nodiscard]] inline const TKey* operator->() const noexcept { return IteratorBase::getKey(); }
    };

    template <typename TIteratorValue> class TIteratorV : public IteratorBase
    {
      public:
        TIteratorV() = delete;

        TIteratorV(const HashTable* ht, TItem* item) noexcept
            : IteratorBase(ht, item)
        {
        }

        [[nodiscard]] inline TIteratorValue& operator*() const noexcept { return *const_cast<TIteratorValue*>(IteratorBase::getValue()); }
        [[nodiscard]] inline TIteratorValue* operator->() const noexcept { return const_cast<TIteratorValue*>(IteratorBase::getValue()); }
    };

    template <typename TIteratorValue> class TIteratorKV : public IteratorBase
    {
      public:
        // pretty much similar to std::reference_wrapper, but supports late initialization
        template <typename TYPE> struct reference
        {
            TYPE* ptr = nullptr;

            explicit reference(TYPE* _ptr) noexcept
                : ptr(_ptr)
            {
            }

            reference(const reference&) noexcept = default;
            reference(reference&&) noexcept = default;
            reference& operator=(const reference&) noexcept = default;
            reference& operator=(reference&&) noexcept = default;
            void set(TYPE* _ptr) noexcept { ptr = _ptr; }
            TYPE& get() const noexcept
            {
                EXLBR_ASSERT(ptr);
                return *ptr;
            }

            operator TYPE&() const noexcept { return get(); }
        };

        using KeyValue = std::pair<const reference<const TKey>, const reference<TIteratorValue>>;

      private:
        void updateTmpKV() const noexcept
        {
            const reference<const TKey>& refKey = tmpKv.first;
            const_cast<reference<const TKey>&>(refKey).set(IteratorBase::getKey());
            const reference<TIteratorValue>& refVal = tmpKv.second;
            const_cast<reference<TIteratorValue>&>(refVal).set(const_cast<TIteratorValue*>(IteratorBase::getValue()));
        }

      public:
        TIteratorKV() = delete;

        TIteratorKV(const TIteratorKV& other)
            : IteratorBase(other)
            , tmpKv(reference<const TKey>(nullptr), reference<TIteratorValue>(nullptr))
        {
        }

        TIteratorKV& operator=(const TIteratorKV& other) noexcept
        {
            IteratorBase::copyFrom(other);
            // note: we'll automatically update tmpKv on the next access = no need to copy
            return *this;
        }

        TIteratorKV(const HashTable* ht, TItem* item) noexcept
            : IteratorBase(ht, item)
            , tmpKv(reference<const TKey>(nullptr), reference<TIteratorValue>(nullptr))
        {
        }

        [[nodiscard]] inline const TKey& key() const noexcept { return *IteratorBase::getKey(); }
        [[nodiscard]] inline TIteratorValue& value() const noexcept { return *const_cast<TIteratorValue*>(IteratorBase::getValue()); }
        [[nodiscard]] inline KeyValue& operator*() const noexcept
        {
            updateTmpKV();
            return tmpKv;
        }

        [[nodiscard]] inline KeyValue* operator->() const noexcept
        {
            updateTmpKV();
            return &tmpKv;
        }

      private:
        mutable KeyValue tmpKv;
    };

    using IteratorKV = TIteratorKV<TValue>;
    using ConstIteratorKV = TIteratorKV<const TValue>;
    using IteratorV = TIteratorV<TValue>;
    using ConstIteratorV = TIteratorV<const TValue>;

    HashTable(int n = 0) noexcept
        //: m_storage(nullptr)
        : m_numBuckets(kNumInlineItems)
        , m_numElements(0)
        , m_numTombstones(0)
    {
        m_storage = constructInline(TKeyInfo::getEmpty());
    }

    ~HashTable()
    {
        if (m_storage)
        {
            destroyAndFreeMemory();
        }
    }

    inline void clear()
    {
        if (empty())
        {
            return;
        }

        const uint32_t numBuckets = m_numBuckets;
        TItem* EXLBR_RESTRICT item = m_storage;
        TItem* const endItem = item + numBuckets;
        for (; item != endItem; item++)
        {
            // destroy value if need
            if constexpr (!std::is_trivially_destructible<TValue>::value)
            {
                if (item->isValid())
                {
                    destruct(item->value());
                }
            }

            // set key to empty
            *item->key() = TKeyInfo::getEmpty();
        }
        // TODO: shrink if needed?
        m_numElements = 0;
        m_numTombstones = 0;
    }

  private:
    template <typename TK, class... Args> inline std::pair<IteratorKV, bool> emplaceToExisting(size_t numBuckets, TK&& key, Args&&... args)
    {
        EXLBR_ASSERT(numBuckets > 0);
        EXLBR_ASSERT(isPow2(numBuckets));
        const size_t hashValue = TKeyInfo::hash(key);
        const size_t bucketIndex = hashValue & (numBuckets - 1);
        TItem* const firstItem = m_storage;
        TItem* const endItem = firstItem + numBuckets;
        TItem* EXLBR_RESTRICT currentItem = firstItem + bucketIndex;
        TItem* EXLBR_RESTRICT foundTombstoneItem = nullptr;

        while (true)
        {
            // key is already exist
            if (currentItem->isEqual(key))
            {
                return std::make_pair(IteratorKV(this, currentItem), false);
            }

            // if we found an empty bucket, the key doesn't exist in the set.
            if (currentItem->isEmpty())
            {
                TItem* EXLBR_RESTRICT insertItem = ((foundTombstoneItem == nullptr) ? currentItem : foundTombstoneItem);

                if (foundTombstoneItem)
                {
                    m_numTombstones--;
                }

                // move key
                *insertItem->key() = std::move(key);
                // construct value if need
                if constexpr (has_values::value)
                {
                    construct<TValue>(insertItem->value(), std::forward<Args>(args)...);
                }
                m_numElements++;
                return std::make_pair(IteratorKV(this, insertItem), true);
            }

            // if we found a tombstone, remember it.  If key is not exist in the table, we prefer to return tombmstone to minimize probing.
            if (currentItem->isTombstone() && foundTombstoneItem == nullptr)
            {
                foundTombstoneItem = currentItem;
            }

            currentItem++;
            currentItem = (currentItem == endItem) ? firstItem : currentItem;
        }
    }

    inline void reinsert(size_t numBucketsNew, TItem* EXLBR_RESTRICT item, TItem* const enditem) noexcept
    {
        // re-insert existing elements
        for (; item != enditem; item++)
        {
            if (item->isValid())
            {
                if constexpr (has_values::value)
                {
                    emplaceToExisting(numBucketsNew, std::move(*item->key()), std::move(*item->value()));
                }
                else
                {
                    emplaceToExisting(numBucketsNew, std::move(*item->key()));
                }

                // destroy old value if need
                if constexpr ((!std::is_trivially_destructible<TValue>::value) && (has_values::value))
                {
                    destruct(item->value());
                }
            }

            // note: this won't automatically call value dtors!
            destruct(item);
        }
    }

    template <typename TK, class... Args>
    inline std::pair<IteratorKV, bool> emplaceReallocate(uint32_t numBucketsNew, TK&& key, Args&&... args)
    {
        const uint32_t numBuckets = m_numBuckets;
        TItem* storage = m_storage;
        TItem* EXLBR_RESTRICT item = storage;
        TItem* const enditem = item + numBuckets;

        // check if such element is already exist
        // in this case we don't need to do anything
        TItem* existingItem = findImpl(key);
        if (existingItem != enditem)
        {
            return std::make_pair(IteratorKV(this, existingItem), false);
        }

        bool isInlineStorage = isUsingInlineStorage();

        numBucketsNew = create(numBucketsNew);

        //
        // insert a new element (one of the args might still point to the old storage in case of hash table 'aliasing'
        //
        // i.e.
        // auto it = table.find("key");
        // table.emplace("another_key", it->second);   // <--- when hash table grows it->second will point to a memory we are about to free
        std::pair<IteratorKV, bool> it = emplaceToExisting(size_t(numBucketsNew), std::forward<TK>(key), std::forward<Args>(args)...);

        reinsert(size_t(numBucketsNew), item, enditem);

        if (!isInlineStorage)
        {
            EXLBR_FREE(storage);
        }

        return it;
    }

  public:
    template <typename TK, class... Args> inline std::pair<IteratorKV, bool> emplace(TK&& key, Args&&... args)
    {
        static_assert(std::is_same<TKey, typename std::remove_const<typename std::remove_reference<TK>::type>::type>::value,
                      "Expected unversal reference of TKey type. Wrong key type?");

        EXLBR_ASSERT(!TKeyInfo::isEqual(TKeyInfo::getTombstone(), key));
        EXLBR_ASSERT(!TKeyInfo::isEqual(TKeyInfo::getEmpty(), key));
        EXLBR_ASSERT(!TKeyInfo::isEqual(TKeyInfo::getEmpty(), TKeyInfo::getTombstone()));
        uint32_t numBuckets = m_numBuckets;

        // Growth threshold calculation: 75% load factor
        // shr(numBuckets, 1u) = numBuckets/2 (50%)
        // shr(numBuckets, 2u) = numBuckets/4 (25%)
        // Total: 50% + 25% + 1 = 75% + 1
        //
        // We count both live elements AND tombstones because:
        // 1. Tombstones occupy slots and increase probe distances during lookup
        // 2. A table full of tombstones can cause infinite loops in linear probing
        // 3. Tombstones degrade performance almost as much as live elements
        //
        // Growing at 75% occupancy (elements + tombstones) maintains good performance
        // while preventing pathological cases where tombstones dominate the table.
        const uint32_t numBucketsThreshold = shr(numBuckets, 1u) + shr(numBuckets, 2u) + 1;
        if (EXLBR_LIKELY(m_numElements + m_numTombstones < numBucketsThreshold))
        {
            return emplaceToExisting(numBuckets, std::forward<TK>(key), std::forward<Args>(args)...);
        }
        return emplaceReallocate(std::max(numBuckets * 2, 64u), std::forward<TK>(key), std::forward<Args>(args)...);
    }

    float max_load_factor(float lf = 0.75f)
    {
        return 0.75f;
    }

    float load_factor()
    {
        return (float(m_numElements) / float(m_numBuckets));
    }

    [[nodiscard]] inline int count(const TKey& key) const noexcept
    {
        return has(key) ? 1 : 0;
    }

    [[nodiscard]] inline ConstIteratorKV find(const TKey& key) const noexcept
    {
        TItem* item = findImpl(key);
        return ConstIteratorKV(this, item);
    }
    [[nodiscard]] inline IteratorKV find(const TKey& key) noexcept
    {
        TItem* item = findImpl(key);
        return IteratorKV(this, item);
    }

    inline TItem* eraseImpl(const IteratorBase it)
    {
        TItem* EXLBR_RESTRICT item = m_storage;
        TItem* const endItem = item + m_numBuckets;

        if (it == IteratorHelper<IteratorBase>::end(*this))
        {
            return endItem;
        }

        EXLBR_ASSERT(m_numElements != 0);
        m_numElements--;

        if constexpr ((!std::is_trivially_destructible<TValue>::value) && (has_values::value))
        {
            TValue* itemValue = const_cast<TValue*>(it.getValue());
            destruct(itemValue);
        }

        TKey* itemKey = const_cast<TKey*>(it.getKey());
        if (m_numElements == 0)
        {
            // hash table is now empty. it is safe to write empty value insted of tombstone
            *itemKey = TKeyInfo::getEmpty();
            return endItem;
        }

        // overwrite key with empty key
        *itemKey = TKeyInfo::getTombstone();
        m_numTombstones++;
        return IteratorBase::getNextValidItem(it.m_item, endItem);
    }

    inline IteratorKV erase(const IteratorKV& it)
    {
        TItem* item = eraseImpl(it);
        return IteratorKV(this, item);
    }

    inline ConstIteratorKV erase(const ConstIteratorKV& it)
    {
        TItem* item = eraseImpl(it);
        return ConstIteratorKV(this, item);
    }

    inline bool erase(const TKey& key)
    {
        auto it = find(key);
        erase(it);
        return (it != iend());
    }

  private:
    void resize(uint32_t numBucketsNew)
    {
        EXLBR_ASSERT(isPow2(numBucketsNew));
        const uint32_t numBuckets = m_numBuckets;
        TItem* storage = m_storage;
        TItem* EXLBR_RESTRICT item = storage;
        TItem* const enditem = item + numBuckets;
        bool isInlineStorage = isUsingInlineStorage();

        numBucketsNew = create(numBucketsNew);

        reinsert(numBucketsNew, item, enditem);

        if (!isInlineStorage)
        {
            EXLBR_FREE(storage);
        }
    }

  public:
    inline void rehash() { resize(m_numBuckets); }

    inline bool reserve(uint32_t numBucketsNew)
    {
        if (numBucketsNew == 0 || numBucketsNew < capacity())
        {
            return false;
        }
        numBucketsNew = nextPow2(numBucketsNew);
        resize(numBucketsNew);
        return true;
    }

    [[nodiscard]] inline uint32_t getNumTombstones() const noexcept { return m_numTombstones; }
    [[nodiscard]] inline uint32_t size() const noexcept { return m_numElements; }
    [[nodiscard]] inline uint32_t capacity() const noexcept { return m_numBuckets; }
    [[nodiscard]] inline bool empty() const noexcept { return (m_numElements == 0); }

    [[nodiscard]] inline bool has(const TKey& key) const noexcept { return (find(key) != iend()); }

    inline TValue& operator[](const TKey& key)
    {
        std::pair<IteratorKV, bool> emplaceIt = emplace(key);
        return emplaceIt.first.value();
    }

    [[nodiscard]] inline IteratorK begin() const { return IteratorHelper<IteratorK>::begin(*this); }
    [[nodiscard]] inline IteratorK end() const { return IteratorHelper<IteratorK>::end(*this); }

    [[nodiscard]] inline ConstIteratorV vbegin() const { return IteratorHelper<ConstIteratorV>::begin(*this); }
    [[nodiscard]] inline ConstIteratorV vend() const { return IteratorHelper<ConstIteratorV>::end(*this); }
    [[nodiscard]] inline IteratorV vbegin() { return IteratorHelper<IteratorV>::begin(*this); }
    [[nodiscard]] inline IteratorV vend() { return IteratorHelper<IteratorV>::end(*this); }

    [[nodiscard]] inline ConstIteratorKV ibegin() const { return IteratorHelper<ConstIteratorKV>::begin(*this); }
    [[nodiscard]] inline ConstIteratorKV iend() const { return IteratorHelper<ConstIteratorKV>::end(*this); }
    [[nodiscard]] inline IteratorKV ibegin() { return IteratorHelper<IteratorKV>::begin(*this); }
    [[nodiscard]] inline IteratorKV iend() { return IteratorHelper<IteratorKV>::end(*this); }

    template <typename TIterator> struct TypedIteratorHelper
    {
        const HashTable* ht;
        TypedIteratorHelper(const HashTable* _ht)
            : ht(_ht)
        {
        }
        TIterator begin() { return IteratorHelper<TIterator>::begin(*ht); }
        TIterator end() { return IteratorHelper<TIterator>::end(*ht); }
    };

    using Keys = TypedIteratorHelper<IteratorK>;
    using Values = TypedIteratorHelper<IteratorV>;
    using Items = TypedIteratorHelper<IteratorKV>;
    using ConstValues = TypedIteratorHelper<ConstIteratorV>;
    using ConstItems = TypedIteratorHelper<ConstIteratorKV>;

    [[nodiscard]] inline Keys keys() const { return Keys(this); }
    [[nodiscard]] inline ConstValues values() const { return ConstValues(this); }
    [[nodiscard]] inline ConstItems items() const { return ConstItems(this); }

    [[nodiscard]] inline Values values() { return Values(this); }
    [[nodiscard]] inline Items items() { return Items(this); }

    // copy ctor
    HashTable(const HashTable& other)
    {
        EXLBR_ASSERT(&other != this);
        m_storage = constructInline(TKeyInfo::getEmpty());
        create(other.m_numBuckets);
        copyFrom(other);
    }

    // copy assignment
    HashTable& operator=(const HashTable& other)
    {
        if (&other == this)
        {
            return *this;
        }
        destroyAndFreeMemory();
        m_storage = constructInline(TKeyInfo::getEmpty());
        create(other.m_numBuckets);
        copyFrom(other);
        return *this;
    }

    // move ctor
    HashTable(HashTable&& other) noexcept
    //: m_storage(nullptr)
    //, m_numBuckets(1)
    //, m_numElements(0)
    {
        EXLBR_ASSERT(&other != this);
        moveFrom(std::move(other));
    }

    // move assignment
    HashTable& operator=(HashTable&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }
        destroyAndFreeMemory();
        moveFrom(std::move(other));
        return *this;
    }

  private:
    // prefix m_ to be able to easily see member access from the code (it could be more expensive in the inner loop)
    TItem* m_storage;         // 8
    uint32_t m_numBuckets;    // 4
    uint32_t m_numElements;   // 4
    uint32_t m_numTombstones; // 4
    // padding 4

    template <typename INTEGRAL_TYPE> inline static constexpr bool isPow2(INTEGRAL_TYPE x) noexcept
    {
        static_assert(std::is_integral<INTEGRAL_TYPE>::value, "isPow2 must be called on an integer type.");
        return (x & (x - 1)) == 0 && (x != 0);
    }

    // We need this inline storage to keep `m_storage` not null all the time.
    // This will save us from `empty()` check inside `find()` function implementation
    static_assert(kNumInlineItems != 0, "Num inline items can't be zero!");
    static_assert(isPow2(kNumInlineItems), "Num inline items should be power of two");
    typename std::aligned_storage<sizeof(TItem) * kNumInlineItems, alignof(TItem)>::type m_inlineStorage;
    static_assert(sizeof(m_inlineStorage) == (sizeof(TItem) * kNumInlineItems), "Incorrect sizeof");
};

// hashmap declaration
template <typename TKey, typename TValue, unsigned kNumInlineItems = 1, typename TKeyInfo = KeyInfo<TKey>>
using HashMap = HashTable<TKey, TValue, kNumInlineItems, TKeyInfo>;

// hashset declaration
template <typename TKey, unsigned kNumInlineItems = 1, typename TKeyInfo = KeyInfo<TKey>>
using HashSet = HashTable<TKey, std::nullptr_t, kNumInlineItems, TKeyInfo>;

} // namespace Excalibur
