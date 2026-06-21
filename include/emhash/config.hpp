// emhash shared configuration macros
// https://github.com/ktprime/emhash
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2026 Huang Yuanbing & bailuzhou AT 163.com

#pragma once

// Allow users to provide their own config by defining EMH_CONFIG_INCLUDED
// before including any emhash header. User config should define all required
// macros (EMH_LIKELY, EMH_INLINE, etc.) or include this file after customizing.
#ifndef EMH_CONFIG_INCLUDED
#define EMH_CONFIG_INCLUDED

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define EMH_LIKELY(condition) __builtin_expect(!!(condition), 1)
#define EMH_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1920)
#include <intrin.h>
#define EMH_LIKELY(condition)   ((condition) ? ((void)__assume(condition), 1) : 0)
#define EMH_UNLIKELY(condition) ((condition) ? 1 : ((void)__assume(!(condition)), 0))
#define EMH_ASSUME(cond) __assume(cond)
#else
#define EMH_LIKELY(condition) (condition)
#define EMH_UNLIKELY(condition) (condition)
#define EMH_ASSUME(cond) ((void)0)
#endif

// Compiler detection
#if defined(_MSC_VER) && !defined(__clang__)
#define EMH_MSVC _MSC_VER
#else
#define EMH_MSVC 0
#endif

// Force inline
#if defined(__GNUC__) || defined(__clang__)
#define EMH_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define EMH_INLINE __forceinline
#else
#define EMH_INLINE inline
#endif

// Unreachable code hint
#if defined(__GNUC__) || defined(__clang__)
#define EMH_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define EMH_UNREACHABLE() __assume(0)
#else
#define EMH_UNREACHABLE() ((void)0)
#endif

// Compatibility aliases (used by lru_time.hpp / lru_size.hpp)
#ifndef EMHASH_LIKELY
#define EMHASH_LIKELY(condition) EMH_LIKELY(condition)
#endif
#ifndef EMHASH_UNLIKELY
#define EMHASH_UNLIKELY(condition) EMH_UNLIKELY(condition)
#endif

// Default cache line size
#ifndef EMH_CACHE_LINE_SIZE
#define EMH_CACHE_LINE_SIZE 64
#endif
#ifndef EMHASH_CACHE_LINE_SIZE
#define EMHASH_CACHE_LINE_SIZE EMH_CACHE_LINE_SIZE
#endif

#endif // EMH_CONFIG_INCLUDED
