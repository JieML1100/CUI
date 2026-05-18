#pragma once
#include "defines.h"
#include "TimeSpan.h"
#include <chrono>
class Stopwatch {
public:
    static Stopwatch* StartNew();
    bool IsRunning() const;
    TimeSpan Elapsed();
    void Stop();
    void Restart();

private:
    Stopwatch();
    std::chrono::time_point<std::chrono::high_resolution_clock> _startTime;
    std::chrono::nanoseconds _elapsed;
    bool _isRunning;
};
