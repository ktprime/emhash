#pragma once

#include <chrono>

#include <qc-core/core.hpp>

namespace qc
{
    //
    // Returns nanoseconds since epoch
    //
    s64 now();

    //
    // Simple clock usable for most timing-related needs
    // Period is the number of seconds per clock cycle
    // Frequency is how many cycles happen per seconds
    // Period = 1 / Frequency
    //
    class Clock
    {
        public: //--------------------------------------------------------------

        //
        // constructs clock with 1.0 sec period and 1.0 sec frequency
        //
        explicit Clock(double frequency = 1.0f);

        //
        // ...
        // the clock starts now
        //
        void restart();
        // the clock starts at the given time in nanoseconds since epoch
        void restart(s64 t);

        //
        // the fractional number of cycles since the clock started
        //
        double age() const;

        //
        // the integral number or cycles since the clock started
        //
        int cycles() const;

        //
        // the fractional part of the current cycle
        //
        double time() const;

        //
        // gets/sets the period
        //
        double period() const;
        void period(double period);

        //
        // gets/sets the frequency
        //
        double frequency() const;
        void frequency(double frequency);

        private: //-------------------------------------------------------------

        std::chrono::steady_clock::time_point _start;
        double _period;
        double _frequency;
    };
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    inline s64 now()
    {
        return std::chrono::nanoseconds(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    inline Clock::Clock(const double frequency) :
        _start(),
        _period(1.0 / frequency),
        _frequency(frequency)
    {}

    inline void Clock::restart()
    {
        _start = std::chrono::steady_clock::now();
    }

    inline void Clock::restart(const s64 t)
    {
        _start = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(t));
    }

    inline double Clock::age() const
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - _start).count() * _frequency;
    }

    inline int Clock::cycles() const
    {
        return int(age());
    }

    inline double Clock::time() const
    {
        const double a(age());
        return a - double(int(a));
    }

    inline double Clock::period() const
    {
        return _period;
    }

    inline void Clock::period(const double period)
    {
        _period = period;
        _frequency = 1.0 / period;
    }

    inline double Clock::frequency() const
    {
        return _frequency;
    }

    inline void Clock::frequency(const double frequency)
    {
        _frequency = frequency;
        _period = 1.0 / frequency;
    }
}
