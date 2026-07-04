#pragma once

// emhash version macros
// Generated from VERSION file at repo root.
// https://github.com/ktprime/emhash

#define EMHASH_VERSION_MAJOR 1
#define EMHASH_VERSION_MINOR 1
#define EMHASH_VERSION_PATCH 0
#define EMHASH_VERSION_STRING "1.1.0"

#define EMHASH_VERSION_CODE ((EMHASH_VERSION_MAJOR << 16) | (EMHASH_VERSION_MINOR << 8) | EMHASH_VERSION_PATCH)

#define EMHASH_VERSION_AT_LEAST(major, minor, patch) \
    ((EMHASH_VERSION_MAJOR > (major)) || \
     (EMHASH_VERSION_MAJOR == (major) && EMHASH_VERSION_MINOR > (minor)) || \
     (EMHASH_VERSION_MAJOR == (major) && EMHASH_VERSION_MINOR == (minor) && EMHASH_VERSION_PATCH >= (patch)))

#define EMHASH_VERSION_BEFORE(major, minor, patch) \
    (!EMHASH_VERSION_AT_LEAST(major, minor, patch))

#ifdef __cplusplus
#   if __cplusplus >= 201703L
namespace emhash {

inline constexpr int version_major = EMHASH_VERSION_MAJOR;
inline constexpr int version_minor = EMHASH_VERSION_MINOR;
inline constexpr int version_patch = EMHASH_VERSION_PATCH;
inline constexpr int version_code = EMHASH_VERSION_CODE;
inline constexpr const char* version_string = EMHASH_VERSION_STRING;

} // namespace emhash
#   endif
#endif