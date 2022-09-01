#pragma once

#include <cerrno>
#include <vector>
#include <memory>


#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || defined(MEMORY_SANITIZER) || defined(UNDEFINED_BEHAVIOR_SANITIZER)
#define ABORT_ON_LOGICAL_ERROR
#endif

namespace DB
{

class Exception
{
public:
    using FramePointers = std::vector<void *>;

    Exception() = default;
    Exception(const std::string & msg, int code, bool remote_ = false);
    Exception(const std::string & msg, const Exception & nested, int code);

    Exception(int code, const std::string & message)
        : Exception(message, code)
    {}
};
}