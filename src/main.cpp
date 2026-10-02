#include "Protocol.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

struct Player {
  sockaddr_in addr;
  int16_t x = 0, y = 0;
  int8_t dx = 0, dy = 0;
  uint8_t id = 0;
};

static uint64_t key_of(const sockaddr_in &addr) {
  return (uint64_t)addr.sin_addr.s_addr << 16 | addr.sin_port;
}

int main() {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
    perror("fcntl");
    return 1;
  }

  sockaddr_in me{};
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl(INADDR_ANY);
  me.sin_port = htons(12345);
  if (bind(fd, (sockaddr *)&me, sizeof me) < 0) {
    perror("bind");
    return 1;
  }

  constexpr auto TICK = std::chrono::nanoseconds(1000000000 / 60);
  auto next_tick = Clock::now() + TICK;
  std::map<uint64_t, Player> players;
  uint16_t snap_sequence = 0;
  uint8_t next_id = 1;

  for (;;) {
    auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(
                    next_tick - Clock::now())
                    .count();
    pollfd pfd{fd, POLLIN, 0};
    if (wait > 0)
      poll(&pfd, 1, (int)wait);

    uint8_t buf[MAX_PACKET];
    sockaddr_in from;
    for (;;) {
      socklen_t len = sizeof from;
      ssize_t n = recvfrom(fd, buf, sizeof buf, 0, (sockaddr *)&from, &len);
      if (n < 0) {
        break;
      }

      Reader r{buf, (size_t)n};
      uint8_t type;
      if (!read_type(r, type))
        continue;

      switch (type) {
      case MSG_JOIN: {
        JoinMsg msg;

        if (!read_join(r, msg))
          break;

        uint64_t key = key_of(from);
        auto it = players.find(key);
        if (it == players.end()) {
          if (players.size() >= MAX_PLAYERS)
            break;

          Player player;
          player.addr = from;
          player.id = next_id++;
          it = players.emplace(key, player).first;
        }

        uint8_t out[MAX_PACKET];
        Writer writer{out, sizeof out};
        WelcomeMsg welcome{it->second.id};
        if (write_welcome(writer, welcome))
          sendto(fd, out, writer.position, 0, (sockaddr *)&from, sizeof from);

        break;
      }
      case MSG_INPUT: {
        InputMsg msg;
        auto it = players.find(key_of(from));
        if (read_input(r, msg) && it != players.end()) {
          it->second.dx = msg.dx;
          it->second.dy = msg.dy;
        }
        break;
      }
      default:
        break;
      }

      if (Clock::now() >= next_tick) {
        for (auto &[key, player] : players) {
          player.x += player.dx;
          player.y += player.dy;
        }

        SnapshotMsg s;
        s.sequence = ++snap_sequence;
        s.count = 0;
        for (auto &[key, player] : players)
          s.players[s.count++] = {player.x, player.y, player.id};

        uint8_t out[MAX_PACKET];
        Writer w{out, sizeof out};
        if (write_snapshot(w, s)) {
          for (auto &[key, player] : players) {
            sendto(fd, out, w.position, 0, (sockaddr *)&player.addr,
                   sizeof player.addr);
          }
        }
        next_tick += TICK;
      }
    }
  }
}