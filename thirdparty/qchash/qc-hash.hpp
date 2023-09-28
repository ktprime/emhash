#pragma once

///
/// QC Hash 3.0.6
///
/// https://github.com/daskie/qc-hash
///
/// Extremely fast unordered map and set library for C++20
///
/// See the README for more info!
///
/// Some nomenclature:
///   - Key: A piece of data that is unique within the map/set
///   - Value: The data mapped by a key in a map. Does not exist in a set
///   - Element: A key-value pair, or just a key in the case of a set. One "thing" in the map/set
///   - Slot: One slot in the backing array. May contain an element or the "vacant" or "grave" magic constants
///   - Vacant: Indicates the slot has never had an element
///   - Grave: Means the slot used to have an element, but it was erased
///   - Size: The number of elements in the map/set
///   - Capacity: The number of elements that the map/set can currently hold without growing. Exactly half the number of
///       slots and always a power of two
///   - Special Slots: Two slots tacked on to the end of the backing array in addition to the reported capacity. Used to
///       hold the special elements if they are present
///   - Special Elements: The elements whose keys match the "vacant" or "grave" constants. Stored in the special slots
///

#if defined _CPPUNWIND || defined __cpp_exceptions
    #define QC_HASH_EXCEPTIONS_ENABLED
#endif

#include <cstdint>
#include <cstring>

#include <bit>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#ifdef QC_HASH_EXCEPTIONS_ENABLED
    #include <stdexcept>
#endif
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace qc::hash
{
    ///
    /// Convenience aliases
    ///
    using u64 = uint64_t;
    using s64 = int64_t;
    using f64 = double;
    using u32 = uint32_t;
    using s32 = int32_t;
    using f32 = float;
    using u16 = uint16_t;
    using s16 = int16_t;
    using u8 = uint8_t;
    using s8 = int8_t;

    /// Only support 64 bit platforms
    static_assert(std::is_same_v<u64, u64> && std::is_same_v<uintptr_t, u64>, "Unsupported architecture");

    inline namespace config
    {
        ///
        /// The capacity new maps/sets will be initialized with, once memory is allocated. The capacity will never be
        /// rehashed below this value. Does not include the two special elements, as they do not count against the load
        /// factor
        ///
        /// Must be a power of two
        ///
        inline constexpr u64 minMapCapacity{16u};
    }

    ///
    /// Helper concepts
    ///
    template <typename T> concept SignedInteger = std::is_same_v<T, s64> || std::is_same_v<T, s32> || std::is_same_v<T, s16> || std::is_same_v<T, s8>;
    template <typename T> concept UnsignedInteger = std::is_same_v<T, u64> || std::is_same_v<T, u32> || std::is_same_v<T, u16> || std::is_same_v<T, u8>;
    template <typename T> concept Enum = std::is_enum_v<T>;
    template <typename T> concept Pointer = std::is_pointer_v<T>;

    namespace _private
    {
        template <u64 size> struct UnsignedHelper;
        template <> struct UnsignedHelper<1u> { using type = u8; };
        template <> struct UnsignedHelper<2u> { using type = u16; };
        template <> struct UnsignedHelper<4u> { using type = u32; };
        template <> struct UnsignedHelper<8u> { using type = u64; };
    }

    ///
    /// Aliases the unsigned integer type of a certain size
    ///
    template <u64 size> using Unsigned = typename _private::UnsignedHelper<size>::type;

    ///
    /// Represents an "unsigned" value by compositing multiple native unsigned types. Useful to alias types that are
    /// larger than the largest native unsigned type or that have an alignment smaller than their size
    ///
    /// Essentially just a wrapper around an array of `elementN` native unsigned types of size `elementSize`
    ///
    /// @tparam elementSize the size of each element
    /// @tparam elementN the number of elements
    ///
    template <u64 elementSize, u64 elementN>
    struct UnsignedMulti
    {
        using Element = Unsigned<elementSize>;

        Element elements[elementN];

        constexpr bool operator==(const UnsignedMulti &) const = default;

        constexpr UnsignedMulti operator~() const;
    };

    ///
    /// Specialize to explicitly specify whether a given type has a unique representation. Essentially a manual override
    /// for `std::has_unique_object_representation`
    ///
    template <typename T> struct IsUniquelyRepresentable : std::bool_constant<std::has_unique_object_representations_v<T>> {};
    template <typename T> struct IsUniquelyRepresentable<std::unique_ptr<T>> : std::true_type {};
    template <typename T> struct IsUniquelyRepresentable<std::shared_ptr<T>> : std::true_type {};
    template <typename T1, typename T2> struct IsUniquelyRepresentable<std::pair<T1, T2>> : std::bool_constant<IsUniquelyRepresentable<T1>::value && IsUniquelyRepresentable<T2>::value> {};
    template <typename CharT, typename Traits> struct IsUniquelyRepresentable<std::basic_string_view<CharT, Traits>> : std::false_type{};

    ///
    /// A key type must meet this requirement to work with this map/set implementation. Essentially there must be a
    /// one-to-one mapping between the raw binary and the logical value of a key
    ///
    template <typename T> concept Rawable = IsUniquelyRepresentable<T>::value;

    namespace _private
    {
        template <typename T> struct RawTypeHelper { using type = Unsigned<sizeof(T)>; };
        template <typename T> requires (alignof(T) != sizeof(T) || sizeof(T) > sizeof(uintmax_t))
        struct RawTypeHelper<T>
        {
            static constexpr u64 align{alignof(T) > alignof(uintmax_t) ? alignof(uintmax_t) : alignof(T)};
            using type = UnsignedMulti<align, sizeof(T) / align>;
        };
    }

    ///
    /// The "raw" type that matches the key type's size and alignment and is used to alias the key
    ///
    template <typename T> using RawType = typename _private::RawTypeHelper<T>::type;

    ///
    /// This default hash simply "grabs" the least significant 64 bits of data from the key's underlying binary
    ///
    /// May specialize for custom types. Must provide a `auto operator()(const T &)` method that returns something
    /// implicitly convertible to `u64`. The lowest bits are used to map to a slot, so prioritize low-order entropy
    ///
    /// Must provide `u64 operator()(const U &)` method to support a heterogeneous type `U`
    ///
    template <Rawable T> struct IdentityHash;

    ///
    /// Specialization of `IdentityHash` for pointers. Simply right-shifts the pointer by the log2 of `T`'s alignment,
    /// thereby discarding redundant bits and maximizing low-order entropy
    ///
    template <typename T> struct IdentityHash<T *>;

    ///
    /// Specialization of `IdentityHash` for `std::unique_ptr`. Works the same as the pointer specilization
    ///
    template <typename T> struct IdentityHash<std::unique_ptr<T>>;

    ///
    /// Specialization of `IdentityHash` for `std::shared_ptr`. Works the same as the pointer specilization
    ///
    template <typename T> struct IdentityHash<std::shared_ptr<T>>;

    ///
    /// A very fast/minimal non-crytographic hash purely to improve collision rates for keys with poor low-order entropy
    ///
    /// Yields different hashes depending on word size and endianness
    ///
    /// May specialize for custom types. Must provide a `auto operator()(const K &)` method that returns something
    /// implicitly convertible to `u64`. The lowest bits are used to map to a slot, so prioritize low-order entropy
    ///
    /// Must provide `u64 operator(const U &)` method to support a heterogeneous type `U`
    ///
    template <typename T> struct FastHash;

    ///
    /// Specialization of `FastHash` for pointers. Facilitates heterogeneity between const and mutable pointers
    ///
    template <typename T> struct FastHash<T *>;

    ///
    /// Specialization of `FastHash` for `std::unique_ptr`
    ///
    template <typename T> struct FastHash<std::unique_ptr<T>>;

    ///
    /// Specialization of `FastHash` for `std::shared_ptr`
    ///
    template <typename T> struct FastHash<std::shared_ptr<T>>;

    ///
    /// Specialization of `FastHash` for `std::string`
    ///
    template <> struct FastHash<std::string>;

    ///
    /// Specialization of `FastHash` for `std::string_view`
    ///
    template <> struct FastHash<std::string_view>;

    namespace fastHash
    {
        ///
        /// Quickly hash a u64 or u32
        ///
        /// @param v the value to mix
        /// @return the mixed value
        ///
        template <UnsignedInteger H> [[nodiscard]] constexpr H mix(H v);

        ///
        /// Direct FastHash function that hashes the given value
        ///
        /// @param v the value to hash
        /// @return the hash of the value
        ///
        template <UnsignedInteger H, typename T> [[nodiscard]] constexpr H hash(const T & v);

        ///
        /// Direct FastHash function that hashes the given data
        ///
        /// @param data the data to hash
        /// @param length the length of the data in bytes
        /// @return the hash of the data
        ///
        template <UnsignedInteger H> [[nodiscard]] H hash(const void * data, u64 length);
    }

    ///
    /// Indicates whether `KOther` is heterogeneous with `K`. May specialize to enable heterogeneous lookup for custom
    /// types
    ///
    template <typename K, typename KOther> struct IsCompatible : std::bool_constant<std::is_same_v<std::decay_t<K>, std::decay_t<KOther>>> {};
    template <SignedInteger K, SignedInteger KOther> struct IsCompatible<K, KOther> : std::bool_constant<sizeof(KOther) <= sizeof(K)> {};
    template <UnsignedInteger K, UnsignedInteger KOther> struct IsCompatible<K, KOther> : std::bool_constant<sizeof(KOther) <= sizeof(K)> {};
    template <SignedInteger K, UnsignedInteger KOther> struct IsCompatible<K, KOther> : std::bool_constant<sizeof(KOther) < sizeof(K)> {};
    template <UnsignedInteger K, SignedInteger KOther> struct IsCompatible<K, KOther> : std::false_type {};
    template <typename T, typename TOther> requires (std::is_same_v<std::decay_t<T>, std::decay_t<TOther>> || std::is_base_of_v<T, TOther>) struct IsCompatible<T *, TOther *> : std::true_type {};
    template <typename T, typename TOther> requires (std::is_same_v<std::decay_t<T>, std::decay_t<TOther>> || std::is_base_of_v<T, TOther>) struct IsCompatible<std::unique_ptr<T>, TOther *> : std::true_type {};
    template <typename T, typename TOther> requires (std::is_same_v<std::decay_t<T>, std::decay_t<TOther>> || std::is_base_of_v<T, TOther>) struct IsCompatible<std::shared_ptr<T>, TOther *> : std::true_type {};

    ///
    /// Specifies whether a key of type `KOther` may be used for lookup operations on a map/set with key type `K`
    ///
    template <typename KOther, typename K> concept Compatible = Rawable<K> && Rawable<KOther> && IsCompatible<K, KOther>::value;

    // Used for testing
    struct RawFriend;

    ///
    /// An associative container that stores unique-key key-pair values. Uses a flat memory model, linear probing, and a
    /// whole lot of optimizations that make this an extremely fast map for small elements
    ///
    /// A custom hasher must provide a `auto operator()(const K &)` method that returns something implicitly convertible
    ///   to `u64`. Additionally, the hash function should provide good low-order entropy, as the low bits determine the
    ///   slot index
    ///
    /// @tparam K the key type
    /// @tparam V the mapped value type
    /// @tparam H the functor type for hashing keys
    /// @tparam KE the functor type for checking key equality
    /// @tparam A the allocator type
    ///
    template <Rawable K, typename V, typename H = IdentityHash<K>, typename A = std::allocator<std::pair<K, V>>> class RawMap;

    ///
    /// An associative container that stores unique-key key-pair values. Uses a flat memory model, linear probing, and a
    /// whole lot of optimizations that make this an extremely fast set for small elements
    ///
    /// This implementation has minimal differences between maps and sets, and those that exist are zero-cost
    /// compile-time abstractions. Thus, a set is simply a map whose value type is `void`
    ///
    /// A custom hasher must provide a `auto operator()(const K &)` method that returns something implicitly convertible
    ///   to `u64`. Additionally, the hash function should provide good low-order entropy, as the low bits determine the
    ///   slot index
    ///
    /// @tparam K the key type
    /// @tparam H the functor type for hashing keys
    /// @tparam A the allocator type
    ///
    template <Rawable K, typename H = IdentityHash<K>, typename A = std::allocator<K>> using RawSet = RawMap<K, void, H, A>;

    template <Rawable K, typename V, typename H, typename A> class RawMap
    {
        inline static constexpr bool _isSet{std::is_same_v<V, void>};
        inline static constexpr bool _isMap{!_isSet};

        ///
        /// Element type
        ///
        using E = std::conditional_t<_isSet, K, std::pair<K, V>>;

        // Internal iterator class forward declaration. Prefer `iterator` and `const_iterator`
        template <bool constant> class _Iterator;

        friend ::qc::hash::RawFriend;

      public:

        static_assert(std::is_move_constructible_v<E>);
        static_assert(std::is_move_assignable_v<E>);
        static_assert(std::is_swappable_v<E>);

        static_assert(requires(const H h, const K k) { u64{h(k)}; });
        static_assert(std::is_move_constructible_v<H>);
        static_assert(std::is_move_assignable_v<H>);
        static_assert(std::is_swappable_v<H>);

        static_assert(std::is_move_constructible_v<A>);
        static_assert(std::is_move_assignable_v<A> || !std::allocator_traits<A>::propagate_on_container_move_assignment::value);
        static_assert(std::is_swappable_v<A> || !std::allocator_traits<A>::propagate_on_container_swap::value);

        using key_type = K;
        using mapped_type = V;
        using value_type = E;
        using hasher = H;
        using allocator_type = A;
        using reference = E &;
        using const_reference = const E &;
        using pointer = E *;
        using const_pointer = const E *;
        using size_type = u64;
        using difference_type = s64;

        using iterator = _Iterator<false>;
        using const_iterator = _Iterator<true>;

        ///
        /// Constructs a new map/set
        ///
        /// The number of backing slots will be the smallest power of two greater than or equal to twice `capacity`
        ///
        /// Memory is not allocated until the first element is inserted
        ///
        /// @param capacity the minimum cpacity
        /// @param hash the hasher
        /// @param alloc the allocator
        ///
        explicit RawMap(u64 capacity = minMapCapacity, const H & hash = {}, const A & alloc = {});
        RawMap(u64 capacity, const A & alloc);
        explicit RawMap(const A & alloc);

        ///
        /// Constructs a new map/set from copies of the elements within the iterator range
        ///
        /// The number of backing slots will be the smallest power of two greater than or equal to twice the larger of
        /// `capacity` or the number of elements within the iterator range
        ///
        /// @param first iterator to the first element to copy, inclusive
        /// @param last iterator to the last element to copy, exclusive
        /// @param capacity the minumum capacity
        /// @param hash the hasher
        /// @param alloc the allocator
        ///
        template <typename It> RawMap(It first, It last, u64 capacity = {}, const H & hash = {}, const A & alloc = {});
        template <typename It> RawMap(It first, It last, u64 capacity, const A & alloc);

        ///
        /// Constructs a new map/set from copies of the elements in the initializer list
        ///
        /// The number of backing slots will be the smallest power of two greater than or equal to twice the larger of
        /// `capacity` or the number of elements in the initializer list
        ///
        /// @param elements the elements to copy
        /// @param capacity the minumum capacity
        /// @param hash the hasher
        /// @param alloc the allocator
        ///
        RawMap(std::initializer_list<E> elements, u64 capacity = {}, const H & hash = {}, const A & alloc = {});
        RawMap(std::initializer_list<E> elements, u64 capacity, const A & alloc);

        ///
        /// Copy constructor - new memory is allocated and each element is copied
        ///
        /// @param other the map/set to copy
        ///
        RawMap(const RawMap & other);

        ///
        /// Move constructor - no memory is allocated and no elements are copied
        ///
        /// The moved-from map/set is left in an empty state, the validitiy of which depends on the move constructors of
        /// the hasher and allocator
        ///
        /// @param other the map/set to move from
        ///
        RawMap(RawMap && other);

        ///
        /// Destructs existing elements and inserts the elements in the initializer list
        ///
        /// @param elements the new elements to copy
        /// @returns this
        ///
        RawMap & operator=(std::initializer_list<E> elements);

        ///
        /// Copy assignment operator - existing elements are destructed, more memory is allocated if necessary, and each
        /// new element is inserted
        ///
        /// @param other the map/set to copy from
        /// @returns this
        ///
        RawMap & operator=(const RawMap & other);

        ///
        /// Move assignment operator - existing elements are destructed and memory is freed
        /// @param other the map/set to move from
        /// @returns this
        ///
        RawMap & operator=(RawMap && other);

        ///
        /// Destructor - all elements are destructed and all memory is freed
        ///
        ~RawMap();

        ///
        /// Copies the element into the map/set if its key is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param element the element to insert
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether it was inserted
        ///
        std::pair<iterator, bool> insert(const E & element);

        ///
        /// Moves the element into the map/set if its key is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param element the element to insert
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether it was inserted
        ///
        std::pair<iterator, bool> insert(E && element);

        ///
        /// Copies each element in [`first`, `last`) into the map/set if its key is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param first the first element in the range to insert, inclusive
        /// @param last the last element in the range to insert, exclusive
        ///
        template <typename It> void insert(It first, It last);

        ///
        /// Copies each element in the initializer list into the map/set if its key is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param elements the elements to insert
        ///
        void insert(std::initializer_list<E> elements);

        ///
        /// Copies the element into the map/set if its key is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param element the element to insert
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether it was inserted
        ///
        std::pair<iterator, bool> emplace(const E & element);

        ///
        /// Moves the element into the map/set if its key is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param element the element to insert
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether it was inserted
        ///
        std::pair<iterator, bool> emplace(E && element);

        ///
        /// Forwards the key and value into the map/set if the key is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// Defined only for maps, not for sets
        ///
        /// @param key the key to forward
        /// @param value the value to forward
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether it was inserted
        ///
        template <typename K_, typename V_> std::pair<iterator, bool> emplace(K_ && key, V_ && value) requires (!std::is_same_v<V, void>);

        ///
        /// Constructs a new key from the forwarded arguments and inserts it if it is not already present
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// Defined only for sets, not for maps
        ///
        /// @param keyArgs the arguments to forward to the key's constructor
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether or was inserted
        ///
        template <typename... KArgs> std::pair<iterator, bool> emplace(KArgs &&... keyArgs) requires (std::is_same_v<V, void>);

        ///
        /// Constructs a key from the forwarded key arguments, and if the key is not present, a new value is constructed
        /// from the forwarded value arguments and the two are inserted as a new element
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// Defined only for maps, not for sets
        ///
        /// @param keyArgs the arguments to forward to the key's constructor
        /// @param valueArgs the arguments to forward to the value's constructor
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether it was inserted
        ///
        template <typename... KArgs, typename... VArgs> std::pair<iterator, bool> emplace(std::piecewise_construct_t, std::tuple<KArgs...> && keyArgs, std::tuple<VArgs...> && valueArgs) requires (!std::is_same_v<V, void>);

        ///
        /// If the key is not already present, a new element is constructed in-place from the forwarded arguments
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// `valueArgs` must be present for maps and absent for sets
        ///
        /// @param keyArgs the arguments to forward to the key's constructor
        /// @param valueArgs the arguments to forward to the value's constructor
        /// @returns an iterator to the element if inserted, or end iterator if not, and whether it was inserted
        ///
        template <typename K_, typename... VArgs> std::pair<iterator, bool> try_emplace(K_ && key, VArgs &&... valueArgs);

        ///
        /// Erase the element for the heterogeneous key if present
        ///
        /// Does *not* invalidate iterators
        ///
        /// @param key the key of the element to erase
        /// @returns whether the element was erased
        ///
        template <Compatible<K> K_> bool erase(const K_ & key);

        ///
        /// Erase the element at the given position
        ///
        /// Undefined behavior if position is the end iterator or otherwise invalid
        ///
        /// Does *not* invalidate iterators
        ///
        /// @param position position of the element to erase
        ///
        void erase(iterator position);

        ///
        /// Clears the map/set, destructing all elements
        ///
        /// Does not alter capacity or free memory
        ///
        /// Invalidates iterators
        ///
        void clear();

        ///
        /// @param key the key to check for
        /// @returns whether the heterogeneous key is present
        ///
        template <Compatible<K> K_> [[nodiscard]] bool contains(const K_ & key) const;

        ///
        /// @param key the key to count
        /// @returns `1` if the heterogeneous key is present or `0` if it is absent
        ///
        template <Compatible<K> K_> [[nodiscard]] u64 count(const K_ & key) const;

        #ifdef QC_HASH_EXCEPTIONS_ENABLED
            ///
            /// Gets the present element for the heterogeneous key
            ///
            /// Defined only for maps, not for sets
            ///
            /// @param key the key to retrieve
            /// @returns the element for the key
            /// @throws `std::out_of_range` if the key is absent
            ///
            template <Compatible<K> K_> [[nodiscard]] std::add_lvalue_reference_t<V> at(const K_ & key) requires (!std::is_same_v<V, void>);
            template <Compatible<K> K_> [[nodiscard]] std::add_lvalue_reference_t<const V> at(const K_ & key) const requires (!std::is_same_v<V, void>);
        #endif

        ///
        /// Gets the element for the heterogeneous key, creating a new element if it is not already present
        ///
        /// Defined only for maps, not for sets
        ///
        /// @param key the key to retrieve
        /// @returns the element for the key
        ///
        template <Compatible<K> K_> [[nodiscard]] std::add_lvalue_reference_t<V> operator[](const K_ & key) requires (!std::is_same_v<V, void>);
        template <Compatible<K> K_> [[nodiscard]] std::add_lvalue_reference_t<V> operator[](K_ && key) requires (!std::is_same_v<V, void>);

        ///
        /// @returns an iterator to the first element in the map/set
        ///
        [[nodiscard]] iterator begin();
        [[nodiscard]] const_iterator begin() const;
        [[nodiscard]] const_iterator cbegin() const;

        ///
        /// @returns an iterator that is conceptually one-past the end of the map/set or an invalid position
        ///
        [[nodiscard]] iterator end();
        [[nodiscard]] const_iterator end() const;
        [[nodiscard]] const_iterator cend() const;

        ///
        /// @param key the key to find
        /// @returns an iterator to the element for the key if present, or the end iterator if absent
        ///
        template <Compatible<K> K_> [[nodiscard]] iterator find(const K_ & key);
        template <Compatible<K> K_> [[nodiscard]] const_iterator find(const K_ & key) const;

        ///
        /// @returns the index of the slot into which the heterogeneous key would fall
        ///
        template <Compatible<K> K_> [[nodiscard]] u64 slot(const K_ & key) const;

        ///
        /// Ensures there are enough slots to comfortably hold `capacity` number of elements
        ///
        /// Equivalent to `rehash(2 * capacity)`
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param capacity the minimum capacity
        ///
        void reserve(u64 capacity);

        ///
        /// Ensures the number of slots is equal to the smallest power of two greater than or equal to both `slotN`
        /// and the current size, down to a minimum of `config::minMapSlotN`
        ///
        /// Equivalent to `reserve(slotN / 2)`
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param slotN the minimum slot count
        ///
        void rehash(u64 slotN);

        ///
        /// Swaps the contents of this map/set with the other's
        ///
        /// Does not allocate or copy memory
        ///
        /// Invalidates iterators
        ///
        /// @param other the map/set to swap with
        ///
        void swap(RawMap & other);

        ///
        /// @returns the number of elements in the map/set
        ///
        [[nodiscard]] u64 size() const;

        ///
        /// @returns whether the map/set is empty
        ///
        [[nodiscard]] bool empty() const;

        ///
        /// @returns how many elements the map/set can hold before needing to rehash; equivalent to `slot_n() / 2`
        ///
        [[nodiscard]] u64 capacity() const;

        ///
        /// @returns the number of slots in the map/set; equivalent to `capacity() * 2`
        ///
        [[nodiscard]] u64 slot_n() const;

        ///
        /// @returns the maximum possible element count; equivalent to `max_slot_n() * 2`
        ///
        [[nodiscard]] u64 max_size() const;

        ///
        /// @returns the maximum possible slot count; equivalent to `max_size() / 2`
        ///
        [[nodiscard]] u64 max_slot_n() const;

        ///
        /// @returns the ratio of elements to slots, maximum being 0.5
        ///
        [[nodiscard]] f32 load_factor() const;

        ///
        /// @returns 0.5, the maximum possible load factor
        ///
        [[nodiscard]] f32 max_load_factor(float) const;

        ///
        /// @returns the hasher
        ///
        [[nodiscard]] const H & hash_function() const;

        ///
        /// @returns the allocator
        ///
        [[nodiscard]] const A & get_allocator() const;

      private:

        using _RawKey = RawType<K>;

        inline static constexpr _RawKey _vacantKey{_RawKey(~_RawKey{})};
        inline static constexpr _RawKey _graveKey{_RawKey(~_RawKey{1u})};
        inline static constexpr _RawKey _specialKeys[2]{_graveKey, _vacantKey};
        inline static constexpr _RawKey _vacantGraveKey{_vacantKey};
        inline static constexpr _RawKey _vacantVacantKey{_graveKey};
        inline static constexpr _RawKey _vacantSpecialKeys[2]{_vacantGraveKey, _vacantVacantKey};
        inline static constexpr _RawKey _terminalKey{0u};

        static K & _key(E & element);
        static const K & _key(const E & element);

        static bool _isPresent(const _RawKey & key);

        static bool _isSpecial(const _RawKey & key);

        u64 _size;
        u64 _slotN; // Does not include special elements
        E * _elements;
        bool _haveSpecial[2];
        H _hash;
        A _alloc;

        template <typename KTuple, typename VTuple, u64... kIndices, u64... vIndices> std::pair<iterator, bool> _emplace(KTuple && kTuple, VTuple && vTuple, std::index_sequence<kIndices...>, std::index_sequence<vIndices...>);

        template <bool preserveInvariants> void _clear();

        template <Compatible<K> K_> u64 _slot(const K_ & key) const;

        void _rehash(u64 slotN);

        template <bool zeroControls> void _allocate();

        void _deallocate();

        void _clearKeys();

        template <bool move> void _forwardData(std::conditional_t<move, RawMap, const RawMap> & other);


        struct _FindKeyResult1 { E * element; bool isPresent; };
        struct _FindKeyResult2 { E * element; bool isPresent; bool isSpecial; unsigned char specialI; };
        template <bool insertionForm> using _FindKeyResult = std::conditional_t<insertionForm, _FindKeyResult2, _FindKeyResult1>;

        // If the key is not present, returns the slot after the the key's bucket
        template <bool insertionForm, Compatible<K> K_> _FindKeyResult<insertionForm> _findKey(const K_ & key) const;
    };

    template <Rawable K, typename V, typename H, typename A> bool operator==(const RawMap<K, V, H, A> & m1, const RawMap<K, V, H, A> & m2);

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    class RawMap<K, V, H, A>::_Iterator
    {
        friend ::qc::hash::RawMap<K, V, H, A>;
        friend ::qc::hash::RawFriend;

        using E = std::conditional_t<constant, const RawMap::E, RawMap::E>;

      public:

        using iterator_category = std::forward_iterator_tag;
        using value_type = E;
        using difference_type = ptrdiff_t;
        using pointer = E *;
        using reference = E &;

        ///
        /// Default constructor - equivalent to the end iterator
        ///
        constexpr _Iterator() = default;

        ///
        /// Copy constructor - a mutable iterator may be implicitly converted to a const iterator
        /// @param other the iterator to copy
        ///
        constexpr _Iterator(const _Iterator & other) = default;
        template <bool constant_> requires (constant && !constant_) constexpr _Iterator(const _Iterator<constant_> & other);

        ///
        /// Copy assignment - a mutable iterator may be implicitly converted to a const iterator
        /// @param other the iterator to copy
        ///
        _Iterator & operator=(const _Iterator & other) = default;
        template <bool constant_> requires (constant && !constant_) _Iterator & operator=(const _Iterator<constant_> & other);

        ///
        /// @returns the element pointed to by the iterator; undefined for invalid iterators
        ///
        [[nodiscard]] E & operator*() const;

        ///
        /// @returns a pointer to the element pointed to by the iterator; undefined for invalid iterators
        ///
        [[nodiscard]] E * operator->() const;

        ///
        /// Increments the iterator to point to the next element in the map/set, or the end iterator if there are no more
        /// elements
        ///
        /// Incrementing the end iterator is undefined
        ///
        /// @returns this
        ///
        _Iterator & operator++();

        ///
        /// Increments the iterator to point to the next element in the map/set, or the end iterator if there are no more
        /// elements
        ///
        /// Incrementing the end iterator is undefined
        ///
        /// @returns a copy of the iterator before it was incremented
        ///
        _Iterator operator++(int);

        ///
        /// @param other the other iterator to compare with
        /// @returns whether this iterator is equivalent to the other iterator
        ///
        template <bool constant_> [[nodiscard]] bool operator==(const _Iterator<constant_> & other) const;

      private:

        E * _element;

        constexpr _Iterator(E * element);
    };
}

namespace std
{
    ///
    /// Swaps the two maps/sets. No memory is copied or allocated
    ///
    /// @param a the map/set to swap with `b`
    /// @param b the map/set to swap with `a`
    ///
    template <typename K, typename V, typename H, typename A> void swap(qc::hash::RawMap<K, V, H, A> & a, qc::hash::RawMap<K, V, H, A> & b);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc::hash
{
    namespace _private
    {
        inline constexpr u64 minMapSlotN{minMapCapacity * 2u};

        // Returns the lowest 64 bits from the given object
        template <UnsignedInteger U, typename T>
        inline constexpr U getLowBytes(const T & v)
        {
            // Key is aligned as `U` and can be simply reinterpreted as such
            if constexpr (alignof(T) >= sizeof(U))
            {
                return reinterpret_cast<const U &>(v);
            }
                // Key's alignment matches its size and can be simply reinterpreted as an unsigned integer
            else if constexpr (alignof(T) == sizeof(T))
            {
                return reinterpret_cast<const Unsigned<sizeof(T)> &>(v);
            }
                // Key is not nicely aligned, manually copy up to a `U`'s worth of memory
                // Could use memcpy, but this gives better debug performance, and both compile to the same in release
            else
            {
                U result{0u};
                using Block = Unsigned<alignof(T) < sizeof(U) ? alignof(T) : sizeof(U)>;
                constexpr u64 n{(sizeof(T) < sizeof(U) ? sizeof(T) : sizeof(U)) / sizeof(Block)};
                const Block * src{reinterpret_cast<const Block *>(&v)};
                Block * dst{reinterpret_cast<Block *>(&result)};

                // We want the lower-order bytes, so need to adjust on big endian systems
                if constexpr (std::endian::native == std::endian::big)
                {
                    constexpr u64 srcBlocks{sizeof(T) / sizeof(Block)};
                    constexpr u64 dstBlocks{sizeof(U) / sizeof(Block)};
                    if constexpr (srcBlocks > n)
                    {
                        src += srcBlocks - n;
                    }
                    if constexpr (dstBlocks > n)
                    {
                        dst += dstBlocks - n;
                    }
                }

                // Copy blocks
                if constexpr (n >= 1u) dst[0] = src[0];
                if constexpr (n >= 2u) dst[1] = src[1];
                if constexpr (n >= 3u) dst[2] = src[2];
                if constexpr (n >= 4u) dst[3] = src[3];
                if constexpr (n >= 5u) dst[4] = src[4];
                if constexpr (n >= 6u) dst[5] = src[5];
                if constexpr (n >= 7u) dst[6] = src[6];
                if constexpr (n >= 8u) dst[7] = src[7];

                return result;
            }
        }
    }

    template <u64 elementSize, u64 elementN>
    inline constexpr auto UnsignedMulti<elementSize, elementN>::operator~() const -> UnsignedMulti
    {
        UnsignedMulti res;
        for (u64 i{0u}; i < elementN; ++i)
        {
            res.elements[i] = Element(~elements[i]);
        }
        return res;
    }

    template <Rawable T>
    struct IdentityHash
    {
        [[nodiscard]] constexpr u64 operator()(const T & v) const
        {
            return _private::getLowBytes<u64>(v);
        }
    };

    template <typename T>
    struct IdentityHash<T *>
    {
        [[nodiscard]] constexpr u64 operator()(const T * const v) const
        {
            // Bit shift away the low zero bits to maximize low-order entropy
            constexpr s32 shift{s32(std::bit_width(alignof(T)) - 1u)};
            return std::bit_cast<u64>(v) >> shift;
        }
    };

    template <typename T>
    struct IdentityHash<std::unique_ptr<T>> : IdentityHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using IdentityHash<T *>::operator();

        [[nodiscard]] constexpr u64 operator()(const std::unique_ptr<T> & v) const
        {
            return (*this)(v.get());
        }
    };

    template <typename T>
    struct IdentityHash<std::shared_ptr<T>> : IdentityHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using IdentityHash<T *>::operator();

        [[nodiscard]] constexpr u64 operator()(const std::shared_ptr<T> & v) const
        {
            return (*this)(v.get());
        }
    };

    template <typename T>
    struct FastHash
    {
        [[nodiscard]] constexpr u64 operator()(const T & v) const
        {
            return fastHash::hash<u64>(v);
        }
    };

    template <typename T>
    struct FastHash<T *>
    {
        [[nodiscard]] constexpr u64 operator()(const T * const v) const
        {
            return fastHash::hash<u64>(v);
        }
    };

    template <typename T>
    struct FastHash<std::unique_ptr<T>> : FastHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using FastHash<T *>::operator();

        [[nodiscard]] constexpr u64 operator()(const std::unique_ptr<T> & v) const
        {
            return (*this)(v.get());
        }
    };

    template <typename T>
    struct FastHash<std::shared_ptr<T>> : FastHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using FastHash<T *>::operator();

        [[nodiscard]] constexpr u64 operator()(const std::shared_ptr<T> & v) const
        {
            return (*this)(v.get());
        }
    };

    template <>
    struct FastHash<std::string>
    {
        [[nodiscard]] u64 operator()(const std::string & v) const
        {
            return fastHash::hash<u64>(v.c_str(), v.length());
        }

        [[nodiscard]] u64 operator()(const std::string_view & v) const
        {
            return fastHash::hash<u64>(v.data(), v.length());
        }

        [[nodiscard]] u64 operator()(const char * v) const
        {
            return fastHash::hash<u64>(v, std::strlen(v));
        }
    };

    // Same as `std::string` specialization
    template <> struct FastHash<std::string_view> : FastHash<std::string> {};

    namespace fastHash
    {
        template <typename H> struct Constants;
        template <> struct Constants<u64> { inline static constexpr u64 m{0xC6A4A7935BD1E995u}; inline static constexpr s32 r{47}; };
        template <> struct Constants<u32> { inline static constexpr u32 m{0x5BD1E995u};         inline static constexpr s32 r{24}; };
        template <typename H> inline constexpr H m{Constants<H>::m};
        template <typename H> inline constexpr s32 r{Constants<H>::r};

        template <UnsignedInteger H>
        inline constexpr H mix(H v)
        {
            v *= m<H>;
            v ^= v >> r<H>;
            return v * m<H>;
        }

        template <UnsignedInteger H, typename T>
        inline constexpr H hash(const T & v)
        {
            // IMPORTANT: These two cases must yield the same hash for the same input bytes

            // Optimized case if the size of the key is less than or equal to the size of the hash
            if constexpr (sizeof(T) <= sizeof(H))
            {
                return (H(sizeof(T)) * m<H>) ^ mix(_private::getLowBytes<H>(v));
            }
            // General case
            else
            {
                return hash<H>(&v, sizeof(T));
            }
        }

        // Based on Murmur2, but simplified, and doesn't require unaligned reads
        template <UnsignedInteger H>
        inline H hash(const void * const data, u64 length)
        {
            const std::byte * bytes{static_cast<const std::byte *>(data)};
            H h{H(length)};

            // Mix in `H` bytes worth at a time
            while (length >= sizeof(H))
            {
                H w;
                std::memcpy(&w, bytes, sizeof(H));

                h *= m<H>;
                h ^= mix(w);

                bytes += sizeof(H);
                length -= sizeof(H);
            };

            // Mix in the last few bytes
            if (length)
            {
                H w{0u};
                std::memcpy(&w, bytes, length);

                h *= m<H>;
                h ^= mix(w);
            }

            return h;
        }
    }

    template <typename K>
    inline RawType<K> & _raw(K & key)
    {
        return reinterpret_cast<RawType<K> &>(key);
    }

    template <typename K>
    inline const RawType<K> & _raw(const K & key)
    {
        return reinterpret_cast<const RawType<K> &>(key);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const u64 capacity, const H & hash, const A & alloc):
        _size{},
        _slotN{capacity <= minMapCapacity ? _private::minMapSlotN : std::bit_ceil(capacity << 1)},
        _elements{},
        _haveSpecial{},
        _hash{hash},
        _alloc{alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const u64 capacity, const A & alloc) :
        RawMap{capacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const A & alloc) :
        RawMap{minMapCapacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    template <typename It>
    inline RawMap<K, V, H, A>::RawMap(const It first, const It last, const u64 capacity, const H & hash, const A & alloc) :
        RawMap{capacity, hash, alloc}
    {
        // Count number of elements to insert
        u64 n{};
        for (It it{first}; it != last; ++it, ++n);

        reserve(n);

        insert(first, last);
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename It>
    inline RawMap<K, V, H, A>::RawMap(const It first, const It last, const u64 capacity, const A & alloc) :
        RawMap{first, last, capacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const std::initializer_list<E> elements, u64 capacity, const H & hash, const A & alloc) :
        RawMap{capacity ? capacity : elements.size(), hash, alloc}
    {
        insert(elements);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const std::initializer_list<E> elements, const u64 capacity, const A & alloc) :
        RawMap{elements, capacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const RawMap & other) :
        _size{other._size},
        _slotN{other._slotN},
        _elements{},
        _haveSpecial{other._haveSpecial[0], other._haveSpecial[1]},
        _hash{other._hash},
        _alloc{std::allocator_traits<A>::select_on_container_copy_construction(other._alloc)}
    {
        if (_size)
        {
            _allocate<false>();
            _forwardData<false>(other);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(RawMap && other) :
        _size{std::exchange(other._size, 0u)},
        _slotN{std::exchange(other._slotN, _private::minMapSlotN)},
        _elements{std::exchange(other._elements, nullptr)},
        _haveSpecial{std::exchange(other._haveSpecial[0], false), std::exchange(other._haveSpecial[1], false)},
        _hash{std::move(other._hash)},
        _alloc{std::move(other._alloc)}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A> & RawMap<K, V, H, A>::operator=(const std::initializer_list<E> elements)
    {
        return *this = RawMap(elements);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A> & RawMap<K, V, H, A>::operator=(const RawMap & other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (_elements)
        {
            _clear<false>();
            if (!other._size || _slotN != other._slotN || _alloc != other._alloc)
            {
                _deallocate();
            }
        }

        _size = other._size;
        _slotN = other._slotN;
        _haveSpecial[0] = other._haveSpecial[0];
        _haveSpecial[1] = other._haveSpecial[1];
        _hash = other._hash;
        if constexpr (std::allocator_traits<A>::propagate_on_container_copy_assignment::value)
        {
            _alloc = std::allocator_traits<A>::select_on_container_copy_construction(other._alloc);
        }

        if (_size)
        {
            if (!_elements)
            {
                _allocate<false>();
            }

            _forwardData<false>(other);
        }

        return *this;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A> & RawMap<K, V, H, A>::operator=(RawMap && other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (_elements)
        {
            _clear<false>();
            _deallocate();
        }

        _size = other._size;
        _slotN = other._slotN;
        _haveSpecial[0] = other._haveSpecial[0];
        _haveSpecial[1] = other._haveSpecial[1];
        _hash = std::move(other._hash);
        if constexpr (std::allocator_traits<A>::propagate_on_container_move_assignment::value)
        {
            _alloc = std::move(other._alloc);
        }

        if (_alloc == other._alloc || std::allocator_traits<A>::propagate_on_container_move_assignment::value)
        {
            _elements = std::exchange(other._elements, nullptr);
            other._size = {};
        }
        else
        {
            if (_size)
            {
                _allocate<false>();
                _forwardData<true>(other);
                other._clear<false>();
                other._size = 0u;
            }
            if (other._elements)
            {
                other._deallocate();
            }
        }

        other._slotN = _private::minMapSlotN;
        other._haveSpecial[0] = false;
        other._haveSpecial[1] = false;

        return *this;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::~RawMap()
    {
        if (_elements)
        {
            _clear<false>();
            _deallocate();
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::insert(const E & element) -> std::pair<iterator, bool>
    {
        static_assert(std::is_copy_constructible_v<E>);

        return emplace(element);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::insert(E && element) -> std::pair<iterator, bool>
    {
        return emplace(std::move(element));
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename It>
    inline void RawMap<K, V, H, A>::insert(It first, const It last)
    {
        while (first != last)
        {
            emplace(*first);
            ++first;
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::insert(const std::initializer_list<E> elements)
    {
        for (const E & element : elements)
        {
            emplace(element);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::emplace(const E & element) -> std::pair<iterator, bool>
    {
        static_assert(std::is_copy_constructible_v<E>);

        if constexpr (_isSet)
        {
            return try_emplace(element);
        }
        else
        {
            return try_emplace(element.first, element.second);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::emplace(E && element) -> std::pair<iterator, bool>
    {
        if constexpr (_isSet)
        {
            return try_emplace(std::move(element));
        }
        else
        {
            return try_emplace(std::move(element.first), std::move(element.second));
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename K_, typename V_>
    inline auto RawMap<K, V, H, A>::emplace(K_ && key, V_ && value) -> std::pair<iterator, bool> requires (!std::is_same_v<V, void>)
    {
        return try_emplace(std::forward<K_>(key), std::forward<V_>(value));
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename... KArgs>
    inline auto RawMap<K, V, H, A>::emplace(KArgs &&... keyArgs) -> std::pair<iterator, bool> requires (std::is_same_v<V, void>)
    {
        return try_emplace(K{std::forward<KArgs>(keyArgs)...});
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename... KArgs, typename... VArgs>
    inline auto RawMap<K, V, H, A>::emplace(const std::piecewise_construct_t, std::tuple<KArgs...> && keyArgs, std::tuple<VArgs...> && valueArgs) -> std::pair<iterator, bool> requires (!std::is_same_v<V, void>)
    {
        return _emplace(std::move(keyArgs), std::move(valueArgs), std::index_sequence_for<KArgs...>(), std::index_sequence_for<VArgs...>());
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename KTuple, typename VTuple, u64... kIndices, u64... vIndices>
    inline auto RawMap<K, V, H, A>::_emplace(KTuple && kTuple, VTuple && vTuple, const std::index_sequence<kIndices...>, const std::index_sequence<vIndices...>) -> std::pair<iterator, bool>
    {
        return try_emplace(K{std::move(std::get<kIndices>(kTuple))...}, std::move(std::get<vIndices>(vTuple))...);
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename K_, typename... VArgs>
    inline auto RawMap<K, V, H, A>::try_emplace(K_ && key, VArgs &&... vArgs) -> std::pair<iterator, bool>
    {
        static_assert(!(_isMap && !sizeof...(VArgs) && !std::is_default_constructible_v<V>), "The value type must be default constructible in order to pass no value arguments");
        static_assert(!(_isSet && sizeof...(VArgs)), "Sets do not have values");

        // If we've yet to allocate memory, now is the time
        if (!_elements)
        {
            _allocate<true>();
        }

        _FindKeyResult<true> findResult{_findKey<true>(key)};

        // Key is already present
        if (findResult.isPresent)
        {
            return {iterator{findResult.element}, false};
        }

        if (findResult.isSpecial) [[unlikely]]
        {
            _haveSpecial[findResult.specialI] = true;
        }
        else
        {
            // Rehash if we're at capacity
            if ((_size - _haveSpecial[0] - _haveSpecial[1]) >= (_slotN >> 1)) [[unlikely]]
            {
                _rehash(_slotN << 1);
                findResult = _findKey<true>(key);
            }
        }

        if constexpr (_isSet)
        {
            std::allocator_traits<A>::construct(_alloc, findResult.element, std::forward<K_>(key));
        }
        else
        {
            std::allocator_traits<A>::construct(_alloc, &findResult.element->first, std::forward<K_>(key));
            std::allocator_traits<A>::construct(_alloc, &findResult.element->second, std::forward<VArgs>(vArgs)...);
        }

        ++_size;

        return {iterator{findResult.element}, true};
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline bool RawMap<K, V, H, A>::erase(const K_ & key)
    {
        if (!_size)
        {
            return false;
        }

        const auto[element, isPresent]{_findKey<false>(key)};

        if (isPresent)
        {
            erase(iterator{element});
            return true;
        }
        else
        {
            return false;
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::erase(const iterator position)
    {
        E * const eraseElement{position._element};
        _RawKey & rawKey{_raw(_key(*eraseElement))};
        E * const specialElements{_elements + _slotN};

        std::allocator_traits<A>::destroy(_alloc, eraseElement);

        // General case
        if (eraseElement < specialElements)
        {
            rawKey = _graveKey;
        }
        else [[unlikely]]
        {
            const auto specialI{eraseElement - specialElements};
            _raw(_key(specialElements[specialI])) = _vacantSpecialKeys[specialI];
            _haveSpecial[specialI] = false;
        }

        --_size;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::clear()
    {
        _clear<true>();
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool preserveInvariants>
    inline void RawMap<K, V, H, A>::_clear()
    {
        if constexpr (std::is_trivially_destructible_v<E>)
        {
            if constexpr (preserveInvariants)
            {
                if (_size)
                {
                    _clearKeys();
                    _size = {};
                    _haveSpecial[0] = false;
                    _haveSpecial[1] = false;
                }
            }
        }
        else
        {
            if (_size)
            {
                // General case
                E * element{_elements};
                u64 n{};
                const u64 regularElementN{_size - _haveSpecial[0] - _haveSpecial[1]};
                for (; n < regularElementN; ++element)
                {
                    _RawKey & rawKey{_raw(_key(*element))};
                    if (_isPresent(rawKey))
                    {
                        std::allocator_traits<A>::destroy(_alloc, element);
                        ++n;
                    }
                    if constexpr (preserveInvariants)
                    {
                        rawKey = _vacantKey;
                    }
                }
                // Clear remaining graves
                if constexpr (preserveInvariants)
                {
                    const E * const endRegularElement{_elements + _slotN};
                    for (; element < endRegularElement; ++element)
                    {
                        _raw(_key(*element)) = _vacantKey;
                    }
                }

                // Special keys case
                if (_haveSpecial[0]) [[unlikely]]
                {
                    element = _elements + _slotN;
                    std::allocator_traits<A>::destroy(_alloc, element);
                    if constexpr (preserveInvariants)
                    {
                        _raw(_key(*element)) = _vacantGraveKey;
                        _haveSpecial[0] = false;
                    }
                }
                if (_haveSpecial[1]) [[unlikely]]
                {
                    element = _elements + _slotN + 1;
                    std::allocator_traits<A>::destroy(_alloc, element);
                    if constexpr (preserveInvariants)
                    {
                        _raw(_key(*element)) = _vacantVacantKey;
                        _haveSpecial[1] = false;
                    }
                }

                if constexpr (preserveInvariants)
                {
                    _size = {};
                }
            }
        }

    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline bool RawMap<K, V, H, A>::contains(const K_ & key) const
    {
        return _size ? _findKey<false>(key).isPresent : false;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline u64 RawMap<K, V, H, A>::count(const K_ & key) const
    {
        return contains(key);
    }

    #ifdef QC_HASH_EXCEPTIONS_ENABLED
        template <Rawable K, typename V, typename H, typename A>
        template <Compatible<K> K_>
        inline std::add_lvalue_reference_t<V> RawMap<K, V, H, A>::at(const K_ & key) requires (!std::is_same_v<V, void>)
        {
            return const_cast<V &>(static_cast<const RawMap *>(this)->at(key));
        }

        template <Rawable K, typename V, typename H, typename A>
        template <Compatible<K> K_>
        inline std::add_lvalue_reference_t<const V> RawMap<K, V, H, A>::at(const K_ & key) const requires (!std::is_same_v<V, void>)
        {
            if (!_size)
            {
                throw std::out_of_range{"Map is empty"};
            }

            const auto [element, isPresent]{_findKey<false>(key)};

            if (!isPresent)
            {
                throw std::out_of_range{"Element not found"};
            }

            return element->second;
        }
    #endif

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline std::add_lvalue_reference_t<V> RawMap<K, V, H, A>::operator[](const K_ & key) requires (!std::is_same_v<V, void>)
    {
        return try_emplace(key).first->second;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline std::add_lvalue_reference_t<V> RawMap<K, V, H, A>::operator[](K_ && key) requires (!std::is_same_v<V, void>)
    {
        return try_emplace(std::move(key)).first->second;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::begin() -> iterator
    {
        return const_cast<E *>(static_cast<const RawMap *>(this)->begin()._element);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::begin() const -> const_iterator
    {
        // General case
        if (_size - _haveSpecial[0] - _haveSpecial[1]) [[likely]]
        {
            for (const E * element{_elements}; ; ++element)
            {
                if (_isPresent(_raw(_key(*element))))
                {
                    return const_iterator{element};
                }
            }
        }

        // Special key cases
        if (_haveSpecial[0]) [[unlikely]]
        {
            return const_iterator{_elements + _slotN};
        }
        if (_haveSpecial[1]) [[unlikely]]
        {
            return const_iterator{_elements + _slotN + 1};
        }

        return end();
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::cbegin() const -> const_iterator
    {
        return begin();
    }

    template <Rawable K, typename V, typename H, typename A>
    inline typename RawMap<K, V, H, A>::iterator RawMap<K, V, H, A>::end()
    {
        return iterator{};
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::end() const -> const_iterator
    {
        return const_iterator{};
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::cend() const -> const_iterator
    {
        return const_iterator{};
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline auto RawMap<K, V, H, A>::find(const K_ & key) -> iterator
    {
        return const_cast<E *>(static_cast<const RawMap *>(this)->find(key)._element);
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline auto RawMap<K, V, H, A>::find(const K_ & key) const -> const_iterator
    {
        if (!_size)
        {
            return cend();
        }

        const auto [element, isPresent]{_findKey<false>(key)};
        return isPresent ? const_iterator{element} : cend();
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline u64 RawMap<K, V, H, A>::slot(const K_ & key) const
    {
        const _RawKey & rawKey{_raw(key)};
        if (_isSpecial(rawKey)) [[unlikely]]
        {
            return _slotN + (rawKey == _vacantKey);
        }
        else
        {
            return _slot(key);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline u64 RawMap<K, V, H, A>::_slot(const K_ & key) const
    {
        return _hash(key) & (_slotN - 1u);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::reserve(const u64 capacity)
    {
        rehash(capacity << 1);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::rehash(u64 slotN)
    {
        const u64 currentMinSlotN{_size <= minMapCapacity ? _private::minMapSlotN : ((_size - _haveSpecial[0] - _haveSpecial[1]) << 1)};
        if (slotN < currentMinSlotN)
        {
            slotN = currentMinSlotN;
        }
        else
        {
            slotN = std::bit_ceil(slotN);
        }

        if (slotN != _slotN)
        {
            if (_elements)
            {
                _rehash(slotN);
            }
            else
            {
                _slotN = slotN;
            }
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::_rehash(const u64 slotN)
    {
        const u64 oldSize{_size};
        const u64 oldSlotN{_slotN};
        E * const oldElements{_elements};
        const bool oldHaveSpecial[2]{_haveSpecial[0], _haveSpecial[1]};

        _size = {};
        _slotN = slotN;
        _allocate<true>();
        _haveSpecial[0] = false;
        _haveSpecial[1] = false;

        // General case
        u64 n{};
        const u64 regularElementN{oldSize - oldHaveSpecial[0] - oldHaveSpecial[1]};
        for (E * element{oldElements}; n < regularElementN; ++element)
        {
            if (_isPresent(_raw(_key(*element))))
            {
                emplace(std::move(*element));
                std::allocator_traits<A>::destroy(_alloc, element);
                ++n;
            }
        }

        // Special keys case
        if (oldHaveSpecial[0]) [[unlikely]]
        {
            E * const oldElement{oldElements + oldSlotN};
            std::allocator_traits<A>::construct(_alloc, _elements + _slotN, std::move(*oldElement));
            std::allocator_traits<A>::destroy(_alloc, oldElement);
            ++_size;
            _haveSpecial[0] = true;
        }
        if (oldHaveSpecial[1]) [[unlikely]]
        {
            E * const oldElement{oldElements + oldSlotN + 1};
            std::allocator_traits<A>::construct(_alloc, _elements + _slotN + 1, std::move(*oldElement));
            std::allocator_traits<A>::destroy(_alloc, oldElement);
            ++_size;
            _haveSpecial[1] = true;
        }

        std::allocator_traits<A>::deallocate(_alloc, oldElements, oldSlotN + 4u);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::swap(RawMap & other)
    {
        std::swap(_size, other._size);
        std::swap(_slotN, other._slotN);
        std::swap(_elements, other._elements);
        std::swap(_haveSpecial, other._haveSpecial);
        std::swap(_hash, other._hash);
        if constexpr (std::allocator_traits<A>::propagate_on_container_swap::value)
        {
            std::swap(_alloc, other._alloc);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline u64 RawMap<K, V, H, A>::size() const
    {
        return _size;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool RawMap<K, V, H, A>::empty() const
    {
        return !_size;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline u64 RawMap<K, V, H, A>::capacity() const
    {
        return _slotN >> 1;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline u64 RawMap<K, V, H, A>::slot_n() const
    {
        return _slotN;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline u64 RawMap<K, V, H, A>::max_size() const
    {
        return (max_slot_n() >> 1) + 2u;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline u64 RawMap<K, V, H, A>::max_slot_n() const
    {
        return u64{1u} << 63;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline f32 RawMap<K, V, H, A>::load_factor() const
    {
        return f32(_size) / f32(_slotN);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline f32 RawMap<K, V, H, A>::max_load_factor() const
    {
        return f32(minMapCapacity) / f32(_private::minMapSlotN);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline const H & RawMap<K, V, H, A>::hash_function() const
    {
        return _hash;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline const A & RawMap<K, V, H, A>::get_allocator() const
    {
        return _alloc;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline K & RawMap<K, V, H, A>::_key(E & element)
    {
        if constexpr (_isSet) return element;
        else return element.first;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline const K & RawMap<K, V, H, A>::_key(const E & element)
    {
        if constexpr (_isSet) return element;
        else return element.first;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool RawMap<K, V, H, A>::_isPresent(const _RawKey & key)
    {
        return !_isSpecial(key);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool RawMap<K, V, H, A>::_isSpecial(const _RawKey & key)
    {
        return key == _vacantKey || key == _graveKey;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool zeroKeys>
    inline void RawMap<K, V, H, A>::_allocate()
    {
        _elements = std::allocator_traits<A>::allocate(_alloc, _slotN + 4u);

        if constexpr (zeroKeys)
        {
            _clearKeys();
        }

        // Set the trailing keys to special terminal values so iterators know when to stop
        _raw(_key(_elements[_slotN + 2])) = _terminalKey;
        _raw(_key(_elements[_slotN + 3])) = _terminalKey;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::_deallocate()
    {
        std::allocator_traits<A>::deallocate(_alloc, _elements, _slotN + 4u);
        _elements = nullptr;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::_clearKeys()
    {
        // General case
        E * const specialElements{_elements + _slotN};
        for (E * element{_elements}; element < specialElements; ++element)
        {
            _raw(_key(*element)) = _vacantKey;
        }

        // Special key case
        _raw(_key(specialElements[0])) = _vacantGraveKey;
        _raw(_key(specialElements[1])) = _vacantVacantKey;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool move>
    inline void RawMap<K, V, H, A>::_forwardData(std::conditional_t<move, RawMap, const RawMap> & other)
    {
        if constexpr (std::is_trivially_copyable_v<E>)
        {
            std::memcpy(_elements, other._elements, (_slotN + 2u) * sizeof(E));
        }
        else
        {
            using ElementForwardType = std::conditional_t<move, E &&, const E &>;

            // General case
            std::conditional_t<move, E, const E> * srcElement{other._elements};
            const E * const srcEndElement{other._elements + _slotN};
            E * dstElement{_elements};
            for (; srcElement < srcEndElement; ++srcElement, ++dstElement)
            {
                const _RawKey & rawSrcKey{_raw(_key(*srcElement))};
                if (_isPresent(rawSrcKey))
                {
                    std::allocator_traits<A>::construct(_alloc, dstElement, static_cast<ElementForwardType>(*srcElement));
                }
                else
                {
                    _raw(_key(*dstElement)) = rawSrcKey;
                }
            }

            // Special keys case
            if (_haveSpecial[0])
            {
                std::allocator_traits<A>::construct(_alloc, _elements + _slotN, static_cast<ElementForwardType>(other._elements[_slotN]));
            }
            else
            {
                _raw(_key(_elements[_slotN])) = _vacantGraveKey;
            }
            if (_haveSpecial[1])
            {
                std::allocator_traits<A>::construct(_alloc, _elements + _slotN + 1, static_cast<ElementForwardType>(other._elements[_slotN + 1]));
            }
            else
            {
                _raw(_key(_elements[_slotN + 1])) = _vacantVacantKey;
            }
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool insertionForm, Compatible<K> K_>
    inline auto RawMap<K, V, H, A>::_findKey(const K_ & key) const -> _FindKeyResult<insertionForm>
    {
        const _RawKey & rawKey{_raw(key)};

        // Special key case
        if (_isSpecial(rawKey)) [[unlikely]]
        {
            const unsigned char specialI{rawKey == _vacantKey};
            if constexpr (insertionForm)
            {
                return _FindKeyResult<insertionForm>{.element = _elements + _slotN + specialI, .isPresent = _haveSpecial[specialI], .isSpecial = true, .specialI = specialI};
            }
            else
            {
                return _FindKeyResult<insertionForm>{.element = _elements + _slotN + specialI, .isPresent = _haveSpecial[specialI]};
            }
        }

        // General case

        const E * const lastElement{_elements + _slotN};

        E * element{_elements + _slot(key)};
        E * grave{};

        while (true)
        {
            const _RawKey & rawSlotKey{_raw(_key(*element))};

            if (rawSlotKey == rawKey)
            {
                if constexpr (insertionForm)
                {
                    return {.element = element, .isPresent = true, .isSpecial = false, .specialI = 0u};
                }
                else
                {
                    return {.element = element, .isPresent = true};
                }
            }

            if (rawSlotKey == _vacantKey)
            {
                if constexpr (insertionForm)
                {
                    return {.element = grave ? grave : element, .isPresent = false, .isSpecial = false, .specialI = 0u};
                }
                else
                {
                    return {.element = element, .isPresent = false};
                }
            }

            if constexpr (insertionForm)
            {
                if (rawSlotKey == _graveKey)
                {
                    grave = element;
                }
            }

            ++element;
            if (element == lastElement) [[unlikely]]
            {
                element = _elements;
            }
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool operator==(const RawMap<K, V, H, A> & m1, const RawMap<K, V, H, A> & m2)
    {
        if (m1.size() != m2.size())
        {
            return false;
        }

        if (&m1 == &m2)
        {
            return true;
        }

        const auto endIt{m2.cend()};

        for (const auto & element : m1)
        {
            if constexpr (std::is_same_v<V, void>)
            {
                if (!m2.contains(element))
                {
                    return false;
                }
            }
            else
            {
                const auto it{m2.find(element.first)};
                if (it == endIt || it->second != element.second)
                {
                    return false;
                }
            }
        }

        return true;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    template <bool constant_> requires (constant && !constant_)
    inline constexpr RawMap<K, V, H, A>::_Iterator<constant>::_Iterator(const _Iterator<constant_> & other):
        _element{other._element}
    {}

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline constexpr RawMap<K, V, H, A>::_Iterator<constant>::_Iterator(E * const element) :
        _element{element}
    {}

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    template <bool constant_> requires (constant && !constant_)
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator=(const _Iterator<constant_> & other) -> _Iterator &
    {
        _element = other._element;
        return *this;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator*() const -> E &
    {
        return *_element;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator->() const -> E *
    {
        return _element;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator++() -> _Iterator &
    {
        while (true)
        {
            ++_element;
            const _RawKey & rawKey{_raw(_key(*_element))};

            // Either general present case or terminal case
            if (_isPresent(rawKey))
            {
                if (rawKey == _terminalKey) [[unlikely]]
                {
                    // Terminal case
                    if (_raw(_key(_element[1])) == _terminalKey)
                    {
                        _element = nullptr;
                    }
                }

                return *this;
            }

            // Either general absent case with terminal two ahead or special case
            if (_raw(_key(_element[2])) == _terminalKey) [[unlikely]]
            {
                // At second special slot
                if (_raw(_key(_element[1])) == _terminalKey) [[unlikely]]
                {
                    if (rawKey == _vacantVacantKey) [[likely]]
                    {
                        _element = nullptr;
                    }

                    return *this;
                }

                // At first special slot
                if (_raw(_key(_element[3])) == _terminalKey) [[likely]]
                {
                    if (rawKey == _vacantGraveKey) [[likely]]
                    {
                        if (_raw(_key(_element[1])) == _vacantVacantKey) [[likely]]
                        {
                            _element = nullptr;
                        }
                        else
                        {
                            ++_element;
                        }
                    }

                    return *this;
                }
            }
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator++(int) -> _Iterator
    {
        const _Iterator temp{*this};
        operator++();
        return temp;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    template <bool constant_>
    inline bool RawMap<K, V, H, A>::_Iterator<constant>::operator==(const _Iterator<constant_> & other) const
    {
        return _element == other._element;
    }
}

namespace std
{
    template <typename K, typename V, typename H, typename A>
    inline void swap(qc::hash::RawMap<K, V, H, A> & a, qc::hash::RawMap<K, V, H, A> & b)
    {
        a.swap(b);
    }
}
