#pragma once
#include <chrono>

using Clock = std::chrono::steady_clock;

constexpr int TICK_RATE_HZ = 60;

constexpr auto TICK = std::chrono::nanoseconds(1000000000 / TICK_RATE_HZ);

inline int ms_until(Clock::time_point deadline) {
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                  Clock::now())
                .count();
  return ms > 0 ? (int)ms : 0;
}