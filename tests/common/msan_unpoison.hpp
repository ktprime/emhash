// common/msan_unpoison.hpp
// Auto-included via -include in MSan builds to unpoison std::cout/cerr/clog
// before doctest writes to them. Without this, MSan reports false-positive
// use-of-uninitialized-value because the uninstrumented libc++ runtime that
// initializes the std iostream globals is not MSan-instrumented.
//
// This header includes <iostream> which provides std::ios_base::Init,
// ensuring the standard streams are initialized before the global
// MsanStreamUnpoisoner constructor runs (static init order within a TU).
#pragma once

#include <iostream>

#if defined(__SANITIZE_MEMORY__) || (defined(__has_feature) && __has_feature(memory_sanitizer))
#include <sanitizer/msan_interface.h>

namespace emhash_test {

// Global constructor: runs after std::ios_base::Init from <iostream>.
// libc++ marks its std::ios_base::Init with init_priority(101); use 102
// to guarantee MsanStreamUnpoisoner is constructed after the streams are
// fully initialized, then unpoison their live state.
struct MsanStreamUnpoisoner {
    MsanStreamUnpoisoner() {
        // Unpoison the stream objects themselves (format flags, sentry, etc.)
        __msan_unpoison(&std::cout, sizeof(std::cout));
        __msan_unpoison(&std::cerr, sizeof(std::cerr));
        __msan_unpoison(&std::clog, sizeof(std::clog));

        // Unpoison the streambufs. A generous bound (1 KB) covers the
        // internal state of std::filebuf / std::stringbuf on all platforms.
        auto* cout_buf = std::cout.rdbuf();
        if (cout_buf)
            __msan_unpoison(cout_buf, 1024);
        auto* cerr_buf = std::cerr.rdbuf();
        if (cerr_buf)
            __msan_unpoison(cerr_buf, 1024);
        auto* clog_buf = std::clog.rdbuf();
        if (clog_buf)
            __msan_unpoison(clog_buf, 1024);
    }
};

// Static instance: constructed during global init, before main().
// `init_priority(102)` ensures this runs after libc++'s std::ios_base::Init
// (which uses init_priority(101)). `init_priority` applies to the variable,
// not the struct definition. `inline` (C++17) guarantees exactly one instance
// across TUs when this header is force-included into multiple TUs.
__attribute__((init_priority(102))) inline MsanStreamUnpoisoner msan_stream_unpoisoner;

} // namespace emhash_test

#endif // MSan active