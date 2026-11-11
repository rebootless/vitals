#pragma once

#include "log.h"
#include "state.h"

#include <exception>

// the UI tick: wakes the loop to follow the terminal size; keep it below the shortest refresh (100 ms)
inline constexpr int UI_TICK_MS = 30;

// runs one data source; failures go to the log (once per outage) instead of being swallowed
template <class F>
bool sample(const char* name, F&& fn) {
    try {
        fn();
        log_source(name, true);
        return true;
    } catch (const std::exception& e) {
        log_source(name, false, e.what());
    } catch (...) {
        log_source(name, false);
    }
    return false;
}

// counters to measure the next rates against: rates must not span a pause
void take_baseline();

// samples every source, turns the counters into rates and fills G.last; tick_count is 1 for the first tick
void tick(Clock::time_point t_now, unsigned long long tick_count);
