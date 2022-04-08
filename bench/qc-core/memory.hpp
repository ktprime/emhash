#pragma once

#include <exception>
#include <new>
#include <utility>
#include <vector>

#include <qc-core/core.hpp>

namespace qc::memory
{
    struct RecordAllocatorStats
    {
        size_t current{};
        size_t total{};
        size_t allocations{};
        size_t deallocations{};
    };

    namespace _minutia
    {
        inline std::vector<RecordAllocatorStats> recordAllocatorStatsList(1u);
    }

    //
    // NOT THREAD SAFE!!!
    //
    template <typename T> class RecordAllocator
    {
        template <typename> friend class RecordAllocator;

        public: //--------------------------------------------------------------

        using value_type = T;
        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;
        using is_always_equal = std::false_type;

        RecordAllocator() noexcept :
            _listI{_minutia::recordAllocatorStatsList.size()}
        {
            _minutia::recordAllocatorStatsList.emplace_back();
        }

        RecordAllocator(const RecordAllocator &) noexcept = default;

        template <typename U>
        RecordAllocator(const RecordAllocator<U> & other) noexcept :
            _listI{other._listI}
        {}

        RecordAllocator(RecordAllocator && other) noexcept :
            _listI{std::exchange(other._listI, 0u)}
        {}

        RecordAllocator & operator=(const RecordAllocator &) noexcept = default;

        RecordAllocator & operator=(RecordAllocator && other) noexcept
        {
            _listI = std::exchange(other._listI, 0u);
            return *this;
        }

        ~RecordAllocator() noexcept = default;

        T * allocate(const size_t n)
        {
            const size_t bytes{n * sizeof(T)};
            RecordAllocatorStats & stats{this->stats()};
            stats.current += bytes;
            stats.total += bytes;
            ++stats.allocations;
            return reinterpret_cast<T *>(::operator new(bytes));
        }

        void deallocate(T * const ptr, const size_t n)
        {
            const size_t bytes{n * sizeof(T)};
            RecordAllocatorStats & stats{this->stats()};
            stats.current -= bytes;
            ++stats.deallocations;
            ::operator delete(ptr);
        }

        RecordAllocatorStats & stats()
        {
            return _minutia::recordAllocatorStatsList.at(_listI);
        }

        const RecordAllocatorStats & stats() const
        {
            return _minutia::recordAllocatorStatsList.at(_listI);
        }

        bool operator==(const RecordAllocator &) const noexcept = default;

        private: //-------------------------------------------------------------

        size_t _listI{};
    };

    class Pool
    {
        friend struct _PoolFriend;

        public: //--------------------------------------------------------------

        static constexpr size_t minCapacity{2u * sizeof(size_t)};
        static constexpr size_t maxCapacity{std::numeric_limits<size_t>::max() / sizeof(size_t) * sizeof(size_t) - 2u * sizeof(size_t)};

        Pool(const size_t capacity)
        {
            if (capacity < minCapacity || capacity > maxCapacity) {
                //throw std::exception{};
            }

            _chunkCapacity = (capacity + (sizeof(size_t) - 1u)) / sizeof(size_t);
            _chunks = static_cast<size_t *>(::operator new((_chunkCapacity * sizeof(size_t)) + 2u * sizeof(size_t)));

            _head = _chunks;
            _head[0] = _chunkCapacity;
            _head[1] = _chunkCapacity;

            size_t * const _tail{_chunks + _chunkCapacity};
            _tail[0] = 0u;
            _tail[1] = 0u;
        }

        Pool(const Pool &) = delete;

        Pool(Pool && other) noexcept :
            _chunkCapacity{std::exchange(other._chunkCapacity, 0u)},
            _chunks{std::exchange(other._chunks, nullptr)},
            _head{std::exchange(other._head, nullptr)}
        {}

        Pool & operator=(const Pool &) = delete;

        Pool & operator=(Pool && other) noexcept
        {
            _chunkCapacity = std::exchange(other._chunkCapacity, 0u);
            _chunks = std::exchange(other._chunks, nullptr);
            _head = std::exchange(other._head, nullptr);
            return *this;
        }

        ~Pool()
        {
            if (_chunks) {
                ::operator delete(_chunks);
            }
        }

        template <typename T>
        T * allocate(const size_t n)
        {
            if constexpr (debug) {
                if (!_chunks || !n) {
                    throw std::exception{};
                }
            }

            const size_t allocSize{(n * sizeof(T) + (sizeof(size_t) - 1u)) / sizeof(size_t)};

            size_t * block{_head};
            size_t * prevBlock{nullptr};

            while (true) {
                const size_t blockSize{block[0]};

                if (!blockSize) {
                    // Out of memory, or no large enough contiguous block of memory available
                    throw std::bad_alloc{};
                }

                // There's space. Must be either a perfect fit, or at least two chunks for meta
                if ((blockSize >= allocSize) && (blockSize - allocSize != 1u)) {
                    break;
                }

                prevBlock = block;
                block += block[1];
            }

            size_t offset;
            // Did not fill block
            if (allocSize < block[0]) {
                size_t * const newBlock{block + allocSize};
                newBlock[0] = block[0] - allocSize;
                newBlock[1] = block[1] - allocSize;
                offset = allocSize;
            }
            // Completely filled block
            else {
                offset = block[1];
            }

            if (prevBlock) {
                prevBlock[1] += offset;
            }
            else {
                _head += offset;
            }

            return reinterpret_cast<T *>(block);
        }

        template <typename T>
        void deallocate(T * const ptr, const size_t n) noexcept(!debug)
        {
            if constexpr (debug) {
                if (!_chunks || !ptr || !n) {
                    throw std::exception{};
                }
            }

            size_t * const block{reinterpret_cast<size_t *>(ptr)};
            block[0] = (n * sizeof(T) + (sizeof(size_t) - 1u)) / sizeof(size_t);

            if (block < _head) {
                block[1] = _head - block;
                if (block[1] == block[0]) {
                    block[0] += _head[0];
                    block[1] += _head[1];
                }
                _head = block;
            }
            else {
                size_t * prevBlock{_head};
                size_t * nextBlock{_head + _head[1]};
                while (nextBlock < block) {
                    prevBlock = nextBlock;
                    nextBlock = nextBlock + nextBlock[1];
                }

                // Deal with this block <-> next block
                block[1] = nextBlock - block;
                if (block[1] == block[0]) {
                    block[0] += nextBlock[0];
                    block[1] += nextBlock[1];
                }

                // Deal with prev block <-> this block
                prevBlock[1] = block - prevBlock;
                if (prevBlock[1] == prevBlock[0]) {
                    prevBlock[0] += block[0];
                    prevBlock[1] += block[1];
                }
            }
        }

        size_t capacity() const noexcept
        {
            return _chunkCapacity * sizeof(size_t);
        }

        const void * data() const noexcept
        {
            return _chunks;
        }

        private: //-------------------------------------------------------------

        size_t _chunkCapacity{};
        size_t * _chunks{};
        size_t * _head{};
    };

    template <typename T> class PoolAllocator
    {
        template <typename> friend class PoolAllocator;

        public: //--------------------------------------------------------------

        using value_type = T;
        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;

        PoolAllocator(Pool & pool) noexcept :
            _pool{&pool}
        {}

        PoolAllocator(const PoolAllocator &) noexcept = default;

        template <typename U>
        explicit PoolAllocator(const PoolAllocator<U> & other) noexcept :
            _pool{other._pool}
        {}

        PoolAllocator(PoolAllocator &&) noexcept = default;

        PoolAllocator & operator=(const PoolAllocator &) noexcept = default;

        PoolAllocator & operator=(PoolAllocator &&) noexcept = default;

        ~PoolAllocator() noexcept = default;

        T * allocate(const size_t n)
        {
            return _pool->allocate<T>(n);
        }

        void deallocate(T * const ptr, const size_t n) noexcept
        {
            return _pool->deallocate<T>(ptr, n);
        }

        bool operator==(const PoolAllocator &) const noexcept = default;

        private: //-------------------------------------------------------------

        Pool * _pool{};
    };
}
