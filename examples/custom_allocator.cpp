#include <iostream>
#include <memory_resource>
#include <vector>
#include "hash_table8.hpp"

// Custom allocator with counting (must be at namespace scope for rebind support)
template <typename T>
struct CountingAllocator {
    using value_type = T;
    static inline size_t alloc_count = 0;
    static inline size_t dealloc_count = 0;

    CountingAllocator() noexcept = default;
    template <typename U>
    CountingAllocator(const CountingAllocator<U>&) noexcept {}

    T* allocate(size_t n) {
        alloc_count++;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    void deallocate(T* p, size_t) noexcept {
        dealloc_count++;
        ::operator delete(p);
    }
    template <typename U>
    bool operator==(const CountingAllocator<U>&) const noexcept { return true; }
};

int main() {
    // Use PMR allocator with emhash
    // This allows custom memory allocation strategies

    // Stack-allocated buffer for small maps
    unsigned char buffer[4096];
    std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));

    // PMR vector of PMR map
    std::pmr::vector<emhash8::HashMap<int, int>> maps(&pool);
    maps.emplace_back();  // default allocator
    maps[0][1] = 100;
    maps[0][2] = 200;

    std::cout << "maps[0][1] = " << maps[0][1] << "\n";
    std::cout << "maps[0][2] = " << maps[0][2] << "\n";

    // Custom counting allocator
    using CountMap = emhash8::HashMap<int, int, std::hash<int>, std::equal_to<int>, CountingAllocator<std::pair<const int, int>>>;
    CountMap cmap;
    cmap[1] = 10;
    cmap[2] = 20;
    cmap[3] = 30;

    std::cout << "allocations: " << CountingAllocator<std::pair<const int, int>>::alloc_count << "\n";
    std::cout << "deallocations: " << CountingAllocator<std::pair<const int, int>>::dealloc_count << "\n";

    return 0;
}
