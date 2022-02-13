#ifndef JG_POWER_OF_TWO_GROWTH_POLICY_HPP
#define JG_POWER_OF_TWO_GROWTH_POLICY_HPP

#include <cassert>
#include <cstddef>
#include <limits>

namespace jg::details
{

struct power_of_two_growth_policy
{
    static constexpr auto compute_index(std::size_t hash, std::size_t capacity) -> std::size_t
    {
        return hash & (capacity - 1);
    }

    static constexpr auto compute_closest_capacity(std::size_t min_capacity) -> std::size_t
    {
        // We didn't see that trick yet.

        constexpr auto highest_capacity =
            (std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1));

        if (min_capacity > highest_capacity)
        {
            assert(false && "Maximum capacity for the dense_hash_map reached.");
            return highest_capacity;
        }

        --min_capacity;

        for (auto i = 1; i < std::numeric_limits<std::size_t>::digits; i *= 2)
        {
            min_capacity |= min_capacity >> i;
        }

        return ++min_capacity;
    }

    static constexpr auto minimum_capacity() -> std::size_t { return 8u; }
};

} // namespace jg::details

#endif // JG_POWER_OF_TWO_GROWTH_POLICY_HPP
