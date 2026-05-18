#include "StopWatch.h"

Stopwatch* Stopwatch::StartNew() {
    Stopwatch* sw = new Stopwatch();
    sw->Restart();
    return sw;
}
bool Stopwatch::IsRunning() const {
    return _isRunning;
}
TimeSpan Stopwatch::Elapsed() {
    if (_isRunning) {
        auto now = std::chrono::high_resolution_clock::now();
        _elapsed = now - _startTime;
    }
    return TimeSpan(std::chrono::duration_cast<std::chrono::nanoseconds>(_elapsed).count() / 100);
}
void Stopwatch::Stop() {
    if (_isRunning) {
        auto now = std::chrono::high_resolution_clock::now();
        _elapsed = now - _startTime;
        _isRunning = false;
    }
}
void Stopwatch::Restart() {
    _startTime = std::chrono::high_resolution_clock::now();
    _elapsed = std::chrono::nanoseconds::zero();
    _isRunning = true;
}

Stopwatch::Stopwatch() : _isRunning(false) {}