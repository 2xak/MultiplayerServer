#include "Protocol.hpp"
#include "Timing.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

struct Player {
  sockaddr_in addr;
  int16_t x = 0, y = 0;
  int8_t dx = 0, dy = 0;
  uint8_t id = 0;
  uint32_t bad_packets = 0;
  Clock::time_point last_input = Clock::now();
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

  auto next_tick = Clock::now() + TICK;
  std::map<uint64_t, Player> players;
  uint16_t snap_sequence = 0;
  uint8_t next_id = 1;

  for (;;) {
    // poll for incoming packets or until the next tick
    auto wait = ms_until(next_tick);
    pollfd pfd{fd, POLLIN, 0};
    if (wait > 0)
      poll(&pfd, 1, wait);

    // loop over all incoming packets
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

        // find the player or create a new one if not found
        if (it == players.end()) {
          if (players.size() >= MAX_PLAYERS)
            break;

          Player player;
          player.addr = from;
          player.id = next_id++;
          it = players.emplace(key, player).first;
          printf("player %d joined (%zu online)\n", it->second.id,
                 players.size());
        }

        // do stuffs to the player found or created above
        it->second.last_input = Clock::now();

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
        if (!read_input(r, msg) || it == players.end())
          break;

        Player &player = it->second;
        player.last_input = Clock::now();

        if (!is_valid_input(msg)) {
          ++player.bad_packets;
          if (player.bad_packets == 1 || player.bad_packets % 100 == 0)
            printf("player %d sent invalid input (%u so far)\n", player.id,
                   player.bad_packets);
          break;
        }

        player.dx = msg.dx;
        player.dy = msg.dy;
        break;
      }
      default:
        break;
      }
    }

    // tick
    if (Clock::now() >= next_tick) {
      // remove timed out players
      auto now = Clock::now();
      for (auto it = players.begin(); it != players.end();) {
        if (now - it->second.last_input > PLAYER_TIMEOUT) {
          printf("player %d timed out (%zu online)\n", it->second.id,
                 players.size() - 1);
          it = players.erase(it);
        } else {
          ++it;
        }
      }

      // simulate player movement
      for (auto &[key, player] : players) {
        player.x += player.dx;
        player.y += player.dy;
      }

      // send snapshot to all players
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