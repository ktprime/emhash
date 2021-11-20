#pragma once

/// Copied from: https://gist.github.com/maddouri/0da889b331d910f35e05ba3b7b9d869b
/// Alternative solution for C++14: https://medium.com/@LoopPerfect/c-17-vs-c-14-if-constexpr-b518982bb1e2
/// Alternative solution for C++20: https://brevzin.github.io/c++/2019/01/15/if-constexpr-isnt-broken/
#define define_has_member(member_name)                                  \
    template <typename T>                                                      \
    class has_member_##member_name                                             \
    {                                                                          \
        typedef char yes_type;                                                 \
        typedef long no_type;                                                  \
        template <typename U> static yes_type test(decltype(&U::member_name)); \
        template <typename U> static no_type  test(...);                       \
    public:                                                                    \
        static constexpr bool value = sizeof(test<T>(0)) == sizeof(yes_type);  \
    }

/// Shorthand for testing if "class_" has a member called "member_name"
///
/// @note "define_has_member(member_name)" must be used
///       before calling "has_member(class_, member_name)"
#define has_member(class_, member_name)  has_member_##member_name<class_>::value
