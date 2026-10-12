#include "Prediction.hpp"
#include "Protocol.hpp"
#include "Timing.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

struct PendingPing {
  uint16_t sequence = 0;
  Clock::time_point sent;
  bool active = false; // true = waiting for pong
};

static int join_server(int fd) {
  uint8_t out[MAX_PACKET];
  uint8_t buf[MAX_PACKET];

  for (;;) {
    Writer writer{out, sizeof out};
    write_join(writer, JoinMsg{});
    send(fd, out, writer.position, 0);

    auto deadline = Clock::now() + RETRY_INTERVAL;
    for (int wait = ms_until(deadline); wait > 0; wait = ms_until(deadline)) {
      pollfd pfd{fd, POLLIN, 0};
      if (poll(&pfd, 1, wait) <= 0)
        break;

      ssize_t n = recv(fd, buf, sizeof buf, 0);
      if (n < 0)
        continue;

      Reader reader{buf, (size_t)n};
      uint8_t type;
      if (!read_type(reader, type) || type != MSG_WELCOME)
        continue;

      WelcomeMsg welcome;
      if (read_welcome(reader, welcome))
        return welcome.id;
    }
  }
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IOLBF, 0);
  srand(time(nullptr) ^ getpid());

  // bot_test [ip] [local_port] [--drop=PERCENT]
  const char *ip = "127.0.0.1";
  int local_port = 0;
  int drop_percent = 0;
  int positional = 0;
  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], "--drop=", 7) == 0)
      drop_percent = atoi(argv[i] + 7);
    else if (positional == 0) {
      ip = argv[i];
      ++positional;
    } else if (positional == 1) {
      local_port = atoi(argv[i]);
      ++positional;
    }
  }
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  // fixed local port for rejoin test
  if (local_port > 0) {
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons((uint16_t)local_port);
    if (bind(fd, (sockaddr *)&local, sizeof local) < 0) {
      perror("bind");
      return 1;
    }
  }

  // init adress
  sockaddr_in srv{};
  srv.sin_family = AF_INET;
  srv.sin_port = htons(12345);
  inet_pton(AF_INET, ip, &srv.sin_addr);
  connect(fd, (sockaddr *)&srv, sizeof srv);

  uint8_t out[MAX_PACKET];
  uint8_t buf[MAX_PACKET];

  int my_id = join_server(fd);
  printf("my id: %d\n", my_id);

  // variables
  uint16_t sequence = 0;
  uint16_t last_snapshot = 0;
  bool has_snapshot = false;
  int8_t dir_x = 0, dir_y = 0;
  // variables for ping
  constexpr size_t PING_SLOTS = 64;
  PendingPing pending_pings[PING_SLOTS];
  uint16_t ping_sequence = 0;
  double smoothed_rtt = 0;
  bool has_rtt = false;
  // schedules var for server silence dectection
  auto next_input = Clock::now();
  auto next_ping = Clock::now() + PING_INTERVAL;
  auto last_packet = Clock::now();
  // prediction var
  Position predicted = SPAWN;
  std::deque<InputMsg> pending_inputs;
  constexpr size_t MAX_PENDING = 256;
  int corrections = 0;

  for (;;) {
    // send input msg
    if (Clock::now() >= next_input) {
      if (sequence % TICK_RATE_HZ == 0) {
        dir_x = (int8_t)(rand() % 3 - 1);
        dir_y = (int8_t)(rand() % 3 - 1);
      }
      InputMsg input{++sequence, dir_x, dir_y};

      predicted = simulate_step(predicted, input.dx, input.dy);
      pending_inputs.push_back(input);
      if (pending_inputs.size() > MAX_PENDING)
        pending_inputs.pop_front();

      // lose packet test
      if (rand() % 100 >= drop_percent) {
        Writer writer{out, sizeof out};
        if (write_input(writer, input))
          send(fd, out, writer.position, 0);
      }
      next_input += TICK;
    }

    // send a ping on interval
    if (Clock::now() >= next_ping) {
      uint16_t ping_seq = ++ping_sequence;
      pending_pings[ping_seq % PING_SLOTS] = {ping_seq, Clock::now(), true};

      Writer p_writer{out, sizeof out};
      if (write_ping(p_writer, PingMsg{ping_seq}))
        send(fd, out, p_writer.position, 0);

      next_ping += PING_INTERVAL;
    }

    // poll and receive based on msg type
    auto next_event = next_input < next_ping ? next_input : next_ping;
    pollfd pfd{fd, POLLIN, 0};
    if (poll(&pfd, 1, ms_until(next_event)) > 0) {
      SnapshotMsg latest;
      bool got_snapshot = false;

      ssize_t n;
      while ((n = recv(fd, buf, sizeof buf, MSG_DONTWAIT)) > 0) {
        Reader reader{buf, (size_t)n};
        uint8_t type;
        if (!read_type(reader, type))
          continue;

        last_packet = Clock::now();

        switch (type) {
        case MSG_SNAPSHOT: {
          SnapshotMsg snap;
          if (read_snapshot(reader, snap) &&
              (!got_snapshot ||
               sequence_newer(snap.sequence, latest.sequence))) {
            latest = snap;
            got_snapshot = true;
          }
          break;
        }
        case MSG_PONG: {
          PongMsg pong;
          if (!read_pong(reader, pong))
            break;

          PendingPing &slot = pending_pings[pong.sequence % PING_SLOTS];
          if (slot.active && slot.sequence == pong.sequence) {
            double rtt = std::chrono::duration<double, std::milli>(
                             Clock::now() - slot.sent)
                             .count();

            slot.active = false;
            // SRTT formula
            smoothed_rtt = has_rtt ? 0.875 * smoothed_rtt + 0.125 * rtt : rtt;
            has_rtt = true;
            printf("rtt %.2f ms (avg %.2f ms)\n", rtt, smoothed_rtt);
          }
          break;
        }
        default:
          break;
        }

        // use the newest and latest snapshot
        if (got_snapshot &&
            (!has_snapshot || sequence_newer(latest.sequence, last_snapshot))) {
          has_snapshot = true;
          ;
          last_snapshot = latest.sequence;

          for (uint8_t i = 0; i < latest.count; ++i) {
            if (latest.players[i].id == my_id) {
              Position server_pos{latest.players[i].x, latest.players[i].y};

              Position corrected = reconcile(
                  server_pos, latest.last_processed_input, pending_inputs);
              if (corrected.x != predicted.x || corrected.y != predicted.y) {
                ++corrections;
                printf("correction: predicted (%d, %d) -> (%d, %d)\n",
                       predicted.x, predicted.y, corrected.x, corrected.y);
                predicted = corrected;
              }
              printf("snapshot #%u: server (%d,%d) me (%d, %d), pending %zu, "
                     "corrections %d, "
                     "%u players\n",
                     latest.sequence, server_pos.x, server_pos.y, predicted.x,
                     predicted.y, pending_inputs.size(), corrections,
                     latest.count);

              break;
            }
          }
        }
      }
    }

    // server silence handle
    if (Clock::now() - last_packet > SERVER_SILENCE_TIMEOUT) {
      printf("server silent, rejoining...\n");

      my_id = join_server(fd);
      printf("rejoined, my id: %d\n", my_id);

      sequence = 0;
      has_snapshot = false;
      predicted = SPAWN;
      pending_inputs.clear();
      for (auto &slot : pending_pings)
        slot.active = false;
      has_rtt = false;
      smoothed_rtt = 0;

      next_input = Clock::now();
      next_ping = Clock::now() + PING_INTERVAL;
      last_packet = Clock::now();
    }
  }
}