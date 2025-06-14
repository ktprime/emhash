/**
 * Copyright 2023 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

#ifndef INDIVI_SPARQUE_H
#define INDIVI_SPARQUE_H

#include <algorithm>
#include <array>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

// #define INDIVI_SQ_DEBUG
#if defined(INDIVI_SQ_DEBUG) && !defined(NDEBUG)
#include <deque>
#include <sstream>
#include <unordered_set>
  #define DEBUG_EXPR(expr)  { expr; }
  #define SANITY_CHECK_SQ   { sanity_check(); }
#else
  #define DEBUG_EXPR(expr) {}
  #define SANITY_CHECK_SQ
#endif

namespace indivi
{
namespace detail
{
  static constexpr float MergeRatio = 1.f;   // merge chunks iff sum of their sizes <= floor(MergeRatio * ChunkSize)
  static constexpr float StealRatio = 1.f/3; // steal to balance chunk iff size <= floor(StealRatio * ChunkSize)
  static_assert(MergeRatio > 0.f && MergeRatio <= 1.f, "sparque: MergeRatio must be > 0 and <= 1");
  static_assert(StealRatio > 0.f && StealRatio <= MergeRatio / 2, "sparque: StealRatio must be > 0 and <= MergeRatio / 2");
  
  template <class T, class InputIt>
  void fill_chunk(T* newChunk, size_t size, InputIt& it)
  {
    std::uninitialized_copy_n(it, size, newChunk);
    it += size;
  }
  
  template <class T>
  void fill_chunk(T* newChunk, size_t size, const T& value)
  {
    std::uninitialized_fill_n(newChunk, size, value);
  }
  
  namespace is_nothrow_swappable_impl {
    template <typename T>
    struct test {
      static bool const value = noexcept(std::swap(std::declval<T&>(), std::declval<T&>()));
    };
  }
  
  template <typename T>
  struct is_nothrow_swappable
      : std::integral_constant<bool, is_nothrow_swappable_impl::test<T>::value>
  {};
}

/*
 * Sparque (sparse deque) is an indexed sequence container that allows fast random insertion and deletion.
 * Like std::deque, its elements are not stored contiguously and storage is automatically adjusted as needed.
 * It is based on a counted B+ tree, where each memory chunk behave as a double-ended vector,
 * and offers basic exception safety guarantees.
 *
 * The complexity (efficiency) of common operations on sparques is as follows:
 * - Random access - O(log_b(n)), where b is the number of children per node
 * - Insertion or removal of elements at the end or beginning - constant O(1)
 * - Insertion or removal of elements - amortized O(m), where m is the number of elements per chunk
 * - Iteration - contant O(n)
 * Its space complexity is O(n).
 *
 * Sparque meets the requirements of Container, AllocatorAwareContainer, SequenceContainer and ReversibleContainer.
 * Insertion and deletion at either end never invalidates pointers or references to the rest of the elements.
 * Insertion and deletion always invalidates iterators.
 * For performance reasons, to get a random iterator prefer `sparque.nth()` to `sparque.begin() + offset`.
 * 
 * Design specificities:
 * - First and last branch of the tree do no respect balancing factor (to allow 0(1) operations at both end)
 * - A steal threshold exists (default = 1/3) for balancing nodes by bulk-stealing and benefit from an hysteresis effect
 * - Leafs and Nodes both use an internal vector for storage (to allow using indexes instead of pointers for hierarchy)
 * - Each leaf stores its previous and next neighbor index for fast iteration (even in a sparse dataset)
 * 
 * Template parameters:
 * - T: the type of the elements
 * - ChunkSize: the number of elements per chunk (default = max(4, 1024 / sizeof(T)), must be >= 2)
 * - NodeSize: the number of children per node/leaf (default = 16, must be >= 2 and < 2^15)
 * - Allocator: the allocator used to acquire/release memory (must meet the requirements of Allocator)
 */
template <class T,
          uint16_t ChunkSize = (4u * sizeof(T) >= 1024u) ? 4u : 1024u / sizeof(T),
          uint16_t NodeSize = 16u,
          class Allocator = std::allocator<T>>
class sparque
{
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using allocator_type = Allocator;
  
  static_assert(ChunkSize >= 2u, "sparque: ChunkSize must be >= 2");
  static_assert(NodeSize >= 2u, "sparque: NodeSize must be >= 2");
  static_assert(NodeSize < 0x8000, "sparque: NodeSize must be < 2^15");
  static_assert(std::is_same<typename Allocator::value_type, value_type>::value,
                "sparque: Allocator::value_type must be the same as value_type");
  
#if defined(INDIVI_SQ_DEBUG) && !defined(NDEBUG)
  struct DbgCounters {
    uint64_t c1  = 0u;
    uint64_t c2  = 0u;
    uint64_t c3  = 0u;
    uint64_t c4  = 0u;
    uint64_t c5  = 0u;
    uint64_t c6  = 0u;
    uint64_t c7  = 0u;
    uint64_t c8  = 0u;
    uint64_t c9  = 0u;
    uint64_t c10 = 0u;
    uint64_t c11 = 0u;
    uint64_t c12 = 0u;
    uint64_t c13 = 0u;
    uint64_t c14 = 0u;
    uint64_t c15 = 0u;
  } dbg;
#endif

private:
  enum ConstU32 : uint32_t { InvalidIndex = (uint32_t)-1 };
  enum ConstU16 : uint16_t { LeafFlag = 0x8000,
                             HalfNode = ((NodeSize + 1u) / 2u),
                             HalfChunk = ((ChunkSize + 1u) / 2u), HalfChunk_Floor = (ChunkSize / 2u),
                             MergeSize = (uint16_t)(detail::MergeRatio * ChunkSize),
                             StealSize = (uint16_t)(detail::StealRatio * ChunkSize) };
  
  struct Leaf;  // forward declaration
  
  struct SizeId
  {
    size_type size;
    uint32_t id;
  };
  
  struct Span
  {
    uint16_t off = 0u;
    uint16_t end = 0u;
    
    bool empty() const noexcept { return off == end; }
    bool full() const noexcept { return (end - off) == ChunkSize; }
    uint16_t size() const noexcept { return end - off; }
    bool room_right() const noexcept { return end < ChunkSize; }
    bool room_left() const noexcept { return off > 0u; }
    
    bool operator==(const Span& other) const noexcept
    {
      return off == other.off && end == other.end;
    }
  };
  
  struct DataAlc : Allocator
  {
    Leaf* data = nullptr;
    
    DataAlc() = default;
    DataAlc(const Allocator& alloc, Leaf* data_ = nullptr)
      : Allocator(alloc)
      , data(data_)
    {}
    
    DataAlc(const DataAlc& other)
      : Allocator(other)
      , data(other.data)
    {}
    DataAlc(DataAlc&& other) noexcept
      : Allocator(std::move(other))
      , data(other.data)
    {
      other.data = nullptr;
    }
    
    DataAlc& operator=(const DataAlc& other) = delete;
    DataAlc& operator=(DataAlc&& other) = delete;
    
    T* alloc() { return Allocator::allocate(ChunkSize); }
    void dealloc(T* p) { Allocator::deallocate(p, ChunkSize); }
  };
  
  struct Deleter // unique_ptr deleter for Allocator
  {
    DataAlc& alc;
    
    Deleter(DataAlc& alc_) : alc(alc_) {}
    void operator()(T* p) const { alc.dealloc(p); }
  };
  
  struct Node
  {
    std::array<size_type, NodeSize> counts;  // children counts
    std::array<uint32_t, NodeSize> children; // children indexes
    uint32_t parent;  // parent index (or next free)
    uint16_t pos;     // position in parent
    uint16_t _size;   // number of children + LeafFlag
    
    size_type count() const noexcept
    {
      size_type count = 0u;
      const uint32_t size_ = size();
      for (uint32_t i = 0u; i < size_; ++i)
        count += counts[i];
      return count;
    }
    
    bool full() const noexcept { return size() == NodeSize; }
    
    uint16_t size() const noexcept { return _size & ~LeafFlag; }
    void set_size_leafs(uint16_t size) noexcept
    {
      _size = size | LeafFlag;
    }

    bool has_leafs() const noexcept { return _size & LeafFlag; }
    
    bool has_single_node() const noexcept { return _size == 1u; }
    bool has_single_leaf() const noexcept { return _size == (LeafFlag | 1u); }
  };
  
  struct Leaf
  {
    std::array<Span, NodeSize> spans;  // chunks offset/end
    std::array<T*, NodeSize> chunks;   // chunks data
    uint32_t prev;    // previous leaf
    uint32_t next;    // next leaf (or next free)
    uint32_t parent;  // parent index
    uint16_t pos;     // position in parent
    uint16_t size;    // number of chunks
    
    ~Leaf() noexcept
    {
      assert(size == 0u);
    }
    
    void destroy(DataAlc& alc) noexcept
    {
      const uint32_t size_ = size;
      for (uint32_t i = 0u; i < size_; ++i)
      {
        const auto& span = spans[i];
        const auto& chunk = chunks[i];
        
        uint32_t pos_ = span.off;
        const uint32_t end_ = span.end;
        while (pos_ < end_)
          chunk[pos_++].~T();
        alc.dealloc(chunk);
      }
      size = 0u;
    }
    
    uint32_t last() const noexcept { return size - 1u; }
    
    size_type count() const noexcept
    {
      size_type count = 0u;
      const uint32_t size_ = size;
      for (uint32_t i = 0u; i < size_; ++i)
        count += spans[i].size();
      return count;
    }
    
    void emplace_at(uint32_t pos_, uint16_t off, uint16_t end, T* chunk) noexcept
    {
      assert(pos_ < NodeSize);
      Span& span = spans[pos_];
      assert(span.empty());
      span.off = off;
      span.end = end;
      assert(chunks[pos_] == nullptr);
      chunks[pos_] = chunk;
    }
    
    void shift_right(uint32_t index) noexcept
    {
      assert(size < NodeSize);
      assert(index <= size);
      std::memmove( spans.data() + index + 1,  spans.data() + index, (size - index) * sizeof(Span));
      spans[index].off = 0u;
      spans[index].end = 0u;
      std::memmove(chunks.data() + index + 1, chunks.data() + index, (size - index) * sizeof(T*));
    #ifndef NDEBUG
      chunks[index] = nullptr;
    #endif
      ++size;
    }
    
    void erase_last_n(size_type count_, DataAlc& alc) noexcept
    {
      assert(count_ < count());
      assert(count_ > 0u);
      assert(size > 0u);
      
      uint32_t i = size - 1u;
      do
      {
        const Span& span = spans[i];
        T* chunk = chunks[i];
        
        size_type size_ = span.size();
        if (size_ <= count_) // whole
        {
          uint32_t pos_ = span.off;
          const uint32_t end_ = span.end;
          while (pos_ < end_)
            chunk[pos_++].~T();
          alc.dealloc(chunk);
          
        #ifndef NDEBUG
          spans[i].off = 0u;
          spans[i].end = 0u;
          chunks[i] = nullptr;
        #endif
          --size;
        }
        else // partial
        {
          uint32_t pos_ = span.end - (uint32_t)count_;
          const uint32_t end_ = span.end;
          while (pos_ < end_)
            chunk[pos_++].~T();
          
          spans[i].end -= (uint16_t)count_;
          break;
        }
        
        --i;
        count_ -= size_;
      }
      while (count_ > 0u);
    }
    
    void erase_chunk(uint32_t index, DataAlc& alc)
    {
      assert(index < size);
      const Span& span = spans[index];
      T* chunk = chunks[index];
      
      uint32_t pos_ = span.off;
      const uint32_t end_ = span.end;
      while (pos_ < end_)
        chunk[pos_++].~T();
      alc.dealloc(chunk);
      
      // shift left
      std::memmove( spans.data() + index,  spans.data() + index + 1, (size - 1u - index) * sizeof(Span));
      std::memmove(chunks.data() + index, chunks.data() + index + 1, (size - 1u - index) * sizeof(T*));
    #ifndef NDEBUG
      spans[size - 1].off = 0u;
      spans[size - 1].end = 0u;
      chunks[size - 1] = nullptr;
    #endif
      --size;
    }
    
    void steal_all(Leaf& src) noexcept
    {
      assert(size + src.size <= NodeSize);
      std::memcpy( spans.data() + size,  src.spans.data(), src.size * sizeof(Span));
      std::memcpy(chunks.data() + size, src.chunks.data(), src.size * sizeof(T*));
      
      size += src.size;
      src.size = 0u;
    }
    
    void steal_half(Leaf& src) noexcept // full src to empty dst only
    {
      assert(size == 0u);
      assert(src.size == NodeSize);
      std::memcpy( spans.data(),  src.spans.data() + HalfNode, (NodeSize - HalfNode) * sizeof(Span));
      std::memcpy(chunks.data(), src.chunks.data() + HalfNode, (NodeSize - HalfNode) * sizeof(T*));
      
      size = NodeSize - HalfNode;
      src.size = HalfNode;
      
    #ifndef NDEBUG
      for (uint32_t i = HalfNode; i < NodeSize; ++i)
      {
        src.spans[i].off = 0u;
        src.spans[i].end = 0u;
        src.chunks[i] = nullptr;
      }
    #endif
    }
    
    size_type steal_first(Leaf& src) noexcept
    {
      assert(size < NodeSize);
      assert(src.size > 1u);
      
      size_type stolen = (size_type)src.spans[0].size();
      spans[size] = src.spans[0];
      chunks[size] = src.chunks[0];
      ++size;
      
      --src.size;
      std::memmove( src.spans.data(),  src.spans.data() + 1, src.size * sizeof(Span));
      std::memmove(src.chunks.data(), src.chunks.data() + 1, src.size * sizeof(T*));
      
    #ifndef NDEBUG
      src.spans[src.size].off = 0u;
      src.spans[src.size].end = 0u;
      src.chunks[src.size] = nullptr;
    #endif
      return stolen;
    }
    
    size_type steal_last(Leaf& src) noexcept
    {
      assert(size < NodeSize);
      assert(src.size > 1u);
      
      std::memmove( spans.data() + 1,  spans.data(), size * sizeof(Span));
      std::memmove(chunks.data() + 1, chunks.data(), size * sizeof(T*));
      ++size;
      --src.size;
      spans[0] = src.spans[src.size];
      chunks[0] = src.chunks[src.size];
      
    #ifndef NDEBUG
      src.spans[src.size].off = 0u;
      src.spans[src.size].end = 0u;
      src.chunks[src.size] = nullptr;
    #endif
      return (size_type)spans[0].size();
    }
  };
  
  using LeafAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Leaf>;
  using NodeAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  
  class LeafVec : LeafAllocator
  {
  private:
    DataAlc mDataAlc;     // vector data, chunk Allocator
    uint32_t mSize = 0u;  // vector size (active leafs count)
    uint32_t mCapa = 0u;  // vector capacity
    uint32_t mFirst = InvalidIndex; // first active leaf
    uint32_t mFree = InvalidIndex;  // first free leaf
    
    static constexpr double GrowthFactor = 1.5;
    
  public:
    LeafVec() = default;
    
    LeafVec(const allocator_type& alloc)
      : LeafAllocator(alloc)
      , mDataAlc(alloc)
    {}
    
    LeafVec(const LeafVec& other)
      : LeafAllocator(std::allocator_traits<LeafAllocator>::select_on_container_copy_construction(
              other.get_leaf_allocator()))
      , mDataAlc(std::allocator_traits<allocator_type>::select_on_container_copy_construction(
              other.get_element_allocator()))
    {
      init(other);
    }
    
    LeafVec(const LeafVec& other, const allocator_type& alloc)
      : LeafAllocator(alloc)
      , mDataAlc(alloc)
    {
      init(other);
    }
    
    LeafVec(LeafVec&& other) noexcept
      : LeafAllocator(std::move(other.get_leaf_allocator()))
      , mDataAlc(std::move(other.mDataAlc))
      , mSize(other.mSize)
      , mCapa(other.mCapa)
      , mFirst(other.mFirst)
      , mFree(other.mFree)
    {
      other.mSize = 0u;
      other.mCapa = 0u;
      other.mFirst = InvalidIndex;
      other.mFree = InvalidIndex;
    }
    
    ~LeafVec() noexcept
    {
      destroy();
      get_leaf_allocator().deallocate(mDataAlc.data, mCapa);
    }
    
    void operator=(const LeafVec& other) = delete;
    void operator=(LeafVec&& other) noexcept
    {
      assert(mSize == 0u);
      assert(mCapa == 0u);
      
      mDataAlc.data = other.mDataAlc.data;
      mSize = other.mSize;
      mCapa = other.mCapa;
      mFirst = other.mFirst;
      mFree = other.mFree;
      
      other.mDataAlc.data = nullptr;
      other.mSize = 0u;
      other.mCapa = 0u;
      other.mFirst = InvalidIndex;
      other.mFree = InvalidIndex;
    }
    
    DataAlc& chunk_allocator() noexcept { return mDataAlc; }
    const DataAlc& chunk_allocator() const noexcept { return mDataAlc; }
    allocator_type& get_element_allocator() noexcept { return mDataAlc; }
    const allocator_type& get_element_allocator() const noexcept { return mDataAlc; }
    LeafAllocator& get_leaf_allocator() noexcept { return *this; }
    const LeafAllocator& get_leaf_allocator() const noexcept { return *this; }
    
    void swap_allocator(LeafVec& other)
    {
      swap_allocator(other, std::integral_constant<bool,
              std::allocator_traits<allocator_type>::propagate_on_container_swap::value>{});
    }
    template <typename = void>
    void swap_allocator(LeafVec& other, std::true_type) noexcept( // propagate
        detail::is_nothrow_swappable<allocator_type>::value)
    {
      std::swap(get_element_allocator(), other.get_element_allocator());
      std::swap(get_leaf_allocator(), other.get_leaf_allocator());
    }
    template <typename = void>
    void swap_allocator(LeafVec& other, std::false_type) noexcept // no propagate
    {
      assert(get_element_allocator() == other.get_element_allocator());
      assert(get_leaf_allocator() == other.get_leaf_allocator());
      (void)other;
    }
    
    void swap(LeafVec& other) noexcept
    {
      swap_allocator(other);
      std::swap(mDataAlc.data, other.mDataAlc.data);
      std::swap(mSize, other.mSize);
      std::swap(mCapa, other.mCapa);
      std::swap(mFirst, other.mFirst);
      std::swap(mFree, other.mFree);
    }
    
    uint32_t size() const noexcept { return mSize; }
    uint32_t capacity() const noexcept { return mCapa; }
    bool empty() const noexcept { return mSize == 0u; }
    
    uint32_t first() const noexcept { return mFirst; }
    void set_first(uint32_t first) noexcept { mFirst = first; }
    
    uint32_t freed() const noexcept { return mFree; }
    
    Leaf& back() noexcept // only valid if no free
    {
      assert(mSize > 0u);
      assert(mFree == InvalidIndex);
      return mDataAlc.data[mSize - 1u];
    }
    
    Leaf& operator[](size_type pos) noexcept
    {
      assert(pos < mCapa);
      return mDataAlc.data[pos];
    }
    const Leaf& operator[](size_type pos) const noexcept
    {
      assert(pos < mCapa);
      return mDataAlc.data[pos];
    }
    
    void clear() noexcept
    {
      destroy();
      mSize = 0u;
      mFirst = InvalidIndex;
      mFree = InvalidIndex;
    }
    
    void purge() noexcept
    {
      clear();
      get_leaf_allocator().deallocate(mDataAlc.data, mCapa);
      mDataAlc.data = nullptr;
      mCapa = 0u;
    }
    
    void growEmpty(uint32_t capa)
    {
      assert(mSize == 0u);
      if (capa > mCapa)
      {
        Leaf* newStorage = get_leaf_allocator().allocate(capa);
        get_leaf_allocator().deallocate(mDataAlc.data, mCapa);
        mDataAlc.data = newStorage;
        mCapa = capa;
      }
    }
    
    template <class InputIt>
    void emplace_back(size_type count, uint32_t parent, uint16_t pos, InputIt& it)
    {
      assert(mSize < mCapa);
      assert(count <= (uint32_t)NodeSize * ChunkSize);
      ::new (static_cast<void*>(&mDataAlc.data[mSize])) Leaf();
      
      Leaf& leaf = mDataAlc.data[mSize];
      leaf.prev = mSize - 1u;
      leaf.next = mSize + 1u; // default: not last
      leaf.parent = parent;
      leaf.size = 0u;
      leaf.pos = pos;
      ++mSize;
      
      uint32_t j = 0u;
      do {
        size_type size = std::min<size_type>(ChunkSize, count);
        
        auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
        detail::fill_chunk(newStorage.get(), size, it);
        
        leaf.emplace_at(j, 0u, (uint16_t)size, newStorage.release());
        ++leaf.size;
        
        ++j;
        count -= size;
      }
      while (count > 0);
    }
    
    uint32_t push_back()
    {
      uint32_t index;
      if (mFree != InvalidIndex)
      {
        index = mFree;
        mFree = mDataAlc.data[mFree].next;
      }
      else
      {
        grow();
        index = mSize;
      }
      ::new (static_cast<void*>(mDataAlc.data + index)) Leaf();
      Leaf& leaf = mDataAlc.data[index];
    #ifndef NDEBUG
      leaf.chunks.fill(nullptr);
    #endif
      leaf.size = 0u;
      ++mSize;
      
      return index;
    }
    
    void free_leaf(Leaf& leaf, uint32_t index) noexcept
    {
      assert(mSize > 1u);
      assert(mFirst != InvalidIndex);
      assert(&leaf == &mDataAlc.data[index]);
      leaf.destroy(chunk_allocator());
      leaf.~Leaf();
      --mSize;
      leaf.next = mFree;
      mFree = index;
    }
    
    void free_last(Leaf& leaf) noexcept
    {
      assert(mSize == 1u);
      leaf.destroy(chunk_allocator());
      leaf.~Leaf();
      mSize = 0u;
      mFirst = InvalidIndex;
      mFree = InvalidIndex;
    }
    
    void on_ctr_failed() noexcept
    {
      assert(mFree == InvalidIndex);
      for (uint32_t idx = 0u; idx < mSize; ++idx)
      {
        mDataAlc.data[idx].destroy(chunk_allocator());
        mDataAlc.data[idx].~Leaf();
      }
      get_leaf_allocator().deallocate(mDataAlc.data, mCapa);
      
      mDataAlc.data = nullptr;
      mSize = 0u;
      mCapa = 0u;
      mFirst = InvalidIndex;
    }
    
  private:
    void destroy() noexcept
    {
    #ifndef NDEBUG
      uint32_t i = 0u;
    #endif
      uint32_t idx = mFirst;
      while (idx != InvalidIndex)
      {
        Leaf& leaf = mDataAlc.data[idx];
        idx = leaf.next;
        leaf.destroy(chunk_allocator());
        leaf.~Leaf();
      #ifndef NDEBUG
        ++i;
      #endif
      }
      assert(i == mSize);
    }
    
    void grow()
    {
      if (mSize == mCapa)
      {
        if (mCapa == std::numeric_limits<uint32_t>::max())
          throw std::length_error("sparque: leafs vector maximum capacity reached");
        
        uint64_t newCapa = (uint64_t)std::ceil(mCapa * GrowthFactor);
        newCapa = std::max<uint64_t>(newCapa, 1u);
        newCapa = std::min<uint64_t>(newCapa, std::numeric_limits<uint32_t>::max());
        
        Leaf* newStorage = get_leaf_allocator().allocate(newCapa);
        if (mSize != 0u)
          std::memcpy(static_cast<void*>(newStorage), mDataAlc.data, mSize * sizeof(Leaf));
        get_leaf_allocator().deallocate(mDataAlc.data, mCapa);
        mDataAlc.data = newStorage;
        mCapa = (uint32_t)newCapa;
      }
    }
    
    void init(const LeafVec& other)
    {
      assert(mDataAlc.data == nullptr);
      assert(mSize == 0u);
      assert(mCapa == 0u);
      assert(mFirst == InvalidIndex);
      assert(mFree == InvalidIndex);
      
      if (other.empty())
        return;
      
      mDataAlc.data = get_leaf_allocator().allocate(other.mCapa);
      mCapa = other.mCapa;
      std::memcpy(static_cast<void*>(mDataAlc.data), other.mDataAlc.data, mCapa * sizeof(Leaf));
      mSize = other.mSize;
      mFirst = other.mFirst;
      mFree = other.mFree;
      
      assert(mFirst != InvalidIndex);
      uint32_t leafIdx = mFirst;
      uint32_t chunkIdx = 0u;
      try
      {
        do
        {
          const Leaf& srcLeaf = other.mDataAlc.data[leafIdx];
          Leaf& dstLeaf = mDataAlc.data[leafIdx];
          
          const uint32_t leafSize = srcLeaf.size;
          for (chunkIdx = 0u; chunkIdx < leafSize; ++chunkIdx)
          {
            const Span& span = srcLeaf.spans[chunkIdx];
            
            auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
            const T* srcChunk = srcLeaf.chunks[chunkIdx];
            std::uninitialized_copy_n(srcChunk + span.off, span.size(), newStorage.get() + span.off);
            
            dstLeaf.chunks[chunkIdx] = newStorage.release();
          }
          
          leafIdx = srcLeaf.next;
        }
        while (leafIdx != InvalidIndex);
      }
      catch (...)
      {
        if (leafIdx != InvalidIndex)
        {
          mDataAlc.data[leafIdx].size = (uint16_t)chunkIdx;
          
          do
          {
            Leaf& leaf = mDataAlc.data[leafIdx];
            leafIdx = leaf.prev;
            leaf.destroy(chunk_allocator());
            leaf.~Leaf();
          }
          while (leafIdx != InvalidIndex);
        }
        get_leaf_allocator().deallocate(mDataAlc.data, mCapa);
        
        throw;
      }
    }
  };
  
  class NodeVec : NodeAllocator
  {
  private:
    Node* mData = nullptr;
    uint32_t mSize = 0u;  // vector size (active nodes count)
    uint32_t mCapa = 0u;  // vector capacity
    uint32_t mRoot = InvalidIndex;  // root
    uint32_t mFree = InvalidIndex;  // first free node
    
    static constexpr double GrowthFactor = 1.5;
    
  public:
    NodeVec() = default;
    
    NodeVec(const allocator_type& alloc)
      : NodeAllocator(alloc)
    {}
    
    NodeVec(const NodeVec& other)
      : NodeAllocator(std::allocator_traits<NodeAllocator>::select_on_container_copy_construction(
              other.get_node_allocator()))
    {
      if (other.mSize > 0u)
      {
        mData = get_node_allocator().allocate(other.mCapa);
        mCapa = other.mCapa;
        std::memcpy(static_cast<void*>(mData), other.mData, mCapa * sizeof(Node));
        mSize = other.mSize;
        mRoot = other.mRoot;
        mFree = other.mFree;
      }
    }
    
    NodeVec(const NodeVec& other, const allocator_type& alloc)
      : NodeAllocator(alloc)
    {
      if (other.mSize > 0u)
      {
        mData = get_node_allocator().allocate(other.mCapa);
        mCapa = other.mCapa;
        std::memcpy(static_cast<void*>(mData), other.mData, mCapa * sizeof(Node));
        mSize = other.mSize;
        mRoot = other.mRoot;
        mFree = other.mFree;
      }
    }
    
    NodeVec(NodeVec&& other) noexcept
      : NodeAllocator(std::move(other.get_node_allocator()))
      , mData(other.mData)
      , mSize(other.mSize)
      , mCapa(other.mCapa)
      , mRoot(other.mRoot)
      , mFree(other.mFree)
    {
      other.mData = nullptr;
      other.mSize = 0u;
      other.mCapa = 0u;
      other.mRoot = InvalidIndex;
      other.mFree = InvalidIndex;
    }
    
    ~NodeVec()
    {
      get_node_allocator().deallocate(mData, mCapa);
    }
    
    void operator=(const NodeVec& other) = delete;
    void operator=(NodeVec&& other) noexcept
    {
      assert(mSize == 0u);
      assert(mCapa == 0u);
      
      mData = other.mData;
      mSize = other.mSize;
      mCapa = other.mCapa;
      mRoot = other.mRoot;
      mFree = other.mFree;
      
      other.mData = nullptr;
      other.mSize = 0u;
      other.mCapa = 0u;
      other.mRoot = InvalidIndex;
      other.mFree = InvalidIndex;
    }
    
    NodeAllocator& get_node_allocator() noexcept { return *this; }
    const NodeAllocator& get_node_allocator() const noexcept { return *this; }
    
    void swap_allocator(NodeVec& other)
    {
      swap_allocator(other, std::integral_constant<bool,
              std::allocator_traits<allocator_type>::propagate_on_container_swap::value>{});
    }
    template <typename = void>
    void swap_allocator(NodeVec& other, std::true_type) noexcept( // propagate
        detail::is_nothrow_swappable<allocator_type>::value)
    {
      std::swap(get_node_allocator(), other.get_node_allocator());
    }
    template <typename = void>
    void swap_allocator(NodeVec& other, std::false_type) noexcept // no propagate
    {
      assert(get_node_allocator() == other.get_node_allocator());
      (void)other;
    }
    
    void swap(NodeVec& other) noexcept
    {
      swap_allocator(other);
      std::swap(mData, other.mData);
      std::swap(mSize, other.mSize);
      std::swap(mCapa, other.mCapa);
      std::swap(mRoot, other.mRoot);
      std::swap(mFree, other.mFree);
    }
    
    uint32_t size() const noexcept { return mSize; }
    void set_size(uint32_t size) noexcept { mSize = size; }
    
    uint32_t capacity() const noexcept { return mCapa; };
    bool empty() const noexcept { return mSize == 0u; }
    
    uint32_t root() const noexcept { return mRoot; }
    void set_root(uint32_t root) noexcept { mRoot = root; }
    
    uint32_t freed() const noexcept { return mFree; }
    
    const Node& back() const noexcept // only valid if no free
    {
      assert(mSize > 0u);
      assert(mFree == InvalidIndex);
      return mData[mSize - 1u];
    }
    
    Node& operator[](size_type pos) noexcept
    {
      assert(pos < mCapa);
      return mData[pos];
    }
    const Node& operator[](size_type pos) const noexcept
    {
      assert(pos < mCapa);
      return mData[pos];
    }
    
    void clear() noexcept
    {
      mSize = 0u;
      mRoot = InvalidIndex;
      mFree = InvalidIndex;
    }
    
    void purge() noexcept
    {
      clear();
      get_node_allocator().deallocate(mData, mCapa);
      mData = nullptr;
      mCapa = 0u;
    }
    
    void growEmpty(uint32_t capa)
    {
      assert(mSize == 0u);
      if (capa > mCapa)
      {
        Node* newStorage = get_node_allocator().allocate(capa);
        get_node_allocator().deallocate(mData, mCapa);
        mData = newStorage;
        mCapa = capa;
      }
    }
    
    void emplace_at(uint32_t index, uint32_t parent, uint16_t pos, uint16_t size,
                    const std::array<size_type, NodeSize>& counts, const std::array<uint32_t, NodeSize>& children) noexcept
    {
      assert(index < mCapa);
      ::new (static_cast<void*>(&mData[index])) Node();
      Node& node = mData[index];
      
      node.counts = counts;
      node.children = children;
      node.parent = parent;
      node._size = size;
      node.pos = pos;
    }
    
    uint32_t push_back()
    {
      uint32_t index;
      if (mFree != InvalidIndex)
      {
        index = mFree;
        mFree = mData[mFree].parent;
      }
      else
      {
        grow();
        index = mSize;
      }
      ::new (static_cast<void*>(mData + index)) Node();
      Node& node = mData[index];
      node.counts.fill(0u);
    #ifndef NDEBUG
      node.children.fill(InvalidIndex);
    #endif
      node._size = 0u;
      ++mSize;
      
      return index;
    }
    
    void free_node(Node& node, uint32_t index) noexcept
    {
      assert(mSize >= 1u);
      assert(mRoot != InvalidIndex);
      assert(&node == &mData[index]);
      node.~Node();
      --mSize;
      node.parent = mFree;
      mFree = index;
    }
    
    void free_last(Node& node) noexcept
    {
      assert(mSize == 1u);
      node.~Node();
      mSize = 0u;
      mRoot = InvalidIndex;
      mFree = InvalidIndex;
    }
    
    void reset_empty() noexcept
    {
      if (empty())
      {
        mRoot = InvalidIndex;
        mFree = InvalidIndex;
      }
    }
    
    void on_ctr_failed() noexcept
    {
      assert(mFree == InvalidIndex);
      get_node_allocator().deallocate(mData, mCapa);
      
      mData = nullptr;
      mSize = 0u;
      mCapa = 0u;
      mRoot = InvalidIndex;
    }
    
  private:
    void grow()
    {
      if (mSize == mCapa)
      {
        if (mCapa == std::numeric_limits<uint32_t>::max())
          throw std::length_error("sparque: nodes vector maximum capacity reached");
        
        uint64_t newCapa = (uint64_t)std::ceil(mCapa * GrowthFactor);
        newCapa = std::max<uint64_t>(newCapa, 1u);
        newCapa = std::min<uint64_t>(newCapa, std::numeric_limits<uint32_t>::max());
        
        Node* newStorage = get_node_allocator().allocate(newCapa);
        if (mSize != 0u)
          std::memcpy(newStorage, mData, mSize * sizeof(Node));
        get_node_allocator().deallocate(mData, mCapa);
        mData = newStorage;
        mCapa = (uint32_t)newCapa;
      }
    }
  };
  
  class ScopedLeaf
  {
  private:
    LeafVec& mLeafs;
    Leaf& mLeaf;
    uint32_t mIndex;
    
  public:
    ScopedLeaf(LeafVec& leafs, Leaf& leaf, uint32_t index) noexcept
      : mLeafs(leafs)
      , mLeaf(leaf)
      , mIndex(index)
    {
      assert(index != InvalidIndex);
    }
    
    ~ScopedLeaf() noexcept
    {
      if (mIndex != InvalidIndex)
      {
        if (mLeafs.size() > 1u)
          mLeafs.free_leaf(mLeaf, mIndex);
        else
          mLeafs.free_last(mLeaf);
      }
    }
    
    void release() noexcept { mIndex = InvalidIndex; }
  };
  
public:
  template <typename Pointer, typename Reference>
  class Iterator
  {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = typename sparque::value_type;
    using size_type = typename sparque::size_type;
    using difference_type = typename sparque::difference_type;
    using pointer = Pointer;
    using reference = Reference;
    
  private:
    friend sparque;
    
    const sparque* sparq; // owner
    T* chunk;       // current chunk
    size_type nth;  // index in sparque
    uint32_t cur;   // current leaf
    uint32_t prev;  // previous leaf
    uint32_t next;  // next leaf
    uint32_t index; // index in leaf
    uint32_t size;  // leaf size
    uint32_t pos;   // position in chunk
    uint32_t off;   // chunk offset
    uint32_t end;   // chunk end
    
    Iterator(const sparque* sparq, size_type nth, T* chunk, uint32_t cur, uint32_t prev, uint32_t next,
             uint32_t index, uint32_t size, uint32_t pos, uint32_t off, uint32_t end)
        : sparq(sparq), chunk(chunk), nth(nth), cur(cur), prev(prev), next(next)
        , index(index), size(size), pos(pos), off(off), end(end)
    {
      assert(cur != InvalidIndex || next == InvalidIndex);
      assert(pos >= off);
      assert(pos < end || (end == 0u && cur == InvalidIndex && next == InvalidIndex));
    }
    
    Iterator(const sparque* sparq, size_type nth, const Leaf& leaf, uint32_t cur, uint32_t index, uint32_t pos)
        : sparq(sparq), chunk(leaf.chunks[index]), nth(nth), cur(cur), prev(leaf.prev), next(leaf.next)
        , index(index), size(leaf.size), pos(pos), off(leaf.spans[index].off), end(leaf.spans[index].end)
    {
      assert(cur != InvalidIndex || next == InvalidIndex);
      assert(pos >= off);
      assert(pos < end || (end == 0u && cur == InvalidIndex && next == InvalidIndex));
    }
    
    void set(const sparque* sparq_, size_type nth_, const Leaf& leaf_, uint32_t cur_, uint32_t index_, uint32_t pos_) noexcept
    {
      sparq = sparq_;
      chunk = leaf_.chunks[index_];
      nth   = nth_;
      cur   = cur_;
      prev  = leaf_.prev;
      next  = leaf_.next;
      index = index_;
      size  = leaf_.size;
      pos   = pos_;
      off   = leaf_.spans[index_].off;
      end   = leaf_.spans[index_].end;
      
      assert(cur != InvalidIndex || next == InvalidIndex);
      assert(pos >= off);
      assert(pos <= end || (end == 0u && cur == InvalidIndex && next == InvalidIndex)); // pos == end: end as last + 1
    }
    
    void move_to_next(size_type delta) noexcept
    {
      assert(delta > 0);
      assert(nth <= sparq->mSize);
      
      if (pos + delta < end) // same chunk
      {
        pos += (uint32_t)delta;
        return;
      }
      
      delta -= end - pos;
      uint32_t chunkIndex = index + 1;
      const Leaf& leaf = sparq->mLeafs[cur];
      while (chunkIndex < size) // check parent leaf
      {
        size_type chunkSize = leaf.spans[chunkIndex].size();
        if (delta < chunkSize)
        {
          index = chunkIndex;
          off = leaf.spans[chunkIndex].off;
          end = leaf.spans[chunkIndex].end;
          pos = off + (uint32_t)delta;
          chunk = leaf.chunks[chunkIndex];
          return;
        }
        delta -= chunkSize;
        ++chunkIndex;
      }
      
      // check parent node
      if (nth < sparq->mSize)
      {
        move_to_next(leaf.parent, leaf.pos + 1u, delta);
      }
      else // end
      {
        *this = Iterator::endin(sparq);
      }
    }
    
    void move_to_next(uint32_t parentIdx, uint32_t childPos, size_type delta) noexcept
    {
      uint32_t height = 1u;
      const NodeVec& nodes = sparq->mNodes;
      while (true)
      {
        assert(parentIdx != InvalidIndex);
        const Node& parent = nodes[parentIdx];
        const uint32_t parentSize = parent.size();
        while (childPos < parentSize)
        {
          size_type count = parent.counts[childPos];
          if (delta < count)
          {
            move_in_node(parent.children[childPos], height, delta);
            return;
          }
          delta -= count;
          ++childPos;
        }
        childPos = parent.pos + 1u;
        parentIdx = parent.parent;
        ++height;
      }
    }
    
    void move_to_prev(size_type delta) noexcept
    {
      assert(delta >= 0);
      assert(nth <= sparq->mSize);
      
      if (off + delta <= (size_type)pos) // same chunk
      {
        pos -= (uint32_t)delta;
        return;
      }
      
      delta -= (pos + 1u) - off;
      cur = cur != InvalidIndex ? cur : prev; // handle end
      const Leaf& leaf = sparq->mLeafs[cur];
      if (cur == prev)
      {
        prev = leaf.prev;
        index = leaf.size;
        size = index;
      }
      int32_t chunkIndex = (int32_t)index - 1;
      while (chunkIndex >= 0) // check parent leaf
      {
        size_type chunkSize = leaf.spans[chunkIndex].size();
        if (delta < chunkSize)
        {
          index = (uint32_t)chunkIndex;
          off = leaf.spans[chunkIndex].off;
          end = leaf.spans[chunkIndex].end;
          pos = end - (uint32_t)(delta + 1u);
          chunk = leaf.chunks[chunkIndex];
          return;
        }
        delta -= chunkSize;
        --chunkIndex;
      }
      
      // check parent node
      move_to_prev(leaf.parent, (int32_t)leaf.pos - 1, delta);
    }
    
    void move_to_prev(uint32_t parentIdx, int32_t childPos, size_type delta) noexcept
    {
      uint32_t height = 1u;
      const NodeVec& nodes = sparq->mNodes;
      while (true)
      {
        assert(parentIdx != InvalidIndex);
        const Node& parent = nodes[parentIdx];
        
        while (childPos >= 0)
        {
          size_type count = parent.counts[childPos];
          if (delta < count)
          {
            move_in_node(parent.children[childPos], height, (count - 1u) - delta);
            return;
          }
          delta -= count;
          --childPos;
        }
        childPos = (int32_t)parent.pos - 1;
        parentIdx = parent.parent;
        ++height;
      }
    }
    
    void move_in_node(uint32_t childIndex, uint32_t height, size_type delta) noexcept
    {
      assert(childIndex != InvalidIndex);
      
      const NodeVec& nodes = sparq->mNodes;
      for (uint32_t h = 1u; h < height; ++h)
      {
        const Node& node = nodes[childIndex];
        uint32_t childIdx = 0u;
        size_type childSize = node.counts[childIdx];
        while (delta >= childSize)
        {
          delta -= childSize;
          ++childIdx;
          assert(childIdx < NodeSize);
          childSize = node.counts[childIdx];
        }
        childIndex = node.children[childIdx];
      }
      const Leaf& leaf = sparq->mLeafs[childIndex];
      uint32_t chunkIdx = 0u;
      size_type chunkSize = leaf.spans[chunkIdx].size();
      while (delta >= chunkSize)
      {
        delta -= chunkSize;
        ++chunkIdx;
        assert(chunkIdx < NodeSize);
        chunkSize = leaf.spans[chunkIdx].size();
      }
      assert(leaf.chunks[chunkIdx] != nullptr);
      
      *this = iterator(sparq, nth, leaf, childIndex, chunkIdx, leaf.spans[chunkIdx].off + (uint32_t)delta);
    }
    
  public:
    Iterator() = default;
    Iterator(const Iterator& other) = default;
    
    template <typename P, typename R,
             typename = std::enable_if<std::is_convertible<P, pointer>::value>>
    Iterator(const Iterator<P, R>& other)
        : sparq(other.sparq), chunk(other.chunk), nth(other.nth), cur(other.cur), prev(other.prev), next(other.next)
        , index(other.index), size(other.size), pos(other.pos), off(other.off), end(other.end)
    {
      assert(cur != InvalidIndex || next == InvalidIndex);
      assert(pos >= off);
      assert(pos < end || (end == 0u && cur == InvalidIndex && next == InvalidIndex));
    }
    
    Iterator& operator=(const Iterator& other) noexcept = default;
    
    template <typename P, typename R,
             typename = std::enable_if<std::is_convertible<P, pointer>::value>>
    Iterator& operator=(const Iterator<P, R>& other) noexcept
    {
      sparq = other.sparq;
      chunk = other.chunk;
      nth   = other.nth;
      cur   = other.cur;
      prev  = other.prev;
      next  = other.next;
      index = other.index;
      size  = other.size;
      pos   = other.pos;
      off   = other.off;
      end   = other.end;
      
      return *this;
    }
    
    Iterator& operator++() noexcept
    {
      assert(nth < sparq->mSize);
      ++nth;
      ++pos;
      
      if (pos < end)
      {
        return *this;
      }
      // else next chunk
      {
        ++index;
        
        if (index < size)
        {
          const Leaf& curLeaf = sparq->mLeafs[cur];
          assert(!curLeaf.spans[index].empty());
          
          pos = curLeaf.spans[index].off;
          off = pos;
          end = curLeaf.spans[index].end;
          chunk = curLeaf.chunks[index];
          return *this;
        }
        // else next leaf
        {
          index = 0u;
          if (next != InvalidIndex)
          {
            const Leaf& nextLeaf = sparq->mLeafs[next];
            
            assert(nextLeaf.prev == cur);
            prev = cur;
            cur = next;
            next = nextLeaf.next;
            size = nextLeaf.size;
            
            assert(!nextLeaf.spans[0].empty());
            off = nextLeaf.spans[0].off;
            pos = off;
            end = nextLeaf.spans[0].end;
            chunk = nextLeaf.chunks[0];
            return *this;
          }
          // else end
          {
          #ifndef NDEBUG
            chunk = nullptr;
          #endif
            assert(nth == sparq->mSize);
            assert(sparq->mLastLeaf == cur);
            prev = cur;
            cur = InvalidIndex;
            size = 0u;
            
            pos = 0u;
            off = 0u;
            end = 0u;
            return *this;
          }
        }
      }
    }
    
    Iterator operator++(int) noexcept
    {
      Iterator retval = *this;
      ++(*this);
      return retval;
    }
    
    Iterator& operator--() noexcept
    {
      assert(nth > 0u);
      --nth;
      
      if (pos-- > off)
      {
        return *this;
      }
      // else prev chunk
      {
        if (index-- > 0u)
        {
          const Leaf& curLeaf = sparq->mLeafs[cur];
          assert(!curLeaf.spans[index].empty());
          
          off = curLeaf.spans[index].off;
          end = curLeaf.spans[index].end;
          pos = end - 1u;
          chunk = curLeaf.chunks[index];
          return *this;
        }
        // else prev leaf
        {
          assert(prev != InvalidIndex);
          const Leaf& prevLeaf = sparq->mLeafs[prev];
          
          assert(prevLeaf.next == cur);
          next = cur;
          cur = prev;
          prev = prevLeaf.prev;
          size = prevLeaf.size;
          
          index = prevLeaf.last();
          off = prevLeaf.spans[index].off;
          end = prevLeaf.spans[index].end;
          pos = end - 1u;
          chunk = prevLeaf.chunks[index];
          return *this;
        }
      }
    }
    
    Iterator operator--(int) noexcept
    {
      Iterator retval = *this;
      --(*this);
      return retval;
    }
    
    Iterator& operator+=(difference_type diff) noexcept
    {
      assert(diff <  0 || nth + (size_type)diff <= sparq->mSize);
      assert(diff >= 0 || nth >= (size_type)(-diff));
      nth += diff;
      
      if (diff > 0)
        move_to_next((size_type)diff);
      else
        move_to_prev((size_type)(-diff));
      
      return *this;
    }
    
    Iterator& operator-=(difference_type diff) noexcept
    {
      assert(diff <  0 || nth >= (size_type)diff);
      assert(diff >= 0 || nth + (size_type)(-diff) <= sparq->mSize);
      nth -= diff;
      
      if (diff >= 0)
        move_to_prev((size_type)diff);
      else
        move_to_next((size_type)(-diff));
      
      return *this;
    }
    
    Iterator operator+(difference_type diff) const noexcept
    {
      Iterator retval = *this;
      return retval += diff;
    }
    
    Iterator operator-(difference_type diff) const noexcept
    {
      Iterator retval = *this;
      return retval -= diff;
    }
    
    bool operator==(const Iterator& other) const noexcept
    {
      assert(sparq == other.sparq);
      assert(nth != other.nth || chunk == other.chunk);
      assert(nth != other.nth || index == other.index);
      assert(nth != other.nth || cur   == other.cur);
      assert(nth != other.nth || prev  == other.prev);
      assert(nth != other.nth || next  == other.next);
      assert(nth != other.nth || pos   == other.pos);
      assert(nth != other.nth || off   == other.off);
      assert(nth != other.nth || end   == other.end);
      return nth == other.nth;
    }
    bool operator!=(const Iterator& other) const noexcept { return !(*this == other); }
    
    bool operator<(const Iterator& other) const noexcept
    {
      assert(sparq == other.sparq);
      return nth < other.nth;
    }
    bool operator>(const Iterator& other) const noexcept
    {
      assert(sparq == other.sparq);
      return nth > other.nth;
    }
    
    bool operator<=(const Iterator& other) const noexcept
    {
      assert(sparq == other.sparq);
      return nth <= other.nth;
    }
    bool operator>=(const Iterator& other) const noexcept
    {
      assert(sparq == other.sparq);
      return nth >= other.nth;
    }
    
    difference_type operator-(const Iterator& other) const noexcept
    {
      assert(sparq == other.sparq);
      return nth - other.nth;
    }
    
    reference operator*() const noexcept { return chunk[pos]; }
    
    pointer operator->() const noexcept { return &chunk[pos]; }
    
  private:
    static Iterator begin(const sparque* sparq)
    {
      if (sparq->mSize > 0u)
      {
        uint32_t firstIdx = sparq->mLeafs.first();
        assert(firstIdx != InvalidIndex);
        const Leaf& firstLeaf = sparq->mLeafs[firstIdx];
        assert(firstLeaf.prev == InvalidIndex);
        assert(!firstLeaf.spans[0].empty());
        
        return Iterator(sparq, 0u, firstLeaf.chunks[0], firstIdx, InvalidIndex, firstLeaf.next,
                        0u, firstLeaf.size, firstLeaf.spans[0].off, firstLeaf.spans[0].off, firstLeaf.spans[0].end);
      }
      return endin(sparq);
    }
    
    static Iterator endin(const sparque* sparq)
    {
      return Iterator(sparq, sparq->mSize, nullptr, InvalidIndex, sparq->mLastLeaf, InvalidIndex, 0u, 0u, 0u, 0u, 0u);
    }
    
    static Iterator lastin(const sparque* sparq)
    {
      assert(sparq->mSize > 0u);
      const uint32_t lastLeafIdx = sparq->mLastLeaf;
      assert(lastLeafIdx != InvalidIndex);
      const Leaf& lastLeaf = sparq->mLeafs[lastLeafIdx];
      const uint32_t lastIdx = lastLeaf.last();
      const Span& lastSpan = lastLeaf.spans[lastIdx];
      
      return iterator(sparq, sparq->mSize - 1u, lastLeaf.chunks[lastIdx], lastLeafIdx, lastLeaf.prev, lastLeaf.next,
                      lastIdx, lastLeaf.size, lastSpan.end - 1u, lastSpan.off, lastSpan.end);
    }
  };
  
  using iterator = Iterator<T*, T&>;
  using const_iterator = Iterator<const T*, const T&>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  
private:
  // allocator
  void copy_assign_alloc(const sparque& other)
  {
    copy_assign_alloc(other, std::integral_constant<bool,
            std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value>{});
  }
  
  template <typename = void>
  void copy_assign_alloc(const sparque& other, std::true_type) // propagate
  {
    if (get_element_allocator() != other.get_element_allocator())
      purge();
    
    get_element_allocator() = other.get_element_allocator();
    get_leaf_allocator() = other.get_leaf_allocator();
    get_node_allocator() = other.get_node_allocator();
  }
  
  template <typename = void>
  void copy_assign_alloc(const sparque&, std::false_type) {} // no propagate
  
  void move_assign_alloc(sparque& other) noexcept(
      !std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value
      || std::is_nothrow_move_assignable<allocator_type>::value)
  {
    move_assign_alloc(other, std::integral_constant<bool,
            std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value>{});
  }
  
  template <typename = void>
  void move_assign_alloc(sparque& other, std::true_type) noexcept( // propagate
      std::is_nothrow_move_assignable<allocator_type>::value)
  {
    get_element_allocator() = std::move(other.get_element_allocator());
    get_leaf_allocator() = std::move(other.get_leaf_allocator());
    get_node_allocator() = std::move(other.get_node_allocator());
  }
  
  template <typename = void>
  void move_assign_alloc(const sparque&, std::false_type) noexcept {} // no propagate
  
  void move_assign(sparque& other) noexcept(
      std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value
      && std::is_nothrow_move_assignable<allocator_type>::value)
  {
    move_assign(other, std::integral_constant<bool,
            std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value>{});
  }
  
  template <typename = void>
  void move_assign(sparque& other, std::true_type) // propagate
  {
    purge();
    
    move_assign_alloc(other);
    
    // steal
    mLeafs = std::move(other.mLeafs);
    mNodes = std::move(other.mNodes);
    mSize = other.mSize;
    mHeight = other.mHeight;
    mLastLeaf = other.mLastLeaf;
    
    other.mSize = 0u;
    other.mHeight = 0u;
    other.mLastLeaf = InvalidIndex;
  }
  
  template <typename = void>
  void move_assign(sparque& other, std::false_type) // no propagate
  {
    if (get_element_allocator() != other.get_element_allocator())
    {
      assign_impl(std::make_move_iterator(other.begin()),
                  std::make_move_iterator(other.end()));
    }
    else
    {
      move_assign(other, std::true_type{});
    }
  }
  
  // construct
  template <class InputIt>
  SizeId fill_nodes(InputIt& it, uint32_t parent, uint16_t pos,
                    size_type& remain, uint32_t& nextNode, uint32_t& height)
  {
    uint32_t nodeIdx = nextNode++;
    SizeId nodeInfo{ 0u, nodeIdx };
    std::array<size_type, NodeSize> sizeBuffer;
    sizeBuffer.fill(0u);
    std::array<uint32_t, NodeSize> childBuffer;
  #ifndef NDEBUG
    childBuffer.fill(InvalidIndex);
  #endif
    
    ++height;
    uint16_t j = 0u;
    if (height < mHeight) // sub-nodes
    {
      const uint32_t endNode = mNodes.capacity();
      for (; j < NodeSize && nextNode < endNode; ++j)
      {
        SizeId childInfo = fill_nodes(it, nodeIdx, j, remain, nextNode, height);
        sizeBuffer[j] = childInfo.size;
        childBuffer[j] = childInfo.id;
        nodeInfo.size += childInfo.size;
      }
    }
    else  // leafs
    {
      for (; j < NodeSize && remain > 0u; ++j)
      {
        uint32_t leafIndex = mLeafs.size();
        size_type leafSize = std::min<size_type>(NodeSize * ChunkSize, remain);
        remain -= leafSize;
        
        mLeafs.emplace_back(leafSize, nodeIdx, j, it);
        
        sizeBuffer[j] = leafSize;
        childBuffer[j] = leafIndex;
        nodeInfo.size += leafSize;
      }
      j |= LeafFlag; // set flag through size
    }
    --height;
    
    // apply all at once (single mem access)
    mNodes.emplace_at(nodeIdx, parent, pos, j, sizeBuffer, childBuffer);
    
    return nodeInfo;
  }
  
  template <class InputIt>
  void init_tree(size_type count, uint32_t leafs, uint32_t nodes, InputIt& it)
  {
    mLeafs.growEmpty(leafs);
    
    try
    {
      if (nodes > 0u)
      {
        mNodes.growEmpty(nodes);
        
        // fill, depth-first
        uint32_t nextNode = 0u;
        uint32_t curHeight = 1u;
        size_type remain = count;
      #ifndef NDEBUG
        SizeId res = fill_nodes(it, InvalidIndex, 0u, remain, nextNode, curHeight);
        assert(res.size == count);
        assert(res.id == 0u);
        assert(remain == 0u);
      #else
        fill_nodes(it, InvalidIndex, 0u, remain, nextNode, curHeight);
      #endif
        mNodes.set_root(0);
        mNodes.set_size(nodes);
        
        assert(mNodes.back().size() > 0u);
        assert(mNodes.back().has_leafs());
      }
      else
      {
        assert(leafs == 1u);
        mLeafs.emplace_back(count, InvalidIndex, 0u, it);
      }
      mLeafs.back().next = InvalidIndex;
      mLastLeaf = mLeafs.size() - 1u;
      mLeafs.set_first(0);
      
      SANITY_CHECK_SQ;
    }
    catch (...)
    {
      mLeafs.on_ctr_failed();
      mNodes.on_ctr_failed();
      throw;
    }
  }
  
  template <class InputIt, typename std::enable_if<
                              std::is_same<typename std::iterator_traits<InputIt>::iterator_category,
                                           std::random_access_iterator_tag>::value, bool>::type = true>
  void construct_impl(InputIt first, InputIt last) // random iter
  {
    size_type count = last - first;
    if (count == 0u)
      return;
    
    const uint32_t chunks = div_ceil_chunk((uint32_t)count);
    const uint32_t leafs = div_ceil_node(chunks);
    
    mSize = count;
    mHeight = (uint32_t)std::ceil(log_node(chunks));
    mHeight = mHeight != 0u ? mHeight : 1u;
    uint32_t nodes = count_nodes(leafs, mHeight);
    
    // init data
    init_tree(count, leafs, nodes, first);
  }
  
  template <class InputIt, typename std::enable_if<
                              !std::is_same<typename std::iterator_traits<InputIt>::iterator_category,
                                            std::random_access_iterator_tag>::value, bool>::type = true>
  void construct_impl(InputIt first, InputIt last) // forward iter
  {
    push_back_range(first, last);
  }
  
  // assign
  template <class InputIt, typename std::enable_if<
                              std::is_same<typename std::iterator_traits<InputIt>::iterator_category,
                                           std::random_access_iterator_tag>::value, bool>::type = true>
  void assign_impl(InputIt first, InputIt last) // random iter
  {
    size_type count = last - first;
    assert(count <= max_size());
    
    // erase tail data
    difference_type sizeDiff = (difference_type)mSize - count;
    if (sizeDiff > 0)
      erase_last_n(sizeDiff);
    
    // overwrite common data
    iterator end_ = end();
    for (iterator it = begin(); it != end_; ++it, ++first)
      *it = *first;
    
    // add missing data
    if (sizeDiff < 0)
      push_back_n(-sizeDiff, first);
  }
  
  template <class InputIt, typename std::enable_if<
                              !std::is_same<typename std::iterator_traits<InputIt>::iterator_category,
                                            std::random_access_iterator_tag>::value, bool>::type = true>
  void assign_impl(InputIt first, InputIt last) // forward iter
  {
    // overwrite common data
    iterator it = begin();
    iterator end_ = end();
    for (; it != end_ && first != last; ++it, ++first)
      *it = *first;
    
    if (first == last)
    {
      // erase tail data
      if (it != end_)
        erase_last_n(mSize - it.nth);
    }
    else
    {
      // add missing data
      push_back_range(first, last);
    }
  }
  
  // modify
  void shift_right_nodes(Node& node, uint32_t index) noexcept
  {
    const uint32_t size_ = node.size();
    assert(size_ > 0u);
    assert(size_ < NodeSize);
    assert(index <= size_);
    
    assert(!node.has_leafs());
    for (uint32_t i = index; i < size_; ++i)
      ++mNodes[node.children[i]].pos;
    
    std::memmove(  node.counts.data() + index + 1,   node.counts.data() + index, (size_ - index) * sizeof(size_type));
    node.counts[index] = 0u;
    std::memmove(node.children.data() + index + 1, node.children.data() + index, (size_ - index) * sizeof(uint32_t));
  #ifndef NDEBUG
    node.children[index] = InvalidIndex;
  #endif
    ++node._size;
  }
  
  void shift_right_leafs(Node& node, uint32_t index) noexcept
  {
    const uint32_t size_ = node.size();
    assert(size_ > 0u);
    assert(size_ < NodeSize);
    assert(index <= size_);
    
    assert(node.has_leafs());
    for (uint32_t i = index; i < size_; ++i)
      ++mLeafs[node.children[i]].pos;
    
    std::memmove(  node.counts.data() + index + 1,   node.counts.data() + index, (size_ - index) * sizeof(size_type));
    node.counts[index] = 0u;
    std::memmove(node.children.data() + index + 1, node.children.data() + index, (size_ - index) * sizeof(uint32_t));
  #ifndef NDEBUG
    node.children[index] = InvalidIndex;
  #endif
    ++node._size;
  }
  
  void steal_all_children(Node& dst, uint32_t dstIndex, Node& src) noexcept
  {
    const uint16_t dstSize = dst.size();
    const uint16_t srcSize = src.size();
    assert(dstSize + srcSize <= NodeSize);
    
    // update children
    if (src.has_leafs())
    {
      for (uint16_t i = 0u; i < srcSize; ++i)
      {
        mLeafs[src.children[i]].pos = dstSize + i;
        mLeafs[src.children[i]].parent = dstIndex;
      }
    }
    else
    {
      for (uint16_t i = 0u; i < srcSize; ++i)
      {
        mNodes[src.children[i]].pos = dstSize + i;
        mNodes[src.children[i]].parent = dstIndex;
      }
    }
    
    std::memcpy(  dst.counts.data() + dstSize,   src.counts.data(), srcSize * sizeof(size_type));
    std::memcpy(dst.children.data() + dstSize, src.children.data(), srcSize * sizeof(uint32_t));
    
    dst._size += srcSize;       // preserve flag
    src._size -= srcSize - 1u;  // keep 1 for erase_node
    
  #ifndef NDEBUG
    src.counts[0] = 0u;
    src.children[0] = InvalidIndex;
  #endif
  }
  
  void steal_half_children(Node& dst, uint32_t dstIndex, Node& src) noexcept // full to empty only
  {
    assert(dst.size() == 0u);
    assert(src.size() == NodeSize);
    
    // update children
    if (src.has_leafs())
    {
      for (uint16_t i = HalfNode; i < NodeSize; ++i)
      {
        mLeafs[src.children[i]].pos = i - HalfNode;
        mLeafs[src.children[i]].parent = dstIndex;
      }
      dst.set_size_leafs(NodeSize - HalfNode);
    }
    else
    {
      for (uint16_t i = HalfNode; i < NodeSize; ++i)
      {
        mNodes[src.children[i]].pos = i - HalfNode;
        mNodes[src.children[i]].parent = dstIndex;
      }
      dst._size = NodeSize - HalfNode;
    }
    
    std::memcpy(  dst.counts.data(),   src.counts.data() + HalfNode, (NodeSize - HalfNode) * sizeof(size_type));
    std::memcpy(dst.children.data(), src.children.data() + HalfNode, (NodeSize - HalfNode) * sizeof(uint32_t));
    
    src._size -= NodeSize - HalfNode; // preserve flag

  #ifndef NDEBUG
    for (uint32_t i = HalfNode; i < NodeSize; ++i)
    {
      src.counts[i] = 0u;
      src.children[i] = InvalidIndex;
    }
  #endif
  }
  
  size_type steal_first_child(Node& dst, uint32_t dstIndex, Node& src) noexcept
  {
    const uint16_t dstSize = dst.size();
    const uint32_t srcSize = src.size();
    assert(dstSize < NodeSize);
    assert(srcSize > 1u);
    
    dst.counts[dstSize] = src.counts[0];
    dst.children[dstSize] = src.children[0];
    ++dst._size;
    
    // update children
    if (src.has_leafs())
    {
      mLeafs[src.children[0]].pos = dstSize;
      mLeafs[src.children[0]].parent = dstIndex;
      for (uint32_t i = 1u; i < srcSize; ++i)
        --mLeafs[src.children[i]].pos;
    }
    else
    {
      mNodes[src.children[0]].pos = dstSize;
      mNodes[src.children[0]].parent = dstIndex;
      for (uint32_t i = 1u; i < srcSize; ++i)
        --mNodes[src.children[i]].pos;
    }
    
    std::memmove(  src.counts.data(),   src.counts.data() + 1, (srcSize - 1) * sizeof(size_type));
    std::memmove(src.children.data(), src.children.data() + 1, (srcSize - 1) * sizeof(uint32_t));
    --src._size;
    
  #ifndef NDEBUG
    src.counts[srcSize - 1] = 0u;
    src.children[srcSize - 1] = InvalidIndex;
  #endif
    return dst.counts[dstSize];
  }
  
  size_type steal_last_child(Node& dst, uint32_t dstIndex, Node& src) noexcept
  {
    const uint32_t dstSize = dst.size();
    const uint32_t srcSize = src.size();
    assert(dstSize < NodeSize);
    assert(srcSize > 1u);
    
    std::memmove(  dst.counts.data() + 1,   dst.counts.data(), dstSize * sizeof(size_type));
    std::memmove(dst.children.data() + 1, dst.children.data(), dstSize * sizeof(uint32_t));
    ++dst._size;
    
    dst.counts[0] = src.counts[srcSize - 1];
    dst.children[0] = src.children[srcSize - 1];
    --src._size;
    
    // update children
    if (src.has_leafs())
    {
      mLeafs[dst.children[0]].pos = 0u;
      mLeafs[dst.children[0]].parent = dstIndex;
      for (uint32_t i = 1u; i <= dstSize; ++i)
        ++mLeafs[dst.children[i]].pos;
    }
    else
    {
      mNodes[dst.children[0]].pos = 0u;
      mNodes[dst.children[0]].parent = dstIndex;
      for (uint32_t i = 1u; i <= dstSize; ++i)
        ++mNodes[dst.children[i]].pos;
    }
    
  #ifndef NDEBUG
    src.counts[srcSize - 1] = 0u;
    src.children[srcSize - 1] = InvalidIndex;
  #endif
    return dst.counts[0];
  }
  
  // balance
  void balance_node(Node& node, uint32_t index) noexcept
  {
    const uint32_t nodeSize = node.size();
    assert(nodeSize > 0u);
    
    Node& parent = mNodes[node.parent];
    bool hasLeftSibling = node.pos > 0u;
    bool hasRightSibling = node.pos + 1u < parent.size();
    uint32_t leftNodeSize  = hasLeftSibling  ? mNodes[parent.children[node.pos - 1]].size() : NodeSize;
    uint32_t rightNodeSize = hasRightSibling ? mNodes[parent.children[node.pos + 1]].size() : NodeSize;
    
    // check neighbor nodes size
    if (nodeSize + leftNodeSize <= NodeSize  // merge current to left
        && (nodeSize + rightNodeSize > NodeSize
            || leftNodeSize <= rightNodeSize))
    {
      uint32_t leftNodeIndex = parent.children[node.pos - 1];
      Node& leftNode = mNodes[leftNodeIndex];
      assert(node.parent == leftNode.parent);
      
      steal_all_children(leftNode, leftNodeIndex, node);
      parent.counts[leftNode.pos] += parent.counts[node.pos];
      parent.counts[node.pos] = 0u;
      
      erase_node(node, index, true);
    }
    else if (nodeSize + rightNodeSize <= NodeSize) // merge right to current
    {
      uint32_t rightIndex = parent.children[node.pos + 1];
      Node& rightNode = mNodes[rightIndex];
      assert(node.parent == rightNode.parent);
      
      steal_all_children(node, index, rightNode);
      parent.counts[node.pos] += parent.counts[rightNode.pos];
      parent.counts[rightNode.pos] = 0u;
      
      erase_node(rightNode, rightIndex, true);
    }
    else if (hasLeftSibling)  // steal leaf from left
    {
      Node& leftNode = mNodes[parent.children[node.pos - 1]];
      assert(node.parent == leftNode.parent);
      assert(leftNodeSize > HalfNode);
      
      size_type stolen = steal_last_child(node, index, leftNode);
      parent.counts[node.pos] += stolen;
      parent.counts[node.pos - 1] -= stolen;
    }
    else if (hasRightSibling) // steal leaf from right
    {
      Node& rightNode = mNodes[parent.children[node.pos + 1]];
      assert(node.parent == rightNode.parent);
      assert(rightNodeSize > HalfNode);
      
      size_type stolen = steal_first_child(node, index, rightNode);
      parent.counts[node.pos] += stolen;
      parent.counts[node.pos + 1] -= stolen;
    }
  }
  
  iterator balance_leaf(Leaf& leaf, uint32_t index, uint32_t chunkIndex, uint32_t pos, size_type nth) noexcept
  {
    //Note: chunkIndex and pos of next value (if chunkIndex valid then pos must also be valid)
    assert(leaf.size > 0u);
    assert(leaf.size < HalfNode);
    
    bool hasLeftSibling  = leaf.parent != InvalidIndex && leaf.pos > 0u;
    bool hasRightSibling = leaf.parent != InvalidIndex && leaf.pos + 1u < mNodes[leaf.parent].size();
    uint32_t leftLeafSize  = hasLeftSibling  ? mLeafs[leaf.prev].size : NodeSize;
    uint32_t rightLeafSize = hasRightSibling ? mLeafs[leaf.next].size : NodeSize;
    
    // check prev/next leafs size
    if (leaf.size + leftLeafSize <= NodeSize // merge current to left
        && (leaf.size + rightLeafSize > NodeSize
            || leftLeafSize <= rightLeafSize))
    {
      // move chunks
      Leaf& leftLeaf = mLeafs[leaf.prev];
      Node& parent = mNodes[leftLeaf.parent];
      assert(leaf.parent == leftLeaf.parent);
      
      leftLeaf.steal_all(leaf);
      parent.counts[leftLeaf.pos] += parent.counts[leaf.pos];
      parent.counts[leaf.pos] = 0u;
      
      // delete leaf
      erase_leaf(leaf, index, true);
      
      SANITY_CHECK_SQ;
      uint32_t newIndex = leftLeafSize + chunkIndex;
      if (newIndex < leftLeaf.size) // merged chunk
      {
        return iterator(this, nth, leftLeaf, leaf.prev, newIndex, pos);
      }
      else if (leftLeaf.next != InvalidIndex) // next leaf
      {
        const Leaf& nextLeaf = mLeafs[leftLeaf.next];
        return iterator(this, nth, nextLeaf, leftLeaf.next, 0u, nextLeaf.spans[0].off);
      }
      else // end
      {
        return iterator::endin(this);
      }
    }
    else if (leaf.size + rightLeafSize <= NodeSize) // merge right to current
    {
      // move data
      Leaf& rightLeaf = mLeafs[leaf.next];
      Node& parent = mNodes[rightLeaf.parent];
      assert(leaf.parent == rightLeaf.parent);
      
      uint32_t oldSize = leaf.size;
      leaf.steal_all(rightLeaf);
      parent.counts[leaf.pos] += parent.counts[rightLeaf.pos];
      parent.counts[rightLeaf.pos] = 0u;
      
      // delete leaf
      erase_leaf(rightLeaf, leaf.next, true);
      
      SANITY_CHECK_SQ;
      assert(chunkIndex < leaf.size);
      uint32_t newPos = (chunkIndex != oldSize) ? pos : leaf.spans[chunkIndex].off;
      return iterator(this, nth, leaf, index, chunkIndex, newPos);
    }
    else if (hasLeftSibling) // steal chunk from left
    {
      Leaf& leftLeaf = mLeafs[leaf.prev];
      Node& parent = mNodes[leftLeaf.parent];
      assert(leaf.parent == leftLeaf.parent);
      assert(leftLeafSize > HalfNode);
      
      size_type stolen = leaf.steal_last(leftLeaf);
      parent.counts[leftLeaf.pos] -= stolen;
      parent.counts[leaf.pos] += stolen;
      
      SANITY_CHECK_SQ;
      uint32_t newChunkIndex = chunkIndex + 1u;
      if (newChunkIndex < leaf.size) // merged chunk
      {
        return iterator(this, nth, leaf, index, newChunkIndex, pos);
      }
      if (leaf.next != InvalidIndex) // next leaf
      {
        const Leaf& nextLeaf = mLeafs[leaf.next];
        return iterator(this, nth, nextLeaf, leaf.next, 0u, nextLeaf.spans[0].off);
      }
      else // end
      {
        return iterator::endin(this);
      }
    }
    else if (hasRightSibling) // steal chunk from right
    {
      Leaf& rightLeaf = mLeafs[leaf.next];
      Node& parent = mNodes[rightLeaf.parent];
      assert(leaf.parent == rightLeaf.parent);
      assert(rightLeafSize > HalfNode);
      
      uint32_t oldSize = leaf.size;
      size_type stolen = leaf.steal_first(rightLeaf);
      parent.counts[rightLeaf.pos] -= stolen;
      parent.counts[leaf.pos] += stolen;
      
      SANITY_CHECK_SQ;
      assert(chunkIndex < leaf.size);
      uint32_t newPos = (chunkIndex != oldSize) ? pos : leaf.spans[chunkIndex].off;
      return iterator(this, nth, leaf, index, chunkIndex, newPos);
    }
    else // No balancing
    {
      SANITY_CHECK_SQ;
      if (chunkIndex < leaf.size) // current chunk
      {
        return iterator(this, nth, leaf, index, chunkIndex, pos);
      }
      if (leaf.next != InvalidIndex) // next leaf
      {
        const Leaf& nextLeaf = mLeafs[leaf.next];
        return iterator(this, nth, nextLeaf, leaf.next, 0u, nextLeaf.spans[0].off);
      }
      else // end
      {
        return iterator::endin(this);
      }
    }
  }
  
  iterator erase_value_balance_chunk(Leaf& leaf, uint32_t newChunkSize, const const_iterator& pos) noexcept
  {
    // check prev/next chunks for best merge strategy
    const bool hasLeftChunk  = pos.index > 0u;
    const bool hasRightChunk = pos.index + 1u < leaf.size;
    uint32_t leftChunkSize  = hasLeftChunk  ? leaf.spans[pos.index - 1u].size() : ChunkSize;
    uint32_t rightChunkSize = hasRightChunk ? leaf.spans[pos.index + 1u].size() : ChunkSize;
    
    // merge chunks (if possible)
    if (newChunkSize + leftChunkSize <= MergeSize  // merge current to left
        && (newChunkSize + rightChunkSize > MergeSize || leftChunkSize <= rightChunkSize))
    {
      uint32_t srcIndex = pos.index;
      uint32_t dstIndex = pos.index - 1u; // out: next value index
      uint32_t erasePos = pos.pos; // out: next value pos
      merge_erased_chunk_left(leaf, srcIndex, dstIndex, erasePos);
      DEBUG_EXPR(++dbg.c3);
      
      // delete chunk
      leaf.erase_chunk(srcIndex, chunk_allocator());
      update_counts_minus(leaf.parent, leaf.pos);
      
      // balance leaf
      if (leaf.size < HalfNode)
      {
        DEBUG_EXPR(++dbg.c8);
        return balance_leaf(leaf, pos.cur, dstIndex, erasePos, pos.nth);
      }
      
      if (dstIndex < leaf.size) // same leaf
      {
        return iterator(this, pos.nth, leaf, pos.cur, dstIndex, erasePos);
      }
      else if (leaf.next != InvalidIndex) // next leaf
      {
        const Leaf& nextLeaf = mLeafs[leaf.next];
        return iterator(this, pos.nth, nextLeaf, leaf.next, 0u, nextLeaf.spans[0].off);
      }
      else // end
      {
        return iterator::endin(this);
      }
    }
    else if (newChunkSize + rightChunkSize <= MergeSize) // merge current to right
    {
      uint32_t srcIndex = pos.index;
      uint32_t dstIndex = pos.index + 1u;
      uint32_t erasePos = pos.pos; // out: next value pos
      merge_erased_chunk_right(leaf, srcIndex, dstIndex, erasePos);
      DEBUG_EXPR(++dbg.c4);
      
      // delete chunk
      leaf.erase_chunk(srcIndex, chunk_allocator());
      update_counts_minus(leaf.parent, leaf.pos);
      
      // balance leaf
      if (leaf.size < HalfNode)
      {
        DEBUG_EXPR(++dbg.c9);
        return balance_leaf(leaf, pos.cur, srcIndex, erasePos, pos.nth);
      }
      
      return iterator(this, pos.nth, leaf, pos.cur, srcIndex, erasePos);
    }
    else  // just shift in chunk
    {
      Span& span = leaf.spans[pos.index];
      bool shifted = erase_shift(pos.chunk, pos.pos, span);
      uint32_t newPos = pos.pos + (uint32_t)shifted;
      DEBUG_EXPR(++dbg.c2);
      
      // Steal bulk to balance (if possible)
      if (newChunkSize <= StealSize)
      {
        uint32_t leftSteal  = hasLeftChunk  ? (leftChunkSize  - newChunkSize) / 2u : 0u;
        uint32_t rightSteal = hasRightChunk ? (rightChunkSize - newChunkSize) / 2u : 0u;
        if (rightSteal > 0u && rightSteal >= leftSteal) // from right
        {
          DEBUG_EXPR(++dbg.c12);
          uint32_t shift = steal_from_right(leaf, pos.index, rightSteal);
          newPos -= shift;
          DEBUG_EXPR(if (shift != 0) ++dbg.c13);
        }
        else if (leftSteal > 0u) // from left
        {
          DEBUG_EXPR(++dbg.c14);
          uint32_t shift = steal_from_left(leaf, pos.index, leftSteal);
          newPos += shift;
          DEBUG_EXPR(if (shift != 0) ++dbg.c15);
        }
      }

      update_counts_minus(leaf.parent, leaf.pos);
      SANITY_CHECK_SQ;
      
      if (newPos < span.end) // same chunk
      {
        return iterator(this, pos.nth, pos.chunk, pos.cur, pos.prev, pos.next,
                        pos.index, pos.size, newPos, span.off, span.end);
      }
      else if (pos.index + 1u < pos.size) // next chunk
      {
        return iterator(this, pos.nth, leaf, pos.cur, pos.index + 1u, leaf.spans[pos.index + 1u].off);
      }
      else if (leaf.next != InvalidIndex) // next leaf
      {
        const Leaf& nextLeaf = mLeafs[leaf.next];
        return iterator(this, pos.nth, nextLeaf, leaf.next, 0u, nextLeaf.spans[0].off);
      }
      else // end
      {
        return iterator::endin(this);
      }
    }
  }
  
  // update
  void push_back_leaf()
  {
    uint32_t index = mLeafs.push_back();
    Leaf& leaf = mLeafs[index];
    ScopedLeaf scopedLeaf(mLeafs, leaf, index);
    
    if (!mNodes.empty())
    {
      Leaf& lastLeaf = mLeafs[mLastLeaf];
      Node& parentNode = mNodes[lastLeaf.parent];
      uint16_t nodeEnd = parentNode.size();
      if (nodeEnd < NodeSize)  // same parent
      {
        parentNode.children[nodeEnd] = index;
        assert(parentNode.counts[nodeEnd] == 0u);
        ++parentNode._size;
        leaf.parent = lastLeaf.parent;
        leaf.pos = nodeEnd;
        lastLeaf.next = index;
      }
      else  // new parent
      {
        // find first not full parent
        uint32_t height = 1u;
        uint32_t partial = parentNode.parent;
        while (partial != InvalidIndex && mNodes[partial].full())
        {
          partial = mNodes[partial].parent;
          ++height;
        }
        
        if (partial == InvalidIndex)  // root full: increase height
        {
          uint32_t root = mNodes.push_back();
          uint32_t first = mNodes.root();
          Node& rootNode = mNodes[root];
          Node& firstNode = mNodes[first];
          
          assert(firstNode.parent == InvalidIndex);
          firstNode.parent = root;
          assert(firstNode.pos == 0u);
          rootNode.parent = InvalidIndex;
          rootNode.pos = 0u;
          rootNode.children[0] = first;
          rootNode.counts[0] = firstNode.count();
          ++rootNode._size;
          mNodes.set_root(root);
          
          partial = root;
          ++mHeight;
        }
        // add branch
        uint16_t childIndex = mNodes[partial].size();
        for (uint32_t i = 0u; i < height; ++i)
        {
          uint32_t child = mNodes.push_back();
          Node& childNode = mNodes[child];
          Node& partialNode = mNodes[partial];
          
          childNode.parent = partial;
          childNode.pos = childIndex;
          assert(partialNode.counts[childIndex] == 0u);
          partialNode.children[childIndex] = child;
          ++partialNode._size;
          
          partial = child;
          childIndex = 0u;
        }
        // add leaf
        Node& node = mNodes[partial];
        leaf.parent = partial;
        leaf.pos = 0u;
        node.children[0] = index;
        node.set_size_leafs(1u);
        
        lastLeaf.next = index;
      }
    }
    else // leafs only
    {
      if (mLeafs.first() == InvalidIndex) // first
      {
        leaf.parent = InvalidIndex;
        leaf.pos = 0;
        mLeafs.set_first(index);
        mHeight = 1u;
      }
      else  // second
      {
        uint32_t parent = mNodes.push_back();
        
        assert(mLastLeaf == mLeafs.first());
        Leaf& leaf0 = mLeafs[mLastLeaf];
        leaf0.parent = parent;
        leaf0.pos = 0u;
        leaf0.next = index;
        
        leaf.parent = parent;
        leaf.pos = 1u;
        
        assert(leaf0.count() == mSize);
        Node& node = mNodes[parent];
        node.parent = InvalidIndex;
        node.pos = 0u;
        node.children[0] = mLastLeaf;
        node.children[1] = index;
        node.counts[0] = mSize;
        node.set_size_leafs(2u);
        
        assert(mNodes.root() == InvalidIndex);
        mNodes.set_root(parent);
        mHeight = 2u;
      }
    }
    
    leaf.prev = mLastLeaf;
    leaf.next = InvalidIndex;
    mLastLeaf = index;
    
    scopedLeaf.release();
  }
  
  void push_front_leaf()
  {
    uint32_t index = mLeafs.push_back();
    Leaf& leaf = mLeafs[index];
    ScopedLeaf scopedLeaf(mLeafs, leaf, index);
    
    if (!mNodes.empty())
    {
      Leaf& firstLeaf = mLeafs[mLeafs.first()];
      Node& parentNode = mNodes[firstLeaf.parent];
      if (!parentNode.full())  // same parent
      {
        shift_right_leafs(parentNode, 0u);
        assert(parentNode.counts[0] == 0u);
        parentNode.children[0] = index;
        leaf.parent = firstLeaf.parent;
        leaf.pos = 0u;
        firstLeaf.prev = index;
      }
      else  // new parent
      {
        // find first not full parent
        uint32_t height = 1u;
        uint32_t partial = parentNode.parent;
        while (partial != InvalidIndex && mNodes[partial].full())
        {
          partial = mNodes[partial].parent;
          ++height;
        }
        
        if (partial == InvalidIndex)  // root full: increase height
        {
          uint32_t root = mNodes.push_back();
          uint32_t first = mNodes.root();
          Node& rootNode = mNodes[root];
          Node& firstNode = mNodes[first];
          
          assert(firstNode.parent == InvalidIndex);
          firstNode.parent = root;
          firstNode.pos = 1u;
          rootNode.parent = InvalidIndex;
          rootNode.pos = 0u;
          rootNode.children[1] = first;
          rootNode.counts[1] = firstNode.count();
          rootNode._size = 2u;
          mNodes.set_root(root);
          
          partial = root;
          ++mHeight;
        }
        else
          shift_right_nodes(mNodes[partial], 0u);
        
        // add branch
        for (uint32_t i = 0u; i < height; ++i)
        {
          uint32_t child = mNodes.push_back();
          Node& childNode = mNodes[child];
          Node& partialNode = mNodes[partial];
          
          childNode.parent = partial;
          childNode.pos = 0u;
          assert(partialNode.counts[0] == 0u);
          partialNode.children[0] = child;
          partialNode._size += (i != 0u) ? 1u : 0u;
          
          partial = child;
        }
        // add leaf
        Node& node = mNodes[partial];
        leaf.parent = partial;
        leaf.pos = 0u;
        node.children[0] = index;
        node.set_size_leafs(1u);
        
        firstLeaf.prev = index;
      }
    }
    else  // leafs only
    {
      if (mLeafs.first() == InvalidIndex) // first
      {
        leaf.parent = InvalidIndex;
        leaf.pos = 0u;
        mLastLeaf = index;
        mHeight = 1u;
      }
      else  // second
      {
        uint32_t parent = mNodes.push_back();
        
        assert(mLastLeaf == mLeafs.first());
        Leaf& leaf0 = mLeafs[mLastLeaf];
        leaf0.parent = parent;
        leaf0.pos = 1u;
        leaf0.prev = index;
        
        leaf.parent = parent;
        leaf.pos = 0u;
        
        Node& node = mNodes[parent];
        node.parent = InvalidIndex;
        node.pos = 0u;
        node.children[0] = index;
        node.children[1] = mLastLeaf;
        node.counts[1] = leaf0.count();
        node.set_size_leafs(2u);
        
        assert(mNodes.root() == InvalidIndex);
        mNodes.set_root(parent);
        mHeight = 2u;
      }
    }
    
    leaf.prev = InvalidIndex;
    leaf.next = mLeafs.first();
    mLeafs.set_first(index);
    
    scopedLeaf.release();
  }
  
  void split_node(uint32_t oldIndex)
  {
    uint32_t newIndex = mNodes.push_back();
    Node& oldNode = mNodes[oldIndex];
    Node& newNode = mNodes[newIndex];
    Node& parent = mNodes[oldNode.parent];
    assert(oldNode.size() == NodeSize);
    assert(newNode.size() == 0u);
    assert(parent.size() < NodeSize);
    
    uint32_t newPos = oldNode.pos + 1u;
    shift_right_nodes(parent, newPos);
    parent.children[newPos] = newIndex;
    
    newNode.parent = oldNode.parent;
    newNode.pos = (uint16_t)newPos;
    
    // split children
    steal_half_children(newNode, newIndex, oldNode);
    parent.counts[newPos - 1] = oldNode.count();
    parent.counts[newPos] = newNode.count();
  }
  
  void split_leaf_in_parent(Leaf& oldLeaf, Leaf& newLeaf, Node& parentNode, uint32_t oldIndex, uint32_t newIndex) noexcept
  {
    uint32_t newPos = oldLeaf.pos + 1u;
    shift_right_leafs(parentNode, newPos);
    parentNode.children[newPos] = newIndex;
    
    newLeaf.prev = oldIndex;
    newLeaf.next = oldLeaf.next;
    newLeaf.parent = oldLeaf.parent;
    newLeaf.pos = (uint16_t)newPos;
    oldLeaf.next = newIndex;
    
    // split chunks
    newLeaf.steal_half(oldLeaf);
    parentNode.counts[newPos - 1] = oldLeaf.count();
    parentNode.counts[newPos] = newLeaf.count();
    
    if (oldIndex == mLastLeaf)
      mLastLeaf = newIndex;
    else
      mLeafs[newLeaf.next].prev = newIndex;
  }
  
  void split_leaf(uint32_t oldIndex)
  {
    uint32_t newIndex = mLeafs.push_back();
    Leaf& oldLeaf = mLeafs[oldIndex];
    Leaf& newLeaf = mLeafs[newIndex];
    ScopedLeaf scopedLeaf(mLeafs, newLeaf, newIndex);
    assert(oldLeaf.size == NodeSize);
    assert(newLeaf.size == 0u);
    
    if (!mNodes.empty())
    {
      Node& parentNode = mNodes[oldLeaf.parent];
      uint16_t parentSize = parentNode.size();
      if (parentSize < NodeSize) // same parent
      {
        split_leaf_in_parent(oldLeaf, newLeaf, parentNode, oldIndex, newIndex);
      }
      else // new parent
      {
        // find first not full parent
        int32_t indexesSize = 1u;
        std::array<uint32_t, 64> fullNodes; // large enough
        fullNodes[0] = oldLeaf.parent;
        
        uint32_t partial = parentNode.parent;
        while (partial != InvalidIndex && mNodes[partial].full())
        {
          fullNodes[indexesSize++] = partial;
          partial = mNodes[partial].parent;
        }
        
        if (partial == InvalidIndex)  // root full: increase height
        {
          uint32_t root = mNodes.push_back();
          uint32_t first = mNodes.root();
          Node& rootNode = mNodes[root];
          Node& firstNode = mNodes[first];
          
          assert(firstNode.parent == InvalidIndex);
          firstNode.parent = root;
          assert(firstNode.pos == 0u);
          rootNode.parent = InvalidIndex;
          rootNode.pos = 0u;
          rootNode.children[0] = first;
          rootNode.counts[0] = firstNode.count();
          ++rootNode._size;
          mNodes.set_root(root);
          
          partial = root;
          ++mHeight;
        }
        
        // split full parent nodes
        for (int32_t i = indexesSize - 1; i >= 0; --i)
        {
          split_node(fullNodes[i]);
        }
        
        // split full leaf
        Node& parent = mNodes[oldLeaf.parent];
        split_leaf_in_parent(oldLeaf, newLeaf, parent, oldIndex, newIndex);
      }
    }
    else // single leaf
    {
      assert(mLeafs.size() == 2u); // old + new
      uint32_t parent = mNodes.push_back();
      
      assert(mLastLeaf == mLeafs.first());
      assert(oldIndex == mLastLeaf);
      oldLeaf.parent = parent;
      oldLeaf.pos = 0u;
      oldLeaf.next = newIndex;
      
      newLeaf.prev = oldIndex;
      newLeaf.next = InvalidIndex;
      newLeaf.parent = parent;
      newLeaf.pos = 1u;
      
      Node& node = mNodes[parent];
      node.parent = InvalidIndex;
      node.pos = 0u;
      node.children[0] = oldIndex;
      node.children[1] = newIndex;
      node.set_size_leafs(2u);
      
      // split chunks
      assert(oldLeaf.count() == mSize);
      newLeaf.steal_half(oldLeaf);
      size_type newCount = newLeaf.count();
      node.counts[0] = mSize - newCount;
      node.counts[1] = newCount;
      
      assert(mNodes.root() == InvalidIndex);
      mNodes.set_root(parent);
      mLastLeaf = newIndex;
      mHeight = 2u;
    }
    
    scopedLeaf.release();
    SANITY_CHECK_SQ;
  }
  
  // add
  template <class U>
  void insert_in_split_chunk(Leaf& leaf, T* newChunk, iterator& pos, U&& value)
  {
    //Note: pos could be invalid here as 'last + 1' (i.e. pos.pos == pos.end)
    static_assert(std::is_same<U&&, const T&>::value || std::is_same<U&&, T&&>::value, "sparque: wrong template");
    assert(leaf.size < NodeSize);
    
    leaf.shift_right(pos.index + 1);
    leaf.chunks[pos.index + 1] = newChunk;
    
    Span& newSpan = leaf.spans[pos.index + 1];
    Span& oldSpan = leaf.spans[pos.index];
    T* oldChunk = leaf.chunks[pos.index];
    
    if (pos.pos <= HalfChunk_Floor) // insert in old
    {
      // move to right from HalfNFloor included
      uint16_t copy_n = ChunkSize - HalfChunk_Floor;
      std::uninitialized_copy_n(std::make_move_iterator(oldChunk + HalfChunk_Floor), copy_n, newChunk);
      newSpan.end = copy_n;
      
      destroy_range(oldChunk + HalfChunk_Floor + 1, oldChunk + ChunkSize); // keep HalfChunk_Floor
      oldSpan.end -= --copy_n;
      
      update_counts_plus(leaf.parent, leaf.pos);
      
      // shift oldChunk to insert new value
      std::move_backward(oldChunk + pos.pos, oldChunk + HalfChunk_Floor, oldChunk + HalfChunk_Floor + 1);
      oldChunk[pos.pos] = std::forward<U>(value);
      
      ++pos.size;
      pos.end = oldSpan.end;
    }
    else // insert in new
    {
      // move to right from pos.pos included
      uint16_t newPos = (uint16_t)pos.pos - (HalfChunk_Floor + 1);
      uint16_t copy_n = ChunkSize - (uint16_t)pos.pos;
      std::uninitialized_copy_n(std::make_move_iterator(oldChunk + pos.pos), copy_n, newChunk + newPos + 1);
      newSpan.off = newPos + 1;
      newSpan.end = newPos + 1 + copy_n;
      
      destroy_range(oldChunk + pos.pos, oldChunk + ChunkSize);
      oldSpan.end -= copy_n;
      
      // prepend new value
      ::new (static_cast<void*>(newChunk + newPos)) T(std::forward<U>(value));
      --newSpan.off;
      
      update_counts_plus(leaf.parent, leaf.pos);
      
      // move the rest before, from HalfNFloor not included
      std::uninitialized_copy_n(std::make_move_iterator(oldChunk + HalfChunk_Floor + 1), newPos, newChunk);
      newSpan.off = 0u;
      
      destroy_range(oldChunk + HalfChunk_Floor + 1, oldChunk + pos.pos);
      oldSpan.end -= newPos;
      
      pos.chunk = newChunk;
      ++pos.index;
      ++pos.size;
      pos.pos = newPos;
      pos.off = 0u;
      pos.end = newSpan.end;
    }
    SANITY_CHECK_SQ;
  }
  
  template <class... Args>
  void emplace_in_split_chunk(Leaf& leaf, T* newChunk, iterator& pos, Args&&... args)
  {
    //Note: pos could be invalid here as 'last + 1' (i.e. pos.pos == pos.end)
    assert(leaf.size < NodeSize);
    
    leaf.shift_right(pos.index + 1);
    leaf.chunks[pos.index + 1] = newChunk;
    
    Span& newSpan = leaf.spans[pos.index + 1];
    Span& oldSpan = leaf.spans[pos.index];
    T* oldChunk = leaf.chunks[pos.index];
    
    if (pos.pos <= HalfChunk_Floor) // insert in old
    {
      // move to right from HalfNFloor included
      uint16_t copy_n = ChunkSize - HalfChunk_Floor;
      std::uninitialized_copy_n(std::make_move_iterator(oldChunk + HalfChunk_Floor), copy_n, newChunk);
      newSpan.end = copy_n;
      
      destroy_range(oldChunk + HalfChunk_Floor + 1, oldChunk + ChunkSize); // keep HalfChunk_Floor
      oldSpan.end -= --copy_n;
      
      update_counts_plus(leaf.parent, leaf.pos);
      
      // shift oldChunk to insert new value
      std::move_backward(oldChunk + pos.pos, oldChunk + HalfChunk_Floor, oldChunk + HalfChunk_Floor + 1);
      oldChunk[pos.pos] = std::move(T(std::forward<Args>(args)...));
      
      ++pos.size;
      pos.end = oldSpan.end;
    }
    else // insert in new
    {
      // move to right from pos.pos included
      uint16_t newPos = (uint16_t)pos.pos - (HalfChunk_Floor + 1);
      uint16_t copy_n = ChunkSize - (uint16_t)pos.pos;
      std::uninitialized_copy_n(std::make_move_iterator(oldChunk + pos.pos), copy_n, newChunk + newPos + 1);
      newSpan.off = newPos + 1;
      newSpan.end = newPos + 1 + copy_n;
      
      destroy_range(oldChunk + pos.pos, oldChunk + ChunkSize);
      oldSpan.end -= copy_n;
      
      // prepend new value
      ::new (static_cast<void*>(newChunk + newPos)) T(std::forward<Args>(args)...);
      --newSpan.off;
      
      update_counts_plus(leaf.parent, leaf.pos);
      
      // move the rest before, from HalfNFloor not included
      std::uninitialized_copy_n(std::make_move_iterator(oldChunk + HalfChunk_Floor + 1), newPos, newChunk);
      newSpan.off = 0u;
      
      destroy_range(oldChunk + HalfChunk_Floor + 1, oldChunk + pos.pos);
      oldSpan.end -= newPos;
      
      pos.chunk = newChunk;
      ++pos.index;
      ++pos.size;
      pos.pos = newPos;
      pos.off = 0u;
      pos.end = newSpan.end;
    }
    SANITY_CHECK_SQ;
  }
  
  template <class U>
  iterator insert_in_full_chunk(Leaf& leaf, const const_iterator& pos, U&& value)
  {
    //Note: pos could be invalid here as 'last + 1' (i.e. pos.pos == pos.end)
    static_assert(std::is_same<U&&, const T&>::value || std::is_same<U&&, T&&>::value, "sparque: wrong template");
    assert(pos.off == 0u);
    assert(pos.end == ChunkSize);
    iterator res;
    
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    // split chunk
    if (leaf.size < NodeSize) // same leaf
    {
      res = pos;
      insert_in_split_chunk(leaf, newStorage.release(), res, std::forward<U>(value));
    }
    else // new leaf
    {
      split_leaf(pos.cur); // invalidate leaf
      
      assert(mLeafs[pos.cur].size == HalfNode);
      bool sameLeaf = pos.index < HalfNode; // HalfNode in right split
      uint32_t newCur = sameLeaf ? pos.cur : mLeafs[pos.cur].next;
      uint32_t newIndex = sameLeaf ? pos.index : pos.index - HalfNode;
      Leaf& newLeaf = mLeafs[newCur];
      res.set(this, pos.nth, newLeaf, newCur, newIndex, pos.pos);
      
      insert_in_split_chunk(newLeaf, newStorage.release(), res, std::forward<U>(value));
    }
    
    return res;
  }
  
  template <class... Args>
  iterator emplace_in_full_chunk(Leaf& leaf, const const_iterator& pos, Args&&... args)
  {
    //Note: pos could be invalid here as 'last + 1' (i.e. pos.pos == pos.end)
    assert(pos.off == 0u);
    assert(pos.end == ChunkSize);
    iterator res;
    
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    // split chunk
    if (leaf.size < NodeSize) // same leaf
    {
      res = pos;
      emplace_in_split_chunk(leaf, newStorage.release(), res, std::forward<Args>(args)...);
    }
    else // new leaf
    {
      split_leaf(pos.cur); // invalidate leaf
      
      assert(mLeafs[pos.cur].size == HalfNode);
      bool sameLeaf = pos.index < HalfNode; // HalfNode in right split
      uint32_t newCur = sameLeaf ? pos.cur : mLeafs[pos.cur].next;
      uint32_t newIndex = sameLeaf ? pos.index : pos.index - HalfNode;
      Leaf& newLeaf = mLeafs[newCur];
      res.set(this, pos.nth, newLeaf, newCur, newIndex, pos.pos);
      
      emplace_in_split_chunk(newLeaf, newStorage.release(), res, std::forward<Args>(args)...);
    }
    
    return res;
  }
  
  template <class U>
  iterator insert_at_end(U&& value)
  {
    static_assert(std::is_same<U&&, const T&>::value || std::is_same<U&&, T&&>::value, "sparque: wrong template");
    
    if (empty())
    {
      push_back(std::forward<U>(value));
      return iterator::lastin(this);
    }
    
    Leaf& leaf = mLeafs[mLastLeaf];
    uint16_t chunkSize = leaf.spans[leaf.last()].size();
    
    if (chunkSize < ChunkSize) // insert in chunk
    {
      iterator res = iterator::lastin(this);
      Span& span = leaf.spans[res.index];
      ++res.nth;
      
      if (res.end < ChunkSize) // emplace back
      {
        T* ptrEnd = res.chunk + res.end;
        ::new (static_cast<void*>(ptrEnd)) T(std::forward<U>(value));
        
        ++span.end;
        update_counts_plus(leaf.parent, leaf.pos);
        
        ++res.end;
        ++res.pos;
      }
      else // shift left
      {
        assert(res.off > 0u);
        T* ptrOff = res.chunk + res.off;
        ::new (static_cast<void*>(ptrOff - 1)) T(std::move(*ptrOff));
        
        --span.off;
        update_counts_plus(leaf.parent, leaf.pos);
        
        T* ptrEnd = res.chunk + ChunkSize;
        std::move(ptrOff + 1, ptrEnd, ptrOff);
        
        *(ptrEnd - 1) = std::forward<U>(value);
        
        --res.off;
      }
      return res;
    }
    else
    {
      const_iterator res = const_iterator::lastin(this);
      ++res.nth;
      ++res.pos;
      return insert_in_full_chunk(leaf, res, std::forward<U>(value));
    }
  }
  
  template <class... Args>
  iterator emplace_at_end(Args&&... args)
  {
    if (empty())
    {
      emplace_back(std::forward<Args>(args)...);
      return iterator::lastin(this);
    }
    
    Leaf& leaf = mLeafs[mLastLeaf];
    uint16_t chunkSize = leaf.spans[leaf.last()].size();
    
    if (chunkSize < ChunkSize) // insert in chunk
    {
      iterator res = iterator::lastin(this);
      Span& span = leaf.spans[res.index];
      ++res.nth;
      
      if (res.end < ChunkSize) // emplace back
      {
        T* ptrEnd = res.chunk + res.end;
        ::new (static_cast<void*>(ptrEnd)) T(std::forward<Args>(args)...);
        
        ++span.end;
        update_counts_plus(leaf.parent, leaf.pos);
        
        ++res.end;
        ++res.pos;
      }
      else // shift left
      {
        assert(res.off > 0u);
        T* ptrOff = res.chunk + res.off;
        ::new (static_cast<void*>(ptrOff - 1)) T(std::move(*ptrOff));
        
        --span.off;
        update_counts_plus(leaf.parent, leaf.pos);
        
        T* ptrEnd = res.chunk + ChunkSize;
        std::move(ptrOff + 1, ptrEnd, ptrOff);
        
        *(ptrEnd - 1) = std::move(T(std::forward<Args>(args)...));
        
        --res.off;
      }
      return res;
    }
    else
    {
      const_iterator res = const_iterator::lastin(this);
      ++res.nth;
      ++res.pos;
      return emplace_in_full_chunk(leaf, res, std::forward<Args>(args)...);
    }
  }
  
  // remove
  void remove_node_leaf(Node& node, uint32_t pos) noexcept
  {
    const uint32_t size_ = node.size();
    assert(size_ > 0u);
    assert(pos < size_);
    
    assert(node.has_leafs());
    for (uint32_t i = pos + 1u; i < size_; ++i)
      --mLeafs[node.children[i]].pos;
    
    std::memmove(  node.counts.data() + pos,   node.counts.data() + pos + 1, (size_ - (pos + 1u)) * sizeof(size_type));
    node.counts[size_ - 1] = 0u;
    std::memmove(node.children.data() + pos, node.children.data() + pos + 1, (size_ - (pos + 1u)) * sizeof(uint32_t));
  #ifndef NDEBUG
    node.children[size_ - 1] = InvalidIndex;
  #endif
    --node._size;
  }
  
  void remove_node_node(Node& node, uint32_t pos) noexcept
  {
    const uint32_t size_ = node.size();
    assert(size_ > 0u);
    assert(pos < size_);
    
    assert(!node.has_leafs());
    for (uint32_t i = pos + 1u; i < size_; ++i)
      --mNodes[node.children[i]].pos;
    
    std::memmove(  node.counts.data() + pos,   node.counts.data() + pos + 1, (size_ - (pos + 1u)) * sizeof(size_type));
    node.counts[size_ - 1] = 0u;
    std::memmove(node.children.data() + pos, node.children.data() + pos + 1, (size_ - (pos + 1u)) * sizeof(uint32_t));
  #ifndef NDEBUG
    node.children[size_ - 1] = InvalidIndex;
  #endif
    --node._size;
  }
  
  void erase_node(Node& node, uint32_t index, bool tryMerge) noexcept
  {
    DEBUG_EXPR(++dbg.c11);
    assert(node.counts[0] == 0u);
    assert(node.size() == 1u);
    assert(node.parent != InvalidIndex);
    
    const uint32_t parentIndex = node.parent;
    Node& parentNode = mNodes[parentIndex];
    const bool isRoot = parentNode.parent == InvalidIndex;
    const uint32_t parentSize = parentNode.size() - 1u;
    if (parentSize >= HalfNode && !isRoot)
    {
      remove_node_node(parentNode, node.pos);
    }
    else  // balance
    {
      if (!isRoot)
      {
        if (parentSize > 0u)
        {
          remove_node_node(parentNode, node.pos);
          
          if (tryMerge)
            balance_node(parentNode, parentIndex);
        }
        else  // empty
        {
          erase_node(parentNode, node.parent, tryMerge);
        }
      }
      else  // root
      {
        assert(parentSize >= 1u);
        assert(node.parent == mNodes.root());
        
        if (parentSize > 1u)  // update
        {
          remove_node_node(parentNode, node.pos);
        }
        else  // reduce height
        {
          uint32_t other = (node.pos != 0u) ? 0u : 1u;
          uint32_t remain = parentNode.children[other];
          Node& remainNode = mNodes[remain];
          if (remainNode.size() > 1u)
          {
            mNodes.set_root(remain);
            remainNode.parent = InvalidIndex;
            remainNode.pos = 0;
            
            --mHeight;
            mNodes.free_node(parentNode, node.parent);
          }
          else  // remove single-child root nodes
          {
            --mHeight;
            mNodes.free_node(parentNode, node.parent);
            while (mNodes[remain].has_single_node())
            {
              --mHeight;
              uint32_t child = mNodes[remain].children[0];
              mNodes.free_node(mNodes[remain], remain);
              remain = child;
            }
            
            if (!mNodes[remain].has_single_leaf())
            {
              mNodes.set_root(remain);
              mNodes[remain].parent = InvalidIndex;
              assert(mNodes[remain].pos == 0u);
            }
            else
            {
              assert(mSize <= ChunkSize * NodeSize);
              assert(mHeight == 2u);
              mHeight = 1u;
              Leaf& lastLeaf = mLeafs[mNodes[remain].children[0]];
              lastLeaf.parent = InvalidIndex;
              mNodes.free_node(mNodes[remain], remain);
            }
          }
        }
      }
    }
    
    mNodes.free_node(node, index);
  }
  
  void erase_leaf(Leaf& leaf, uint32_t index, bool tryMerge) noexcept
  {
    DEBUG_EXPR(++dbg.c10);
    assert(leaf.size == 0u);
    
    // update prev/next leafs
    if (leaf.prev != InvalidIndex)
      mLeafs[leaf.prev].next = leaf.next;
    else
      mLeafs.set_first(leaf.next);
    
    if (leaf.next != InvalidIndex)
      mLeafs[leaf.next].prev = leaf.prev;
    else
      mLastLeaf = leaf.prev;
    
    // update parent/sibling
    if (leaf.parent != InvalidIndex)
    {
      Node& node = mNodes[leaf.parent];
      const bool isRoot = node.parent == InvalidIndex;
      assert(node.counts[leaf.pos] == 0u);
      const uint32_t nodeSize = node.size() - 1u;
      if (nodeSize >= HalfNode && !isRoot)
      {
        remove_node_leaf(node, leaf.pos);
      }
      else  // balance
      {
        if (!isRoot)
        {
          if (nodeSize > 0u)
          {
            remove_node_leaf(node, leaf.pos);
            
            if (tryMerge)
              balance_node(node, leaf.parent);
          }
          else  // empty
          {
            erase_node(node, leaf.parent, tryMerge);
            mNodes.reset_empty();
          }
        }
        else  // root
        {
          assert(nodeSize >= 1u);
          assert(leaf.parent == mNodes.root());
          
          if (nodeSize > 1u)  // update
          {
            remove_node_leaf(node, leaf.pos);
          }
          else  // reduce height
          {
            uint32_t other = (leaf.pos != 0u) ? 0u : 1u;
            uint32_t remain = node.children[other];
            Leaf& remainLeaf = mLeafs[remain];
            remainLeaf.parent = InvalidIndex;
            remainLeaf.pos = 0;
            
            assert(mHeight == 2u);
            --mHeight;
            mNodes.free_last(node);
          }
        }
      }
      
      mLeafs.free_leaf(leaf, index);
    }
    else  // single leaf
    {
      assert(mSize == 0);
      assert(mHeight == 1u);
      assert(mNodes.empty());
      --mHeight;
      mLeafs.free_last(leaf);
      mLastLeaf = InvalidIndex;
    }
  }
  
  iterator erase_value_erase_chunk(Leaf& leaf, const const_iterator& pos) noexcept
  {
    leaf.erase_chunk(pos.index, chunk_allocator());
    update_counts_minus(leaf.parent, leaf.pos);
    
    if (leaf.size >= HalfNode) // balanced leaf
    {
      DEBUG_EXPR(++dbg.c5);
      SANITY_CHECK_SQ;
      if (pos.index < leaf.size) // next chunk
      {
        return iterator(this, pos.nth, leaf, pos.cur, pos.index, leaf.spans[pos.index].off);
      }
      else if (leaf.next != InvalidIndex) // next leaf
      {
        const Leaf& nextLeaf = mLeafs[leaf.next];
        return iterator(this, pos.nth, nextLeaf, leaf.next, 0u, nextLeaf.spans[0].off);
      }
      else  // end
      {
        return iterator::endin(this);
      }
    }
    else if (leaf.size > 0u) // balance leaf
    {
      DEBUG_EXPR(++dbg.c6);
      return balance_leaf(leaf, pos.cur, pos.index, leaf.spans[pos.index].off, pos.nth);
    }
    else // empty leaf
    {
      DEBUG_EXPR(++dbg.c7);
      uint32_t next = leaf.next;
      erase_leaf(leaf, pos.cur, true);
      
      iterator res = iterator::endin(this);
      if (next != InvalidIndex)
      {
        const Leaf& nextLeaf = mLeafs[next];
        res = iterator(this, pos.nth, nextLeaf, next, 0u, nextLeaf.spans[0].off);
      }
      
      SANITY_CHECK_SQ;
      return res;
    }
  }
  
  void purge() noexcept
  {
    mLeafs.purge();
    mNodes.purge();
    mSize = 0u;
    mHeight = 0u;
    mLastLeaf = InvalidIndex;
  }
  
  // resize
  void destroy_node(uint32_t index) noexcept
  {
    assert(index != InvalidIndex);
    
    Node& node = mNodes[index];
    uint32_t size = node.size();
    assert(size >= 1u);
    if (!node.has_leafs())
    {
      uint32_t i = size;
      do {
        destroy_node(node.children[--i]);
      }
      while (i > 0u);
    }
    else
    {
      assert(node.children[size - 1u] == mLastLeaf);
      uint32_t firstIdx = node.children[0];
      assert(firstIdx != mLeafs.first());
      Leaf& firstLeaf = mLeafs[firstIdx];
      Leaf& prevLeaf = mLeafs[firstLeaf.prev];
      prevLeaf.next = InvalidIndex;
      mLastLeaf = firstLeaf.prev;
      
      uint32_t i = size;
      do {
        destroy_leaf(node.children[--i]);
      }
      while (i > 0u);
    }
    
    mNodes.free_node(node, index);
  }
  
  void destroy_leaf(uint32_t index) noexcept
  {
    assert(index != InvalidIndex);
    
    Leaf& leaf = mLeafs[index];
    mLeafs.free_leaf(leaf, index);
  }
  
  void erase_last_n_in_node(uint32_t index, size_type count) noexcept
  {
    assert(index != InvalidIndex);
    assert(count > 0u);
    
    Node& node = mNodes[index];
    assert(node.size() >= 1u);
    uint32_t i = node.size() - 1u;
    if (!node.has_leafs())
    {
      do
      {
        uint32_t childIdx = node.children[i];
        size_type childCount = node.counts[i];
        if (childCount <= count)
        {
          destroy_node(childIdx);
          --node._size;
          node.counts[i] = 0u;
        #ifndef NDEBUG
          node.children[i] = InvalidIndex;
        #endif
        }
        else
        {
          erase_last_n_in_node(childIdx, count);
          node.counts[i] -= count;
          break;
        }
        
        --i;
        count -= childCount;
      }
      while (count > 0u);
    }
    else
    {
      uint32_t prev = InvalidIndex;
      do
      {
        uint32_t childIdx = node.children[i];
        size_type childCount = node.counts[i];
        Leaf& leaf = mLeafs[childIdx];
        if (childCount <= count)
        {
          prev = leaf.prev;
          destroy_leaf(childIdx);
          --node._size;
          node.counts[i] = 0u;
        #ifndef NDEBUG
          node.children[i] = InvalidIndex;
        #endif
        }
        else
        {
          erase_last_n_in_leaf(childIdx, count);
          node.counts[i] -= count;
          leaf.next = InvalidIndex;
          mLastLeaf = childIdx;
          break;
        }
        
        --i;
        count -= childCount;
      }
      while (count > 0u);
      
      if (count == 0) // destroyed last
      {
        mLastLeaf = prev;
        mLeafs[prev].next = InvalidIndex;
      }
    }
  }
  
  void erase_last_n_in_leaf(uint32_t index, size_type count) noexcept
  {
    assert(index != InvalidIndex);
    assert(count > 0u);
    
    Leaf& leaf = mLeafs[index];
    leaf.erase_last_n(count, chunk_allocator());
  }
  
  void erase_last_n(size_type count) noexcept
  {
    assert(count <= mSize);
    assert(count > 0u);
    if (count == mSize)
    {
      clear();
      return;
    }
    
    // iter from last top-node/leaf to substract N
    uint32_t nodeIdx = mNodes.root();
    if (nodeIdx != InvalidIndex)
    {
      erase_last_n_in_node(nodeIdx, count);
      
      // update height
      uint32_t nodeSize = mNodes[nodeIdx].size();
      while (nodeSize == 1u)
      {
        Node& node = mNodes[nodeIdx];
        uint32_t nextIdx = node.children[0];
        
        if (!node.has_leafs())
        {
          mNodes.free_node(node, nodeIdx);
          mNodes.set_root(nextIdx);
          nodeIdx = nextIdx;
          --mHeight;
          assert(mHeight > 1u);
        }
        else
        {
          mNodes.free_last(node);
          assert(mLeafs.size() == 1u);
          mLeafs[mLastLeaf].parent = InvalidIndex;
          --mHeight;
          assert(mHeight == 1u);
          break;
        }
        
        Node& nextNode = mNodes[nodeIdx];
        nextNode.parent = InvalidIndex;
        nodeSize = nextNode.size();
      }
    }
    else // single leaf
    {
      assert(mLeafs.size() == 1u);
      erase_last_n_in_leaf(mLastLeaf, count);
    }
    
    mSize -= count;
    SANITY_CHECK_SQ;
  }
  
  template <class InputIt>
  void push_back_n_in_leaf(uint32_t index, size_type& count, InputIt& it,
                           uint32_t childIdx = 0u, size_type added = 0u)
  {
    assert(index != InvalidIndex);
    assert(count > 0u);
    assert(childIdx < NodeSize);

    Leaf& leaf = mLeafs[index];
    assert(childIdx == leaf.size);
    try
    {
      do
      {
        // new chunk
        auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
        size_type count_ = std::min<size_type>(ChunkSize, count);
        detail::fill_chunk(newStorage.get(), (uint32_t)count_, it);
        
        leaf.emplace_at(childIdx, 0u, (uint16_t)count_, newStorage.release());
        ++leaf.size;
        added += count_;
        count -= count_;
      }
      while (count > 0u && (++childIdx < NodeSize));
    }
    catch (...)
    {
      update_counts_plus_n(leaf.parent, leaf.pos, added);
      if (childIdx == 0u)
      {
        assert(leaf.count() == 0u);
        if (mLeafs.size() > 1u)
          mLeafs.free_leaf(leaf, index);
        else
          mLeafs.free_last(leaf);
      }
      throw;
    }
    // only update count per filled leaf (optimization compromise)
    update_counts_plus_n(leaf.parent, leaf.pos, added);
  }
  
  template <class InputIt>
  void push_back_range_in_leaf(uint32_t index, InputIt& first, InputIt& last,
                               uint32_t childIdx = 0u, size_type added = 0u)
  {
    assert(index != InvalidIndex);
    assert(first != last);
    assert(childIdx < NodeSize);
    
    Leaf& leaf = mLeafs[index];
    assert(childIdx == leaf.size);
    try
    {
      do
      {
        // new chunk
        auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
        
        size_type count_ = 0u;
        for (; count_ < ChunkSize && first != last; ++count_, ++first)
        {
          ::new (static_cast<void*>(newStorage.get() + count_)) T(*first);
        }
        
        leaf.emplace_at(childIdx, 0u, (uint16_t)count_, newStorage.release());
        ++leaf.size;
        added += count_;
      }
      while (first != last && (++childIdx < NodeSize));
    }
    catch (...)
    {
      update_counts_plus_n(leaf.parent, leaf.pos, added);
      if (childIdx == 0u)
      {
        assert(leaf.count() == 0u);
        if (mLeafs.size() > 1u)
          mLeafs.free_leaf(leaf, index);
        else
          mLeafs.free_last(leaf);
      }
      throw;
    }
    // only update count per filled leaf (optimization compromise)
    update_counts_plus_n(leaf.parent, leaf.pos, added);
  }
  
  template <class InputIt>
  void push_back_n(size_type count, InputIt& it)
  {
    assert(count > 0u);
    
    if (mLastLeaf != InvalidIndex)
    {
      // fill last chunk
      Leaf& lastLeaf = mLeafs[mLastLeaf];
      uint32_t childIdx = lastLeaf.last();
      Span& span = lastLeaf.spans[childIdx];
      T* chunk = lastLeaf.chunks[childIdx];
      
      size_type count_ = std::min<size_type>(ChunkSize - span.end, count);
      detail::fill_chunk(chunk + span.end, (uint32_t)count_, it);
      span.end += (uint16_t)count_;
      count -= count_;
      
      // fill last leaf
      if (++childIdx < NodeSize && count > 0u)
        push_back_n_in_leaf(mLastLeaf, count, it, childIdx, count_);
      else
        update_counts_plus_n(lastLeaf.parent, lastLeaf.pos, count_);
    }
    
    // new leafs
    while (count > 0u)
    {
      push_back_leaf();
      push_back_n_in_leaf(mLastLeaf, count, it);
    }
  }
  
  template <class InputIt>
  void push_back_range(InputIt& first, InputIt& last)
  {
    if (mLastLeaf != InvalidIndex)
    {
      // fill last chunk
      Leaf& lastLeaf = mLeafs[mLastLeaf];
      uint32_t childIdx = lastLeaf.last();
      Span& span = lastLeaf.spans[childIdx];
      T* chunk = lastLeaf.chunks[childIdx];
      
      size_type count_ = 0u;
      for (; span.end < ChunkSize && first != last; ++span.end, ++first, ++count_)
      {
        ::new (static_cast<void*>(chunk + span.end)) T(*first);
      }
      
      // fill last leaf
      if (++childIdx < NodeSize && first != last)
        push_back_range_in_leaf(mLastLeaf, first, last, childIdx, count_);
      else
        update_counts_plus_n(lastLeaf.parent, lastLeaf.pos, count_);
    }
    
    // new leafs
    while (first != last)
    {
      push_back_leaf();
      push_back_range_in_leaf(mLastLeaf, first, last);
    }
  }
  
  // counts
  void update_counts_plus(uint32_t parent, uint32_t pos) noexcept
  {
    ++mSize;
    while (parent != InvalidIndex)
    {
      Node& node = mNodes[parent];
      ++node.counts[pos];
      
      pos = node.pos;
      parent = node.parent;
    }
    SANITY_CHECK_SQ;
  }
  
  void update_counts_minus(uint32_t parent, uint32_t pos) noexcept
  {
    --mSize;
    while (parent != InvalidIndex)
    {
      Node& node = mNodes[parent];
      --node.counts[pos];
      
      pos = node.pos;
      parent = node.parent;
    }
  }
  
  void update_counts_plus_n(uint32_t parent, uint32_t pos, size_type count) noexcept
  {
    mSize += count;
    while (parent != InvalidIndex)
    {
      Node& node = mNodes[parent];
      node.counts[pos] += count;
      
      pos = node.pos;
      parent = node.parent;
    }
    SANITY_CHECK_SQ;
  }
  
private:
  // Members
  LeafVec mLeafs;         // leafs sparse vector
  NodeVec mNodes;         // nodes sparse vector
  size_type mSize = 0u;   // total number of elements
  uint32_t mHeight = 0u;  // tree height (including leafs)
  uint32_t mLastLeaf = InvalidIndex;  // last leaf index
  
  DataAlc& chunk_allocator() noexcept { return mLeafs.chunk_allocator(); }
  const DataAlc& chunk_allocator() const noexcept { return mLeafs.chunk_allocator(); }
  Allocator& get_element_allocator() noexcept { return mLeafs.get_element_allocator(); }
  const Allocator& get_element_allocator() const noexcept { return mLeafs.get_element_allocator(); }
  LeafAllocator& get_leaf_allocator() noexcept { return mLeafs.get_leaf_allocator(); }
  const LeafAllocator& get_leaf_allocator() const noexcept { return mLeafs.get_leaf_allocator(); }
  NodeAllocator& get_node_allocator() noexcept { return mNodes.get_node_allocator(); }
  const NodeAllocator& get_node_allocator() const noexcept { return mNodes.get_node_allocator(); }
  
public:
  //
  // Constructor/Destructor
  //
  sparque() = default;
  
  explicit sparque(const Allocator& alloc)
    : mLeafs(alloc)
    , mNodes(alloc)
  {}
  
  sparque(size_type count, const T& value, const Allocator& alloc = Allocator())
    : mLeafs(alloc)
    , mNodes(alloc)
    , mSize(count)
  {
    if (count == 0u)
      return;
    
    const uint32_t chunks = div_ceil_chunk((uint32_t)count);
    const uint32_t leafs = div_ceil_node(chunks);
    
    mHeight = (uint32_t)std::ceil(log_node(chunks));
    mHeight = mHeight != 0u ? mHeight : 1u;
    uint32_t nodes = count_nodes(leafs, mHeight);
    
    // init data
    init_tree(count, leafs, nodes, value);
  }
  
  explicit sparque(size_type count, const Allocator& alloc = Allocator())
    : sparque(count, T(), alloc)
  {}
  
  template <class InputIt,
           typename = typename std::iterator_traits<InputIt>::iterator_category>
  sparque(InputIt first, InputIt last, const Allocator& alloc = Allocator())
    : mLeafs(alloc)
    , mNodes(alloc)
  {
    construct_impl(first, last);
  }
  
  sparque(const sparque& other) = default;
  
  sparque(const sparque& other, const Allocator& alloc)
    : mLeafs(other.mLeafs, alloc)
    , mNodes(other.mNodes, alloc)
    , mSize(other.mSize)
    , mHeight(other.mHeight)
    , mLastLeaf(other.mLastLeaf)
  {}
  
  sparque(sparque&& other) noexcept(std::is_nothrow_move_constructible<Allocator>::value)
    : mLeafs(std::move(other.mLeafs))
    , mNodes(std::move(other.mNodes))
    , mSize(other.mSize)
    , mHeight(other.mHeight)
    , mLastLeaf(other.mLastLeaf)
  {
    other.mSize = 0u;
    other.mHeight = 0u;
    other.mLastLeaf = InvalidIndex;
  }
  
  sparque(sparque&& other, const Allocator& alloc)
    : sparque(alloc)
  {
    if (get_element_allocator() == other.get_element_allocator())
    {
      mLeafs = std::move(other.mLeafs);
      mNodes = std::move(other.mNodes);
      mSize = other.mSize;
      mHeight = other.mHeight;
      mLastLeaf = other.mLastLeaf;
      
      other.mSize = 0u;
      other.mHeight = 0u;
      other.mLastLeaf = InvalidIndex;
    }
    else
    {
      construct_impl(std::make_move_iterator(other.begin()),
                     std::make_move_iterator(other.end()));
    }
  }
  
  sparque(std::initializer_list<T> init, const Allocator& alloc = Allocator())
    : sparque(init.begin(), init.end(), alloc)
  {}
  
  ~sparque() = default;
  
  //
  // Assignment
  //
  sparque& operator=(const sparque& other)
  {
    if (this == &other)
      return *this;
    
    copy_assign_alloc(other);
    
    // erase tail data
    difference_type sizeDiff = (difference_type)mSize - other.mSize;
    if (sizeDiff > 0)
      erase_last_n(sizeDiff);
    
    // overwrite common data
    const_iterator otherIt = other.cbegin();
    iterator end_ = end();
    for (iterator it = begin(); it != end_; ++it, ++otherIt)
      *it = *otherIt;
    
    // add missing data
    if (sizeDiff < 0)
      push_back_n(-sizeDiff, otherIt);
    
    return *this;
  }
  
  sparque& operator=(sparque&& other) noexcept(
      std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
      && std::is_nothrow_move_assignable<Allocator>::value)
  {
    move_assign(other);
    return *this;
  }
  
  sparque& operator=(std::initializer_list<T> ilist)
  {
    assign_impl(ilist.begin(), ilist.end());
    return *this;
  }
  
  void assign(size_type count, const T& value)
  {
    assert(count <= max_size());
    
    // erase tail data
    difference_type sizeDiff = (difference_type)mSize - count;
    if (sizeDiff > 0)
      erase_last_n(sizeDiff);
    
    // overwrite common data
    iterator end_ = end();
    for (iterator it = begin(); it != end_; ++it)
      *it = value;
    
    // add missing data
    if (sizeDiff < 0)
      push_back_n(-sizeDiff, value);
  }
  
  template <class InputIt,
            typename = typename std::iterator_traits<InputIt>::iterator_category>
  void assign(InputIt first, InputIt last)
  {
    assign_impl(first, last);
  }
  
  void assign(std::initializer_list<T> ilist)
  {
    assign_impl(ilist.begin(), ilist.end());
  }
  
  //
  // Allocator
  //
  allocator_type get_allocator() const noexcept { return chunk_allocator(); }
  
  //
  // Capacity
  //
  bool empty() const noexcept { return mSize == 0u; }
  
  size_type size() const noexcept { return mSize; }
  
  size_type max_size() const noexcept { return std::numeric_limits<difference_type>::max(); }
  
  //
  // Non-standard
  //
  uint32_t height() const noexcept { return mHeight; }
  
  uint32_t node_count() const noexcept { return mNodes.size(); }
  
  uint32_t leaf_count() const noexcept { return mLeafs.size(); }
  
  // Note: complexity is O(n)
  size_type count_chunks() const noexcept
  {
    size_type count = 0u;
    uint32_t leafIdx = mLeafs.first();
    while (leafIdx != InvalidIndex)
    {
      const Leaf& leaf = mLeafs[leafIdx];
      count += leaf.size;
      leafIdx = leaf.next;
    }
    return count;
  }
  
  //
  // Iterators
  //
  iterator begin() noexcept { return iterator::begin(this); }
  const_iterator begin() const noexcept { return const_iterator::begin(this); }
  const_iterator cbegin() const noexcept { return const_iterator::begin(this); }
  
  iterator end() noexcept { return iterator::endin(this); }
  const_iterator end() const noexcept { return const_iterator::endin(this); }
  const_iterator cend() const noexcept { return const_iterator::endin(this); }
  
  reverse_iterator rbegin() noexcept { return reverse_iterator(iterator::endin(this)); }
  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(const_iterator::endin(this)); }
  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(const_iterator::endin(this)); }
  
  reverse_iterator rend() noexcept { return reverse_iterator(iterator::begin(this)); }
  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(const_iterator::begin(this)); }
  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(const_iterator::begin(this)); }
  
  const_iterator nth(size_type pos) const noexcept
  {
    assert(pos <= size());
    if (pos >= size())
      return const_iterator::endin(this);
    
    const size_type pos_ = pos;
    const uint32_t first = mNodes.root();
    uint32_t index = first != InvalidIndex ? first : mLeafs.first();
    const uint32_t height = mHeight;
    for (uint32_t h = 1u; h < height; ++h)
    {
      const Node& node = mNodes[index];
      uint32_t childIdx = 0u;
      size_type size = node.counts[childIdx];
      while (pos >= size)
      {
        pos -= size;
        ++childIdx;
        assert(childIdx < NodeSize);
        size = node.counts[childIdx];
      }
      index = node.children[childIdx];
    }
    const Leaf& leaf = mLeafs[index];
    uint32_t childIdx = 0u;
    size_type size = leaf.spans[childIdx].size();
    while (pos >= size)
    {
      pos -= size;
      ++childIdx;
      assert(childIdx < NodeSize);
      size = leaf.spans[childIdx].size();
    }
    assert(leaf.chunks[childIdx] != nullptr);
    
    return const_iterator(this, pos_, leaf, index, childIdx, leaf.spans[childIdx].off + (uint32_t)pos);
  }
  iterator nth(size_type pos) noexcept
  {
    return iterator(static_cast<const sparque*>(this)->nth(pos));
  }
  
  //
  // Element access
  //
  const_reference operator[](size_type pos) const noexcept
  {
    assert(pos < size());
    const uint32_t first = mNodes.root();
    uint32_t index = first != InvalidIndex ? first : mLeafs.first();
    const uint32_t height = mHeight;
    for (uint32_t h = 1u; h < height; ++h)
    {
      const Node& node = mNodes[index];
      uint32_t childIdx = 0u;
      size_type size = node.counts[childIdx];
      while (pos >= size)
      {
        pos -= size;
        ++childIdx;
        assert(childIdx < NodeSize);
        size = node.counts[childIdx];
      }
      index = node.children[childIdx];
    }
    const Leaf& leaf = mLeafs[index];
    uint32_t childIdx = 0u;
    size_type size = leaf.spans[childIdx].size();
    while (pos >= size)
    {
      pos -= size;
      ++childIdx;
      assert(childIdx < NodeSize);
      size = leaf.spans[childIdx].size();
    }
    assert(leaf.chunks[childIdx] != nullptr);
    
    return leaf.chunks[childIdx][leaf.spans[childIdx].off + pos];
  }
  reference operator[](size_type pos) noexcept
  {
    return const_cast<reference>(
          static_cast<const sparque*>(this)->operator[](pos));
  }
  
  const_reference at(size_type pos) const
  {
    if (pos >= size())
      throw std::out_of_range("sparque::at");
    return operator[](pos);
  }
  reference at(size_type pos)
  {
    if (pos >= size())
      throw std::out_of_range("sparque::at");
    return operator[](pos);
  }
  
  const_reference back() const
  {
    assert(!empty());
    const Leaf& leaf = mLeafs[mLastLeaf];
    const int last = leaf.size - 1;
    const Span& span = leaf.spans[last];
    return leaf.chunks[last][span.end - 1];
  }
  reference back()
  {
    return const_cast<reference>(
        static_cast<const sparque*>(this)->back());
  }
  
  const_reference front() const
  {
    assert(!empty());
    const Leaf& leaf = mLeafs[mLeafs.first()];
    const Span& span = leaf.spans[0];
    return leaf.chunks[0][span.off];
  }
  reference front()
  {
    return const_cast<reference>(
        static_cast<const sparque*>(this)->front());
  }
  
  //
  // Modifiers
  //
  void clear() noexcept
  {
    mLeafs.clear();
    mNodes.clear();
    mSize = 0u;
    mHeight = 0u;
    mLastLeaf = InvalidIndex;
  }
  
  void push_back(const T& value)
  {
    uint32_t last = 0u;
    if (!empty())
    {
      // check last leaf
      assert(mLastLeaf != InvalidIndex);
      Leaf& leaf = mLeafs[mLastLeaf];
      last = leaf.last();
      if (leaf.spans[last].room_right())  // check last chunk
      {
        push_back(leaf, value);
        update_counts_plus(leaf.parent, leaf.pos);
        return;
      }
      // check leaf full
      ++last;
      if (last == NodeSize)
        last = 0u;
    }
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    if (last == 0u)
    {
      // only after fallible 'new'
      push_back_leaf();
    }
    // only after fallible 'push_back_leaf'
    ::new (static_cast<void*>(newStorage.get())) T(value);
    
    assert(mLastLeaf != InvalidIndex);
    Leaf& leaf = mLeafs[mLastLeaf];
    leaf.emplace_at(last, 0u, 1u, newStorage.release());
    ++leaf.size;
    update_counts_plus(leaf.parent, leaf.pos);
  }
  
  void push_back(T&& value)
  {
    uint32_t last = 0u;
    if (!empty())
    {
      // check last leaf
      assert(mLastLeaf != InvalidIndex);
      Leaf& leaf = mLeafs[mLastLeaf];
      last = leaf.last();
      if (leaf.spans[last].room_right())  // check last chunk
      {
        push_back(leaf, std::forward<T>(value));
        update_counts_plus(leaf.parent, leaf.pos);
        return;
      }
      // check leaf full
      ++last;
      if (last == NodeSize)
        last = 0u;
    }
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    if (last == 0u)
    {
      // only after fallible 'new'
      push_back_leaf();
    }
    // only after fallible 'push_back_leaf'
    ::new (static_cast<void*>(newStorage.get())) T(std::forward<T>(value));
    
    assert(mLastLeaf != InvalidIndex);
    Leaf& leaf = mLeafs[mLastLeaf];
    leaf.emplace_at(last, 0u, 1u, newStorage.release());
    ++leaf.size;
    update_counts_plus(leaf.parent, leaf.pos);
  }
  
  template <class... Args>
  void emplace_back(Args&&... args)
  {
    uint32_t last = 0u;
    if (!empty())
    {
      // check last leaf
      assert(mLastLeaf != InvalidIndex);
      Leaf& leaf = mLeafs[mLastLeaf];
      last = leaf.last();
      if (leaf.spans[last].room_right())  // check last chunk
      {
        emplace_back(leaf, std::forward<Args>(args)...);
        update_counts_plus(leaf.parent, leaf.pos);
        return;
      }
      // check leaf full
      ++last;
      if (last == NodeSize)
        last = 0u;
    }
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    if (last == 0u)
    {
      // only after fallible 'new'
      push_back_leaf();
    }
    // only after fallible 'push_back_leaf'
    ::new (static_cast<void*>(newStorage.get())) T(std::forward<Args>(args)...);
    
    assert(mLastLeaf != InvalidIndex);
    Leaf& leaf = mLeafs[mLastLeaf];
    leaf.emplace_at(last, 0u, 1u, newStorage.release());
    ++leaf.size;
    update_counts_plus(leaf.parent, leaf.pos);
  }
  
  void push_front(const T& value)
  {
    bool newLeaf = true;
    if (!empty())
    {
      // check first leaf
      assert(mLeafs.first() != InvalidIndex);
      Leaf& leaf = mLeafs[mLeafs.first()];
      if (leaf.spans[0].room_left())  // check first chunk
      {
        push_front(leaf, value);
        update_counts_plus(leaf.parent, 0u);
        return;
      }
      // check leaf full
      newLeaf = leaf.size == NodeSize;
    }
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    if (newLeaf)
    {
      // only after fallible 'new'
      push_front_leaf();
    }
    else
    {
      Leaf& leaf = mLeafs[mLeafs.first()];
      leaf.shift_right(0);
    }
    // only after fallible 'push_front_leaf'
    ::new (static_cast<void*>(newStorage.get() + ChunkSize - 1)) T(value);
    
    assert(mLeafs.first() != InvalidIndex);
    Leaf& leaf = mLeafs[mLeafs.first()];
    leaf.emplace_at(0u, ChunkSize - 1, ChunkSize, newStorage.release());
    leaf.size += newLeaf ? 1u : 0u;
    update_counts_plus(leaf.parent, 0u);
  }
  
  void push_front(T&& value)
  {
    bool newLeaf = true;
    if (!empty())
    {
      // check first leaf
      assert(mLeafs.first() != InvalidIndex);
      Leaf& leaf = mLeafs[mLeafs.first()];
      if (leaf.spans[0].room_left())  // check first chunk
      {
        push_front(leaf, std::forward<T>(value));
        update_counts_plus(leaf.parent, 0u);
        return;
      }
      // check leaf full
      newLeaf = leaf.size == NodeSize;
    }
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    if (newLeaf)
    {
      // only after fallible 'new'
      push_front_leaf();
    }
    else
    {
      Leaf& leaf = mLeafs[mLeafs.first()];
      leaf.shift_right(0);
    }
    // only after fallible 'push_front_leaf'
    ::new (static_cast<void*>(newStorage.get() + ChunkSize - 1)) T(std::forward<T>(value));
    
    assert(mLeafs.first() != InvalidIndex);
    Leaf& leaf = mLeafs[mLeafs.first()];
    leaf.emplace_at(0u, ChunkSize - 1, ChunkSize, newStorage.release());
    leaf.size += newLeaf ? 1u : 0u;
    update_counts_plus(leaf.parent, 0u);
  }
  
  template<class... Args>
  void emplace_front(Args&&... args)
  {
    bool newLeaf = true;
    if (!empty())
    {
      // check first leaf
      assert(mLeafs.first() != InvalidIndex);
      Leaf& leaf = mLeafs[mLeafs.first()];
      if (leaf.spans[0].room_left())  // check first chunk
      {
        emplace_front(leaf, std::forward<Args>(args)...);
        update_counts_plus(leaf.parent, 0u);
        return;
      }
      // check leaf full
      newLeaf = leaf.size == NodeSize;
    }
    // new chunk
    auto newStorage = std::unique_ptr<T, Deleter>(chunk_allocator().alloc(), Deleter(chunk_allocator()));
    
    if (newLeaf)
    {
      // only after fallible 'new'
      push_front_leaf();
    }
    else
    {
      Leaf& leaf = mLeafs[mLeafs.first()];
      leaf.shift_right(0);
    }
    // only after fallible 'push_front_leaf'
    ::new (static_cast<void*>(newStorage.get() + ChunkSize - 1)) T(std::forward<Args>(args)...);
    
    assert(mLeafs.first() != InvalidIndex);
    Leaf& leaf = mLeafs[mLeafs.first()];
    leaf.emplace_at(0u, ChunkSize - 1, ChunkSize, newStorage.release());
    leaf.size += newLeaf ? 1u : 0u;
    update_counts_plus(leaf.parent, 0u);
  }
  
  void pop_back()
  {
    assert(!empty());
    assert(mLastLeaf != InvalidIndex);
    
    Leaf& leaf = mLeafs[mLastLeaf];
    const uint32_t last = leaf.last();
    Span& span = leaf.spans[last];
    T* chunk = leaf.chunks[last];
    if (span.size() > 1u)
    {
      chunk[span.end - 1u].~T();
      --span.end;
      
      update_counts_minus(leaf.parent, leaf.pos);
    }
    else  // empty chunk
    {
      assert(span.size() > 0u);
      chunk[span.off].~T();
      --span.end;
      chunk_allocator().dealloc(chunk);
    #ifndef NDEBUG
      leaf.chunks[last] = nullptr;
    #endif
      
      --leaf.size;
      update_counts_minus(leaf.parent, leaf.pos);
      
      if (leaf.size == 0u)  // empty leaf
        erase_leaf(leaf, mLastLeaf, false);
    }
    SANITY_CHECK_SQ;
  }
  
  void pop_front()
  {
    assert(!empty());
    assert(mLeafs.first() != InvalidIndex);
    
    const uint32_t first = mLeafs.first();
    Leaf& leaf = mLeafs[first];
    Span& span = leaf.spans[0];
    T* chunk = leaf.chunks[0];
    if (span.size() > 1u)
    {
      chunk[span.off].~T();
      ++span.off;
      
      update_counts_minus(leaf.parent, leaf.pos);
    }
    else  // empty chunk
    {
      assert(span.size() > 0u);
      chunk[span.off].~T();
      chunk_allocator().dealloc(chunk);
      
      --leaf.size;
      std::memmove( leaf.spans.data(),  leaf.spans.data() + 1, leaf.size * sizeof(Span));
      std::memmove(leaf.chunks.data(), leaf.chunks.data() + 1, leaf.size * sizeof(T*));
      leaf.spans[leaf.size].off = 0u;
      leaf.spans[leaf.size].end = 0u;
    #ifndef NDEBUG
      leaf.chunks[leaf.size] = nullptr;
    #endif
      
      update_counts_minus(leaf.parent, leaf.pos);
      
      if (leaf.size == 0u)  // empty leaf
        erase_leaf(leaf, first, false);
    }
    SANITY_CHECK_SQ;
  }
  
  // Note: the behavior is undefined if value is a reference into *this.
  iterator insert(const_iterator pos, const T& value)
  {
    assert(is_valid(pos));
    if (pos.cur != InvalidIndex) // not end
    {
      assert(pos.nth < mSize);
      uint32_t chunkSize = pos.end - pos.off;
      Leaf& leaf = mLeafs[pos.cur];
      
      if (chunkSize < ChunkSize) // insert in chunk
      {
        Span& span = leaf.spans[pos.index];
        iterator res = pos;
        
        if (pos.pos > pos.off || pos.off == 0u)
        {
          assert(pos.pos >= pos.off);
          bool shiftRight = pos.end < ChunkSize && (pos.off == 0u || (pos.end - pos.pos) <= (pos.pos - pos.off));
          if (shiftRight)
          {
            T* ptrEnd = pos.chunk + pos.end;
            ::new (static_cast<void*>(ptrEnd)) T(std::move(*(ptrEnd - 1)));
            
            ++span.end;
            update_counts_plus(leaf.parent, leaf.pos);
            
            T* ptrPos = pos.chunk + pos.pos;
            std::move_backward(ptrPos, ptrEnd - 1, ptrEnd);
            
            *ptrPos = value;
            
            ++res.end;
          }
          else // shift left
          {
            assert(pos.off > 0u);
            T* ptrOff = pos.chunk + pos.off;
            ::new (static_cast<void*>(ptrOff - 1)) T(std::move(*ptrOff));
            
            --span.off;
            update_counts_plus(leaf.parent, leaf.pos);
            
            T* ptrPos = pos.chunk + pos.pos;
            std::move(ptrOff + 1, ptrPos, ptrOff);
            
            *(ptrPos - 1) = value;
            
            --res.off;
            --res.pos;
          }
        }
        else // new first
        {
          assert(pos.off > 0u);
          assert(pos.pos == pos.off);
          T* ptrOff = pos.chunk + pos.off;
          ::new (static_cast<void*>(ptrOff - 1)) T(value);
          
          --span.off;
          update_counts_plus(leaf.parent, leaf.pos);
          
          --res.off;
          --res.pos;
        }
        return res;
      }
      else // chunk full
      {
        return insert_in_full_chunk(leaf, pos, value);
      }
    }
    else // end
    {
      assert(pos.nth >= mSize);
      return insert_at_end(value);
    }
  }
  
  // Note: the behavior is undefined if value is a reference into *this.
  iterator insert(const_iterator pos, T&& value)
  {
    assert(is_valid(pos));
    if (pos.cur != InvalidIndex) // not end
    {
      assert(pos.nth < mSize);
      uint32_t chunkSize = pos.end - pos.off;
      Leaf& leaf = mLeafs[pos.cur];
      
      if (chunkSize < ChunkSize) // insert in chunk
      {
        Span& span = leaf.spans[pos.index];
        iterator res = pos;
        
        if (pos.pos > pos.off || pos.off == 0u)
        {
          assert(pos.pos >= pos.off);
          bool shiftRight = pos.end < ChunkSize && (pos.off == 0u || (pos.end - pos.pos) <= (pos.pos - pos.off));
          if (shiftRight)
          {
            T* ptrEnd = pos.chunk + pos.end;
            ::new (static_cast<void*>(ptrEnd)) T(std::move(*(ptrEnd - 1)));
            
            ++span.end;
            update_counts_plus(leaf.parent, leaf.pos);
            
            T* ptrPos = pos.chunk + pos.pos;
            std::move_backward(ptrPos, ptrEnd - 1, ptrEnd);
            
            *ptrPos = std::forward<T>(value);
            
            ++res.end;
          }
          else // shift left
          {
            assert(pos.off > 0u);
            T* ptrOff = pos.chunk + pos.off;
            ::new (static_cast<void*>(ptrOff - 1)) T(std::move(*ptrOff));
            
            --span.off;
            update_counts_plus(leaf.parent, leaf.pos);
            
            T* ptrPos = pos.chunk + pos.pos;
            std::move(ptrOff + 1, ptrPos, ptrOff);
            
            *(ptrPos - 1) = std::forward<T>(value);
            
            --res.off;
            --res.pos;
          }
        }
        else // new first
        {
          assert(pos.off > 0u);
          assert(pos.pos == pos.off);
          T* ptrOff = pos.chunk + pos.off;
          ::new (static_cast<void*>(ptrOff - 1)) T(std::forward<T>(value));
          
          --span.off;
          update_counts_plus(leaf.parent, leaf.pos);
          
          --res.off;
          --res.pos;
        }
        return res;
      }
      else // chunk full
      {
        return insert_in_full_chunk(leaf, pos, std::forward<T>(value));
      }
    }
    else // end
    {
      assert(pos.nth >= mSize);
      return insert_at_end(std::forward<T>(value));
    }
  }
  
  template <class... Args>
  iterator emplace(const_iterator pos, Args&&... args)
  {
    assert(is_valid(pos));
    if (pos.cur != InvalidIndex) // not end
    {
      assert(pos.nth < mSize);
      uint32_t chunkSize = pos.end - pos.off;
      Leaf& leaf = mLeafs[pos.cur];
      
      if (chunkSize < ChunkSize) // insert in chunk
      {
        Span& span = leaf.spans[pos.index];
        iterator res = pos;
        
        if (pos.pos > pos.off || pos.off == 0u)
        {
          assert(pos.pos >= pos.off);
          bool shiftRight = pos.end < ChunkSize && (pos.off == 0u || (pos.end - pos.pos) <= (pos.pos - pos.off));
          if (shiftRight)
          {
            T* ptrEnd = pos.chunk + pos.end;
            ::new (static_cast<void*>(ptrEnd)) T(std::move(*(ptrEnd - 1)));
            
            ++span.end;
            update_counts_plus(leaf.parent, leaf.pos);
            
            T* ptrPos = pos.chunk + pos.pos;
            std::move_backward(ptrPos, ptrEnd - 1, ptrEnd);
            
            *ptrPos = std::move(T(std::forward<Args>(args)...));
            
            ++res.end;
          }
          else // shift left
          {
            assert(pos.off > 0u);
            T* ptrOff = pos.chunk + pos.off;
            ::new (static_cast<void*>(ptrOff - 1)) T(std::move(*ptrOff));
            
            --span.off;
            update_counts_plus(leaf.parent, leaf.pos);
            
            T* ptrPos = pos.chunk + pos.pos;
            std::move(ptrOff + 1, ptrPos, ptrOff);
            
            *(ptrPos - 1) = std::move(T(std::forward<Args>(args)...));
            
            --res.off;
            --res.pos;
          }
        }
        else // new first
        {
          assert(pos.off > 0u);
          assert(pos.pos == pos.off);
          T* ptrOff = pos.chunk + pos.off;
          ::new (static_cast<void*>(ptrOff - 1)) T(std::forward<Args>(args)...);
          
          --span.off;
          update_counts_plus(leaf.parent, leaf.pos);
          
          --res.off;
          --res.pos;
        }
        return res;
      }
      else // chunk full
      {
        return emplace_in_full_chunk(leaf, pos, std::forward<Args>(args)...);
      }
    }
    else // end
    {
      assert(pos.nth >= mSize);
      return emplace_at_end(std::forward<Args>(args)...);
    }
  }
  
  iterator erase(const_iterator pos)
  {
    assert(is_dereferenceable(pos));
    Leaf& leaf = mLeafs[pos.cur];
    uint32_t newChunkSize = pos.end - 1u - pos.off;
    
    if (newChunkSize >= HalfChunk) // erase in chunk
    {
      iterator res;
      uint32_t nextIndex = pos.index + 1u;
      Span& span = leaf.spans[pos.index];
      bool shifted = erase_shift(pos.chunk, pos.pos, span);
      uint32_t nextPos = pos.pos + (uint32_t)shifted;
      DEBUG_EXPR(++dbg.c1);
      
      update_counts_minus(leaf.parent, leaf.pos);
      SANITY_CHECK_SQ;
      
      if (nextPos < span.end) // same chunk
      {
        res = pos;
        res.pos = nextPos;
        res.off = span.off;
        res.end = span.end;
      }
      else if (nextIndex < pos.size) // next chunk
      {
        res = iterator(this, pos.nth, leaf, pos.cur, nextIndex, leaf.spans[nextIndex].off);
      }
      else if (leaf.next != InvalidIndex) // next leaf
      {
        const Leaf& nextLeaf = mLeafs[leaf.next];
        res = iterator(this, pos.nth, nextLeaf, leaf.next, 0u, nextLeaf.spans[0].off);
      }
      else // end
      {
        res = iterator::endin(this);
      }
      return res;
    }
    else if (newChunkSize > 0u) // balance chunk
    {
      return erase_value_balance_chunk(leaf, newChunkSize, pos);
    }
    else // empty chunk
    {
      return erase_value_erase_chunk(leaf, pos);
    }
  }
  
  //TODO: ranged insert/erase
  
  void resize(size_type count, const T& value = T())
  {
    if (count < mSize)
      erase_last_n(mSize - count);
    else if (count > mSize)
      push_back_n(count - mSize, value);
  }
  
  void swap(sparque& other) noexcept(
      !std::allocator_traits<Allocator>::propagate_on_container_swap::value
      || detail::is_nothrow_swappable<allocator_type>::value)
  {
    mLeafs.swap(other.mLeafs);
    mNodes.swap(other.mNodes);
    
    std::swap(mSize, other.mSize);
    std::swap(mLastLeaf, other.mLastLeaf);
    std::swap(mHeight, other.mHeight);
  }
  
#if defined(INDIVI_SQ_DEBUG) && !defined(NDEBUG)
  std::string toString(const std::string& prefix = "", bool nodes = false) const
  {
    if (empty())
      return prefix + "\n";
    
    std::ostringstream oss;
    std::deque<uint32_t> dq;
    uint32_t curChildren = 0u;
    uint32_t nxtChildren = 0u;
    
    if (nodes && !mNodes.empty())
    {
      oss << prefix;
      
      uint32_t root = mNodes.root();
      const Node& rootNode = mNodes[root];
      
      uint32_t sz = rootNode.size();
      for (uint32_t i = 0u; i < sz; ++i)
      {
        oss << rootNode.counts[i] << (i+1u < sz ? "," : "");
        dq.push_back(rootNode.children[i]);
      }
      oss << "\n" << prefix;
      curChildren = (uint32_t)dq.size();
      
      bool hasLeafs = rootNode.has_leafs();
      while (!hasLeafs)
      {
        for (uint32_t i = 0u; i < curChildren; ++i)
        {
          uint32_t index = dq.front();
          dq.pop_front();
          if (index == InvalidIndex)
          {
            oss.seekp(-1, std::ios_base::end);
            oss << "| ";
            continue;
          }
          
          const Node& node = mNodes[index];
          if (node.has_leafs())
            hasLeafs = true;
          
          sz = node.size();
          for (uint32_t j = 0u; j < sz; ++j)
          {
            oss << node.counts[j] << (j+1u < sz ? "," : "");
            dq.push_back(node.children[j]);
            ++nxtChildren;
          }
          
          if (i+1u < curChildren)
          {
            oss << " | ";
            dq.push_back(InvalidIndex);
            ++nxtChildren;
          }
        }
        oss << "\n" << prefix;
        curChildren = nxtChildren;
        nxtChildren = 0u;
      }
      
      // leafs
      for (uint32_t index : dq)
      {
        if (index == InvalidIndex)
        {
          oss.seekp(-1, std::ios_base::end);
          oss << "| ";
          continue;
        }
        
        const Leaf& leaf = mLeafs[index];
        const uint32_t sz = leaf.size;
        for (uint32_t i = 0u; i < sz; ++i)
        {
          oss << leaf.spans[i].size() << (i+1u < sz ? "," : " | ");
        }
      }
    }
    else // leafs only
    {
      oss << prefix;
      
      uint32_t index = mLeafs.first();
      while (index != InvalidIndex)
      {
        const Leaf& leaf = mLeafs[index];
        const uint32_t sz = leaf.size;
        for (uint32_t i = 0u; i < sz; ++i)
        {
          oss << leaf.spans[i].size() << (i+1u < sz ? "," : " | ");
        }
        
        index = leaf.next;
      }
    }
    
    oss.seekp(-2, std::ios_base::end);
    oss << " \n";
    
    return oss.str();
  }
#endif

private:
  //
  // Static functions
  //
  static double log_node(double x)
  {
    static const double log_base = std::log(NodeSize);
    return std::log(x) / log_base;
  }
  
  static uint32_t div_ceil_node(uint32_t numerator)
  {
    // same as ceil((float)numerator / NodeSize)
    return (numerator + NodeSize - 1u) / NodeSize;
  }
  
  static uint32_t div_ceil_chunk(uint32_t numerator)
  {
    // same as ceil((float)numerator / ChunkSize)
    return (numerator + ChunkSize - 1u) / ChunkSize;
  }
  
  static uint32_t count_nodes(uint32_t leafs, uint32_t height)
  {
    uint32_t perLevel = leafs > 1u ? div_ceil_node(leafs) : 0u;
    uint32_t nodes = perLevel;
    for (int i = 0; i < (int)height - 2; ++i)
    {
      perLevel = div_ceil_node(perLevel);
      nodes += perLevel;
    }
    return nodes;
  }
  
  template <class U>
  static void push_back(Leaf& leaf, U&& value)
  {
    static_assert(std::is_same<U&&, const T&>::value || std::is_same<U&&, T&&>::value, "sparque: wrong template");
    const uint32_t last = leaf.last();
    Span& span = leaf.spans[last];
    assert(span.end < ChunkSize);
    T* chunk = leaf.chunks[last];
    ::new (static_cast<void*>(chunk + span.end)) T(std::forward<U>(value));
    ++span.end;
  }
  
  template <class... Args>
  static void emplace_back(Leaf& leaf, Args&&... args)
  {
    const uint32_t last = leaf.last();
    Span& span = leaf.spans[last];
    assert(span.end < ChunkSize);
    T* chunk = leaf.chunks[last];
    ::new (static_cast<void*>(chunk + span.end)) T(std::forward<Args>(args)...);
    ++span.end;
  }
  
  template <class U>
  static void push_front(Leaf& leaf, U&& value)
  {
    static_assert(std::is_same<U&&, const T&>::value || std::is_same<U&&, T&&>::value, "sparque: wrong template");
    Span& span = leaf.spans[0];
    assert(span.off > 0u);
    T* chunk = leaf.chunks[0];
    ::new (static_cast<void*>(chunk + span.off - 1u)) T(std::forward<U>(value));
    --span.off;
  }
  
  template <class... Args>
  static void emplace_front(Leaf& leaf, Args&&... args)
  {
    Span& span = leaf.spans[0];
    assert(span.off > 0u);
    T* chunk = leaf.chunks[0];
    ::new (static_cast<void*>(chunk + span.off - 1u)) T(std::forward<Args>(args)...);
    --span.off;
  }
  
  static void align_chunk_left(Leaf& leaf, uint32_t index)
  {
    assert(index < NodeSize);
    Span& span = leaf.spans[index];
    assert(span.room_left());
    T* chunk = leaf.chunks[index];
    assert(chunk);
    
    T* prev_off = chunk + span.off;
    T* prev_end = chunk + span.end;
    const uint32_t size = span.size();
    
    // before offset
    uint32_t copy_n = std::min<uint32_t>(span.off, size);
    T* ot = std::uninitialized_copy_n(std::make_move_iterator(prev_off), copy_n, chunk);
    
    // after offset
    T* it = prev_off + copy_n;
    span.off = 0u;
    ot = std::move(it, prev_end, ot);
    
    // destroy remaining
    it = std::max(ot, prev_off);
    destroy_range(it, prev_end);
    span.end = (uint16_t)size;
  }
  
  static void align_chunk_right(Leaf& leaf, uint32_t index)
  {
    Span& span = leaf.spans[index];
    assert(span.room_right());
    T* chunk = leaf.chunks[index];
    assert(chunk);
    
    T* prev_off = chunk + span.off;
    T* prev_end = chunk + span.end;
    const uint32_t size = span.size();
    
    // after end
    uint32_t copy_n = std::min<uint32_t>(ChunkSize - span.end, size);
    std::uninitialized_copy_n(std::make_move_iterator(prev_end - copy_n), copy_n, chunk + ChunkSize - copy_n);
    
    // before end
    span.end = ChunkSize;
    std::move_backward(prev_off, prev_end - copy_n, chunk + ChunkSize - copy_n);
    
    // destroy remaining
    destroy_range(prev_off, prev_off + copy_n);
    span.off = (uint16_t)(ChunkSize - size);
  }
  
  static bool erase_shift(T* chunk, uint32_t pos, Span& span)
  {
    const bool shiftRight = (pos - span.off) < (span.end - 1u - pos);
    if (shiftRight)
    {
      erase_shift_right(chunk, pos, span.off);
      ++span.off;
    }
    else
    {
      erase_shift_left(chunk, pos, span.end);
      --span.end;
    }
    return shiftRight;
  }
  
  static void erase_shift_right(T* chunk, uint32_t pos, uint32_t offset)
  {
    // increase offset
    T* it = chunk + pos;
    std::move_backward(chunk + offset, it, it + 1);
    (chunk + offset)->~T();
  }
  
  static void erase_shift_left(T* chunk, uint32_t pos, uint32_t end)
  {
    // decrease end
    T* it = chunk + pos;
    std::move(it + 1, chunk + end, it);
    (chunk + end - 1)->~T();
  }
  
  static void merge_erased_chunk_left(Leaf& leaf, uint32_t srcIndex, uint32_t& dstIndex, uint32_t& erasePos)
  {
    assert(dstIndex == srcIndex - 1u);
    
    // Output
    // dstIndex: next value chunk
    // erasePos: next value pos
    Span& srcSpan = leaf.spans[srcIndex];
    Span& dstSpan = leaf.spans[dstIndex];
    uint32_t srcChunkSize = srcSpan.size() - 1u; // minus erased
    
    // merge src at dst end
    if (dstSpan.end + srcChunkSize > ChunkSize)
      align_chunk_left(leaf, dstIndex);
    
    // move data
    uint32_t dstSpanEnd = dstSpan.end;
    uint32_t copyBefore = erasePos - srcSpan.off;
    T* src = leaf.chunks[srcIndex];
    T* dst = leaf.chunks[dstIndex] + dstSpanEnd;
    
    std::uninitialized_copy_n(std::make_move_iterator(src + srcSpan.off), copyBefore, dst);
    dstSpan.end += (uint16_t)copyBefore;
    
    uint32_t copyAfter = srcSpan.end - 1u - erasePos;
    std::uninitialized_copy_n(std::make_move_iterator(src + erasePos + 1), copyAfter, dst + copyBefore);
    dstSpan.end += (uint16_t)copyAfter;
    
    // output
    if (copyAfter > 0u)
    {
      erasePos = dstSpanEnd + copyBefore;
    }
    else
    {
      dstIndex = srcIndex;
      erasePos = (srcIndex + 1u < leaf.size) ? leaf.spans[srcIndex + 1u].off
                                             : std::numeric_limits<uint32_t>::max();
    }
  }
  
  static void merge_erased_chunk_right(Leaf& leaf, uint32_t srcIndex, uint32_t dstIndex, uint32_t& erasePos)
  {
    assert(dstIndex == srcIndex + 1u);
    
    // Output
    // erasePos: next value pos
    Span& srcSpan = leaf.spans[srcIndex];
    Span& dstSpan = leaf.spans[dstIndex];
    uint32_t srcChunkSize = srcSpan.size() - 1u; // minus erased
    
    if (dstSpan.off < srcChunkSize)
      align_chunk_right(leaf, dstIndex);
    
    // move data
    uint32_t copyAfter = srcSpan.end - 1u - erasePos;
    T* src = leaf.chunks[srcIndex];
    T* dst = leaf.chunks[dstIndex] + dstSpan.off - copyAfter;
    
    std::uninitialized_copy_n(std::make_move_iterator(src + erasePos + 1), copyAfter, dst);
    dstSpan.off -= (uint16_t)copyAfter;
    
    uint32_t copyBefore = erasePos - srcSpan.off;
    std::uninitialized_copy_n(std::make_move_iterator(src + srcSpan.off), copyBefore, dst - copyBefore);
    dstSpan.off -= (uint16_t)copyBefore;
    
    // output
    erasePos = dstSpan.off + copyBefore;
  }
  
  static uint32_t steal_from_right(Leaf& leaf, uint32_t dstIndex, uint32_t count)
  {
    assert(count > 0u);
    uint32_t srcIndex = dstIndex + 1u;
    assert(srcIndex < leaf.size);
    Span& srcSpan = leaf.spans[srcIndex];
    Span& dstSpan = leaf.spans[dstIndex];
    assert(srcSpan.size() > count);
    
    uint32_t shift = 0u;
    if (dstSpan.end + count > ChunkSize)
    {
      shift = dstSpan.off;
      align_chunk_left(leaf, dstIndex);
      assert(dstSpan.end + count <= ChunkSize);
    }
    
    // move data
    T* src = leaf.chunks[srcIndex] + srcSpan.off;
    T* dst = leaf.chunks[dstIndex] + dstSpan.end;
    std::uninitialized_copy_n(std::make_move_iterator(src), count, dst);
    dstSpan.end += (uint16_t)count;
    destroy_range(src, src + count);
    srcSpan.off += (uint16_t)count;
    
    return shift;
  }
  
  static uint32_t steal_from_left(Leaf& leaf, uint32_t dstIndex, uint32_t count)
  {
    assert(count > 0u);
    assert(dstIndex > 0u);
    uint32_t srcIndex = dstIndex - 1u;
    Span& srcSpan = leaf.spans[srcIndex];
    Span& dstSpan = leaf.spans[dstIndex];
    assert(srcSpan.size() > count);
    
    uint32_t shift = 0u;
    if (dstSpan.off < count)
    {
      shift = ChunkSize - dstSpan.end;
      align_chunk_right(leaf, dstIndex);
      assert(dstSpan.off >= count);
    }
    
    // move data
    T* src = leaf.chunks[srcIndex] + srcSpan.end - count;
    T* dst = leaf.chunks[dstIndex] + dstSpan.off - count;
    std::uninitialized_copy_n(std::make_move_iterator(src), count, dst);
    dstSpan.off -= (uint16_t)count;
    destroy_range(src, src + count);
    srcSpan.end -= (uint16_t)count;
    
    return shift;
  }
  
  static void destroy_range(T* first, const T* end) noexcept
  {
    assert(first <= end);
    while (first < end)
      (first++)->~T();
  }
  
  //
  // Non-member functions
  //
  template <uint16_t RN, uint16_t RB>
  friend bool operator==(const sparque& lhs, const sparque<T, RN, RB>& rhs) noexcept
  {
    return lhs.size() == rhs.size()
           && std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin());
  }
  
  template <uint16_t RN, uint16_t RB>
  friend bool operator!=(const sparque& lhs, const sparque<T, RN, RB>& rhs) noexcept
  {
    return !(lhs == rhs);
  }
  
  template <uint16_t RN, uint16_t RB>
  friend bool operator<(const sparque& lhs, const sparque<T, RN, RB>& rhs) noexcept
  {
    return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
  }
  
  template <uint16_t RN, uint16_t RB>
  friend bool operator<=(const sparque& lhs, const sparque<T, RN, RB>& rhs) noexcept
  {
    return !(rhs < lhs);
  }
  
  template <uint16_t RN, uint16_t RB>
  friend bool operator>(const sparque& lhs, const sparque<T, RN, RB>& rhs) noexcept
  {
    return rhs < lhs;
  }
  
  template <uint16_t RN, uint16_t RB>
  friend bool operator>=(const sparque& lhs, const sparque<T, RN, RB>& rhs) noexcept
  {
    return !(lhs < rhs);
  }
  
  friend void swap(sparque& lhs, sparque& rhs) noexcept(noexcept(lhs.swap(rhs)))
  {
    lhs.swap(rhs);
  }
  
  //
  // Debug functions
  //
#ifndef NDEBUG
  bool is_valid(const const_iterator& it) const noexcept
  {
    assert(it.sparq == this);
    assert(it.nth <= mSize);
    bool isEnd = it.nth == mSize;
    if (!isEnd)
    {
      assert(it.chunk != nullptr);
      assert(it.cur != InvalidIndex);
      assert(it.cur < mLeafs.capacity());
      const Leaf& leaf = mLeafs[it.cur];
      assert(it.prev == leaf.prev);
      assert(it.next == leaf.next);
      assert(it.index < leaf.size);
      assert(it.size == leaf.size);
      assert(it.chunk == leaf.chunks[it.index]);
      const Span& span = leaf.spans[it.index];
      assert(it.off == span.off);
      assert(it.end == span.end);
      assert(it.pos >= span.off);
      assert(it.pos < span.end);
    }
    else
    {
      assert(it == const_iterator::endin(this));
    }
    return true;
  }
  
  bool is_dereferenceable(const const_iterator& it) const noexcept
  {
    assert(it.nth < mSize);
    assert(is_valid(it));
    return true;
  }
#endif

#if defined(INDIVI_SQ_DEBUG) && !defined(NDEBUG)
  void check_leafs(uint32_t idx, uint32_t& leafCount, size_type expectedSize,
                   uint32_t& expectedPrev, uint32_t& expectedNext, std::unordered_set<uint32_t>& usedLeafs,
                   const std::unordered_set<uint32_t>& freeLeafs, uint32_t maxLeafIdx) const noexcept
  {
    assert(mLeafs.size() == 1u);
    check_leaf(idx, InvalidIndex, 0u, leafCount, expectedSize, expectedPrev, expectedNext, usedLeafs, freeLeafs, maxLeafIdx);
  }
  
  void check_leaf(uint32_t idx, uint32_t parent, uint16_t pos, uint32_t& leafCount,
                  size_type expectedSize, uint32_t& expectedPrev, uint32_t& expectedNext,
                  std::unordered_set<uint32_t>& usedLeafs, const std::unordered_set<uint32_t>& freeLeafs,
                  uint32_t maxLeafIdx) const noexcept
  {
    const Leaf& leaf = mLeafs[idx];
    ++leafCount;
    auto pair = usedLeafs.insert(idx);
    assert(pair.second && "leafs list cycle");
    assert(idx < maxLeafIdx);
    assert(!leaf.spans[0].empty());
    assert(leaf.parent == parent);
    assert(leaf.pos == pos);
    assert(freeLeafs.count(idx) == 0u);
    assert(leaf.prev == expectedPrev);
    assert(idx == expectedNext);
    expectedPrev = idx;
    expectedNext = leaf.next;
    
    bool wasZero = false;
    size_type totalSize = 0u;
    for (uint32_t i = 0; i < NodeSize; ++i)
    {
      size_type size = leaf.spans[i].size();
      totalSize += size;
      assert(!wasZero || size == 0u);
      wasZero = size == 0;
      assert(wasZero || leaf.chunks[i] != nullptr);
    }
    assert(totalSize == expectedSize);
  }
  
  void check_nodes(uint32_t idx, uint32_t parent, uint16_t pos, uint32_t& nodeCount, uint32_t& leafCount,
                   size_type expectedSize, uint32_t& expectedPrev, uint32_t& expectedNext,
                   std::unordered_set<uint32_t>& usedNodes, std::unordered_set<uint32_t>& usedLeafs,
                   const std::unordered_set<uint32_t>& freeNodes, const std::unordered_set<uint32_t>& freeLeafs,
                   uint32_t maxNodeIdx, uint32_t maxLeafIdx) const noexcept
  {
    const Node& node = mNodes[idx];
    ++nodeCount;
    auto pair = usedNodes.insert(idx);
    assert(pair.second && "nodes list cycle");
    assert(idx < maxNodeIdx);
    assert(node.counts[0] > 0u);
    assert(node.parent == parent);
    assert(node.pos == pos);
    assert(freeNodes.count(idx) == 0u);
    
    bool wasZero = false;
    size_type totalSize = 0u;
    for (uint32_t i = 0; i < NodeSize; ++i)
    {
      size_type size = node.counts[i];
      totalSize += size;
      assert(!wasZero || size == 0u);
      wasZero = size == 0u;
      
      if (size > 0u)
      {
        if (!node.has_leafs())
          check_nodes(node.children[i], idx, (uint16_t)i, nodeCount, leafCount,
                      node.counts[i], expectedPrev, expectedNext, usedNodes, usedLeafs,
                      freeNodes, freeLeafs, maxNodeIdx, maxLeafIdx);
        else
          check_leaf(node.children[i], idx, (uint16_t)i, leafCount,
                     node.counts[i], expectedPrev, expectedNext, usedLeafs, freeLeafs, maxLeafIdx);
      }
    }
    assert(totalSize == expectedSize);
  }
  
  std::unordered_set<uint32_t> check_leafs_freelist() const noexcept
  {
    std::unordered_set<uint32_t> freeLeafs;
    
    uint32_t freed = mLeafs.freed();
    while (freed != InvalidIndex)
    {
      auto pair = freeLeafs.insert(freed);
      assert(pair.second && "leafs freelist cycle");
      freed = mLeafs[freed].next;
    }
    return freeLeafs;
  }
  
  std::unordered_set<uint32_t> check_nodes_freelist() const noexcept
  {
    std::unordered_set<uint32_t> freeNodes;
    
    uint32_t freed = mNodes.freed();
    while (freed != InvalidIndex)
    {
      auto pair = freeNodes.insert(freed);
      assert(pair.second && "nodes freelist cycle");
      freed = mNodes[freed].parent;
    }
    return freeNodes;
  }
  
  void sanity_check() const noexcept
  {
    uint32_t expectedPrev = InvalidIndex;
    uint32_t expectedNext = mLeafs.first();
    uint32_t nodeCount = 0u;
    uint32_t leafCount = 0u;
    
    std::unordered_set<uint32_t> usedNodes;
    std::unordered_set<uint32_t> usedLeafs;
    std::unordered_set<uint32_t> freeNodes = check_nodes_freelist();
    std::unordered_set<uint32_t> freeLeafs = check_leafs_freelist();
    uint32_t maxNodeIdx = (uint32_t)(mNodes.size() + freeNodes.size());
    uint32_t maxLeafIdx = (uint32_t)(mLeafs.size() + freeLeafs.size());
    assert(maxNodeIdx <= mNodes.capacity());
    assert(maxLeafIdx <= mLeafs.capacity());
    
    if (!mNodes.empty())
      check_nodes(mNodes.root(), InvalidIndex, 0u, nodeCount, leafCount,
                  mSize, expectedPrev, expectedNext, usedNodes, usedLeafs, freeNodes, freeLeafs, maxNodeIdx, maxLeafIdx);
    else if (!mLeafs.empty())
      check_leafs(mLeafs.first(), leafCount, mSize, expectedPrev, expectedNext, usedLeafs, freeLeafs, maxLeafIdx);
    
    assert(nodeCount == mNodes.size());
    assert(leafCount == mLeafs.size());
    for (uint32_t i = 0u; i < usedNodes.size() + freeNodes.size(); ++i)
      assert(usedNodes.count(i) != 0u || freeNodes.count(i) != 0u);
    for (uint32_t i = 0u; i < usedLeafs.size() + freeLeafs.size(); ++i)
      assert(usedLeafs.count(i) != 0u || freeLeafs.count(i) != 0u);
    assert(expectedPrev == mLastLeaf);
    assert(expectedNext == InvalidIndex);
  }

#endif
}; // end of sparque

} // namespace indivi


#undef SANITY_CHECK_SQ
#undef DEBUG_EXPR
#undef INDIVI_SQ_DEBUG

#endif // INDIVI_SPARQUE_H
