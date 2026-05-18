#pragma once
#include "defines.h"
class TimeSpan {
    static const __int64 TicksPerMillisecond = 10000;
    static const __int64 TicksPerSecond = TicksPerMillisecond * 1000;
    static const __int64 TicksPerMinute = TicksPerSecond * 60;
    static const __int64 TicksPerHour = TicksPerMinute * 60;
    static const __int64 TicksPerDay = TicksPerHour * 24;
public:
    TimeSpan(__int64 ticks = 0);
    TimeSpan(int hours, int minutes, int seconds);
    TimeSpan(int days, int hours, int minutes, int seconds);
    TimeSpan(int days, int hours, int minutes, int seconds, int milliseconds);
    void Add(TimeSpan ts);
    int Days() const;
    int Hours() const;
    int Minutes() const;
    int Seconds() const;
    int Milliseconds() const;
    int TotalDays() const;
    int TotalHours() const;
    int TotalMinutes() const;
    int TotalSeconds() const;
    int TotalMilliseconds() const;
    __int64 Ticks() const;
    bool operator==(TimeSpan ts) const;
    bool operator!=(TimeSpan ts) const;
    bool operator<(TimeSpan ts) const;
    bool operator<=(TimeSpan ts) const;
    bool operator>(TimeSpan ts) const;
    bool operator>=(TimeSpan ts) const;
    TimeSpan operator+(TimeSpan ts) const;
    TimeSpan operator-(TimeSpan ts) const;
    TimeSpan& operator+=(TimeSpan ts);
    TimeSpan& operator-=(TimeSpan ts);
private:
    __int64 _ticks;
};
