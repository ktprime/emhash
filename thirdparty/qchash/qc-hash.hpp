#pragma once

///
/// QC Hash 3.0.0
///
/// Austin Quick : 2016 - 2021
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

#include <cstdint>
#include <cstring>

#include <bit>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace qc::hash
{
    // This code assumes that pointers are the same size as `size_t`
    static_assert(sizeof(void *) == sizeof(size_t), "Unsupported architecture");

    namespace config
    {
        ///
        /// The capacity new maps/sets will be initialized with, once memory is allocated. The capacity will never be
        /// rehashed below this value. Does not include the two special elements, as they do not count against the load
        /// factor
        ///
        /// Must be a power of two
        ///
        constexpr size_t minCapacity{16u};
    }

    // Helper concepts
    template <typename T> concept NativeInteger = std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= sizeof(size_t);
    template <typename T> concept NativeSignedInteger = NativeInteger<T> && std::is_signed_v<T>;
    template <typename T> concept NativeUnsignedInteger = NativeInteger<T> && std::is_unsigned_v<T>;
    template <typename T> concept NativeEnum = std::is_enum_v<T> && sizeof(T) <= sizeof(size_t);
    template <typename T> concept Pointer = std::is_pointer_v<T>;

    ///
    /// Aliases the unsigned integer type of a certain size
    ///
    /// Does not use sized aliases such as `uint64_t` which may be undefined on certain systems
    ///
    /// @tparam size must be a power of two and <= the size of the largest unsigned integer type
    ///
    template <size_t size>
    requires (size <= sizeof(uintmax_t) && std::has_single_bit(size))
    using Unsigned = std::conditional_t<size == sizeof(unsigned char),
        unsigned char,
        std::conditional_t<size == sizeof(unsigned short),
            unsigned short,
            std::conditional_t<size == sizeof(unsigned int),
                unsigned int,
                std::conditional_t<size == sizeof(unsigned long),
                    unsigned long,
                    unsigned long long
                >
            >
        >
    >;

    ///
    /// Represents an "unsigned" value by compositing multiple native unsigned types. Useful to alias types that are
    /// larger than the largest native unsigned type or that have an alignment smaller than their size
    ///
    /// Essentially just a wrapper around an array of `elementCount` native unsigned types of size `elementSize`
    ///
    /// @tparam elementSize the size of each element
    /// @tparam elementCount the number of elements
    ///
    template <size_t elementSize, size_t elementCount>
    struct UnsignedMulti
    {
        using Element = Unsigned<elementSize>;

        Element elements[elementCount];

        constexpr bool operator==(const UnsignedMulti &) const noexcept = default;

        constexpr UnsignedMulti operator~() const noexcept;
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

    template <typename T> struct _RawTypeHelper { using type = Unsigned<sizeof(T)>; };
    template <typename T> requires (alignof(T) != sizeof(T) || sizeof(T) > sizeof(uintmax_t))
    struct _RawTypeHelper<T>
    {
        static constexpr size_t align{alignof(T) > alignof(uintmax_t) ? alignof(uintmax_t) : alignof(T)};
        using type = UnsignedMulti<align, sizeof(T) / align>;
    };

    ///
    /// The "raw" type that matches the key type's size and alignment and is used to alias the key
    ///
    template <typename T> using RawType = typename _RawTypeHelper<T>::type;

    ///
    /// This default hash simply "grabs" the least significant `size_t`'s worth of data from the key's underlying binary
    ///
    /// May specialize for custom types. Must provide a `size_t operator(const T &)` method that returns something
    /// implicitly convertible to `size_t`. The lowest bits are used to map to a slot, so prioritize low-order entropy
    ///
    /// Must provide `size_t operator(const U &)` method to support a heterogeneous type `U`
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
    /// May specialize for custom types. Must provide a `size_t operator(const K &)` method that returns something
    /// implicitly convertible to `size_t`. The lowest bits are used to map to a slot, so prioritize low-order entropy
    ///
    /// Must provide `size_t operator(const U &)` method to support a heterogeneous type `U`
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

    ///
    /// Direct FastHash function that hashes the given value
    ///
    /// @param v the value to hash
    /// @return the hash of the value
    ///
    template <typename T> constexpr size_t fastHash(const T & v) noexcept;

    ///
    /// Direct FastHash function that hashes the given data
    ///
    /// @param data the data to hash
    /// @param length the length of the data in bytes
    /// @return the hash of the data
    ///
    size_t fastHash(const void * data, size_t length) noexcept;

    // TODO: Only needed due to limited MSVC `requires` keyword support. This should be inlined
    template <typename K, typename H> concept _Hashable = requires (const H h, const K k) { size_t{h(k)}; };

    ///
    /// Indicates whether `KOther` is heterogeneous with `K`. May specialize to enable heterogeneous lookup for custom
    /// types
    ///
    template <typename K, typename KOther> struct IsCompatible : std::bool_constant<std::is_same_v<std::decay_t<K>, std::decay_t<KOther>>> {};
    template <NativeSignedInteger K, NativeSignedInteger KOther> struct IsCompatible<K, KOther> : std::bool_constant<sizeof(KOther) <= sizeof(K)> {};
    template <NativeUnsignedInteger K, NativeUnsignedInteger KOther> struct IsCompatible<K, KOther> : std::bool_constant<sizeof(KOther) <= sizeof(K)> {};
    template <NativeSignedInteger K, NativeUnsignedInteger KOther> struct IsCompatible<K, KOther> : std::bool_constant<sizeof(KOther) < sizeof(K)> {};
    template <NativeUnsignedInteger K, NativeSignedInteger KOther> struct IsCompatible<K, KOther> : std::false_type {};
    template <typename T, typename TOther> requires (std::is_same_v<std::decay_t<T>, std::decay_t<TOther>> || std::is_base_of_v<T, TOther>) struct IsCompatible<T *, TOther *> : std::true_type {};
    template <typename T, typename TOther> requires (std::is_same_v<std::decay_t<T>, std::decay_t<TOther>> || std::is_base_of_v<T, TOther>) struct IsCompatible<std::unique_ptr<T>, TOther *> : std::true_type {};
    template <typename T, typename TOther> requires (std::is_same_v<std::decay_t<T>, std::decay_t<TOther>> || std::is_base_of_v<T, TOther>) struct IsCompatible<std::shared_ptr<T>, TOther *> : std::true_type {};

    ///
    /// Specifies whether a key of type `KOther` may be used for lookup operations on a map/set with key type `K`
    ///
    template <typename KOther, typename K> concept Compatible = Rawable<K> && Rawable<KOther> && IsCompatible<K, KOther>::value;

    ///
    /// An associative container that stores unique-key key-pair values. Uses a flat memory model, linear probing, and a
    /// whole lot of optimizations that make this an extremely fast map for small elements
    ///
    /// A custom hasher must provide a `operator(const K &)` that returns something implicitly convertible to `size_t`.
    /// Additionally, the hash function should provide good low-order entropy, as the low bits determine the slot index
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
    /// A custom hasher must provide a `operator(const K &)` that returns something implicitly convertible to `size_t`.
    /// Additionally, the hash function should provide good low-order entropy, as the low bits determine the slot index
    ///
    /// @tparam K the key type
    /// @tparam H the functor type for hashing keys
    /// @tparam A the allocator type
    ///
    template <Rawable K, typename H = IdentityHash<K>, typename A = std::allocator<K>> using RawSet = RawMap<K, void, H, A>;

    template <Rawable K, typename V, typename H, typename A> class RawMap
    {
        static constexpr bool _isSet{std::is_same_v<V, void>};
        static constexpr bool _isMap{!_isSet};

        ///
        /// Element type
        ///
        using E = std::conditional_t<_isSet, K, std::pair<K, V>>;

        // Internal iterator class forward declaration. Prefer `iterator` and `const_iterator`
        template <bool constant> class _Iterator;

        // Friend class used for testing
        //friend struct _RawFriend;

        public: //--------------------------------------------------------------

        static_assert(std::is_nothrow_move_constructible_v<E>);
        static_assert(std::is_nothrow_move_assignable_v<E>);
        static_assert(std::is_nothrow_swappable_v<E>);
        static_assert(std::is_nothrow_destructible_v<E>);

        static_assert(_Hashable<K, H>);
        static_assert(std::is_nothrow_move_constructible_v<H>);
        static_assert(std::is_nothrow_move_assignable_v<H>);
        static_assert(std::is_nothrow_swappable_v<H>);
        static_assert(std::is_nothrow_destructible_v<H>);

        static_assert(std::is_nothrow_move_constructible_v<A>);
        static_assert(std::is_nothrow_move_assignable_v<A> || !std::allocator_traits<A>::propagate_on_container_move_assignment::value);
        static_assert(std::is_nothrow_swappable_v<A> || !std::allocator_traits<A>::propagate_on_container_swap::value);
        static_assert(std::is_nothrow_destructible_v<A>);

        using key_type = K;
        using mapped_type = V;
        using value_type = E;
        using hasher = H;
        using allocator_type = A;
        using reference = E &;
        using const_reference = const E &;
        using pointer = E *;
        using const_pointer = const E *;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using iterator = _Iterator<false>;
        using const_iterator = _Iterator<true>;

        ///
        /// Constructs a new map/set
        ///
        /// The number of backing slots will be the smallest power of two greater than or equal to twice `minCapacity`
        ///
        /// Memory is not allocated until the first element is inserted
        ///
        /// @param minCapacity the minimum cpacity
        /// @param hash the hasher
        /// @param alloc the allocator
        ///
        explicit RawMap(size_t minCapacity = config::minCapacity, const H & hash = {}, const A & alloc = {}) noexcept;
        RawMap(size_t minCapacity, const A & alloc) noexcept;
        explicit RawMap(const A & alloc) noexcept;

        ///
        /// Constructs a new map/set from copies of the elements within the iterator range
        ///
        /// The number of backing slots will be the smallest power of two greater than or equal to twice the larger of
        /// `minCapacity` or the number of elements within the iterator range
        ///
        /// @param first iterator to the first element to copy, inclusive
        /// @param last iterator to the last element to copy, exclusive
        /// @param minCapacity the minumum capacity
        /// @param hash the hasher
        /// @param alloc the allocator
        ///
        template <typename It> RawMap(It first, It last, size_t minCapacity = {}, const H & hash = {}, const A & alloc = {});
        template <typename It> RawMap(It first, It last, size_t minCapacity, const A & alloc);

        ///
        /// Constructs a new map/set from copies of the elements in the initializer list
        ///
        /// The number of backing slots will be the smallest power of two greater than or equal to twice the larger of
        /// `minCapacity` or the number of elements in the initializer list
        ///
        /// @param elements the elements to copy
        /// @param minCapacity the minumum capacity
        /// @param hash the hasher
        /// @param alloc the allocator
        ///
        RawMap(std::initializer_list<E> elements, size_t minCapacity = {}, const H & hash = {}, const A & alloc = {});
        RawMap(std::initializer_list<E> elements, size_t minCapacity, const A & alloc);

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
        RawMap(RawMap && other) noexcept;

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
        RawMap & operator=(RawMap && other) noexcept;

        ///
        /// Destructor - all elements are destructed and all memory is freed
        ///
        ~RawMap() noexcept;

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
        void clear() noexcept;

        ///
        /// @param key the key to check for
        /// @returns whether the heterogeneous key is present
        ///
        template <Compatible<K> K_> bool contains(const K_ & key) const;

        ///
        /// @param key the key to count
        /// @returns `1` if the heterogeneous key is present or `0` if it is absent
        ///
        template <Compatible<K> K_> size_t count(const K_ & key) const;

        ///
        /// Gets the present element for the heterogeneous key
        ///
        /// Defined only for maps, not for sets
        ///
        /// @param key the key to retrieve
        /// @returns the element for the key
        /// @throws `std::out_of_range` if the key is absent
        ///
        template <Compatible<K> K_> std::add_lvalue_reference_t<V> at(const K_ & key) requires (!std::is_same_v<V, void>);
        template <Compatible<K> K_> std::add_lvalue_reference_t<const V> at(const K_ & key) const requires (!std::is_same_v<V, void>);

        ///
        /// Gets the element for the key, creating a new element if it is not already present
        ///
        /// Defined only for maps, not for sets
        ///
        /// @param key the key to retrieve
        /// @returns the element for the key
        ///
        std::add_lvalue_reference_t<V> operator[](const K & key) requires (!std::is_same_v<V, void>);
        std::add_lvalue_reference_t<V> operator[](K && key) requires (!std::is_same_v<V, void>);

        ///
        /// @returns an iterator to the first element in the map/set
        ///
        iterator begin() noexcept;
        const_iterator begin() const noexcept;
        const_iterator cbegin() const noexcept;

        ///
        /// @returns an iterator that is conceptually one-past the end of the map/set or an invalid position
        ///
        iterator end() noexcept;
        const_iterator end() const noexcept;
        const_iterator cend() const noexcept;

        ///
        /// @param key the key to find
        /// @returns an iterator to the element for the key if present, or the end iterator if absent
        ///
        template <Compatible<K> K_> iterator find(const K_ & key);
        template <Compatible<K> K_> const_iterator find(const K_ & key) const;

        ///
        /// @returns the index of the slot into which the heterogeneous key would fall
        ///
        template <Compatible<K> K_> size_t slot(const K_ & key) const noexcept;

        ///
        /// Ensures there are enough slots to comfortably hold `capacity` number of elements
        ///
        /// Equivalent to `rehash(2 * capacity)`
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param capacity the minimum capacity
        ///
        void reserve(size_t capacity);

        ///
        /// Ensures the number of slots is equal to the smallest power of two greater than or equal to both `slotCount`
        /// and the current size, down to a minimum of `config::minSlotCount`
        ///
        /// Equivalent to `reserve(slotCount / 2)`
        ///
        /// Invalidates iterators if there is a rehash
        ///
        /// @param slotCount the minimum slot count
        ///
        void rehash(size_t slotCount);

        ///
        /// Swaps the contents of this map/set with the other's
        ///
        /// Does not allocate or copy memory
        ///
        /// Invalidates iterators
        ///
        /// @param other the map/set to swap with
        ///
        void swap(RawMap & other) noexcept;

        ///
        /// @returns the number of elements in the map/set
        ///
        size_t size() const noexcept;

        ///
        /// @returns whether the map/set is empty
        ///
        bool empty() const noexcept;

        ///
        /// @returns how many elements the map/set can hold before needing to rehash; equivalent to `slot_count() / 2`
        ///
        size_t capacity() const noexcept;

        ///
        /// @returns the number of slots in the map/set; equivalent to `capacity() * 2`
        ///
        size_t slot_count() const noexcept;

        ///
        /// @returns the maximum possible element count; equivalent to `max_slot_count() * 2`
        ///
        size_t max_size() const noexcept;

        ///
        /// @returns the maximum possible slot count; equivalent to `max_size() / 2`
        ///
        size_t max_slot_count() const noexcept;

        ///
        /// @returns the ratio of elements to slots, maximum being 0.5
        ///
        float load_factor() const noexcept;

        ///
        /// @returns 0.5, the maximum possible load factor
        ///
        float max_load_factor(float lf) const noexcept;

        ///
        /// @returns the hasher
        ///
        const H & hash_function() const noexcept;

        ///
        /// @returns the allocator
        ///
        const A & get_allocator() const noexcept;

        private: //-------------------------------------------------------------

        using _RawKey = RawType<K>;

        static constexpr _RawKey _vacantKey{_RawKey(~_RawKey{})};
        static constexpr _RawKey _graveKey{_RawKey(~_RawKey{1u})};
        static constexpr _RawKey _specialKeys[2]{_graveKey, _vacantKey};
        static constexpr _RawKey _vacantGraveKey{_vacantKey};
        static constexpr _RawKey _vacantVacantKey{_graveKey};
        static constexpr _RawKey _vacantSpecialKeys[2]{_vacantGraveKey, _vacantVacantKey};
        static constexpr _RawKey _terminalKey{0u};

        static K & _key(E & element) noexcept;
        static const K & _key(const E & element) noexcept;

        static bool _isPresent(const _RawKey & key) noexcept;

        static bool _isSpecial(const _RawKey & key) noexcept;

        uint32_t _size;
        uint32_t _slotCount; // Does not include special elements
        E * _elements;
        bool _haveSpecial[2];
        H _hash;
        A _alloc;

        template <typename KTuple, typename VTuple, size_t... kIndices, size_t... vIndices> std::pair<iterator, bool> _emplace(KTuple && kTuple, VTuple && vTuple, std::index_sequence<kIndices...>, std::index_sequence<vIndices...>);

        template <bool preserveInvariants> void _clear() noexcept;

        template <Compatible<K> K_> size_t _slot(const K_ & key) const noexcept;

        void _rehash(size_t slotCount);

        template <bool zeroControls> void _allocate();

        void _deallocate();

        void _clearKeys() noexcept;

        template <bool move> void _forwardData(std::conditional_t<move, RawMap, const RawMap> & other);

        template <bool insertionForm> struct _FindKeyResult;
        template <> struct _FindKeyResult<false> { E * element; bool isPresent; };
        template <> struct _FindKeyResult<true> { E * element; bool isPresent; bool isSpecial; unsigned char specialI; };

        // If the key is not present, returns the slot after the the key's bucket
        template <bool insertionForm, Compatible<K> K_> _FindKeyResult<insertionForm> _findKey(const K_ & key) const noexcept;
    };

    template <Rawable K, typename V, typename H, typename A> bool operator==(const RawMap<K, V, H, A> & m1, const RawMap<K, V, H, A> & m2);

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    class RawMap<K, V, H, A>::_Iterator
    {
        friend ::qc::hash::RawMap<K, V, H, A>;
        //friend ::qc::hash::_RawFriend;

        using E = std::conditional_t<constant, const RawMap::E, RawMap::E>;

        public: //--------------------------------------------------------------

        using iterator_category = std::forward_iterator_tag;
        using value_type = E;
        using difference_type = ptrdiff_t;
        using pointer = E *;
        using reference = E &;

        ///
        /// Default constructor - equivalent to the end iterator
        ///
        constexpr _Iterator() noexcept = default;

        ///
        /// Copy constructor - a mutable iterator may be implicitly converted to a const iterator
        /// @param other the iterator to copy
        ///
        constexpr _Iterator(const _Iterator & other) noexcept = default;
        template <bool constant_> requires (constant && !constant_) constexpr _Iterator(const _Iterator<constant_> & other) noexcept;

        ///
        /// @returns the element pointed to by the iterator; undefined for invalid iterators
        ///
        E & operator*() const noexcept;

        ///
        /// @returns a pointer to the element pointed to by the iterator; undefined for invalid iterators
        ///
        E * operator->() const noexcept;

        ///
        /// Increments the iterator to point to the next element in the map/set, or the end iterator if there are no more
        /// elements
        ///
        /// Incrementing the end iterator is undefined
        ///
        /// @returns this
        ///
        _Iterator & operator++() noexcept;

        ///
        /// Increments the iterator to point to the next element in the map/set, or the end iterator if there are no more
        /// elements
        ///
        /// Incrementing the end iterator is undefined
        ///
        /// @returns a copy of the iterator before it was incremented
        ///
        _Iterator operator++(int) noexcept;

        ///
        /// @param other the other iterator to compare with
        /// @returns whether this iterator is equivalent to the other iterator
        ///
        template <bool constant_> bool operator==(const _Iterator<constant_> & other) const noexcept;

        private: //-------------------------------------------------------------

        E * _element;

        constexpr _Iterator(E * element) noexcept;
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
    template <typename K, typename V, typename H, typename A> void swap(qc::hash::RawMap<K, V, H, A> & a, qc::hash::RawMap<K, V, H, A> & b) noexcept;
}

// INLINE IMPLEMENTATION ///////////////////////////////////////////////////////////////////////////////////////////////

namespace qc::hash
{
    constexpr size_t _minSlotCount{config::minCapacity * 2u};

    // Returns the lowest `size_t`'s worth of bytes from the given object
    template <typename T>
    inline constexpr size_t _toSizeT(const T & v) noexcept
    {
        // Key is aligned as `size_t` and can be simply reinterpreted as such
        if constexpr (alignof(T) >= sizeof(size_t)) {
            return reinterpret_cast<const size_t &>(v);
        }
        // Key's alignment matches its size and can be simply reinterpreted as an unsigned integer
        else if constexpr (alignof(T) == sizeof(T)) {
            return reinterpret_cast<const Unsigned<sizeof(T)> &>(v);
        }
        // Key is not nicely aligned, manually copy up to a `size_t`'s worth of memory
        // Could use memcpy, but this gives better debug performance, and both compile to the same in release
        else {
            size_t result{0u};
            using Block = Unsigned<alignof(T) < sizeof(size_t) ? alignof(T) : sizeof(size_t)>;
            constexpr size_t n{(sizeof(T) < sizeof(size_t) ? sizeof(T) : sizeof(size_t)) / sizeof(Block)};
            const Block * src{reinterpret_cast<const Block *>(&v)};
            Block * dst{reinterpret_cast<Block *>(&result)};

            // We want the lower-order bytes, so need to adjust on big endian systems
            if constexpr (std::endian::native == std::endian::big) {
                constexpr size_t srcBlocks{sizeof(T) / sizeof(Block)};
                constexpr size_t dstBlocks{sizeof(size_t) / sizeof(Block)};
                if constexpr (srcBlocks > n) {
                    src += srcBlocks - n;
                }
                if constexpr (dstBlocks > n) {
                    dst += dstBlocks - n;
                }
            }

            // Copy blocks
            if constexpr (n >= 1) dst[0] = src[0];
            if constexpr (n >= 2) dst[1] = src[1];
            if constexpr (n >= 3) dst[2] = src[2];
            if constexpr (n >= 4) dst[3] = src[3];
            if constexpr (n >= 5) dst[4] = src[4];
            if constexpr (n >= 6) dst[5] = src[5];
            if constexpr (n >= 7) dst[6] = src[6];
            if constexpr (n >= 8) dst[7] = src[7];

            return result;
        }
    }

    template <size_t elementSize, size_t elementCount>
    inline constexpr auto UnsignedMulti<elementSize, elementCount>::operator~() const noexcept -> UnsignedMulti
    {
        UnsignedMulti res;
        for (size_t i{0u}; i < elementCount; ++i) {
            res.elements[i] = Element(~elements[i]);
        }
        return res;
    }

    template <Rawable T>
    struct IdentityHash
    {
        constexpr size_t operator()(const T & v) const noexcept
        {
            return _toSizeT(v);
        }
    };

    template <typename T>
    struct IdentityHash<T *>
    {
        constexpr size_t operator()(const T * const v) const noexcept
        {
            // Bit shift away the low zero bits to maximize low-order entropy
            constexpr int shift{int(std::bit_width(alignof(T)) - 1u)};
            return reinterpret_cast<size_t>(v) >> shift;
        }
    };

    template <typename T>
    struct IdentityHash<std::unique_ptr<T>> : IdentityHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using IdentityHash<T *>::operator();

        constexpr size_t operator()(const std::unique_ptr<T> & v) const noexcept
        {
            return (*this)(v.get());
        }
    };

    template <typename T>
    struct IdentityHash<std::shared_ptr<T>> : IdentityHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using IdentityHash<T *>::operator();

        constexpr size_t operator()(const std::shared_ptr<T> & v) const noexcept
        {
            return (*this)(v.get());
        }
    };

    template <typename T>
    struct FastHash
    {
        constexpr size_t operator()(const T & v) const noexcept
        {
            return fastHash(v);
        }
    };

    template <typename T>
    struct FastHash<T *>
    {
        constexpr size_t operator()(const T * const v) const noexcept
        {
            return fastHash(v);
        }
    };

    template <typename T>
    struct FastHash<std::unique_ptr<T>> : FastHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using FastHash<T *>::operator();

        constexpr size_t operator()(const std::unique_ptr<T> & v) const noexcept
        {
            return (*this)(v.get());
        }
    };

    template <typename T>
    struct FastHash<std::shared_ptr<T>> : FastHash<T *>
    {
        // Allows heterogeneity with raw pointers
        using FastHash<T *>::operator();

        constexpr size_t operator()(const std::shared_ptr<T> & v) const noexcept
        {
            return (*this)(v.get());
        }
    };

    template <>
    struct FastHash<std::string>
    {
        size_t operator()(const std::string & v) const noexcept
        {
            return fastHash(v.c_str(), v.length());
        }

        size_t operator()(const std::string_view & v) const noexcept
        {
            return fastHash(v.data(), v.length());
        }

        size_t operator()(const char * v) const noexcept
        {
            return fastHash(v, std::strlen(v));
        }
    };

    // Same as `std::string` specialization
    template <> struct FastHash<std::string_view> : FastHash<std::string> {};

    namespace _fastHash
    {
        static_assert(sizeof(size_t) == 4u || sizeof(size_t) == 8u);

        inline constexpr size_t m{sizeof(size_t) == 4 ? 0x5BD1E995u : 0xC6A4A7935BD1E995u};
        inline constexpr int r{sizeof(size_t) == 4 ? 24 : 47};

        inline constexpr size_t mix(size_t v) noexcept
        {
            v *= m;
            v ^= v >> r;
            return v * m;
        }
    }

    template <typename T>
    inline constexpr size_t fastHash(const T & v) noexcept
    {
        using namespace _fastHash;

        // IMPORTANT: These two cases must yield the same hash for the same input bytes

        // Optimized case if the key fits within a single word
        if constexpr (sizeof(T) <= sizeof(size_t)) {
            return (sizeof(T) * m) ^ mix(_toSizeT(v));
        }
        // General case
        else {
            return fastHash(&v, sizeof(T));
        }
    }

    // Based on Murmur2, but simplified, and doesn't require unaligned reads
    inline size_t fastHash(const void * const data, size_t length) noexcept
    {
        using namespace _fastHash;

        const uint8_t * bytes{static_cast<const uint8_t *>(data)};
        size_t h{length};

        // Mix in words at a time
        while (length >= sizeof(size_t)) {
            size_t w;
            std::memcpy(&w, bytes, sizeof(size_t));

            h *= m;
            h ^= mix(w);

            bytes += sizeof(size_t);
            length -= sizeof(size_t);
        };

        // Mix in the last few bytes
        if (length) {
            size_t w{0u};
            std::memcpy(&w, bytes, length);

            h *= m;
            h ^= mix(w);
        }

        return h;
    }

    template <typename K>
    inline RawType<K> & _raw(K & key) noexcept
    {
        return reinterpret_cast<RawType<K> &>(key);
    }

    template <typename K>
    inline const RawType<K> & _raw(const K & key) noexcept
    {
        return reinterpret_cast<const RawType<K> &>(key);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const size_t minCapacity, const H & hash, const A & alloc) noexcept:
        _size{0},
        _slotCount{uint32_t(minCapacity <= config::minCapacity ? _minSlotCount : std::bit_ceil(minCapacity << 1))},
        _elements{},
        _haveSpecial{},
        _hash{hash},
        _alloc{alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const size_t minCapacity, const A & alloc) noexcept :
        RawMap{minCapacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const A & alloc) noexcept :
        RawMap{config::minCapacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    template <typename It>
    inline RawMap<K, V, H, A>::RawMap(const It first, const It last, const size_t minCapacity, const H & hash, const A & alloc) :
        RawMap{minCapacity, hash, alloc}
    {
        // Count number of elements to insert
        size_t n{};
        for (It it{first}; it != last; ++it, ++n);

        reserve(n);

        insert(first, last);
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename It>
    inline RawMap<K, V, H, A>::RawMap(const It first, const It last, const size_t minCapacity, const A & alloc) :
        RawMap{first, last, minCapacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const std::initializer_list<E> elements, size_t minCapacity, const H & hash, const A & alloc) :
        RawMap{minCapacity ? minCapacity : elements.size(), hash, alloc}
    {
        insert(elements);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const std::initializer_list<E> elements, const size_t minCapacity, const A & alloc) :
        RawMap{elements, minCapacity, H{}, alloc}
    {}

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(const RawMap & other) :
        _size{other._size},
        _slotCount{other._slotCount},
        _elements{},
        _haveSpecial{other._haveSpecial[0], other._haveSpecial[1]},
        _hash{other._hash},
        _alloc{std::allocator_traits<A>::select_on_container_copy_construction(other._alloc)}
    {
        if (_size) {
            _allocate<false>();
            _forwardData<false>(other);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::RawMap(RawMap && other) noexcept :
        _size{std::exchange(other._size, 0u)},
        _slotCount{std::exchange(other._slotCount, _minSlotCount)},
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
        if (&other == this) {
            return *this;
        }

        if (_elements) {
            _clear<false>();
            if (!other._size || _slotCount != other._slotCount || _alloc != other._alloc) {
                _deallocate();
            }
        }

        _size = other._size;
        _slotCount = other._slotCount;
        _haveSpecial[0] = other._haveSpecial[0];
        _haveSpecial[1] = other._haveSpecial[1];
        _hash = other._hash;
        if constexpr (std::allocator_traits<A>::propagate_on_container_copy_assignment::value) {
            _alloc = std::allocator_traits<A>::select_on_container_copy_construction(other._alloc);
        }

        if (_size) {
            if (!_elements) {
                _allocate<false>();
            }

            _forwardData<false>(other);
        }

        return *this;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A> & RawMap<K, V, H, A>::operator=(RawMap && other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        if (_elements) {
            _clear<false>();
            _deallocate();
        }

        _size = other._size;
        _slotCount = other._slotCount;
        _haveSpecial[0] = other._haveSpecial[0];
        _haveSpecial[1] = other._haveSpecial[1];
        _hash = std::move(other._hash);
        if constexpr (std::allocator_traits<A>::propagate_on_container_move_assignment::value) {
            _alloc = std::move(other._alloc);
        }

        if (_alloc == other._alloc || std::allocator_traits<A>::propagate_on_container_move_assignment::value) {
            _elements = std::exchange(other._elements, nullptr);
            other._size = {};
        }
        else {
            if (_size) {
                _allocate<false>();
                _forwardData<true>(other);
                other._clear<false>();
                other._size = 0u;
            }
            if (other._elements) {
                other._deallocate();
            }
        }

        other._slotCount = _minSlotCount;
        other._haveSpecial[0] = false;
        other._haveSpecial[1] = false;

        return *this;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline RawMap<K, V, H, A>::~RawMap() noexcept
    {
        if (_elements) {
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
        while (first != last) {
            emplace(*first);
            ++first;
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::insert(const std::initializer_list<E> elements)
    {
        for (const E & element : elements) {
            emplace(element);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::emplace(const E & element) -> std::pair<iterator, bool>
    {
        static_assert(std::is_copy_constructible_v<E>);

        if constexpr (_isSet) {
            return try_emplace(element);
        }
        else {
            return try_emplace(element.first, element.second);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::emplace(E && element) -> std::pair<iterator, bool>
    {
        if constexpr (_isSet) {
            return try_emplace(std::move(element));
        }
        else {
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
        return try_emplace(std::forward<KArgs>(keyArgs)...);
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename... KArgs, typename... VArgs>
    inline auto RawMap<K, V, H, A>::emplace(const std::piecewise_construct_t, std::tuple<KArgs...> && keyArgs, std::tuple<VArgs...> && valueArgs) -> std::pair<iterator, bool> requires (!std::is_same_v<V, void>)
    {
        return _emplace(std::move(keyArgs), std::move(valueArgs), std::index_sequence_for<KArgs...>(), std::index_sequence_for<VArgs...>());
    }

    template <Rawable K, typename V, typename H, typename A>
    template <typename KTuple, typename VTuple, size_t... kIndices, size_t... vIndices>
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
        if (!_elements) {
            _allocate<true>();
        }

        _FindKeyResult<true> findResult{_findKey<true>(key)};

        // Key is already present
        if (findResult.isPresent) {
            return std::pair<iterator, bool>{iterator{findResult.element}, false};
        }

        if (findResult.isSpecial) [[unlikely]] {
            _haveSpecial[findResult.specialI] = true;
        }
        else {
            // Rehash if we're at capacity
            if ((_size - _haveSpecial[0] - _haveSpecial[1]) >= (_slotCount * 7 / 8)) [[unlikely]] {
                _rehash(_slotCount << 1);
                findResult = _findKey<true>(key);
            }
        }

        if constexpr (_isSet) {
            std::allocator_traits<A>::construct(_alloc, findResult.element, std::forward<K_>(key));
        }
        else {
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
        if (!_size) {
            return false;
        }

        const auto [element, isPresent]{_findKey<false>(key)};

        if (isPresent) {
            erase(iterator{element});
            return true;
        }
        else {
            return false;
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::erase(const iterator position)
    {
        E * const eraseElement{position._element};
        _RawKey & rawKey{_raw(_key(*eraseElement))};
        E * const specialElements{_elements + _slotCount};

        std::allocator_traits<A>::destroy(_alloc, eraseElement);

        // General case
        if (eraseElement < specialElements) {
            rawKey = _graveKey;
        }
        else [[unlikely]] {
            const auto specialI{eraseElement - specialElements};
            _raw(_key(specialElements[specialI])) = _vacantSpecialKeys[specialI];
            _haveSpecial[specialI] = false;
        }

        --_size;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::clear() noexcept
    {
        _clear<true>();
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool preserveInvariants>
    inline void RawMap<K, V, H, A>::_clear() noexcept
    {
        if constexpr (std::is_trivially_destructible_v<E>) {
            if constexpr (preserveInvariants) {
                if (_size) {
                    _clearKeys();
                    _size = {};
                    _haveSpecial[0] = false;
                    _haveSpecial[1] = false;
                }
            }
        }
        else {
            if (_size) {
                // General case
                E * element{_elements};
                size_t n{};
                const size_t regularElementCount{_size - _haveSpecial[0] - _haveSpecial[1]};
                for (; n < regularElementCount; ++element) {
                    _RawKey & rawKey{_raw(_key(*element))};
                    if (_isPresent(rawKey)) {
                        std::allocator_traits<A>::destroy(_alloc, element);
                        ++n;
                    }
                    if constexpr (preserveInvariants) {
                        rawKey = _vacantKey;
                    }
                }
                // Clear remaining graves
                if constexpr (preserveInvariants) {
                    const E * const endRegularElement{_elements + _slotCount};
                    for (; element < endRegularElement; ++element) {
                        _raw(_key(*element)) = _vacantKey;
                    }
                }

                // Special keys case
                if (_haveSpecial[0]) [[unlikely]] {
                    element = _elements + _slotCount;
                    std::allocator_traits<A>::destroy(_alloc, element);
                    if constexpr (preserveInvariants) {
                        _raw(_key(*element)) = _vacantGraveKey;
                        _haveSpecial[0] = false;
                    }
                }
                if (_haveSpecial[1]) [[unlikely]] {
                    element = _elements + _slotCount + 1;
                    std::allocator_traits<A>::destroy(_alloc, element);
                    if constexpr (preserveInvariants) {
                        _raw(_key(*element)) = _vacantVacantKey;
                        _haveSpecial[1] = false;
                    }
                }

                if constexpr (preserveInvariants) {
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
    inline size_t RawMap<K, V, H, A>::count(const K_ & key) const
    {
        return contains(key);
    }

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
        if (!_size) {
            throw std::out_of_range{"Map is empty"};
        }

        const auto [element, isPresent]{_findKey<false>(key)};

        if (!isPresent) {
            throw std::out_of_range{"Element not found"};
        }

        return element->second;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline std::add_lvalue_reference_t<V> RawMap<K, V, H, A>::operator[](const K & key) requires (!std::is_same_v<V, void>)
    {
        return try_emplace(key).first->second;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline std::add_lvalue_reference_t<V> RawMap<K, V, H, A>::operator[](K && key) requires (!std::is_same_v<V, void>)
    {
        return try_emplace(std::move(key)).first->second;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::begin() noexcept -> iterator
    {
        // Separated to dodge a compiler warning
        const const_iterator cit{static_cast<const RawMap *>(this)->begin()};
        return reinterpret_cast<const iterator &>(cit);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::begin() const noexcept -> const_iterator
    {
        // General case
        if (_size - _haveSpecial[0] - _haveSpecial[1]) [[likely]] {
            for (const E * element{_elements}; ; ++element) {
                if (_isPresent(_raw(_key(*element)))) {
                    return const_iterator{element};
                }
            }
        }

        // Special key cases
        if (_haveSpecial[0]) [[unlikely]] {
            return const_iterator{_elements + _slotCount};
        }
        if (_haveSpecial[1]) [[unlikely]] {
            return const_iterator{_elements + _slotCount + 1};
        }

        return end();
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::cbegin() const noexcept -> const_iterator
    {
        return begin();
    }

    template <Rawable K, typename V, typename H, typename A>
    inline typename RawMap<K, V, H, A>::iterator RawMap<K, V, H, A>::end() noexcept
    {
        return iterator{};
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::end() const noexcept -> const_iterator
    {
        return const_iterator{};
    }

    template <Rawable K, typename V, typename H, typename A>
    inline auto RawMap<K, V, H, A>::cend() const noexcept -> const_iterator
    {
        return const_iterator{};
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline auto RawMap<K, V, H, A>::find(const K_ & key) -> iterator
    {
        // Separated to dodge a compiler warning
        const const_iterator temp{static_cast<const RawMap *>(this)->find(key)};
        return reinterpret_cast<const iterator &>(temp);
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline auto RawMap<K, V, H, A>::find(const K_ & key) const -> const_iterator
    {
        if (!_size) {
            return cend();
        }

        const auto [element, isPresent]{_findKey<false>(key)};
        return isPresent ? const_iterator{element} : cend();
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline size_t RawMap<K, V, H, A>::slot(const K_ & key) const noexcept
    {
        const _RawKey & rawKey{_raw(key)};
        if (_isSpecial(rawKey)) [[unlikely]] {
            return _slotCount + (rawKey == _vacantKey);
        }
        else {
            return _slot(key);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    template <Compatible<K> K_>
    inline size_t RawMap<K, V, H, A>::_slot(const K_ & key) const noexcept
    {
        return _hash(key) & (_slotCount - 1u);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::reserve(const size_t capacity)
    {
        rehash(capacity << 1);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::rehash(size_t slotCount)
    {
        const size_t currentMinSlotCount{_size <= config::minCapacity ? _minSlotCount : ((_size - _haveSpecial[0] - _haveSpecial[1]) << 1)};
        if (slotCount < currentMinSlotCount) {
            slotCount = currentMinSlotCount;
        }
        else {
            slotCount = std::bit_ceil(slotCount);
        }

        if (slotCount != _slotCount) {
            if (_elements) {
                _rehash(slotCount);
            }
            else {
                _slotCount = slotCount;
            }
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::_rehash(const size_t slotCount)
    {
        const size_t oldSize{_size};
        const size_t oldSlotCount{_slotCount};
        E * const oldElements{_elements};
        const bool oldHaveSpecial[2]{_haveSpecial[0], _haveSpecial[1]};

        _size = {};
        _slotCount = slotCount;
        _allocate<true>();
        _haveSpecial[0] = false;
        _haveSpecial[1] = false;

        // General case
        size_t n{};
        const size_t regularElementCount{oldSize - oldHaveSpecial[0] - oldHaveSpecial[1]};
        for (E * element{oldElements}; n < regularElementCount; ++element) {
            if (_isPresent(_raw(_key(*element)))) {
                emplace(std::move(*element));
                std::allocator_traits<A>::destroy(_alloc, element);
                ++n;
            }
        }

        // Special keys case
        if (oldHaveSpecial[0]) [[unlikely]] {
            E * const oldElement{oldElements + oldSlotCount};
            std::allocator_traits<A>::construct(_alloc, _elements + _slotCount, std::move(*oldElement));
            std::allocator_traits<A>::destroy(_alloc, oldElement);
            ++_size;
            _haveSpecial[0] = true;
        }
        if (oldHaveSpecial[1]) [[unlikely]] {
            E * const oldElement{oldElements + oldSlotCount + 1};
            std::allocator_traits<A>::construct(_alloc, _elements + _slotCount + 1, std::move(*oldElement));
            std::allocator_traits<A>::destroy(_alloc, oldElement);
            ++_size;
            _haveSpecial[1] = true;
        }

        std::allocator_traits<A>::deallocate(_alloc, oldElements, oldSlotCount + 4u);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::swap(RawMap & other) noexcept
    {
        std::swap(_size, other._size);
        std::swap(_slotCount, other._slotCount);
        std::swap(_elements, other._elements);
        std::swap(_haveSpecial, other._haveSpecial);
        std::swap(_hash, other._hash);
        if constexpr (std::allocator_traits<A>::propagate_on_container_swap::value) {
            std::swap(_alloc, other._alloc);
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline size_t RawMap<K, V, H, A>::size() const noexcept
    {
        return _size;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool RawMap<K, V, H, A>::empty() const noexcept
    {
        return !_size;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline size_t RawMap<K, V, H, A>::capacity() const noexcept
    {
        return _slotCount >> 1;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline size_t RawMap<K, V, H, A>::slot_count() const noexcept
    {
        return _slotCount;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline size_t RawMap<K, V, H, A>::max_size() const noexcept
    {
        return (max_slot_count() >> 1) + 2u;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline size_t RawMap<K, V, H, A>::max_slot_count() const noexcept
    {
        return size_t(1u) << (std::numeric_limits<size_t>::digits - 1);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline float RawMap<K, V, H, A>::load_factor() const noexcept
    {
        return float(_size) / float(_slotCount);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline float RawMap<K, V, H, A>::max_load_factor(float lf) const noexcept
    {
        return float(config::minCapacity) / float(_minSlotCount);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline const H & RawMap<K, V, H, A>::hash_function() const noexcept
    {
        return _hash;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline const A & RawMap<K, V, H, A>::get_allocator() const noexcept
    {
        return _alloc;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline K & RawMap<K, V, H, A>::_key(E & element) noexcept
    {
        if constexpr (_isSet) return element;
        else return element.first;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline const K & RawMap<K, V, H, A>::_key(const E & element) noexcept
    {
        if constexpr (_isSet) return element;
        else return element.first;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool RawMap<K, V, H, A>::_isPresent(const _RawKey & key) noexcept
    {
        return !_isSpecial(key);
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool RawMap<K, V, H, A>::_isSpecial(const _RawKey & key) noexcept
    {
        return key == _vacantKey || key == _graveKey;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool zeroKeys>
    inline void RawMap<K, V, H, A>::_allocate()
    {
        _elements = std::allocator_traits<A>::allocate(_alloc, _slotCount + 4u);

        if constexpr (zeroKeys) {
            _clearKeys();
        }

        // Set the trailing keys to special terminal values so iterators know when to stop
        _raw(_key(_elements[_slotCount + 2])) = _terminalKey;
        _raw(_key(_elements[_slotCount + 3])) = _terminalKey;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::_deallocate()
    {
        std::allocator_traits<A>::deallocate(_alloc, _elements, _slotCount + 4u);
        _elements = nullptr;
    }

    template <Rawable K, typename V, typename H, typename A>
    inline void RawMap<K, V, H, A>::_clearKeys() noexcept
    {
        // General case
        E * const specialElements{_elements + _slotCount};
        for (E * element{_elements}; element < specialElements; ++element) {
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
        if constexpr (std::is_trivially_copyable_v<E>) {
            std::memcpy(_elements, other._elements, (_slotCount + 2u) * sizeof(E));
        }
        else {
            using ElementForwardType = std::conditional_t<move, E &&, const E &>;

            // General case
            std::conditional_t<move, E, const E> * srcElement{other._elements};
            const E * const srcEndElement{other._elements + _slotCount};
            E * dstElement{_elements};
            for (; srcElement < srcEndElement; ++srcElement, ++dstElement) {
                const _RawKey & rawSrcKey{_raw(_key(*srcElement))};
                if (_isPresent(rawSrcKey)) {
                    std::allocator_traits<A>::construct(_alloc, dstElement, static_cast<ElementForwardType>(*srcElement));
                }
                else {
                    _raw(_key(*dstElement)) = rawSrcKey;
                }
            }

            // Special keys case
            if (_haveSpecial[0]) {
                std::allocator_traits<A>::construct(_alloc, _elements + _slotCount, static_cast<ElementForwardType>(other._elements[_slotCount]));
            }
            else {
                _raw(_key(_elements[_slotCount])) = _vacantGraveKey;
            }
            if (_haveSpecial[1]) {
                std::allocator_traits<A>::construct(_alloc, _elements + _slotCount + 1, static_cast<ElementForwardType>(other._elements[_slotCount + 1]));
            }
            else {
                _raw(_key(_elements[_slotCount + 1])) = _vacantVacantKey;
            }
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool insertionForm, Compatible<K> K_>
    inline auto RawMap<K, V, H, A>::_findKey(const K_ & key) const noexcept -> _FindKeyResult<insertionForm>
    {
        const _RawKey & rawKey{_raw(key)};

        // Special key case
        if (_isSpecial(rawKey)) [[unlikely]] {
            const unsigned char specialI{rawKey == _vacantKey};
            if constexpr (insertionForm) {
                return _FindKeyResult<insertionForm>{.element = _elements + _slotCount + specialI, .isPresent = _haveSpecial[specialI], .isSpecial = true, .specialI = specialI};
            }
            else {
                return _FindKeyResult<insertionForm>{.element = _elements + _slotCount + specialI, .isPresent = _haveSpecial[specialI]};
            }
        }

        // General case

        const E * const lastElement{_elements + _slotCount};

        E * element{_elements + _slot(key)};
        E * grave{};

        while (true) {
            const _RawKey & rawSlotKey{_raw(_key(*element))};

            if (rawSlotKey == rawKey) {
                return {.element = element, .isPresent = true};
            }

            if (rawSlotKey == _vacantKey) {
                if constexpr (insertionForm) {
                    return {.element = grave ? grave : element, .isPresent = false};
                }
                else {
                    return {.element = element, .isPresent = false};
                }
            }

            if constexpr (insertionForm) {
                if (rawSlotKey == _graveKey) {
                    grave = element;
                }
            }

            ++element;
            if (element == lastElement) [[unlikely]] {
                element = _elements;
            }
        }
    }

    template <Rawable K, typename V, typename H, typename A>
    inline bool operator==(const RawMap<K, V, H, A> & m1, const RawMap<K, V, H, A> & m2)
    {
        if (m1.size() != m2.size()) {
            return false;
        }

        if (&m1 == &m2) {
            return true;
        }

        const auto endIt{m2.cend()};

        for (const auto & element : m1) {
            if constexpr (std::is_same_v<V, void>) {
                if (!m2.contains(element)) {
                    return false;
                }
            }
            else {
                const auto it{m2.find(element.first)};
                if (it == endIt || it->second != element.second) {
                    return false;
                }
            }
        }

        return true;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    template <bool constant_> requires (constant && !constant_)
    inline constexpr RawMap<K, V, H, A>::_Iterator<constant>::_Iterator(const _Iterator<constant_> & other) noexcept:
        _element{other._element}
    {}

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline constexpr RawMap<K, V, H, A>::_Iterator<constant>::_Iterator(E * const element) noexcept :
        _element{element}
    {}

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator*() const noexcept -> E &
    {
        return *_element;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator->() const noexcept -> E *
    {
        return _element;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator++() noexcept -> _Iterator &
    {
        while (true) {
            ++_element;
            const _RawKey & rawKey{_raw(_key(*_element))};

            // Either general present case or terminal case
            if (_isPresent(rawKey)) {
                if (rawKey == _terminalKey) [[unlikely]] {
                    // Terminal case
                    if (_raw(_key(_element[1])) == _terminalKey) {
                        _element = nullptr;
                    }
                }

                return *this;
            }

            // Either general absent case with terminal two ahead or special case
            if (_raw(_key(_element[2])) == _terminalKey) [[unlikely]] {
                // At second special slot
                if (_raw(_key(_element[1])) == _terminalKey) [[unlikely]] {
                    if (rawKey == _vacantVacantKey) [[likely]] {
                        _element = nullptr;
                    }

                    return *this;
                }

                // At first special slot
                if (_raw(_key(_element[3])) == _terminalKey) [[likely]] {
                    if (rawKey == _vacantGraveKey) [[likely]] {
                        if (_raw(_key(_element[1])) == _vacantVacantKey) [[likely]] {
                            _element = nullptr;
                        }
                        else {
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
    inline auto RawMap<K, V, H, A>::_Iterator<constant>::operator++(int) noexcept -> _Iterator
    {
        const _Iterator temp{*this};
        operator++();
        return temp;
    }

    template <Rawable K, typename V, typename H, typename A>
    template <bool constant>
    template <bool constant_>
    inline bool RawMap<K, V, H, A>::_Iterator<constant>::operator==(const _Iterator<constant_> & other) const noexcept
    {
        return _element == other._element;
    }
}

namespace std
{
    template <typename K, typename V, typename H, typename A>
    inline void swap(qc::hash::RawMap<K, V, H, A> & a, qc::hash::RawMap<K, V, H, A> & b) noexcept
    {
        a.swap(b);
    }
}
