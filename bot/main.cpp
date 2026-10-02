#include "Protocol.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
  srand(time(nullptr) ^ getpid());

  const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  // init adress
  sockaddr_in srv{};
  srv.sin_family = AF_INET;
  srv.sin_port = htons(12345);
  inet_pton(AF_INET, ip, &srv.sin_addr);
  connect(fd, (sockaddr *)&srv, sizeof srv);

  uint8_t out[MAX_PACKET];
  uint8_t buf[MAX_PACKET];

  int my_id = -1;
  while (my_id < 0) {
    Writer writer{out, sizeof out};
    write_join(writer, JoinMsg{});
    send(fd, out, writer.position, 0);

    pollfd pfd{fd, POLLIN, 0};
    if (poll(&pfd, 1, 500) > 0) {
      ssize_t n = recv(fd, buf, sizeof buf, 0);
      if (n < 0)
        continue;

      Reader reader{buf, (size_t)n};
      uint8_t type;
      if (!read_type(reader, type) || type != MSG_WELCOME)
        continue;

      WelcomeMsg welcome;
      if (!read_welcome(reader, welcome))
        continue;

      my_id = welcome.id;
    }
  }
  printf("my id: %d\n", my_id);

  uint16_t sequence = 0;
  for (;;) {
    InputMsg input{++sequence, (int8_t)(rand() % 3 - 1),
                   (int8_t)(rand() % 3 - 1)};
    Writer writer{out, sizeof out};
    if (write_input(writer, input))
      send(fd, out, writer.position, 0);

    pollfd pfd{fd, POLLIN, 0};
    if (poll(&pfd, 1, 50) > 0) {
      uint8_t buf[MAX_PACKET];
      ssize_t n, last = -1;
      while ((n = recv(fd, buf, sizeof buf, MSG_DONTWAIT)) > 0)
        last = n;

      if (last > 0) {
        Reader reader{buf, (size_t)last};
        uint8_t type;
        SnapshotMsg snap;
        if (read_type(reader, type) && type == MSG_SNAPSHOT &&
            read_snapshot(reader, snap)) {
          for (uint8_t i = 0; i < snap.count; ++i) {
            if (snap.players[i].id == my_id) {
              printf("snapshot #%u: me at (%d, %d), %u players\n",
                     snap.sequence, snap.players[i].x, snap.players[i].y,
                     snap.count);
              break;
            }
            fflush(stdout);
          }
        }
      }
    }
  }
}