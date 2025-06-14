/**
 * Copyright 2021 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

#ifndef INDIVI_DEVECTOR_H
#define INDIVI_DEVECTOR_H

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <cassert>
#include <cmath>

#define INDIVI_DV_DEFAULT_INIT    // Default-construct values, instead of value-construct (avoid memory value-initialization)
#ifdef INDIVI_DV_DEFAULT_INIT
  #define INDIVI_DV_VALUE_INIT value_type
#else
  #define INDIVI_DV_VALUE_INIT value_type()
#endif
#define INDIVI_DV_SHIFT_EMPTY     // Shift offset according to realloc-mode when vector becomes empty
//#define INDIVI_DV_DEBUG

namespace indivi
{
// ShiftMode (only apply to 'push/emplace back/front')
// 'insert' (and 'erase') always use NEAR to minimize shifts (e.g. |_|a|b|c|_| -> |_|a|b|d|c| or |a|d|b|c|_|)
enum class ShiftMode : char {
  NEAR,     // shift data to their closest neighbor   (e.g. push_back: |_|_|_|a| -> |_|_|a|b|)
  CENTER,   // shift data to the center of storage    (e.g. push_back: |_|_|_|a| -> |_|a|b|_|)
  FAR,      // shift data to opposite side of storage (e.g. push_back: |_|_|_|a| -> |a|b|_|_|)
};

enum class ReallocMode : char {
  START,    // reallocate data to the start of storage  (e.g. push_back: |a| -> |a|b|_|_|)
  CENTER,   // reallocate data to the center of storage (e.g. push_back: |a| -> |_|a|b|_|)
  END,      // reallocate data to the end of storage    (e.g. push_back: |a| -> |_|_|a|b|)
};

template <ShiftMode shift_mode = ShiftMode::NEAR,
          ReallocMode realloc_mode = ReallocMode::START,
          unsigned int growth_percent = 200>  // Capacity growth in percent, 200% means capacity * 2 (must be > 100)
struct devector_opt {
  static inline ShiftMode SHIFT_MODE()      { return shift_mode; }
  static inline ReallocMode REALLOC_MODE()  { return realloc_mode; }
  static inline float GROWTH_FACTOR()       { return growth_percent * 0.01f; }
  
  static_assert(growth_percent > 100, "devector_opt: growth_percent must be > 100");
};


template <class T,
          class Options = devector_opt<ShiftMode::NEAR, ReallocMode::START, 200>,
          class Allocator = std::allocator<T> >
class devector
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
  using iterator = value_type*;
  using const_iterator = const value_type*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  
  static_assert(std::is_same<typename Allocator::value_type, value_type>::value,
                "devector: Allocator::value_type must be the same as value_type");

private:
  // Members
  struct impl : Allocator
  {
    impl() : begin(nullptr), offset(nullptr), end(nullptr), end_of_storage(nullptr)
    {}
    
    impl(T* begin_, T* offset_, T* end_, T* end_of_storage_)
      : begin(begin_), offset(offset_), end(end_), end_of_storage(end_of_storage_)
    {}
    
    impl(const impl& other) = delete;
    
    impl(impl&& other) noexcept
      : Allocator(std::forward<Allocator>(other))
      , begin(other.begin), offset(other.offset), end(other.end), end_of_storage(other.end_of_storage)
    {
      other.begin = nullptr;
      other.offset = nullptr;
      other.end = nullptr;
      other.end_of_storage = nullptr;
    }
    
    impl& operator=(impl&& other) noexcept
    {
      for (; offset < end; ++offset)
        offset->~T();
      
      Allocator::deallocate(begin, end_of_storage - begin);
      
      Allocator::operator=(std::forward<Allocator>(other));
      
      begin = other.begin;
      offset = other.offset;
      end = other.end;
      end_of_storage = other.end_of_storage;
      
      other.begin = nullptr;
      other.offset = nullptr;
      other.end = nullptr;
      other.end_of_storage = nullptr;
      
      return *this;
    }
    
    T* alloc(size_type n)
    {
      return Allocator::allocate(n);
    }
    
    void dealloc(T* p, size_type n)
    {
      Allocator::deallocate(p, n);
    }
    
    T*  begin;
    T*  offset;
    T*  end;
    T*  end_of_storage;
  } m_;
  
  // Deleter for std::unique_ptr
  struct deleter
  {
    deleter(impl& allocator_, size_type count_)
      : allocator(allocator_), count(count_)
    {}
    
    void operator()(T* p) const
    {
      allocator.dealloc(p, count);
    }
    impl& allocator;
    size_type count;
  };
  
public:
  //
  // Constructor/Destructor
  //
  devector()
  {}
  
  devector(size_type count, const T& value)
  {
    auto new_storage = std::unique_ptr<T, deleter>(m_.alloc(count), deleter(m_, count));
    auto new_begin = new_storage.get();
    std::uninitialized_fill_n(new_begin, count, value);
    
    m_.begin = new_begin;
    m_.offset = m_.begin;
    m_.end = m_.begin + count;
    m_.end_of_storage = m_.end;
    new_storage.release();
  }
  
  explicit devector(size_type count)
  {
    auto new_storage = std::unique_ptr<T, deleter>(m_.alloc(count), deleter(m_, count));
    auto new_begin = new_storage.get();
    auto new_end = new_begin + count;
    
    iterator it = new_begin;
    try {
      for (; it != new_end; ++it)
        ::new (static_cast<void*>(it)) INDIVI_DV_VALUE_INIT;
    }
    catch (...) {
      destroy_range(new_begin, it);
      throw;
    }
    
    m_.begin = new_begin;
    m_.offset = m_.begin;
    m_.end = new_end;
    m_.end_of_storage = m_.end;
    new_storage.release();
  }
  
  devector(const devector& other)
  {
    size_type size = other.size();
    
    auto new_storage = std::unique_ptr<T, deleter>(m_.alloc(size), deleter(m_, size));
    auto new_begin = new_storage.get();
    std::uninitialized_copy(other.begin(), other.end(), new_begin);
    
    m_.begin = new_begin;
    m_.offset = m_.begin;
    m_.end = m_.begin + size;
    m_.end_of_storage = m_.end;
    new_storage.release();
  }
  
  devector(devector&& other) noexcept
    : m_(std::move(other.m_))
  {}
  
  devector(std::initializer_list<T> init)
  {
    size_type size = init.size();
    
    auto new_storage = std::unique_ptr<T, deleter>(m_.alloc(size), deleter(m_, size));
    auto new_begin = new_storage.get();
    std::uninitialized_copy(init.begin(), init.end(), new_begin);
    
    m_.begin = new_begin;
    m_.offset = m_.begin;
    m_.end = m_.begin + size;
    m_.end_of_storage = m_.end;
    new_storage.release();
  }

  ~devector()
  {
    destroy_range(m_.offset, m_.end);
    m_.dealloc(m_.begin, capacity());
  }
  
  //
  // Assignment
  //
  devector& operator=(const devector& other)
  {
    if (this != &other)
    {
      size_type otherSize = other.size();
      size_type capacity_ = capacity();
      if (otherSize <= capacity_)
      {
        auto new_offset = m_.begin + realloc_offset(otherSize, capacity_);
        
        // Before new offset
        m_.offset = destroy_range(m_.offset, new_offset);
        
        // Before old offset
        size_type copy_n = std::min<size_type>(m_.offset - new_offset, otherSize);
        iterator ot = std::uninitialized_copy_n(other.begin(), copy_n, new_offset);
        m_.offset = new_offset;
        
        // Common part
        const_iterator it = other.begin() + copy_n;
        size_type assign_n = std::min<size_type>(other.end() - it, m_.end - ot);
        ot = std::copy_n(it, assign_n, ot);
        
        // Other remaining
        ot = std::uninitialized_copy(it + assign_n, other.end(), ot);
        
        // This overflow
        destroy_range_backward(ot, m_.end);
        m_.end = ot;
      }
      else
      {
        clear();
        reserve_without_offset(otherSize);
        std::uninitialized_copy(other.begin(), other.end(), m_.begin);
        m_.end += otherSize;
      }
    }
    return *this;
  }
  
  devector& operator=(devector&& other) noexcept
  {
    m_ = std::move(other.m_);
    return *this;
  }
  
  devector& operator=(std::initializer_list<T> ilist)
  {
    size_type listSize = ilist.size();
    size_type capacity_ = capacity();
    if (listSize <= capacity_)
    {
      auto new_offset = m_.begin + realloc_offset(listSize, capacity_);
      
      // Before new offset
      m_.offset = destroy_range(m_.offset, new_offset);
      
      // Before old offset
      size_type copy_n = std::min<size_type>(m_.offset - new_offset, listSize);
      std::uninitialized_copy_n(ilist.begin(), copy_n, new_offset);
      m_.offset = new_offset;
      
      // Common part
      const_iterator it = ilist.begin() + copy_n;
      iterator ot = new_offset + copy_n;
      size_type assign_n = std::min<size_type>(ilist.end() - it, m_.end - ot);
      ot = std::copy_n(it, assign_n, ot);
      
      // List remaining
      ot = std::uninitialized_copy(it + assign_n, ilist.end(), ot);
      
      // This overflow
      destroy_range_backward(ot, m_.end);
      m_.end = ot;
    }
    else
    {
      clear();
      reserve_without_offset(listSize);
      std::uninitialized_copy(ilist.begin(), ilist.end(), m_.begin);
      m_.end += listSize;
    }
    
    return *this;
  }
  
  void assign(size_type count, const T& value)
  {
    size_type capacity_ = capacity();
    if (count <= capacity_)
    {
      auto new_offset = m_.begin + realloc_offset(count, capacity_);
      
      // Before new offset
      m_.offset = destroy_range(m_.offset, new_offset);
      
      // Before old offset
      size_type copy_n = std::min<size_type>(m_.offset - new_offset, count);
      std::uninitialized_fill_n(new_offset, copy_n, value);
      m_.offset = new_offset;
      
      // Common part
      iterator ot = new_offset + copy_n;
      size_type assign_n = std::min<size_type>(count - copy_n, m_.end - ot);
      ot = std::fill_n(ot, assign_n, value);
      
      // Remaining
      ot = std::uninitialized_fill_n(ot, count - (assign_n + copy_n), value);
      
      // This overflow
      destroy_range_backward(ot, m_.end);
      m_.end = ot;
    }
    else
    {
      clear();
      reserve_without_offset(count);
      m_.end = std::uninitialized_fill_n(m_.begin, count, value);
    }
  }
  
  void assign(const_iterator first, const_iterator last)
  {
    size_type capacity_ = capacity();
    size_type count = last - first;
    if (count <= capacity_)
    {
      auto new_offset = m_.begin + realloc_offset(count, capacity_);
      
      // Before new offset
      m_.offset = destroy_range(m_.offset, new_offset);
      
      // Before old offset
      size_type copy_n = std::min<size_type>(m_.offset - new_offset, count);
      std::uninitialized_copy_n(first, copy_n, new_offset);
      m_.offset = new_offset;
      
      // Common part
      const_iterator it = first + copy_n;
      iterator ot = new_offset + copy_n;
      size_type assign_n = std::min<size_type>(last - it, m_.end - ot);
      ot = std::copy_n(it, assign_n, ot);
      
      // Remaining
      ot = std::uninitialized_copy(it + assign_n, last, ot);
      
      // This overflow
      destroy_range_backward(ot, m_.end);
      m_.end = ot;
    }
    else
    {
      clear();
      reserve_without_offset(count);
      std::uninitialized_copy(first, last, m_.begin);
      m_.end += count;
    }
  }
  
  void assign(std::initializer_list<T> ilist)
  {
    assign(ilist.begin(), ilist.end());
  }
  
  //
  // Capacity
  //
  bool empty() const noexcept
  {
    return (m_.offset == m_.end);
  }
  
  size_type size() const noexcept
  {
    return (m_.end - m_.offset);
  }
  
  size_type max_size() const noexcept
  {
    return (std::numeric_limits<difference_type>::max() / 2);
  }
  
  size_type capacity() const noexcept
  {
    return (m_.end_of_storage - m_.begin);
  }
  
  void shrink_to_fit()
  {
    if (m_.begin != nullptr) // capacity > 0
    {
      if (empty())
      {
        // Reset
        m_.dealloc(m_.begin, capacity());
        m_.begin = nullptr;
        m_.offset = nullptr;
        m_.end = nullptr;
        m_.end_of_storage = nullptr;
      }
      else if (m_.end != m_.end_of_storage || m_.offset != m_.begin) // size != capacity
      {
        realloc(size());
      }
    }
  }
  
  void reserve(size_type new_cap)
  {
    reserve_shifted(new_cap, 0);
  }
  
  //
  // Iterators
  //
  iterator        begin()       noexcept  { return m_.offset; }
  const_iterator  begin() const noexcept  { return m_.offset; }
  const_iterator cbegin() const noexcept  { return m_.offset; }
  
  iterator        end()       noexcept  { return m_.end; }
  const_iterator  end() const noexcept  { return m_.end; }
  const_iterator cend() const noexcept  { return m_.end; }
  
  reverse_iterator        rbegin()       noexcept { return std::reverse_iterator<      iterator>(m_.end); }
  const_reverse_iterator  rbegin() const noexcept { return std::reverse_iterator<const_iterator>(m_.end); }
  const_reverse_iterator crbegin() const noexcept { return std::reverse_iterator<const_iterator>(m_.end); }
  
  reverse_iterator        rend()       noexcept { return std::reverse_iterator<      iterator>(m_.offset); }
  const_reverse_iterator  rend() const noexcept { return std::reverse_iterator<const_iterator>(m_.offset); }
  const_reverse_iterator crend() const noexcept { return std::reverse_iterator<const_iterator>(m_.offset); }
  
  //
  // Element access
  //
  reference operator[](size_type pos)
  {
    assert(pos < size());
    return *(m_.offset + pos);
  }
  
  const_reference operator[](size_type pos) const
  {
    assert(pos < size());
    return *(m_.offset + pos);
  }
  
  reference at(size_type pos)
  {
    if (pos >= size())
      throw std::out_of_range("devector::at");
    return *(m_.offset + pos);
  }
  
  const_reference at(size_type pos) const
  {
    if (pos >= size())
      throw std::out_of_range("devector::at");
    return *(m_.offset + pos);
  }
  
  reference front()
  {
    assert(!empty());
    return *(m_.offset);
  }
  
  const_reference front() const
  {
    assert(!empty());
    return *(m_.offset);
  }
  
  reference back()
  {
    assert(!empty());
    return *(m_.end - 1);
  }
  
  const_reference back() const
  {
    assert(!empty());
    return *(m_.end - 1);
  }
  
  T* data() noexcept
  {
    return m_.offset;
  }
  
  const T* data() const noexcept
  {
    return m_.offset;
  }
  
  // Non-standard
#ifdef INDIVI_DV_DEBUG
  T* storage() noexcept
  {
    return m_.begin;
  }
  
  const T* storage() const noexcept
  {
    return m_.begin;
  }
#endif
  size_type offset() const noexcept
  {
    return (m_.offset - m_.begin);
  }
  
  //
  // Modifiers
  //
  void clear() noexcept
  {
    destroy_range(m_.offset, m_.end);
    
  #ifdef INDIVI_DV_SHIFT_EMPTY
    m_.offset = m_.begin + realloc_offset(0, capacity());
  #endif
    m_.end = m_.offset;
  }
  
  void push_back(const T& value)
  {
    if (m_.end == m_.end_of_storage)
    {
      if (!shift_data_left())
        reserve_shifted(std::max<size_type>(1, (size_type)(::ceilf((float)capacity() * Options::GROWTH_FACTOR()))), 1);
    }
    assert(m_.end < m_.end_of_storage);
    ::new (static_cast<void*>(m_.end)) T(value);
    ++m_.end;
  }
  
  void push_back(T&& value)
  {
    if (m_.end == m_.end_of_storage)
    {
      if (!shift_data_left())
        reserve_shifted(std::max<size_type>(1, (size_type)(::ceilf((float)capacity() * Options::GROWTH_FACTOR()))), 1);
    }
    assert(m_.end < m_.end_of_storage);
    ::new (static_cast<void*>(m_.end)) T(std::forward<T>(value));
    ++m_.end;
  }
  
  template <class... Args>
  void emplace_back(Args&&... args)
  {
    if (m_.end == m_.end_of_storage)
    {
      if (!shift_data_left())
        reserve_shifted(std::max<size_type>(1, (size_type)(::ceilf((float)capacity() * Options::GROWTH_FACTOR()))), 1);
    }
    assert(m_.end < m_.end_of_storage);
    ::new (static_cast<void*>(m_.end)) T(std::forward<Args>(args)...);
    ++m_.end;
  }
  
  // Non-standard
  void push_front(const T& value)
  {
    if (m_.offset == m_.begin)
    {
      if (!shift_data_right())
        reserve_shifted(std::max<size_type>(1, (size_type)(::ceilf((float)capacity() * Options::GROWTH_FACTOR()))), 0, 1);
    }
    assert(m_.offset > m_.begin);
    ::new (static_cast<void*>(m_.offset-1)) T(value);
    --m_.offset;
  }
  
  void push_front(T&& value)
  {
    if (m_.offset == m_.begin)
    {
      if (!shift_data_right())
        reserve_shifted(std::max<size_type>(1, (size_type)(::ceilf((float)capacity() * Options::GROWTH_FACTOR()))), 0, 1);
    }
    assert(m_.offset > m_.begin);
    ::new (static_cast<void*>(m_.offset-1)) T(std::forward<T>(value));
    --m_.offset;
  }
  
  template <class... Args>
  void emplace_front(Args&&... args)
  {
    if (m_.offset == m_.begin)
    {
      if (!shift_data_right())
        reserve_shifted(std::max<size_type>(1, (size_type)(::ceilf((float)capacity() * Options::GROWTH_FACTOR()))), 0, 1);
    }
    assert(m_.offset > m_.begin);
    ::new (static_cast<void*>(m_.offset-1)) T(std::forward<Args>(args)...);
    --m_.offset;
  }
  
  
  iterator insert(const_iterator pos, const T& value)
  {
    assert(pos >= m_.offset);
    assert(pos <= m_.end);
    
    // Shift left (decrease offset)
    if (m_.offset != m_.begin
        && (((size_type)(pos - m_.offset) <= size() / 2) || m_.end == m_.end_of_storage))
    {
      // First
      if (pos == m_.offset)
      {
        ::new (static_cast<void*>(m_.offset-1)) T(value);
        return --m_.offset;
      }
      // Insert
      iterator it = m_.offset;
      ::new (static_cast<void*>(it - 1)) T(std::move(*it));
      --m_.offset;
      
      it = std::move(it + 1, (iterator)pos, it);
      
      *it = value;
      return it;
    }
    else
    {
      // Re-alloc
      if (m_.end == m_.end_of_storage)
      {
        difference_type diff = pos - m_.offset;
        realloc_insert(pos, value);
        return m_.offset + diff;
      }
      // Shift right (increase end)
      // Last
      if (pos == m_.end)
      {
        ::new (static_cast<void*>(m_.end)) T(value);
        return m_.end++;
      }
      // Insert
      iterator it = m_.end - 1;
      ::new (static_cast<void*>(it + 1)) T(std::move(*it));
      ++m_.end;
      
      it = std::move_backward((iterator)pos, it, it + 1) - 1;
      
      *it = value;
      return it;
    }
  }
  
  iterator insert(const_iterator pos, T&& value)
  {
    assert(pos >= m_.offset);
    assert(pos <= m_.end);
    
    // Shift left (decrease offset)
    if (m_.offset != m_.begin
        && (((size_type)(pos - m_.offset) <= size() / 2) || m_.end == m_.end_of_storage))
    {
      // First
      if (pos == m_.offset)
      {
        ::new (static_cast<void*>(m_.offset-1)) T(std::forward<T>(value));
        return --m_.offset;
      }
      // Insert
      iterator it = m_.offset;
      ::new (static_cast<void*>(it - 1)) T(std::move(*it));
      --m_.offset;
      
      it = std::move(it + 1, (iterator)pos, it);
      
      *it = std::forward<T>(value);
      return it;
    }
    else
    {
      // Re-alloc
      if (m_.end == m_.end_of_storage)
      {
        difference_type diff = pos - m_.offset;
        realloc_insert(pos, std::forward<T>(value));
        return m_.offset + diff;
      }
      // Shift right (increase end)
      // Last
      if (pos == m_.end)
      {
        ::new (static_cast<void*>(m_.end)) T(std::forward<T>(value));
        return m_.end++;
      }
      // Insert
      iterator it = m_.end - 1;
      ::new (static_cast<void*>(it + 1)) T(std::move(*it));
      ++m_.end;
      
      it = std::move_backward((iterator)pos, it, it + 1) - 1;
      
      *it = std::forward<T>(value);
      return it;
    }
  }
  
  iterator insert(const_iterator pos, size_type count, const T& value)
  {
    assert(pos >= m_.offset);
    assert(pos <= m_.end);
    
    size_type size_ = size();
    size_type capacity_ = capacity();
    if (size_ + count > capacity_)
    {
      difference_type diff = pos - m_.offset;
      realloc_insert(pos, count, value);
      return m_.offset + diff;
    }
    if (count == 0)
      return (iterator)pos;
    
    bool can_shift_left  = m_.offset - count >= m_.begin          || pos == m_.end;
    bool can_shift_right = m_.end    + count <= m_.end_of_storage || pos == m_.offset;
    size_type to_shift_left  = pos != m_.end ? (size_type)(pos - m_.offset)
                                            : (size_type)(m_.end_of_storage - m_.end) >= count ? 0 : size_;
    size_type to_shift_right = pos != m_.offset ? (size_type)(m_.end - pos)
                                               : (size_type)(m_.offset - m_.begin) >= count ? 0 : size_;
    
    // Shift left (decrease offset)
    if (can_shift_left && (to_shift_left <= to_shift_right || !can_shift_right))
    {
      // Insert before
      if (pos == m_.offset)
      {
        iterator new_offset = std::max<iterator>(m_.offset - count, m_.begin);
        std::uninitialized_fill_n(new_offset, count, value);
        m_.end = new_offset + size_ + count; // handle (size == 0)
        return m_.offset = new_offset;
      }
      
      iterator old_offset = m_.offset;
      size_type shift_left_dist = std::min<size_type>(m_.offset - m_.begin, count);
      size_type copy_n = std::min<size_type>(pos - m_.offset, shift_left_dist);
      
      // Before offset fill
      size_type fill_n = shift_left_dist - copy_n;
      std::uninitialized_fill_n(m_.offset - fill_n, fill_n, value);
      m_.offset -= fill_n;
      
      // Before offset copy
      iterator new_offset = m_.offset - copy_n;
      std::uninitialized_copy_n(std::make_move_iterator(old_offset), copy_n, new_offset);
      m_.offset = new_offset;
      
      // After offset move
      iterator ot;
      if (copy_n > 0u)
      {
        iterator it = old_offset + copy_n;
        ot = std::move(it, (iterator)pos, old_offset);
      }
      else
        ot = (iterator)pos;
      
      // After offset assign
      size_type assign_n = std::min<size_type>(count - fill_n, m_.end - ot);
      std::fill_n(ot, assign_n, value);
      
      // After offset fill
      size_type fill_n2 = count - (assign_n + fill_n);
      m_.end = std::uninitialized_fill_n(m_.end, fill_n2, value);
      
      return fill_n > 0 ? old_offset - fill_n
                        : (assign_n > 0 ? ot : m_.end - fill_n2);
    }
    else
    {
      // Shift right (increase offset)
      if (can_shift_right)
      {
        assert(size_ > 0);
        assert(pos != m_.end);
        
        iterator old_end = m_.end;
        size_type shift_right_dist = std::min<size_type>(m_.end_of_storage - m_.end, count);
        size_type copy_n = std::min<size_type>(m_.end - pos, shift_right_dist);
        
        // After end fill
        size_type fill_n = shift_right_dist - copy_n;
        m_.end = std::uninitialized_fill_n(m_.end, fill_n, value);
        
        // After end copy
        iterator it = old_end - copy_n;
        m_.end = std::uninitialized_copy_n(std::make_move_iterator(it), copy_n, m_.end);
        
        // Before end move
        iterator ot = std::move_backward((iterator)pos, it, old_end);
        
        // Before end assign
        size_type assign_n = std::min<size_type>(count - fill_n, ot - m_.offset);
        std::fill_n(ot - assign_n, assign_n, value);
        
        // Before end fill
        size_type fill_n2 = count - (assign_n + fill_n);
        std::uninitialized_fill_n(m_.offset - fill_n2, fill_n2, value);
        m_.offset -= fill_n2;
        
        return fill_n2 > 0 ? m_.offset : (assign_n > 0 ? ot - assign_n : old_end);
      }
      
      // Shift both: left first
      //
      iterator old_offset = m_.offset;
      iterator old_end = m_.end;
      size_type shift_left_dist = std::min<size_type>(m_.offset - m_.begin, (count + 1) / 2);
      size_type copy_n = std::min<size_type>(pos - m_.offset, shift_left_dist);
      
      // Before offset fill
      size_type fill_n = shift_left_dist - copy_n;
      std::uninitialized_fill_n(m_.offset - fill_n, fill_n, value);
      m_.offset -= fill_n;
      
      // Before offset copy
      iterator new_offset = m_.offset - copy_n;
      std::uninitialized_copy_n(std::make_move_iterator(old_offset), copy_n, new_offset);
      m_.offset = new_offset;
      
      // After offset move
      iterator it = old_offset + copy_n;
      iterator ot = std::move(it, (iterator)pos, old_offset);
      
      // Then shift right
      //
      size_type shift_right_dist = (m_.offset + size_ + count) - m_.end;
      size_type copy_n2 = std::min<size_type>(m_.end - pos, shift_right_dist);
    
      // After end fill
      size_type fill_n2 = shift_right_dist - copy_n2;
      m_.end = std::uninitialized_fill_n(m_.end, fill_n2, value);
      
      // After end copy
      iterator it2 = old_end - copy_n2;
      m_.end = std::uninitialized_copy_n(std::make_move_iterator(it2), copy_n2, m_.end);
      
      // Before end move
      std::move_backward((iterator)pos, it2, old_end);
      
      // Center
      // After offset assign
      size_type assign_n = count - fill_n - fill_n2;
      std::fill_n(ot, assign_n, value);
      
      return fill_n > 0 ? old_offset - fill_n
                        : (assign_n > 0 ? ot : old_end);
    }
  }
  
  iterator insert(const_iterator pos, iterator first, iterator last)
  {
    assert(pos >= m_.offset);
    assert(pos <= m_.end);
    assert(first <= last);
    
    size_type count = last - first;
    size_type size_ = size();
    size_type capacity_ = capacity();
    if (size_ + count > capacity_)
    {
      difference_type diff = pos - m_.offset;
      realloc_insert(pos, first, last);
      return m_.offset + diff;
    }
    if (count == 0)
      return (iterator)pos;
    
    bool can_shift_left  = m_.offset - count >= m_.begin          || pos == m_.end;
    bool can_shift_right = m_.end    + count <= m_.end_of_storage || pos == m_.offset;
    size_type to_shift_left  = pos != m_.end ? (size_type)(pos - m_.offset)
                                            : (size_type)(m_.end_of_storage - m_.end) >= count ? 0 : size_;
    size_type to_shift_right = pos != m_.offset ? (size_type)(m_.end - pos)
                                               : (size_type)(m_.offset - m_.begin) >= count ? 0 : size_;
    
    // Shift left (decrease offset)
    if (can_shift_left && (to_shift_left <= to_shift_right || !can_shift_right))
    {
      // Insert before
      if (pos == m_.offset)
      {
        iterator new_offset = std::max<iterator>(m_.offset - count, m_.begin);
        std::uninitialized_copy(first, last, new_offset);
        m_.end = new_offset + size_ + count; // handle (size == 0)
        return m_.offset = new_offset;
      }
      
      iterator old_offset = m_.offset;
      size_type shift_left_dist = std::min<size_type>(m_.offset - m_.begin, count);
      size_type copy_n = std::min<size_type>(pos - m_.offset, shift_left_dist);
      
      // Before offset fill
      size_type fill_n = shift_left_dist - copy_n;
      std::uninitialized_copy_n(first, fill_n, m_.offset - fill_n);
      first += fill_n;
      m_.offset -= fill_n;
      
      // Before offset copy
      iterator new_offset = m_.offset - copy_n;
      std::uninitialized_copy_n(std::make_move_iterator(old_offset), copy_n, new_offset);
      m_.offset = new_offset;
      
      // After offset move
      iterator ot;
      if (copy_n > 0u)
      {
        iterator it = old_offset + copy_n;
        ot = std::move(it, (iterator)pos, old_offset);
      }
      else
        ot = (iterator)pos;
      
      // After offset assign
      size_type assign_n = std::min<size_type>(count - fill_n, m_.end - ot);
      std::copy_n(first, assign_n, ot);
      first += assign_n;
      
      // After offset fill
      size_type fill_n2 = count - (assign_n + fill_n);
      m_.end = std::uninitialized_copy_n(first, fill_n2, m_.end);
      
      return fill_n > 0 ? old_offset - fill_n
                        : (assign_n > 0 ? ot : m_.end - fill_n2);
    }
    else
    {
      // Shift right (increase offset)
      if (can_shift_right)
      {
        assert(size_ > 0);
        assert(pos != m_.end);
        
        iterator old_end = m_.end;
        size_type shift_right_dist = std::min<size_type>(m_.end_of_storage - m_.end, count);
        size_type copy_n = std::min<size_type>(m_.end - pos, shift_right_dist);
        
        // After end fill
        size_type fill_n = shift_right_dist - copy_n;
        m_.end = std::uninitialized_copy_n(first, fill_n, m_.end);
        first += fill_n;
        
        // After end copy
        iterator it = old_end - copy_n;
        m_.end = std::uninitialized_copy_n(std::make_move_iterator(it), copy_n, m_.end);
        
        // Before end move
        iterator ot = std::move_backward((iterator)pos, it, old_end);
        
        // Before end assign
        size_type assign_n = std::min<size_type>(count - fill_n, ot - m_.offset);
        std::copy_n(first, assign_n, ot - assign_n);
        first += assign_n;
        
        // Before end fill
        size_type fill_n2 = count - (assign_n + fill_n);
        std::uninitialized_copy_n(first, fill_n2, m_.offset - fill_n2);
        first += fill_n2;
        m_.offset -= fill_n2;
        
        return fill_n2 > 0 ? m_.offset : (assign_n > 0 ? ot - assign_n : old_end);
      }
      
      // Shift both: left first
      //
      iterator old_offset = m_.offset;
      iterator old_end = m_.end;
      size_type shift_left_dist = std::min<size_type>(m_.offset - m_.begin, (count + 1) / 2);
      size_type copy_n = std::min<size_type>(pos - m_.offset, shift_left_dist);
      
      // Before offset fill
      size_type fill_n = shift_left_dist - copy_n;
      std::uninitialized_copy_n(first, fill_n, m_.offset - fill_n);
      first += fill_n;
      m_.offset -= fill_n;
      
      // Before offset copy
      iterator new_offset = m_.offset - copy_n;
      std::uninitialized_copy_n(std::make_move_iterator(old_offset), copy_n, new_offset);
      m_.offset = new_offset;
      
      // After offset move
      iterator it = old_offset + copy_n;
      iterator ot = std::move(it, (iterator)pos, old_offset);
      
      // Then shift right
      //
      size_type shift_right_dist = (m_.offset + size_ + count) - m_.end;
      size_type copy_n2 = std::min<size_type>(m_.end - pos, shift_right_dist);
    
      // After end fill
      size_type fill_n2 = shift_right_dist - copy_n2;
      m_.end = std::uninitialized_copy_n(last - fill_n2, fill_n2, m_.end);
      
      // After end copy
      iterator it2 = old_end - copy_n2;
      m_.end = std::uninitialized_copy_n(std::make_move_iterator(it2), copy_n2, m_.end);
      
      // Before end move
      std::move_backward((iterator)pos, it2, old_end);
      
      // Center
      // After offset assign
      size_type assign_n = count - fill_n - fill_n2;
      std::copy_n(first, assign_n, ot);
      
      return fill_n > 0 ? old_offset - fill_n
                        : (assign_n > 0 ? ot : old_end);
    }
  }
  
  iterator insert(const_iterator pos, std::initializer_list<T> ilist)
  {
    return insert(pos, (iterator)ilist.begin(), (iterator)ilist.end());
  }
  
  template <class... Args>
  iterator emplace(const_iterator pos, Args&&... args)
  {
    assert(pos >= m_.offset);
    assert(pos <= m_.end);
    
    // Shift left (decrease offset)
    if (m_.offset != m_.begin
        && (((size_type)(pos - m_.offset) <= size() / 2) || m_.end == m_.end_of_storage))
    {
      // First
      if (pos == m_.offset)
      {
        ::new (static_cast<void*>(m_.offset-1)) T(std::forward<Args>(args)...);
        return --m_.offset;
      }
      // Insert
      T value(std::forward<Args>(args)...);
      
      iterator it = m_.offset;
      ::new (static_cast<void*>(it - 1)) T(std::move(*it));
      --m_.offset;
      
      it = std::move(it + 1, (iterator)pos, it);
      
      *it = std::move(value);
      return it;
    }
    else
    {
      // Re-alloc
      if (m_.end == m_.end_of_storage)
      {
        difference_type diff = pos - m_.offset;
        realloc_insert(pos, std::forward<Args>(args)...);
        return m_.offset + diff;
      }
      // Shift right (increase end)
      // Last
      if (pos == m_.end)
      {
        ::new (static_cast<void*>(m_.end)) T(std::forward<Args>(args)...);
        return m_.end++;
      }
      // Insert
      T value(std::forward<Args>(args)...);
      
      iterator it = m_.end - 1;
      ::new (static_cast<void*>(it + 1)) T(std::move(*it));
      ++m_.end;
      
      it = std::move_backward((iterator)pos, it, it + 1) - 1;

      *it = std::move(value);
      return it;
    }
  }
  
  void pop_back()
  {
    assert(m_.end != m_.offset);
    
    (--m_.end)->~T();
    
  #ifdef INDIVI_DV_SHIFT_EMPTY
    if (empty())
    {
      m_.offset = m_.begin + realloc_offset(0, capacity());
      m_.end = m_.offset;
    }
  #endif
  }
  
  // Non-standard
  void pop_front()
  {
    assert(m_.end != m_.offset);
    
    (m_.offset++)->~T();
    
  #ifdef INDIVI_DV_SHIFT_EMPTY
    if (empty())
    {
      m_.offset = m_.begin + realloc_offset(0, capacity());
      m_.end = m_.offset;
    }
  #endif
  }
  
  iterator erase(const_iterator pos)
  {
    assert(pos >= m_.offset);
    assert(pos < m_.end);
    
    if ((size_type)(pos - m_.offset) < size() / 2)  // Shift right (increase offset)
    {
      if (pos == m_.offset)
      {
        (m_.offset++)->~T();
        return m_.offset;
      }
      else
      {
        iterator it = (iterator)pos;
        std::move_backward(m_.offset, it, it + 1);
        
        (m_.offset++)->~T();
        return it + 1;
      }
    }
    else // Shift left (decrease end)
    {
      if (pos == m_.end - 1)
      {
        (--m_.end)->~T();
        
      #ifdef INDIVI_DV_SHIFT_EMPTY
        if (empty())
        {
          m_.offset = m_.begin + realloc_offset(0, capacity());
          m_.end = m_.offset;
        }
      #endif
        return m_.end;
      }
      else
      {
        iterator it = (iterator)pos;
        std::move(it + 1, m_.end, it);
        
        (--m_.end)->~T();
        return it;
      }
    }
  }
  
  iterator erase(const_iterator first, const_iterator last)
  {
    if (first == last)
      return (iterator)last;
    
    assert(first >= m_.offset);
    assert(first < last);
    assert(last <= m_.end);
    
    if (first - m_.offset < m_.end - last) // Shift right (increase offset)
    {
      iterator ot = std::move_backward(m_.offset, (iterator)first, (iterator)last);
      
      m_.offset = destroy_range(m_.offset, ot);
      
      return (iterator)last;
    }
    else  // Shift left (decrease end)
    {
      iterator ot = std::move((iterator)last, m_.end, (iterator)first);
      
      m_.end = destroy_range_backward(ot, m_.end);
      
    #ifdef INDIVI_DV_SHIFT_EMPTY
      if (empty())
      {
        m_.offset = m_.begin + realloc_offset(0, capacity());
        m_.end = m_.offset;
        
        return m_.offset;
      }
    #endif
      return (iterator)first;
    }
  }
  
  void resize(size_type count)
  {
    if (count <= size())
    {
      // Remove overflow
      iterator new_end = m_.offset + count;
      m_.end = destroy_range_backward(new_end, m_.end);
      
    #ifdef INDIVI_DV_SHIFT_EMPTY
      if (count == 0)
      {
        m_.offset = m_.begin + realloc_offset(0, capacity());
        m_.end = m_.offset;
      }
    #endif
    }
    else
    {
      if (count > capacity())
      {
        // Re-alloc
        reserve_without_offset(count);
      }
      else
      {
        // Not enough space at end
        if (m_.offset + count > m_.end_of_storage)
          shift_data_left(count);
      }
      // Push back
      iterator new_end = m_.offset + count;
      iterator it = m_.end;
      try {
        for (; it != new_end; ++it)
          ::new (static_cast<void*>(it)) INDIVI_DV_VALUE_INIT;
      }
      catch (...) {
        destroy_range_backward(m_.end, it);
        throw;
      }
      m_.end = new_end;
    }
  }
  
  void resize(size_type count, const value_type& value)
  {
    if (count <= size())
    {
      // Remove overflow
      auto new_end = m_.offset + count;
      m_.end = destroy_range_backward(new_end, m_.end);
      
    #ifdef INDIVI_DV_SHIFT_EMPTY
      if (count == 0)
      {
        m_.offset = m_.begin + realloc_offset(0, capacity());
        m_.end = m_.offset;
      }
    #endif
    }
    else
    {
      if (count > capacity())
      {
        // Re-alloc
        reserve_without_offset(count);
      }
      else
      {
        // Not enough space at end
        if (m_.offset + count > m_.end_of_storage)
          shift_data_left(count);
      }
      // Push back
      auto copy_n = count - size();
      m_.end = std::uninitialized_fill_n(m_.end, copy_n, value);
    }
  }
  
  void swap(devector& other) noexcept
  {
    devector temp(std::move(*this));
    devector::operator=(std::move(other));
    other = std::move(temp);
  }
  
  // Non-standard
  void shift_data_start()
  {
    if (m_.offset != m_.begin)
      shift_data_left(m_.begin, size());
  }
  
  void shift_data_end()
  {
    if (m_.end != m_.end_of_storage)
      shift_data_right(m_.end_of_storage, size());
  }
  
  void shift_data_center()
  {
    size_type size_ = size();
    size_type capacity_ = std::max<size_type>(capacity(), 1);
    if (size_ == 0)
    {
      m_.offset = m_.begin + (capacity_ - 1) / 2;
      m_.end = m_.offset;
      return;
    }
    iterator new_offset = m_.begin + (capacity_ - size_) / 2;

    if (new_offset < m_.offset)
      shift_data_left(new_offset, size_);
    else if (new_offset > m_.offset)
      shift_data_right(new_offset + size_, size_);
  }
  
private:
  iterator destroy_range(iterator first, iterator last)
  {
    for (; first < last; ++first)
      first->~T();
    
    return first;
  }
  
  iterator destroy_range_backward(iterator first, iterator last)
  {
    while (last > first)
      (--last)->~T();
    
    return last;
  }
  
  void shift_data_left(iterator new_offset, size_type size)
  {
    assert(new_offset >= m_.begin);
    assert(new_offset < m_.offset);
    
    // Before offset
    iterator old_offset = m_.offset;
    size_type copy_n = std::min<size_type>(m_.offset - new_offset, size);
    iterator ot = std::uninitialized_copy_n(std::make_move_iterator(m_.offset),
                                            copy_n,
                                            new_offset);
    m_.offset = new_offset;
    
    // After offset
    iterator it = old_offset + copy_n;
    ot = std::move(it, m_.end, ot);
    
    // Destruct remaining
    it = std::max<iterator>(ot, old_offset);
    destroy_range_backward(it, m_.end);
    m_.end = ot;
  }
  
  void shift_data_right(iterator new_end, size_type size)
  {
    assert(new_end <= m_.end_of_storage);
    assert(new_end > m_.end);
    
    // After end
    iterator old_end = m_.end;
    size_type copy_n = std::min<size_type>(new_end - m_.end, size);
    std::uninitialized_copy_n(std::make_move_iterator(m_.end - copy_n),
                              copy_n,
                              new_end - copy_n);
    m_.end = new_end;
    
    // Before end
    iterator it = old_end - copy_n;
    iterator ot = std::move_backward(m_.offset, it, new_end - copy_n);
    
    // Destruct remaining
    it = std::min<iterator>(ot, old_end);
    destroy_range(m_.offset, it);
    m_.offset = ot;
  }
  
  void shift_data_left(size_type new_size)
  {
    size_type size_ = size();
    size_type capacity_ = capacity();
    assert(new_size > size_);
    assert(new_size <= capacity_);
    assert(m_.offset != m_.begin);
    
    switch (Options::SHIFT_MODE())
    {
      case ShiftMode::NEAR:
      {
        iterator new_offset = m_.end - new_size;
        if (size_ == 0)
        {
        #ifdef INDIVI_DV_SHIFT_EMPTY
          m_.offset = m_.begin + realloc_offset(new_size, capacity_);
        #else
          m_.offset = std::max(m_.begin, new_offset);
        #endif
          m_.end = m_.offset;
          return;
        }
        
        shift_data_left(new_offset, size_);
        break;
      }
      case ShiftMode::CENTER:
      {
        size_type off = (capacity_ - new_size) / 2;
        iterator new_offset = m_.begin + off;
        if (size_ == 0)
        {
          m_.offset = new_offset;
          m_.end = m_.offset;
          return;
        }
        
        shift_data_left(new_offset, size_);
        break;
      }
      case ShiftMode::FAR:
      {
        if (size_ == 0)
        {
          m_.offset = m_.begin;
          m_.end = m_.begin;
          return;
        }
        
        shift_data_left(m_.begin, size_);
        break;
      }
      default:
        assert(false && "devector::shift_data_left");
    }
  }
  
  bool shift_data_left()
  {
    size_type size_ = size();
    size_type capacity_ = capacity();
    if (size_ + 1 > capacity_)
      return false;

    assert(m_.offset != m_.begin);
    assert(m_.end == m_.end_of_storage);
    
    switch (Options::SHIFT_MODE())
    {
      case ShiftMode::NEAR:
      {
        if (size_ == 0)
        {
          --m_.offset;
          --m_.end;
          return true;
        }
        
        iterator it = m_.offset;
        ::new (static_cast<void*>(it - 1)) T(std::move(*it));
        --m_.offset;
        
        std::move(it + 1, m_.end, it);
        (--m_.end)->~T();
        break;
      }
      case ShiftMode::CENTER:
      {
        size_type off = (capacity_ - (size_ + 1)) / 2;
        iterator new_offset = m_.begin + off;
        if (size_ == 0)
        {
          m_.offset = new_offset;
          m_.end = new_offset;
          return true;
        }
        
        shift_data_left(new_offset, size_);
        break;
      }
      case ShiftMode::FAR:
      {
        if (size_ == 0)
        {
          m_.offset = m_.begin;
          m_.end = m_.begin;
          return true;
        }
        
        shift_data_left(m_.begin, size_);
        break;
      }
      default:
        assert(false && "devector::shift_data_left");
    }
    return true;
  }
  
  bool shift_data_right()
  {
    size_type size_ = size();
    size_type capacity_ = capacity();
    if (size_ + 1 > capacity_)
      return false;
    
    assert(m_.offset == m_.begin);
    assert(m_.end != m_.end_of_storage);
    
    switch (Options::SHIFT_MODE())
    {
      case ShiftMode::NEAR:
      {
        if (size_ == 0)
        {
          ++m_.offset;
          ++m_.end;
          return true;
        }
        
        iterator it = m_.end - 1;
        ::new (static_cast<void*>(m_.end)) T(std::move(*it));
        ++m_.end;
        
        std::move_backward(m_.offset, it, it + 1);
        (m_.offset++)->~T();
        break;
      }
      case ShiftMode::CENTER:
      {
        size_type off = (capacity_ - (size_ + 1)) / 2;
        iterator new_end = m_.end_of_storage - off;
        if (size_ == 0)
        {
          m_.offset = new_end;
          m_.end = new_end;
          return true;
        }

        shift_data_right(new_end, size_);
        break;
      }
      case ShiftMode::FAR:
      {
        if (size_ == 0)
        {
          m_.offset = m_.end_of_storage;
          m_.end = m_.end_of_storage;
          return true;
        }
        
        shift_data_right(m_.end_of_storage, size_);
        break;
      }
      default:
        assert(false && "devector::shift_data_right");
    }
    return true;
  }
  
  static size_type realloc_offset(size_type new_size, size_type new_cap)
  {
    switch (Options::REALLOC_MODE())
    {
      case ReallocMode::START:
        return 0;
      case ReallocMode::CENTER:
        return (new_cap - std::min<size_type>(std::max<size_type>(new_size, 1), new_cap)) / 2;
      case ReallocMode::END:
        return new_cap - new_size;
      default:
        assert(false && "devector::realloc_offset");
    }
    return 0;
  }
  
  void reserve_shifted(size_type new_cap, size_type rightOffset, size_type leftOffset = 0)
  {
    if (new_cap > capacity())
    {
      if (new_cap > max_size())
        throw std::length_error("devector::reserve");
      
      if (empty())
      {
        assert(new_cap >= leftOffset + rightOffset);
        // Delete and re-allocate
        m_.dealloc(m_.begin, capacity());
        m_.begin = nullptr;
        m_.begin = m_.alloc(new_cap);
        m_.offset = m_.begin + realloc_offset(leftOffset + rightOffset, new_cap) + leftOffset;
        m_.end = m_.offset;
        m_.end_of_storage = m_.begin + new_cap;
      }
      else
      {
        // Allocate new, copy then delete old
        size_type size_ = size();
        assert(new_cap >= size_ + leftOffset + rightOffset);
        
        auto new_storage = std::unique_ptr<T, deleter>(m_.alloc(new_cap), deleter(m_, new_cap));
        auto new_begin = new_storage.get();
        auto new_offset = new_begin
            + realloc_offset(size_ + leftOffset + rightOffset, new_cap) + leftOffset;
        
        std::uninitialized_copy(std::make_move_iterator(m_.offset),
                                std::make_move_iterator(m_.end),
                                new_offset);
        
        destroy_range(m_.offset, m_.end);
        m_.dealloc(m_.begin, capacity());
        
        m_.begin = new_begin;
        m_.offset = new_offset;
        m_.end = m_.offset + size_;
        m_.end_of_storage = m_.begin + new_cap;
        new_storage.release();
      }
    }
  }
  
  void reserve_without_offset(size_type new_cap)
  {
    if (new_cap > capacity())
    {
      if (new_cap > max_size())
        throw std::length_error("devector::reserve_without_offset");
      
      if (empty())
      {
        // Delete and re-allocate
        m_.dealloc(m_.begin, capacity());
        m_.begin = nullptr;
        m_.begin = m_.alloc(new_cap);
        m_.offset = m_.begin;
        m_.end = m_.offset;
        m_.end_of_storage = m_.begin + new_cap;
      }
      else
      {
        realloc(new_cap);
      }
    }
  }
  
  struct impl_data
  {
    impl_data(Allocator& allocator_, T* begin_, size_type offset_, size_type capacity_)
      : allocator(allocator_)
      , begin(begin_), offset(begin_ + offset_), end(begin_ + offset_), end_of_storage(begin_ + capacity_)
    {
      assert(offset_ <= capacity_);
    }
    
    impl_data(const impl_data& other) = delete;
    
    impl_data(impl_data&& other) noexcept
      : allocator(other.allocator)
      , begin(other.begin), offset(other.offset), end(other.end), end_of_storage(other.end_of_storage)
    {
      other.begin = nullptr;
      other.offset = nullptr;
      other.end = nullptr;
      other.end_of_storage = nullptr;
    }
    
    ~impl_data()
    {
      for (; offset < end; ++offset)
        offset->~T();
      
      allocator.deallocate(begin, end_of_storage - begin);
    }
    
    Allocator& allocator;
    T*  begin;
    T*  offset;
    T*  end;
    T*  end_of_storage;
  };
  
  void swap(impl_data& other) noexcept
  {
    T* begin = m_.begin;
    T* offset = m_.offset;
    T* end = m_.end;
    T* end_of_storage = m_.end_of_storage;
    
    m_.begin = other.begin;
    m_.offset = other.offset;
    m_.end = other.end;
    m_.end_of_storage = other.end_of_storage;
    
    other.begin = begin;
    other.offset = offset;
    other.end = end;
    other.end_of_storage = end_of_storage;
  }
  
  void realloc(size_type new_cap)
  {
    // Allocate new, copy then delete old
    size_type size_ = size();
    auto new_storage = std::unique_ptr<T, deleter>(m_.alloc(new_cap), deleter(m_, new_cap));
    auto new_begin = new_storage.get();
    
    std::uninitialized_copy(std::make_move_iterator(m_.offset),
                            std::make_move_iterator(m_.end),
                            new_begin);
    
    destroy_range(m_.offset, m_.end);
    m_.dealloc(m_.begin, capacity());
    
    m_.begin = new_begin;
    m_.offset = m_.begin;
    m_.end = m_.offset + size_;
    m_.end_of_storage = m_.begin + new_cap;
    new_storage.release();
  }
  
  void realloc_insert(const_iterator pos, const T& value)
  {
    size_type new_size = size() + 1;
    
    auto new_vec = move_until(pos, new_size);
    ::new (static_cast<void*>(new_vec.end)) T(value);
    ++new_vec.end;
    move_from(new_vec, (iterator)pos);
    
    swap(new_vec);
  }
  
  void realloc_insert(const_iterator pos, T&& value)
  {
    size_type new_size = size() + 1;
    
    auto new_vec = move_until(pos, new_size);
    ::new (static_cast<void*>(new_vec.end)) T(std::forward<T>(value));
    ++new_vec.end;
    move_from(new_vec, (iterator)pos);
    
    swap(new_vec);
  }
  
  template <class... Args>
  void realloc_insert(const_iterator pos, Args&&... args)
  {
    size_type new_size = size() + 1;
    
    auto new_vec = move_until(pos, new_size);
    ::new (static_cast<void*>(new_vec.end)) T(std::forward<Args>(args)...);
    ++new_vec.end;
    move_from(new_vec, (iterator)pos);
    
    swap(new_vec);
  }
  
  void realloc_insert(const_iterator pos, size_type count, const T& value)
  {
    size_type new_size = size() + count;
    
    auto new_vec = move_until(pos, new_size);
    new_vec.end = std::uninitialized_fill_n(new_vec.end, count, value);
    move_from(new_vec, (iterator)pos);
    
    swap(new_vec);
  }
  
  template <class InputIt>
  void realloc_insert(const_iterator pos, InputIt first, InputIt last)
  {
    size_type new_size = size() + (last - first);
    
    auto new_vec = move_until(pos, new_size);
    new_vec.end = std::uninitialized_copy(first, last, new_vec.end);
    move_from(new_vec, (iterator)pos);
    
    swap(new_vec);
  }
  
  impl_data move_until(const_iterator pos, size_type new_size)
  {
    if (empty())
    {
      return impl_data(m_, m_.alloc(new_size), 0, new_size);
    }
    else
    {
      // Allocate new and move until pos
      size_type new_cap = (size_type)(::ceilf((float)capacity() * Options::GROWTH_FACTOR()));
      if (new_cap < new_size)
        new_cap = new_size;
      
      impl_data new_vec(m_, m_.alloc(new_cap), realloc_offset(new_size, new_cap), new_cap);
      
      auto copy_n = pos - m_.offset;
      std::uninitialized_copy_n(std::make_move_iterator(m_.offset), copy_n, new_vec.offset);
      new_vec.end += copy_n;
      
      return new_vec;
    }
  }
  
  void move_from(impl_data& new_vec, iterator from) const
  {
    assert(new_vec.end_of_storage - new_vec.end >= m_.end - from);
    
    auto copy_n = m_.end - from;
    std::uninitialized_copy_n(std::make_move_iterator(from), copy_n, new_vec.end);
    new_vec.end += copy_n;
  }

}; // end of devector


//
// Non-member functions
//
template <class T>
bool operator==(const devector<T>& lhs, const devector<T>& rhs)
{
  if (lhs.size() != rhs.size())
    return false;
  
  for (auto lit = lhs.cbegin(), rit = rhs.cbegin(); lit != lhs.cend(); ++lit, ++rit)
  {
    if (!(*lit == *rit))
      return false;
  }
  return true;
}

template <class T>
inline bool operator!=(const devector<T>& lhs, const devector<T>& rhs)
{
  if (lhs.size() != rhs.size())
    return true;
  
  for (auto lit = lhs.cbegin(), rit = rhs.cbegin(); lit != lhs.cend(); ++lit, ++rit)
  {
    if (*lit != *rit)
      return true;
  }
  return false;
}

template <class T>
inline bool operator<(const devector<T>& lhs, const devector<T>& rhs)
{
  for (auto lit = lhs.cbegin(), rit = rhs.cbegin(); lit != lhs.cend() && rit != rhs.cend(); ++lit, ++rit)
  {
    if (!(*lit < *rit))
      return false;
  }
  return true;
}

template <class T>
inline bool operator<=(const devector<T>& lhs, const devector<T>& rhs)
{
  for (auto lit = lhs.cbegin(), rit = rhs.cbegin(); lit != lhs.cend() && rit != rhs.cend(); ++lit, ++rit)
  {
    if (!(*lit <= *rit))
      return false;
  }
  return true;
}

template <class T>
inline bool operator>(const devector<T>& lhs, const devector<T>& rhs)
{
  for (auto lit = lhs.cbegin(), rit = rhs.cbegin(); lit != lhs.cend() && rit != rhs.cend(); ++lit, ++rit)
  {
    if (!(*lit > *rit))
      return false;
  }
  return true;
}

template <class T>
inline bool operator>=(const devector<T>& lhs, const devector<T>& rhs)
{
  for (auto lit = lhs.cbegin(), rit = rhs.cbegin(); lit != lhs.cend() && rit != rhs.cend(); ++lit, ++rit)
  {
    if (!(*lit >= *rit))
      return false;
  }
  return true;
}

} // namespace indivi


namespace std
{
  template <class T>
  inline void swap(indivi::devector<T>& lhs, indivi::devector<T>& rhs) noexcept
  {
    lhs.swap(rhs);
  }
}


#endif // INDIVI_DEVECTOR_H
