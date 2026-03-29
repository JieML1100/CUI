#include "DateTime.h"
#include <sstream>
#include <iomanip>
#include <ctime>
void DateTime::GetSystemTime(SYSTEMTIME& sysTime) const {
    FILETIME fileTime;
    fileTime.dwLowDateTime = (DWORD)(dateData & 0xFFFFFFFF);
    fileTime.dwHighDateTime = (DWORD)(dateData >> 32);
    FileTimeToSystemTime(&fileTime, &sysTime);
}

void DateTime::SetSystemTime(const SYSTEMTIME& sysTime) {
    FILETIME fileTime;
    if (SystemTimeToFileTime(&sysTime, &fileTime))
        dateData = (((ULONGLONG)fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
    else
        dateData = 0;
}
DateTime::DateTime() {
    dateData = 0;
}

DateTime::DateTime(UINT64 _timeData) {
    dateData = _timeData;
}

DateTime::DateTime(FILETIME fileTime) {
    dateData = (((ULONGLONG)fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
}

DateTime::DateTime(SYSTEMTIME sysTime) {
    SetSystemTime(sysTime);
}

DateTime::DateTime(int years, int months, int days, int hours, int minutes, int seconds, int milliseconds) {
    SYSTEMTIME sysTime = { 0 };
    sysTime.wYear = years;
    sysTime.wMonth = months;
    sysTime.wDay = days;
    sysTime.wHour = hours;
    sysTime.wMinute = minutes;
    sysTime.wSecond = seconds;
    sysTime.wMilliseconds = milliseconds;
    SetSystemTime(sysTime);
}

DateTime::DateTime(tm time_) {
    SYSTEMTIME sysTime = { 0 };
    sysTime.wYear = time_.tm_year + 1900;
    sysTime.wMonth = time_.tm_mon + 1;
    sysTime.wDay = time_.tm_mday;
    sysTime.wHour = time_.tm_hour;
    sysTime.wMinute = time_.tm_min;
    sysTime.wSecond = time_.tm_sec;
    sysTime.wMilliseconds = 0;
    SetSystemTime(sysTime);
}

DateTime DateTime::AddYears(int years) const {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    sysTime.wYear += years;
    DateTime newDateTime;
    newDateTime.SetSystemTime(sysTime);
    return newDateTime;
}

DateTime DateTime::AddMonths(int months) const {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);

    int y = sysTime.wYear;
    int m = sysTime.wMonth;

    int totalMonths = y * 12 + m - 1 + months;

    y = totalMonths / 12;
    m = totalMonths % 12;

    if (m < 0) {
        m += 12;
        y -= 1;
    }
    sysTime.wYear = y;
    sysTime.wMonth = m + 1;
    static const int daysInMonth[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    int maxDay = daysInMonth[sysTime.wMonth - 1];

    if (sysTime.wMonth == 2 && IsLeapYear(sysTime.wYear))
        maxDay = 29;
    if (sysTime.wDay > maxDay)
        sysTime.wDay = maxDay;

    DateTime newDateTime;
    newDateTime.SetSystemTime(sysTime);
    return newDateTime;
}

DateTime DateTime::AddDays(int days) const {
    return AddTicks((ULONGLONG)days * 864000000000ULL);
}

DateTime DateTime::AddHours(int hours) const {
    return AddTicks((ULONGLONG)hours * 36000000000ULL);
}

DateTime DateTime::AddMinutes(int minutes) const {
    return AddTicks((ULONGLONG)minutes * 600000000ULL);
}

DateTime DateTime::AddSeconds(int seconds) const {
    return AddTicks((ULONGLONG)seconds * 10000000ULL);
}

DateTime DateTime::AddMilliseconds(int milliseconds) const {
    return AddTicks((ULONGLONG)milliseconds * 10000ULL);
}

DateTime DateTime::AddTicks(ULONGLONG ticks) const {
    DateTime newDateTime;
    newDateTime.dateData = dateData + ticks;
    return newDateTime;
}

DateTime DateTime::Add(int years, int months, int days, int hours, int minutes, int seconds, int milliseconds) const {
    DateTime tempDateTime = AddYears(years).AddMonths(months);
    tempDateTime = tempDateTime.AddDays(days).AddHours(hours).AddMinutes(minutes).AddSeconds(seconds).AddMilliseconds(milliseconds);
    return tempDateTime;
}

std::string DateTime::ToString() const {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    char buffer[100];
    sprintf_s(buffer, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
    return std::string(buffer);
}

bool DateTime::operator==(const DateTime& other) const {
    return dateData == other.dateData;
}

bool DateTime::operator!=(const DateTime& other) const {
    return dateData != other.dateData;
}

bool DateTime::operator>(const DateTime& other) const {
    return dateData > other.dateData;
}

bool DateTime::operator<(const DateTime& other) const {
    return dateData < other.dateData;
}

bool DateTime::operator>=(const DateTime& other) const {
    return dateData >= other.dateData;
}

bool DateTime::operator<=(const DateTime& other) const {
    return dateData <= other.dateData;
}

GET_CPP(DateTime, UINT, Year) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wYear;
}

GET_CPP(DateTime, UINT, Month) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wMonth;
}

GET_CPP(DateTime, UINT, DayOfWeek) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wDayOfWeek;
}

GET_CPP(DateTime, UINT, Day) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wDay;
}

GET_CPP(DateTime, UINT, Hour) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wHour;
}

GET_CPP(DateTime, UINT, Minute) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wMinute;
}

GET_CPP(DateTime, UINT, Second) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wSecond;
}

GET_CPP(DateTime, UINT, Milliseconds) {
    SYSTEMTIME sysTime;
    GetSystemTime(sysTime);
    return sysTime.wMilliseconds;
}

GET_CPP(DateTime, ULONGLONG, Data) {
    return this->dateData;
}
DateTime DateTime::Now() {
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);
    return DateTime(fileTime);
}

bool DateTime::IsLeapYear(int year) {
    if (year % 4 != 0)
        return false;
    else if (year % 100 != 0)
        return true;
    else if (year % 400 != 0)
        return false;
    else
        return true;
}

DateTime DateTime::Parse(const std::string& str) {
    int year, month, day, hour, minute, second, millisecond = 0;
    int count = sscanf_s(str.c_str(), "%d-%d-%d %d:%d:%d.%d",
        &year, &month, &day, &hour, &minute, &second, &millisecond);
    if (count >= 6) {
        return DateTime(year, month, day, hour, minute, second, millisecond);
    }
    else {
        return DateTime();
    }
}