#ifndef JG_DENSE_HASH_MAP_HPP
#define JG_DENSE_HASH_MAP_HPP

#include "details/bucket_iterator.hpp"
#include "details/dense_hash_map_iterator.hpp"
#include "details/node.hpp"
#include "details/power_of_two_growth_policy.hpp"
#include "details/type_traits.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace jg
{
namespace details
{
    static constexpr const float default_max_load_factor = 0.875f;

    template <
        class Key, class T, class Container, bool isConst, bool projectToConstKey, class Nodes>
    [[nodiscard]] constexpr auto bucket_iterator_to_iterator(
        const bucket_iterator<Key, T, Container, isConst, projectToConstKey>& bucket_it,
        Nodes& nodes) -> dense_hash_map_iterator<Key, T, Container, isConst, projectToConstKey>
    {
        if (bucket_it.current_node_index() == details::node_end_index<Key, T>)
        {
            return dense_hash_map_iterator<Key, T, Container, isConst, projectToConstKey>{
                nodes.end()};
        }
        else
        {
            return dense_hash_map_iterator<Key, T, Container, isConst, projectToConstKey>{
                std::next(nodes.begin(), bucket_it.current_node_index())};
        }
    }

    template <class Alloc, class T>
    using rebind_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

    template <class Hash>
    using detect_transparent_key_equal = typename Hash::transparent_key_equal;

    template <class Hash>
    inline constexpr bool is_transparent_key_equal_v =
        is_detected<detect_transparent_key_equal, Hash>::value;

    template <class Pred>
    using detect_is_transparent = typename Pred::is_transparent;

    template <class Pred>
    inline constexpr bool is_transparent_v = is_detected<detect_is_transparent, Pred>::value;

    template <class Hash, class Pred, class Key, bool = is_transparent_key_equal_v<Hash>>
    struct key_equal
    {
        using type = Pred;
    };

    template <class Hash, class Pred, class Key>
    struct key_equal<Hash, Pred, Key, true>
    {
        using type = typename Hash::transparent_key_equal;

        static_assert(
            is_transparent_v<type>,
            "The associated transparent key equal is missing a is_transparent tag type.");
        static_assert(
            std::is_same_v<Pred, std::equal_to<Key>> || std::is_same_v<Pred, type>,
            "The associated transparent key equal must be the transparent_key_equal tag or "
            "std::equal_to<Key>");
    };

    template <class InputIt>
    using iter_key_t =
        std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>;

    template <class InputIt>
    using iter_val_t = typename std::iterator_traits<InputIt>::value_type::second_type;

    template <class InputIt>
    using iter_to_alloc_t = std::pair<std::add_const_t<iter_key_t<InputIt>>, iter_val_t<InputIt>>;

    template <class Alloc>
    using detect_allocate = decltype(std::declval<Alloc&>().allocate(std::size_t{}));

    template <class Alloc>
    using detect_value_type = typename Alloc::value_type;

    template <class Alloc>
    inline constexpr bool is_iterator_v = details::is_detected<detect_allocate, Alloc>::value&&
        details::is_detected<detect_value_type, Alloc>::value;

    template <class Alloc>
    using require_allocator = std::enable_if_t<is_iterator_v<Alloc>>;

    template <class T>
    using require_not_allocator = std::enable_if_t<!is_iterator_v<T>>;

    template <class T>
    using require_not_allocator_and_integral =
        std::enable_if_t<!is_iterator_v<T> && !std::is_integral_v<T>>;

    template <class It>
    using require_input_iterator = std::enable_if_t<!std::is_integral_v<It>>;

} // namespace details

template <
    class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
    class Allocator = std::allocator<std::pair<const Key, T>>,
    class GrowthPolicy = details::power_of_two_growth_policy>
class dense_hash_map : private GrowthPolicy
{
private:
    using node_type = details::node<Key, T>;
    using nodes_container_type =
        std::vector<node_type, details::rebind_alloc<Allocator, node_type>>;
    using nodes_size_type = typename nodes_container_type::size_type;
    using buckets_container_type =
        std::vector<nodes_size_type, details::rebind_alloc<Allocator, nodes_size_type>>;
    using node_index_type = details::node_index_t<Key, T>;
    using GrowthPolicy::compute_closest_capacity;
    using GrowthPolicy::compute_index;
    using GrowthPolicy::minimum_capacity;
    using deduced_key_equal = typename details::key_equal<Hash, Pred, Key>::type;

    static inline constexpr node_index_type node_end_index = details::node_end_index<Key, T>;

    static inline constexpr bool is_nothrow_move_constructible =
        std::allocator_traits<Allocator>::is_always_equal::value &&
        std::is_nothrow_move_constructible_v<Hash> &&
        std::is_nothrow_move_constructible_v<deduced_key_equal> &&
        std::is_nothrow_move_constructible_v<nodes_container_type> &&
        std::is_nothrow_move_constructible_v<buckets_container_type>;
    static inline constexpr bool is_nothrow_move_assignable =
        std::allocator_traits<Allocator>::is_always_equal::value &&
        std::is_nothrow_move_assignable_v<Hash> &&
        std::is_nothrow_move_assignable_v<deduced_key_equal> &&
        std::is_nothrow_move_assignable_v<nodes_container_type> &&
        std::is_nothrow_move_assignable_v<buckets_container_type>;
    static inline constexpr bool is_nothrow_swappable =
        std::is_nothrow_swappable_v<buckets_container_type> &&
        std::is_nothrow_swappable_v<nodes_container_type> &&
        std::allocator_traits<Allocator>::is_always_equal::value &&
        std::is_nothrow_swappable_v<Hash> && std::is_nothrow_swappable_v<deduced_key_equal>;
    static inline constexpr bool is_nothrow_default_constructible =
        std::is_nothrow_default_constructible_v<buckets_container_type> &&
        std::is_nothrow_default_constructible_v<nodes_container_type> &&
        std::is_nothrow_default_constructible_v<Hash> &&
        std::is_nothrow_default_constructible_v<deduced_key_equal>;

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = nodes_size_type;
    using difference_type = typename nodes_container_type::difference_type;
    using hasher = Hash;
    using key_equal = deduced_key_equal;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename std::allocator_traits<allocator_type>::pointer;
    using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
    using iterator = details::dense_hash_map_iterator<Key, T, nodes_container_type, false, true>;
    using const_iterator =
        details::dense_hash_map_iterator<Key, T, nodes_container_type, true, true>;
    using local_iterator = details::bucket_iterator<Key, T, nodes_container_type, false, true>;
    using const_local_iterator = details::bucket_iterator<Key, T, nodes_container_type, true, true>;

    constexpr dense_hash_map() noexcept(is_nothrow_default_constructible)
        : dense_hash_map(minimum_capacity())
    {}

    constexpr explicit dense_hash_map(
        size_type bucket_count, const Hash& hash = Hash(), const key_equal& equal = key_equal(),
        const allocator_type& alloc = allocator_type())
        : hash_(hash), key_equal_(equal), buckets_(alloc), nodes_(alloc)
    {
        rehash(bucket_count);
    }

    constexpr dense_hash_map(size_type bucket_count, const allocator_type& alloc)
        : dense_hash_map(bucket_count, hasher(), key_equal(), alloc)
    {}

    constexpr dense_hash_map(
        size_type bucket_count, const hasher& hash, const allocator_type& alloc)
        : dense_hash_map(bucket_count, hash, key_equal(), alloc)
    {}

    constexpr explicit dense_hash_map(const allocator_type& alloc)
        : dense_hash_map(minimum_capacity(), hasher(), key_equal(), alloc)
    {}

    template <class InputIt>
    constexpr dense_hash_map(
        InputIt first, InputIt last, size_type bucket_count = minimum_capacity(),
        const Hash& hash = Hash(), const key_equal& equal = key_equal(),
        const allocator_type& alloc = allocator_type())
        : dense_hash_map(bucket_count, hash, equal, alloc)
    {
        insert(first, last);
    }

    template <class InputIt>
    constexpr dense_hash_map(
        InputIt first, InputIt last, size_type bucket_count, const allocator_type& alloc)
        : dense_hash_map(first, last, bucket_count, hasher(), key_equal(), alloc)
    {}

    template <class InputIt>
    constexpr dense_hash_map(
        InputIt first, InputIt last, size_type bucket_count, const hasher& hash,
        const allocator_type& alloc)
        : dense_hash_map(first, last, bucket_count, hash, key_equal(), alloc)
    {}

    constexpr dense_hash_map(const dense_hash_map& other)
        : dense_hash_map(
              other, std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                         other.get_allocator()))
    {}

    constexpr dense_hash_map(const dense_hash_map& other, const allocator_type& alloc)
        : hash_(other.hash_)
        , key_equal_(other.key_equal_)
        , buckets_(other.buckets_, alloc)
        , nodes_(other.nodes_, alloc)
    {}

    constexpr dense_hash_map(dense_hash_map&& other) noexcept(is_nothrow_move_constructible) =
        default;

    constexpr dense_hash_map(dense_hash_map&& other, const allocator_type& alloc)
        : hash_(std::move(other.hash_))
        , key_equal_(std::move(other.key_equal_))
        , buckets_(std::move(other.buckets_), alloc)
        , nodes_(std::move(other.nodes_), alloc)
    {}

    constexpr dense_hash_map(
        std::initializer_list<value_type> init, size_type bucket_count = minimum_capacity(),
        const hasher& hash = hasher(), const key_equal& equal = key_equal(),
        const allocator_type& alloc = allocator_type())
        : dense_hash_map(init.begin(), init.end(), bucket_count, hash, equal, alloc)
    {}

    constexpr dense_hash_map(
        std::initializer_list<value_type> init, size_type bucket_count, const allocator_type& alloc)
        : dense_hash_map(init, bucket_count, hasher(), key_equal(), alloc)
    {}

    constexpr dense_hash_map(
        std::initializer_list<value_type> init, size_type bucket_count, const hasher& hash,
        const allocator_type& alloc)
        : dense_hash_map(init, bucket_count, hash, key_equal(), alloc)
    {}

    // 2 missing constructors from https://cplusplus.github.io/LWG/issue2713
    template <class InputIterator>
    dense_hash_map(InputIterator first, InputIterator last, const allocator_type& alloc)
        : dense_hash_map(first, last, minimum_capacity(), hasher(), key_equal(), alloc)
    {}

    dense_hash_map(std::initializer_list<value_type> init, const allocator_type& alloc)
        : dense_hash_map(init, minimum_capacity(), hasher(), key_equal(), alloc)
    {}

    ~dense_hash_map() = default;

    constexpr auto operator=(const dense_hash_map& other) -> dense_hash_map& = default;
    constexpr auto operator=(dense_hash_map&& other) noexcept(is_nothrow_move_assignable)
        -> dense_hash_map& = default;

    constexpr auto operator=(std::initializer_list<value_type> ilist) -> dense_hash_map&
    {
        clear();
        insert(ilist.begin(), ilist.end());
        return *this;
    }

    constexpr auto get_allocator() const -> allocator_type { return buckets_.get_allocator(); }

    constexpr auto begin() noexcept -> iterator { return iterator{nodes_.begin()}; }

    constexpr auto begin() const noexcept -> const_iterator
    {
        return const_iterator{nodes_.begin()};
    }

    constexpr auto cbegin() const noexcept -> const_iterator
    {
        return const_iterator{nodes_.cbegin()};
    }

    constexpr auto end() noexcept -> iterator { return iterator{nodes_.end()}; }

    constexpr auto end() const noexcept -> const_iterator { return const_iterator{nodes_.end()}; }

    constexpr auto cend() const noexcept -> const_iterator { return const_iterator{nodes_.cend()}; }

    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return nodes_.empty(); }

    constexpr auto size() const noexcept -> size_type { return nodes_.size(); }

    constexpr auto max_size() const noexcept -> size_type { return nodes_.max_size(); }

    constexpr void clear() noexcept
    {
        nodes_.clear();
        buckets_.clear();
        rehash(0u);
    }

    constexpr auto insert(const value_type& value) -> std::pair<iterator, bool>
    {
        return emplace(value);
    }

    constexpr auto insert(value_type&& value) -> std::pair<iterator, bool>
    {
        return emplace(std::move(value));
    }

    template <class P, std::enable_if_t<std::is_constructible_v<value_type, P&&>, int> = 0>
    constexpr auto insert(P&& value) -> std::pair<iterator, bool>
    {
        return emplace(std::forward<P>(value));
    }

    constexpr auto insert(const_iterator /*hint*/, const value_type& value) -> iterator
    {
        return insert(value).first;
    }

    constexpr auto insert(const_iterator /*hint*/, value_type&& value) -> iterator
    {
        return insert(std::move(value)).first;
    }

    template <class P, std::enable_if_t<std::is_constructible_v<value_type, P&&>, int> = 0>
    constexpr auto insert(const_iterator /*hint*/, P&& value) -> iterator
    {
        return insert(std::move(value)).first;
    }

    template <class InputIt>
    constexpr void insert(InputIt first, InputIt last)
    {
        for (; first != last; ++first)
        {
            insert(*first);
        }
    }

    constexpr void insert(std::initializer_list<value_type> ilist)
    {
        insert(ilist.begin(), ilist.end());
    }

    template <class M>
    constexpr auto insert_or_assign(const key_type& k, M&& obj) -> std::pair<iterator, bool>
    {
        auto result = try_emplace(k, std::forward<M>(obj));

        if (!result.second)
        {
            result.first->second = std::forward<M>(obj);
        }

        return result;
    }

    template <class M>
    constexpr auto insert_or_assign(key_type&& k, M&& obj) -> std::pair<iterator, bool>
    {
        auto result = try_emplace(std::move(k), std::forward<M>(obj));

        if (!result.second)
        {
            result.first->second = std::forward<M>(obj);
        }

        return result;
    }

    template <class M>
    constexpr auto insert_or_assign(const_iterator /*hint*/, const key_type& k, M&& obj) -> iterator
    {
        return insert_or_assign(k, std::forward<M>(obj)).first;
    }

    template <class M>
    constexpr auto insert_or_assign(const_iterator /*hint*/, key_type&& k, M&& obj) -> iterator
    {
        return insert_or_assign(std::move(k), std::forward<M>(obj)).first;
    }

    template <class... Args>
    auto emplace(Args&&... args) -> std::pair<iterator, bool>
    {
        return dispatch_emplace(std::forward<Args>(args)...);
    }

    template <class... Args>
    auto emplace_hint(const_iterator /*hint*/, Args&&... args) -> iterator
    {
        return emplace(std::forward<Args>(args)...).first;
    }

    template <class... Args>
    constexpr auto try_emplace(const key_type& key, Args&&... args) -> std::pair<iterator, bool>
    {
        return do_emplace(
            key, std::piecewise_construct, std::forward_as_tuple(key),
            std::forward_as_tuple(std::forward<Args>(args)...));
    }

    template <class... Args>
    constexpr auto try_emplace(key_type&& key, Args&&... args) -> std::pair<iterator, bool>
    {
        return do_emplace(
            key, std::piecewise_construct, std::forward_as_tuple(std::move(key)),
            std::forward_as_tuple(std::forward<Args>(args)...));
    }

    template <class... Args>
    constexpr auto try_emplace(const_iterator /*hint*/, const key_type& key, Args&&... args)
        -> iterator
    {
        return try_emplace(key, std::forward<Args>(args)...).iterator;
    }

    template <class... Args>
    constexpr auto try_emplace(const_iterator /*hint*/, key_type&& key, Args&&... args) -> iterator
    {
        return try_emplace(std::move(key), std::forward<Args>(args)...).iterator;
    }

    constexpr auto erase(const_iterator pos) -> iterator
    {
        const auto position = std::distance(cbegin(), pos);
        const auto it = std::next(begin(), position);
        const auto previous_next = find_previous_next_using_position(pos->first, position);
        return do_erase(previous_next, it.sub_iterator()).first;
    }

    constexpr auto erase(const_iterator first, const_iterator last) -> iterator
    {
        bool stop = first == last;
        while (!stop)
        {
            --last;
            stop = first == last; // if first == last, erase would invalidate both!
            last = erase(last);
        }

        const auto position = std::distance(cbegin(), last);
        return std::next(begin(), position);
    }

    constexpr auto erase(const key_type& key) -> size_type
    {
        // We have to find out the node we look for and the pointer to it.
        const auto bindex = bucket_index(key);

        std::size_t* previous_next = &buckets_[bindex];

        for (;;)
        {
            if (*previous_next == node_end_index)
            {
                return 0;
            }

            auto& node = nodes_[*previous_next];

            if (key_equal_(node.pair.pair().first, key))
            {
                break;
            }

            previous_next = &node.next;
        }

        do_erase(previous_next, std::next(nodes_.begin(), *previous_next));

        return 1;
    }

    constexpr void swap(dense_hash_map& other) noexcept(is_nothrow_swappable)
    {
        using std::swap;
        swap(buckets_, other.buckets_);
        swap(nodes_, other.nodes_);
        swap(max_load_factor_, other.max_load_factor_);
        swap(hash_, other.hash_);
        swap(key_equal_, other.key_equal_);
    }

    constexpr auto at(const key_type& key) -> T&
    {
        const auto it = find(key);

        if (it == end())
        {
#ifdef JG_NO_EXCEPTION
            std::abort();
#else
            //throw std::out_of_range("The specified key does not exists in this map.");
#endif
        }

        return it->second;
    }

    constexpr auto at(const key_type& key) const -> const T&
    {
        const auto it = find(key);

        if (it == end())
        {
#ifdef JG_NO_EXCEPTION
            std::abort();
#else
            //throw std::out_of_range("The specified key does not exists in this map.");
#endif
        }

        return it->second;
    }

    constexpr auto operator[](const key_type& key) -> T&
    {
        return this->try_emplace(key).first->second;
    }

    constexpr auto operator[](key_type&& key) -> T&
    {
        return this->try_emplace(std::move(key)).first->second;
    }

    constexpr auto count(const key_type& key) const -> size_type
    {
        return find(key) == end() ? 0u : 1u;
    }

    template <
        class K, class Useless = std::enable_if_t<details::is_transparent_key_equal_v<Hash>, K>>
    constexpr auto count(const K& key) const -> size_type
    {
        return find(key) == end() ? 0u : 1u;
    }

    constexpr auto find(const key_type& key) -> iterator
    {
        return details::bucket_iterator_to_iterator(find_in_bucket(key, bucket_index(key)), nodes_);
    }

    constexpr auto find(const key_type& key) const -> const_iterator
    {
        return details::bucket_iterator_to_iterator(find_in_bucket(key, bucket_index(key)), nodes_);
    }

    template <
        class K, class Useless = std::enable_if_t<details::is_transparent_key_equal_v<Hash>, K>>
    constexpr auto find(const K& key) -> iterator
    {
        return details::bucket_iterator_to_iterator(find_in_bucket(key, bucket_index(key)), nodes_);
    }

    template <
        class K, class Useless = std::enable_if_t<details::is_transparent_key_equal_v<Hash>, K>>
    constexpr auto find(const K& key) const -> const_iterator
    {
        return details::bucket_iterator_to_iterator(find_in_bucket(key, bucket_index(key)), nodes_);
    }

    constexpr auto contains(const key_type& key) const -> bool { return find(key) != end(); }

    template <
        class K, class Useless = std::enable_if_t<details::is_transparent_key_equal_v<Hash>, K>>
    constexpr auto contains(const K& key) const -> bool
    {
        return find(key) != end();
    }

    constexpr auto equal_range(const Key& key) -> std::pair<iterator, iterator>
    {
        const auto it = find(key);

        if (it == end())
        {
            return {it, it};
        }

        return {it, std::next(it)};
    }

    constexpr auto equal_range(const Key& key) const -> std::pair<const_iterator, const_iterator>
    {
        const auto it = find(key);

        if (it == end())
        {
            return {it, it};
        }

        return {it, std::next(it)};
    }

    template <
        class K, class Useless = std::enable_if_t<details::is_transparent_key_equal_v<Hash>, K>>
    constexpr auto equal_range(const K& key) -> std::pair<iterator, iterator>
    {
        const auto it = find(key);

        if (it == end())
        {
            return {it, it};
        }

        return {it, std::next(it)};
    }

    template <
        class K, class Useless = std::enable_if_t<details::is_transparent_key_equal_v<Hash>, K>>
    constexpr auto equal_range(const K& key) const -> std::pair<const_iterator, const_iterator>
    {
        const auto it = find(key);

        if (it == end())
        {
            return {it, it};
        }

        return {it, std::next(it)};
    }

    constexpr auto begin(size_type n) -> local_iterator
    {
        return local_iterator{buckets_[n], nodes_};
    }

    constexpr auto begin(size_type n) const -> const_local_iterator
    {
        return const_local_iterator{buckets_[n], nodes_};
    }

    constexpr auto cbegin(size_type n) const -> const_local_iterator
    {
        return const_local_iterator{buckets_[n], nodes_};
    }

    constexpr auto end(size_type /*n*/) -> local_iterator { return local_iterator{nodes_}; }

    constexpr auto end(size_type /*n*/) const -> const_local_iterator
    {
        return const_local_iterator{nodes_};
    }

    constexpr auto cend(size_type /*n*/) const -> const_local_iterator
    {
        return const_local_iterator{nodes_};
    }

    constexpr auto bucket_count() const -> size_type { return buckets_.size(); }

    constexpr auto max_bucket_count() const -> size_type { return buckets_.max_size(); }

    constexpr auto bucket_size(size_type n) const -> size_type
    {
        return static_cast<size_t>(std::distance(begin(n), end(n)));
    }

    constexpr auto bucket(const key_type& key) const -> size_type { return bucket_index(key); }

    constexpr auto load_factor() const -> float
    {
        return size() / static_cast<float>(bucket_count());
    }

    constexpr auto max_load_factor() const -> float { return max_load_factor_; }

    constexpr void max_load_factor(float ml)
    {
        assert(ml > 0.0f && "The max load factor must be greater than 0.0f.");
        max_load_factor_ = ml;
        rehash(8);
    }

    constexpr void rehash(size_type count)
    {
        count = std::max(minimum_capacity(), count);
        count = std::max(count, static_cast<size_type>(size() / max_load_factor()));

        count = compute_closest_capacity(count);

        assert(count > 0 && "The computed rehash size must be greater than 0.");

        if (count == buckets_.size())
        {
            return;
        }

        buckets_.resize(count);

        std::fill(buckets_.begin(), buckets_.end(), node_end_index);

        node_index_type index{0u};

        for (auto& entry : nodes_)
        {
            entry.next = node_end_index;
            reinsert_entry(entry, index);
            index++;
        }
    }

    constexpr void reserve(std::size_t count)
    {
        rehash((size_t)std::ceil(count / max_load_factor()));
        nodes_.reserve(count);
    }

    constexpr auto hash_function() const -> hasher { return hash_; }

    constexpr auto key_eq() const -> key_equal { return key_equal_; }

private:
    template <class K>
    constexpr auto bucket_index(const K& key) const -> size_type
    {
        return compute_index(hash_(key), buckets_.size());
    }

    template <class K>
    constexpr auto find_in_bucket(const K& key, std::size_t bucket_index) -> local_iterator
    {
        auto b = begin(bucket_index);
        auto e = end(0u);
        auto it = std::find_if(b, e, [&key, this](auto& p) { return key_equal_(p.first, key); });
        return it;
    }

    template <class K>
    constexpr auto find_in_bucket(const K& key, std::size_t bucket_index) const
        -> const_local_iterator
    {
        auto b = begin(bucket_index);
        auto e = end(0u);
        auto it = std::find_if(b, e, [&key, this](auto& p) { return key_equal_(p.first, key); });
        return it;
    }

    constexpr auto
    do_erase(std::size_t* previous_next, typename nodes_container_type::iterator sub_it)
        -> std::pair<iterator, bool>
    {
        // Skip the node by pointing the previous "next" to the one sub_it currently point to.
        *previous_next = sub_it->next;

        auto last = std::prev(nodes_.end());

        // No need to do anything if the node was at the end of the vector.
        if (sub_it == last)
        {
            nodes_.pop_back();
            return {end(), true};
        }

        // Swap last node and the one we want to delete.
        using std::swap;
        swap(*sub_it, *last);

        // Now sub_it points to the one we swapped with. We have to readjust sub_it.
        previous_next =
            find_previous_next_using_position(sub_it->pair.pair().first, nodes_.size() - 1);
        *previous_next = std::distance(nodes_.begin(), sub_it);

        // Delete the last node forever and ever.
        nodes_.pop_back();

        return {iterator{sub_it}, true};
    }

    constexpr auto find_previous_next_using_position(const key_type& key, std::size_t position)
        -> std::size_t*
    {
        const std::size_t bindex = bucket_index(key);

        auto previous_next = &buckets_[bindex];
        while (*previous_next != position)
        {
            previous_next = &nodes_[*previous_next].next;
        }

        return previous_next;
    }

    constexpr void reinsert_entry(node_type& entry, node_index_type index)
    {
        const auto bindex = bucket_index(entry.pair.const_key_pair().first);
        auto old_index = std::exchange(buckets_[bindex], index);
        entry.next = old_index;
    }

    constexpr void check_for_rehash()
    {
        if (size() + 1 > bucket_count() * max_load_factor())
        {
            rehash(bucket_count() * 2);
        }
    }

    constexpr auto dispatch_emplace() -> std::pair<iterator, bool>
    {
        return do_emplace(key_type{});
    }

    template <class Key2, class T2>
    constexpr auto dispatch_emplace(Key2&& key, T2&& t) -> std::pair<iterator, bool>
    {
        if constexpr (std::is_same_v<std::decay_t<Key2>, key_type>)
        {
            return do_emplace(key, std::forward<Key2>(key), std::forward<T2>(t));
        }
        else
        {
            key_type new_key{std::forward<Key2>(key)};
            // TODO: double check that I am allowed to optimized by sending new_key here and not key
            // https://eel.is/c++draft/unord.req#lib:emplace,unordered_associative_containers
            return do_emplace(new_key, std::move(new_key), std::forward<T2>(t));
        }
    }

    template <class Pair>
    constexpr auto dispatch_emplace(Pair&& p) -> std::pair<iterator, bool>
    {
        if constexpr (std::is_same_v<std::decay_t<decltype(p.first)>, key_type>)
        {
            return do_emplace(p.first, std::forward<Pair>(p));
        }
        else
        {
            key_type new_key{std::forward<Pair>(p).first};
            return do_emplace(new_key, std::move(new_key), std::forward<Pair>(p).second);
        }
    }

    template <class... Args1, class... Args2>
    constexpr auto dispatch_emplace(
        std::piecewise_construct_t, std::tuple<Args1...> first_args,
        std::tuple<Args2...> second_args) -> std::pair<iterator, bool>
    {
        std::pair<Key, T> p{std::piecewise_construct, std::move(first_args),
                            std::move(second_args)};
        return dispatch_emplace(std::move(p));
    }

    template <class... Args>
    constexpr auto do_emplace(const key_type& key, Args&&... args) -> std::pair<iterator, bool>
    {
        check_for_rehash();

        const auto bindex = bucket_index(key);
        auto local_it = find_in_bucket(key, bindex);

        if (local_it != end(0u))
        {
            return std::pair{details::bucket_iterator_to_iterator(local_it, nodes_), false};
        }

        nodes_.emplace_back(buckets_[bindex], std::forward<Args>(args)...);
        buckets_[bindex] = nodes_.size() - 1;

        return std::pair{std::prev(end()), true};
    }

    hasher hash_;
    key_equal key_equal_;

    buckets_container_type buckets_;
    nodes_container_type nodes_;
    float max_load_factor_ = details::default_max_load_factor;
};

template <class Key, class T, class Hash, class KeyEqual, class Allocator>
constexpr auto operator==(
    const dense_hash_map<Key, T, Hash, KeyEqual, Allocator>& lhs,
    const dense_hash_map<Key, T, Hash, KeyEqual, Allocator>& rhs) -> bool
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (const auto& item : lhs)
    {
        const auto it = rhs.find(item.first);

        if (it == rhs.end() || it->second != item.second)
        {
            return false;
        }
    }

    return true;
}

template <class Key, class T, class Hash, class KeyEqual, class Allocator>
constexpr auto operator!=(
    const dense_hash_map<Key, T, Hash, KeyEqual, Allocator>& lhs,
    const dense_hash_map<Key, T, Hash, KeyEqual, Allocator>& rhs) -> bool
{
    return !(lhs == rhs);
}

template <
    class InputIt, class Hash = std::hash<details::iter_key_t<InputIt>>,
    class Pred = std::equal_to<details::iter_key_t<InputIt>>,
    class Alloc = std::allocator<details::iter_to_alloc_t<InputIt>>,
    class = details::require_input_iterator<InputIt>,
    class = details::require_not_allocator_and_integral<Hash>,
    class = details::require_not_allocator<Pred>, class = details::require_allocator<Alloc>>
dense_hash_map(
    InputIt, InputIt,
    details::node_index_t<details::iter_key_t<InputIt>, details::iter_val_t<InputIt>> = 8,
    Hash = Hash(), Pred = Pred(), Alloc = Alloc())
    ->dense_hash_map<details::iter_key_t<InputIt>, details::iter_val_t<InputIt>, Hash, Pred, Alloc>;

template <
    class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
    class Alloc = std::allocator<std::pair<const Key, T>>,
    class = details::require_not_allocator_and_integral<Hash>,
    class = details::require_not_allocator<Pred>, class = details::require_allocator<Alloc>>
dense_hash_map(
    std::initializer_list<std::pair<Key, T>>, details::node_index_t<Key, T> = 8, Hash = Hash(),
    Pred = Pred(), Alloc = Alloc())
    ->dense_hash_map<Key, T, Hash, Pred, Alloc>;

template <
    class InputIt, class Alloc, class = details::require_input_iterator<InputIt>,
    class = details::require_allocator<Alloc>>
dense_hash_map(
    InputIt, InputIt,
    details::node_index_t<details::iter_key_t<InputIt>, details::iter_val_t<InputIt>>, Alloc)
    ->dense_hash_map<
        details::iter_key_t<InputIt>, details::iter_val_t<InputIt>,
        std::hash<details::iter_key_t<InputIt>>, std::equal_to<details::iter_key_t<InputIt>>,
        Alloc>;

// The rule below is supiciously useless.
template <
    class InputIt, class Alloc, class = details::require_input_iterator<InputIt>,
    class = details::require_allocator<Alloc>>
dense_hash_map(InputIt, InputIt, Alloc)
    ->dense_hash_map<
        details::iter_key_t<InputIt>, details::iter_val_t<InputIt>,
        std::hash<details::iter_key_t<InputIt>>, std::equal_to<details::iter_key_t<InputIt>>,
        Alloc>;

template <
    class InputIt, class Hash, class Alloc, class = details::require_input_iterator<InputIt>,
    class = details::require_not_allocator_and_integral<Hash>,
    class = details::require_allocator<Alloc>>
dense_hash_map(
    InputIt, InputIt,
    details::node_index_t<details::iter_key_t<InputIt>, details::iter_val_t<InputIt>>, Hash, Alloc)
    ->dense_hash_map<
        details::iter_key_t<InputIt>, details::iter_val_t<InputIt>, Hash,
        std::equal_to<details::iter_key_t<InputIt>>, Alloc>;

template <class Key, class T, class Alloc, class = details::require_allocator<Alloc>>
dense_hash_map(std::initializer_list<std::pair<Key, T>>, details::node_index_t<Key, T>, Alloc)
    ->dense_hash_map<Key, T, std::hash<Key>, std::equal_to<Key>, Alloc>;

template <class Key, class T, class Alloc, class = details::require_allocator<Alloc>>
dense_hash_map(std::initializer_list<std::pair<Key, T>>, Alloc)
    ->dense_hash_map<Key, T, std::hash<Key>, std::equal_to<Key>, Alloc>;

template <
    class Key, class T, class Hash, class Alloc,
    class = details::require_not_allocator_and_integral<Hash>,
    class = details::require_allocator<Alloc>>
dense_hash_map(std::initializer_list<std::pair<Key, T>>, details::node_index_t<Key, T>, Hash, Alloc)
    ->dense_hash_map<Key, T, Hash, std::equal_to<Key>, Alloc>;

namespace pmr
{
    template <
        class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
        class GrowthPolicy = details::power_of_two_growth_policy>
    using dense_hash_map = dense_hash_map<
        Key, T, Hash, Pred, std::pmr::polymorphic_allocator<std::pair<const Key, T>>, GrowthPolicy>;
} // namespace pmr

} // namespace jg

namespace std
{
template <class Key, class T, class Hash, class Pred, class Allocator, class GrowthPolicy>
constexpr void swap(
    jg::dense_hash_map<Key, T, Hash, Pred, Allocator, GrowthPolicy>& lhs,
    jg::dense_hash_map<Key, T, Hash, Pred, Allocator, GrowthPolicy>&
        rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

template <class Key, class T, class Hash, class KeyEqual, class Alloc, class Pred>
constexpr void erase_if(jg::dense_hash_map<Key, T, Hash, KeyEqual, Alloc>& c, Pred pred)
{
    auto rit = std::make_reverse_iterator(c.end());
    auto rend = std::make_reverse_iterator(c.begin());

    while (rit != rend)
    {
        if (pred(*rit))
        {
            rit = std::make_reverse_iterator(c.erase(std::prev(rit.base())));
        }
        else
        {
            ++rit;
        }
    }
}

} // namespace std

#endif // JG_DENSE_HASH_MAP_HPP
