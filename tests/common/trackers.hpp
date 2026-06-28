// common/trackers.hpp
// LeakTracker: instrumented value type counting constructions/destructions.
// Extracted from verify/test_string_key_leak.cpp:59 to be shared across all
// memory/leak tests without re-definition.
#pragma once

#include <cstddef>
#include <functional>
#include <string>

struct LeakTracker {
    static int s_alive;
    static int s_constructed;
    static int s_destructed;

    static void reset() { s_alive = s_constructed = s_destructed = 0; }
    static bool clean() { return s_alive == 0; }
    static int constructed() { return s_constructed; }
    static int destructed() { return s_destructed; }

    std::string data;

    LeakTracker() : data("default") { s_alive++; s_constructed++; }
    explicit LeakTracker(const std::string& s) : data(s) { s_alive++; s_constructed++; }
    LeakTracker(const LeakTracker& o) : data(o.data) { s_alive++; s_constructed++; }
    LeakTracker(LeakTracker&& o) noexcept : data(std::move(o.data)) { s_alive++; s_constructed++; }
    LeakTracker& operator=(const LeakTracker& o) { data = o.data; return *this; }
    LeakTracker& operator=(LeakTracker&& o) noexcept { data = std::move(o.data); return *this; }
    ~LeakTracker() { s_alive--; s_destructed++; }

    bool operator==(const LeakTracker& o) const { return data == o.data; }
    bool operator!=(const LeakTracker& o) const { return data != o.data; }
    operator std::string() const { return data; }
};

inline int LeakTracker::s_alive = 0;
inline int LeakTracker::s_constructed = 0;
inline int LeakTracker::s_destructed = 0;

namespace std {
template <>
struct hash<LeakTracker> {
    std::size_t operator()(const LeakTracker& t) const {
        return std::hash<std::string>()(t.data);
    }
};
} // namespace std
