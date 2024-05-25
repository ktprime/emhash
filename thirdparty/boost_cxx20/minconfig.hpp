#pragma once

#include <cstdint>
#include <bit>

// This is a minimal header that contains only the small set
// config entries needed to use boost::unordered, so that the
// whole boost config lib doesn't need to be pulled in.
#if defined(_MSC_VER)
#  ifndef BOOST_MSVC
#    define BOOST_MSVC _MSC_VER
#  endif
#elif defined __clang__
#  ifndef BOOST_CLANG
#    define BOOST_CLANG 1
#  endif
#elif defined(__GNUC__)
#  ifndef BOOST_GCC
#    define BOOST_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#  endif
#endif

#if !defined(BOOST_FORCEINLINE)
#  if defined(_MSC_VER)
#    define BOOST_FORCEINLINE __forceinline
#  elif defined(__GNUC__) && __GNUC__ > 3
     // Clang also defines __GNUC__ (as 4)
#    define BOOST_FORCEINLINE inline __attribute__ ((__always_inline__))
#  else
#    define BOOST_FORCEINLINE inline
#  endif
#endif

#if !defined(BOOST_NOINLINE)
#  if defined(_MSC_VER)
#    define BOOST_NOINLINE __declspec(noinline)
#  elif defined(__GNUC__) && __GNUC__ > 3
     // Clang also defines __GNUC__ (as 4)
#    if defined(__CUDACC__)
       // nvcc doesn't always parse __noinline__,
       // see: https://svn.boost.org/trac/boost/ticket/9392
#      define BOOST_NOINLINE __attribute__ ((noinline))
#    elif defined(__HIP__)
       // See https://github.com/boostorg/config/issues/392
#      define BOOST_NOINLINE __attribute__ ((noinline))
#    else
#      define BOOST_NOINLINE __attribute__ ((__noinline__))
#    endif
#  else
#    define BOOST_NOINLINE
#  endif
#endif

#if defined(BOOST_GCC) || defined(BOOST_CLANG)
#  define BOOST_LIKELY(x) __builtin_expect(x, 1)
#  define BOOST_UNLIKELY(x) __builtin_expect(x, 0)
#  define BOOST_SYMBOL_VISIBLE __attribute__((__visibility__("default")))
#else
#  define BOOST_SYMBOL_VISIBLE
#endif

#if !defined(BOOST_LIKELY)
#  define BOOST_LIKELY(x) x
#endif
#if !defined(BOOST_UNLIKELY)
#  define BOOST_UNLIKELY(x) x
#endif

#if !defined(BOOST_NORETURN)
#  if defined(_MSC_VER)
#    define BOOST_NORETURN __declspec(noreturn)
#  elif defined(__GNUC__) || defined(__CODEGEARC__) && defined(__clang__)
#    define BOOST_NORETURN __attribute__ ((__noreturn__))
#  elif defined(__has_attribute) && defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x5130)
#    if __has_attribute(noreturn)
#      define BOOST_NORETURN [[noreturn]]
#    endif
#  elif defined(__has_cpp_attribute)
#    if __has_cpp_attribute(noreturn)
#      define BOOST_NORETURN [[noreturn]]
#    endif
#  endif
#endif

#if !defined(BOOST_NORETURN)
#  define BOOST_NO_NORETURN
#  define BOOST_NORETURN
#endif

#if BOOST_MSVC
  #if !defined(_CPPUNWIND) && !defined(BOOST_NO_EXCEPTIONS)
    #define BOOST_NO_EXCEPTIONS
  #endif
  #if !defined(_CPPRTTI) && !defined(BOOST_NO_RTTI)
    #define BOOST_NO_RTTI
  #endif
#elif BOOST_GCC
  #if !defined(__EXCEPTIONS) && !defined(BOOST_NO_EXCEPTIONS)
    #define BOOST_NO_EXCEPTIONS
  #endif
  #if !defined(__GXX_RTTI) && !defined(BOOST_NO_RTTI)
    #define BOOST_NO_RTTI
  #endif
#elif BOOST_CLANG
  #if !__has_feature(cxx_exceptions) && !defined(BOOST_NO_EXCEPTIONS)
    #define BOOST_NO_EXCEPTIONS
  #endif
  #if !__has_feature(cxx_rtti) && !defined(BOOST_NO_RTTI)
    #define BOOST_NO_RTTI
  #endif
#endif

// This is the only predef define needed for boost::unordered, so pull it
// out here so we don't need to include all of predef.
#if \
    defined(__ARM_ARCH) || defined(__TARGET_ARCH_ARM) || \
    defined(__TARGET_ARCH_THUMB) || defined(_M_ARM) || \
    defined(__arm__) || defined(__arm64) || defined(__thumb__) || \
    defined(_M_ARM64) || defined(__aarch64__) || defined(__AARCH64EL__) || \
    defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || \
    defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || \
    defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || \
    defined(__ARM_ARCH_6KZ__) || defined(__ARM_ARCH_6T2__) || \
    defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__) || \
    defined(__ARM_ARCH_4T__) || defined(__ARM_ARCH_4__)
#define BOOST_ARCH_ARM 1
#else
#define BOOST_ARCH_ARM 0
#endif
