#ifndef JG_NODE_HPP
#define JG_NODE_HPP

#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace jg::details
{

template <class Key, class T, class Pair = std::pair<Key, T>>
struct node;

template <class Key, class T>
using node_index_t = typename std::vector<node<Key, T>>::size_type;

template <class Key, class T>
constexpr node_index_t<Key, T> node_end_index = std::numeric_limits<node_index_t<Key, T>>::max();

template <class Key, class T>
union union_key_value_pair
{
    using pair_t = std::pair<Key, T>;
    using const_key_pair_t = std::pair<const Key, T>;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    static_assert(
        offsetof(pair_t, first) == offsetof(const_key_pair_t, first) &&
            offsetof(pair_t, second) == offsetof(const_key_pair_t, second),
        "");
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    template <class... Args>
    constexpr union_key_value_pair(Args&&... args) : pair_(std::forward<Args>(args)...)
    {}

    template <class Allocator, class... Args>
    union_key_value_pair(std::allocator_arg_t, const Allocator& alloc, Args&&... args)
    {
        auto alloc_copy = alloc;
        std::allocator_traits<Allocator>::construct(
            alloc_copy, &pair_, std::forward<Args>(args)...);
    }

    constexpr union_key_value_pair(const union_key_value_pair& other) noexcept(
        std::is_nothrow_copy_constructible_v<pair_t>)
        : pair_(other.pair_)
    {}

    constexpr union_key_value_pair(union_key_value_pair&& other) noexcept(
        std::is_nothrow_move_constructible_v<pair_t>)
        : pair_(std::move(other.pair_))
    {}

    constexpr auto
    operator=(const union_key_value_pair& other) noexcept(std::is_nothrow_copy_assignable_v<pair_t>)
        -> union_key_value_pair&
    {
        pair_ = other.pair_;
        return *this;
    }

    constexpr auto
    operator=(union_key_value_pair&& other) noexcept(std::is_nothrow_move_assignable_v<pair_t>)
        -> union_key_value_pair&
    {
        pair_ = std::move(other).pair_;
        return *this;
    }

    ~union_key_value_pair() { pair_.~pair_t(); }

    constexpr pair_t& pair() { return pair_; }

    constexpr const pair_t& pair() const { return pair_; }

    constexpr const_key_pair_t& const_key_pair() { return const_key_pair_; }

    constexpr const const_key_pair_t& const_key_pair() const { return const_key_pair_; }

private:
    pair_t pair_;
    const_key_pair_t const_key_pair_;
};

#ifdef JG_STRICT_TYPE_PUNNING

template <class Key, class T>
struct key_value_pair
{
    using const_key_pair_t = std::pair<const Key, T>;

    template <class... Args>
    constexpr key_value_pair(Args&&... args) : const_key_pair_(std::forward<Args>(args)...)
    {}

    template <class Allocator, class... Args>
    constexpr key_value_pair(std::allocator_arg_t, const Allocator& alloc, Args&&... args)
    {
        auto alloc_copy = alloc;
        std::allocator_traits<Allocator>::construct(
            alloc_copy, &const_key_pair_, std::forward<Args>(args)...);
    }

    constexpr const_key_pair_t& pair() { return const_key_pair_; }

    constexpr const const_key_pair_t& pair() const { return const_key_pair_; }

    constexpr const_key_pair_t& const_key_pair() { return const_key_pair_; }

    constexpr const const_key_pair_t& const_key_pair() const { return const_key_pair_; }

private:
    const_key_pair_t const_key_pair_;
};

template <class Key, class T>
inline constexpr bool are_pairs_standard_layout_v =
    std::is_standard_layout_v<std::pair<const Key, T>>&&
        std::is_standard_layout_v<std::pair<Key, T>>;

template <class Key, class T>
using key_value_pair_t = std::conditional_t<
    are_pairs_standard_layout_v<Key, T>, union_key_value_pair<Key, T>, key_value_pair<Key, T>>;

#else

template <class Key, class T>
using key_value_pair_t = union_key_value_pair<Key, T>;

#endif

template <class T, bool = std::is_copy_constructible_v<T>>
struct disable_copy_constructor
{
    constexpr disable_copy_constructor() = default;
    disable_copy_constructor(const disable_copy_constructor&) = delete;
    constexpr disable_copy_constructor(disable_copy_constructor&&) = default;
    constexpr auto operator=(const disable_copy_constructor&)
        -> disable_copy_constructor& = default;
    constexpr auto operator=(disable_copy_constructor &&) -> disable_copy_constructor& = default;
};

template <class T>
struct disable_copy_constructor<T, true>
{
};

template <class T, bool = std::is_copy_assignable_v<T>>
struct disable_copy_assignment
{
    constexpr disable_copy_assignment() = default;
    constexpr disable_copy_assignment(const disable_copy_assignment&) = default;
    constexpr disable_copy_assignment(disable_copy_assignment&&) = default;
    auto operator=(const disable_copy_assignment&) -> disable_copy_assignment& = delete;
    constexpr auto operator=(disable_copy_assignment &&) -> disable_copy_assignment& = default;
};

template <class T>
struct disable_copy_assignment<T, true>
{
};

template <class T, bool = std::is_move_constructible_v<T>>
struct disable_move_constructor
{
    constexpr disable_move_constructor() = default;
    constexpr disable_move_constructor(const disable_move_constructor&) = default;
    disable_move_constructor(disable_move_constructor&&) = delete;
    constexpr auto operator=(const disable_move_constructor&)
        -> disable_move_constructor& = default;
    constexpr auto operator=(disable_move_constructor &&) -> disable_move_constructor& = default;
};

template <class T>
struct disable_move_constructor<T, true>
{
};

template <class T, bool = std::is_move_assignable_v<T>>
struct disable_move_assignment
{
    disable_move_assignment() = default;
    disable_move_assignment(const disable_move_assignment&) = default;
    disable_move_assignment(disable_move_assignment&&) = default;
    auto operator=(const disable_move_assignment&) -> disable_move_assignment& = default;
    auto operator=(disable_move_assignment &&) -> disable_move_assignment& = delete;
};

template <class T>
struct disable_move_assignment<T, true>
{
};

template <class Key, class T, class Pair>
struct node : disable_copy_constructor<Pair>,
              disable_copy_assignment<Pair>,
              disable_move_constructor<Pair>,
              disable_move_assignment<Pair>
{
    template <class... Args>
    constexpr node(node_index_t<Key, T> next, Args&&... args)
        : next(next), pair(std::forward<Args>(args)...)
    {}

    template <class Allocator, class... Args>
    constexpr node(
        std::allocator_arg_t, const Allocator& alloc, node_index_t<Key, T> next, Args&&... args)
        : next(next), pair(std::allocator_arg, alloc, std::forward<Args>(args)...)
    {}

    template <class Allocator, class Node>
    constexpr node(std::allocator_arg_t, const Allocator& alloc, const Node& other)
        : next(other.next), pair(std::allocator_arg, alloc, other.pair.pair())
    {}

    template <class Allocator, class Node>
    constexpr node(std::allocator_arg_t, const Allocator& alloc, Node&& other)
        : next(std::move(other.next)), pair(std::allocator_arg, alloc, std::move(other.pair.pair()))
    {}

    node_index_t<Key, T> next = node_end_index<Key, T>;
    key_value_pair_t<Key, T> pair;
};

} // namespace jg::details

namespace std
{
template <class Key, class T, class Allocator>
struct uses_allocator<jg::details::node<Key, T>, Allocator> : true_type
{
};
} // namespace std
#endif // JG_NODE_HPP
