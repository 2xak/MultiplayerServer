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
  uint16_t x = 0, y = 0;
  int8_t dx = 0, dy = 0;
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