#pragma once
#include "defines.h"
#include <string>
#ifndef PROPERTY
#define PROPERTY(t,n) __declspec( property (put = Set##n, get = Get##n)) t n
#define READONLY_PROPERTY(t,n) __declspec( property (get = Get##n) ) t n
#define WRITEONLY_PROPERTY(t,n) __declspec( property (put = Set##n) ) t n
#define GET(t,n) t Get##n() 
#define SET(t,n) void Set##n(t value)
#define PROPERTY_CPP(t,n) __declspec( property (put = Set##n, get = Get##n)); t nt Get##n();t Get##n();
#define GET_CPP(c,t,n) t c::Get##n() 
#define SET_CPP(c,t,n) void c::Set##n(t value)
#define EPROPERTY_R(t,n)READONLY_PROPERTY(t, n);GET(t, n)
#endif 
#ifndef typeof
#define typeof(x) decltype(x)
#endif

class DateTime {
private:
    ULONGLONG dateData;
    void GetSystemTime(SYSTEMTIME& sysTime) const;
    void SetSystemTime(const SYSTEMTIME& sysTime);

public:
    DateTime();
    DateTime(UINT64 _timeData);
    DateTime(FILETIME fileTime);
    DateTime(SYSTEMTIME sysTime);
    DateTime(int years, int months, int days, int hours, int minutes, int seconds, int milliseconds);
    DateTime(tm time_);
    DateTime AddYears(int years) const;
    DateTime AddMonths(int months) const;
    DateTime AddDays(int days) const;
    DateTime AddHours(int hours) const;
    DateTime AddMinutes(int minutes) const;
    DateTime AddSeconds(int seconds) const;
    DateTime AddMilliseconds(int milliseconds) const;
    DateTime AddTicks(ULONGLONG ticks) const;
    DateTime Add(int years, int months, int days, int hours, int minutes, int seconds, int milliseconds) const;
    std::string ToString() const;
    bool operator==(const DateTime& other) const;
    bool operator!=(const DateTime& other) const;
    bool operator>(const DateTime& other) const;
    bool operator<(const DateTime& other) const;
    bool operator>=(const DateTime& other) const;
    bool operator<=(const DateTime& other) const;

    READONLY_PROPERTY(UINT, Year);
    GET(UINT, Year);
    READONLY_PROPERTY(UINT, Month);
    GET(UINT, Month);
    READONLY_PROPERTY(UINT, DayOfWeek);
    GET(UINT, DayOfWeek);
    READONLY_PROPERTY(UINT, Day);
    GET(UINT, Day);
    READONLY_PROPERTY(UINT, Hour);
    GET(UINT, Hour);
    READONLY_PROPERTY(UINT, Minute);
    GET(UINT, Minute);
    READONLY_PROPERTY(UINT, Second);
    GET(UINT, Second);
    READONLY_PROPERTY(UINT, Milliseconds);
    GET(UINT, Milliseconds);
    READONLY_PROPERTY(ULONGLONG, Data);
    GET(ULONGLONG, Data);

    static DateTime Now();
    static bool IsLeapYear(int year);
    static DateTime Parse(const std::string& str);
};

