#include "TimeSpan.h"
TimeSpan::TimeSpan(__int64 ticks) : _ticks(ticks) {}

TimeSpan::TimeSpan(int hours, int minutes, int seconds) {
    _ticks = hours * TicksPerHour + minutes * TicksPerMinute + seconds * TicksPerSecond;
}

TimeSpan::TimeSpan(int days, int hours, int minutes, int seconds) {
    _ticks = days * TicksPerDay + hours * TicksPerHour + minutes * TicksPerMinute + seconds * TicksPerSecond;
}

TimeSpan::TimeSpan(int days, int hours, int minutes, int seconds, int milliseconds) {
    _ticks = days * TicksPerDay + hours * TicksPerHour + minutes * TicksPerMinute
        + seconds * TicksPerSecond + milliseconds * TicksPerMillisecond;
}

void TimeSpan::Add(TimeSpan ts) {
    _ticks += ts._ticks;
}

int TimeSpan::Days() const {
    return static_cast<int>(_ticks / TicksPerDay);
}

int TimeSpan::Hours() const {
    return static_cast<int>((_ticks % TicksPerDay) / TicksPerHour);
}

int TimeSpan::Minutes() const {
    return static_cast<int>((_ticks % TicksPerHour) / TicksPerMinute);
}

int TimeSpan::Seconds() const {
    return static_cast<int>((_ticks % TicksPerMinute) / TicksPerSecond);
}

int TimeSpan::Milliseconds() const {
    return static_cast<int>((_ticks % TicksPerSecond) / TicksPerMillisecond);
}
int TimeSpan::TotalDays() const {
    return static_cast<int>(_ticks / TicksPerDay);
}

int TimeSpan::TotalHours() const {
    return static_cast<int>(_ticks / TicksPerHour);
}

int TimeSpan::TotalMinutes() const {
    return static_cast<int>(_ticks / TicksPerMinute);
}

int TimeSpan::TotalSeconds() const {
    return static_cast<int>(_ticks / TicksPerSecond);
}

int TimeSpan::TotalMilliseconds() const {
    return static_cast<int>(_ticks / TicksPerMillisecond);
}

__int64 TimeSpan::Ticks() const {
    return _ticks;
}
bool TimeSpan::operator==(TimeSpan ts) const {
    return _ticks == ts._ticks;
}

bool TimeSpan::operator!=(TimeSpan ts) const {
    return _ticks != ts._ticks;
}

bool TimeSpan::operator<(TimeSpan ts) const {
    return _ticks < ts._ticks;
}

bool TimeSpan::operator<=(TimeSpan ts) const {
    return _ticks <= ts._ticks;
}

bool TimeSpan::operator>(TimeSpan ts) const {
    return _ticks > ts._ticks;
}

bool TimeSpan::operator>=(TimeSpan ts) const {
    return _ticks >= ts._ticks;
}

TimeSpan TimeSpan::operator+(TimeSpan ts) const {
    return TimeSpan(_ticks + ts._ticks);
}

TimeSpan TimeSpan::operator-(TimeSpan ts) const {
    return TimeSpan(_ticks - ts._ticks);
}

TimeSpan& TimeSpan::operator+=(TimeSpan ts) {
    _ticks += ts._ticks;
    return *this;
}

TimeSpan& TimeSpan::operator-=(TimeSpan ts) {
    _ticks -= ts._ticks;
    return *this;
}