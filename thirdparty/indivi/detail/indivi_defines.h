/**
 * Copyright 2024 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

#ifndef INDIVI_DEFINES_H
#define INDIVI_DEFINES_H

#include <cstdint>

// Compiler
#ifdef _MSC_VER
  #define INDIVI_MSVC _MSC_VER
#elif defined __clang__
  #define INDIVI_CLANG  1
#elif defined(__GNUC__)
  #define INDIVI_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

// C++
#if ((__cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
  #define INDIVI_CPP17
#endif
#if ((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L))
  #define INDIVI_CPP20
#endif

// Architecture
#ifdef SIZE_MAX
  #if ((((SIZE_MAX >> 16) >> 16) >> 16) >> 15) != 0
    #define INDIVI_ARCH_64
  #endif
#else
  #ifdef UINTPTR_MAX
    #if ((((UINTPTR_MAX >> 16) >> 16) >> 16) >> 15) != 0
      #define INDIVI_ARCH_64
    #endif
  #endif
#endif

#ifdef __ARM_ARCH_ISA_A64
  #define INDIVI_ARCH_ARM64
#endif

// SIMD
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  #define INDIVI_SIMD_SSE2
#elif (defined(__ARM_NEON) || defined(__ARM_NEON__)) && !defined(__ARM_BIG_ENDIAN)
  #define INDIVI_SIMD_L_ENDIAN_NEON
#endif


#endif // INDIVI_DEFINES_H
