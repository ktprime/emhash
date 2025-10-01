#pragma once
#ifndef EXLBR_IGNORE_BUILTIN_KEYINFO

    #include <string>

    #include "wyhash.h"

// Uncomment to switch to a simple hash.
// It is faster but causes many more collisions, so it's most likely a net loss.
// #define EXLBR_USE_SIMPLE_HASH (1)

namespace Excalibur
{

// generic type (without implementation)
template <typename T> struct KeyInfo
{
    //    static inline T getTombstone() noexcept;
    //    static inline T getEmpty() noexcept;
    //    static inline size_t hash(const T& key) noexcept;
    //    static inline bool isEqual(const T& lhs, const T& rhs) noexcept;
    //    static inline bool isValid(const T& key) noexcept;
};

template <> struct KeyInfo<int32_t>
{
    static inline bool isValid(const int32_t& key) noexcept { return key < INT32_C(0x7ffffffe); }
    static inline int32_t getTombstone() noexcept { return INT32_C(0x7fffffff); }
    static inline int32_t getEmpty() noexcept { return INT32_C(0x7ffffffe); }
    #if EXLBR_USE_SIMPLE_HASH
    static inline size_t hash(const int32_t& key) noexcept { return key * 37U; }
    #else
    static inline size_t hash(const int32_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    #endif
    static inline bool isEqual(const int32_t& lhs, const int32_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<uint32_t>
{
    static inline bool isValid(const uint32_t& key) noexcept { return key < UINT32_C(0xfffffffe); }
    static inline uint32_t getTombstone() noexcept { return UINT32_C(0xfffffffe); }
    static inline uint32_t getEmpty() noexcept { return UINT32_C(0xffffffff); }
    #if EXLBR_USE_SIMPLE_HASH
    static inline size_t hash(const uint32_t& key) noexcept { return key * 37U; }
    #else
    static inline size_t hash(const uint32_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    #endif
    static inline bool isEqual(const uint32_t& lhs, const uint32_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<int64_t>
{
    static inline bool isValid(const int64_t& key) noexcept { return key < INT64_C(0x7ffffffffffffffe); }
    static inline int64_t getTombstone() noexcept { return INT64_C(0x7fffffffffffffff); }
    static inline int64_t getEmpty() noexcept { return INT64_C(0x7ffffffffffffffe); }
    #if EXLBR_USE_SIMPLE_HASH
    static inline size_t hash(const int64_t& key) noexcept { return key * 37ULL; }
    #else
    static inline size_t hash(const int64_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    #endif
    static inline bool isEqual(const int64_t& lhs, const int64_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<uint64_t>
{
    static inline bool isValid(const uint64_t& key) noexcept { return key < UINT64_C(0xfffffffffffffffe); }
    static inline uint64_t getTombstone() noexcept { return UINT64_C(0xfffffffffffffffe); }
    static inline uint64_t getEmpty() noexcept { return UINT64_C(0xffffffffffffffff); }
    #if EXLBR_USE_SIMPLE_HASH
    static inline size_t hash(const uint64_t& key) noexcept { return key * 37ULL; }
    #else
    static inline size_t hash(const uint64_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    #endif
    static inline bool isEqual(const uint64_t& lhs, const uint64_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<std::string>
{
    static inline bool isValid(const std::string& key) noexcept { return !key.empty() && key.data()[0] != char(1); }
    static inline std::string getTombstone() noexcept
    {
        // and let's hope that small string optimization will do the job
        return std::string(1, char(1));
    }
    static inline std::string getEmpty() noexcept { return std::string(); }
    static inline size_t hash(const std::string& key) noexcept { return std::hash<std::string>{}(key); }
    static inline bool isEqual(const std::string& lhs, const std::string& rhs) noexcept { return lhs == rhs; }
};

} // namespace Excalibur

#endif