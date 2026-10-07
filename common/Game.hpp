#pragma once
#include <algorithm>
#include <cstdint>

#include "Timing.hpp"

constexpr int WORLD_W = 4000;
constexpr int WORLD_H = 4000;
static_assert(WORLD_W < 32767 && WORLD_H < 32767,
              "smaller than int16_t type for position");

constexpr int SPEED_PER_SEC = 600;
static_assert(SPEED_PER_SEC % TICK_RATE_HZ == 0,
              "speed must divide evenly by tick rate");
constexpr int STEP = SPEED_PER_SEC / TICK_RATE_HZ;

constexpr int DIAG_NUM = 181; // 1/sqrt(2)
constexpr int DIAG_DEN = 256;
constexpr int DIAG_STEP = STEP * DIAG_NUM / DIAG_DEN;

struct Position {
  int16_t x = 0;
  int16_t y = 0;
};

constexpr Position SPAWN{WORLD_W / 2, WORLD_H / 2};

inline Position simulate_step(Position p, const int dx, const int dy) {
  int step = (dx != 0 && dy != 0) ? DIAG_STEP : STEP;

  int nx = p.x + dx * step;
  int ny = p.y + dy * step;

  return Position{(int16_t)std::clamp(nx, 0, WORLD_W),
                  (int16_t)std::clamp(ny, 0, WORLD_H)};
}