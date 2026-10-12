#pragma once
#include "Game.hpp"
#include "Protocol.hpp"
#include <deque>

inline Position reconcile(Position server_pos, uint16_t ack,
                          std::deque<InputMsg> &pending) {
  while (!pending.empty() && !sequence_newer(pending.front().sequence, ack))
    pending.pop_front();

  Position p = server_pos;
  for (const InputMsg &input : pending)
    p = simulate_step(p, input.dx, input.dy);

  return p;
}