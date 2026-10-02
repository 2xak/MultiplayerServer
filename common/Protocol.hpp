#pragma once
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

enum MsgType : uint8_t {
  MSG_JOIN = 1,
  MSG_INPUT = 2,
  MSG_SNAPSHOT = 3,
  MSG_WELCOME = 4
};

constexpr size_t MAX_PLAYERS = 100;
constexpr size_t MAX_PACKET = 512;

struct Writer {
  uint8_t *buffer;
  size_t capacity;
  size_t position = 0;

  bool u8(uint8_t value) {
    if (position + sizeof(value) > capacity)
      return false;
    buffer[position++] = value;
    return true;
  }

  bool u16(uint16_t value) {
    if (position + sizeof(value) > capacity)
      return false;
    value = htons(value);
    std::memcpy(buffer + position, &value, sizeof(value));
    position += sizeof(value);
    return true;
  }
};

struct Reader {
  const uint8_t *buffer;
  size_t length;
  size_t position = 0;

  bool u8(uint8_t &value) {
    if (position + sizeof(value) > length)
      return false;
    value = buffer[position++];
    return true;
  }

  bool u16(uint16_t &value) {
    if (position + sizeof(value) > length)
      return false;
    std::memcpy(&value, buffer + position, sizeof(value));
    value = ntohs(value);
    position += sizeof(value);
    return true;
  }

  bool done() const { return position == length; }
};

struct PlayerState {
  int16_t x, y;
  uint8_t id;
};

struct JoinMsg {};

struct InputMsg {
  uint16_t sequence;
  int8_t dx, dy;
};

struct SnapshotMsg {
  uint16_t sequence;
  uint8_t count;
  PlayerState players[MAX_PLAYERS];
};

struct WelcomeMsg {
  uint8_t id;
};

inline bool read_type(Reader &reader, uint8_t &type) { return reader.u8(type); }

inline bool write_join(Writer &writer, const JoinMsg &) {
  return writer.u8(MSG_JOIN);
}
inline bool read_join(Reader &reader, JoinMsg &) { return reader.done(); }

inline bool write_input(Writer &writer, const InputMsg &msg) {
  return writer.u8(MSG_INPUT) && writer.u16(msg.sequence) &&
         writer.u8((uint8_t)msg.dx) && writer.u8((uint8_t)msg.dy);
}

inline bool read_input(Reader &reader, InputMsg &msg) {
  uint8_t dx, dy;
  if (!reader.u16(msg.sequence) || !reader.u8(dx) || !reader.u8(dy))
    return false;
  msg.dx = (int8_t)dx;
  msg.dy = (int8_t)dy;
  return reader.done();
}

inline bool write_snapshot(Writer &writer, const SnapshotMsg &msg) {
  if (!writer.u8(MSG_SNAPSHOT) || !writer.u16(msg.sequence) ||
      !writer.u8(msg.count))
    return false;
  for (uint8_t i = 0; i < msg.count; ++i) {
    if (!writer.u8(msg.players[i].id) ||
        !writer.u16((uint16_t)msg.players[i].x) ||
        !writer.u16((uint16_t)msg.players[i].y))
      return false;
  }
  return true;
}
inline bool read_snapshot(Reader &reader, SnapshotMsg &msg) {
  if (!reader.u16(msg.sequence) || !reader.u8(msg.count))
    return false;
  if (msg.count > MAX_PLAYERS)
    return false;
  for (uint8_t i = 0; i < msg.count; ++i) {
    uint8_t id;
    uint16_t x, y;
    if (!reader.u8(id) || !reader.u16(x) || !reader.u16(y))
      return false;
    msg.players[i].id = id;
    msg.players[i].x = (int16_t)x;
    msg.players[i].y = (int16_t)y;
  }
  return reader.done();
}

inline bool write_welcome(Writer &writer, const WelcomeMsg &msg) {
  return writer.u8(MSG_WELCOME) && writer.u8(msg.id);
}
inline bool read_welcome(Reader &reader, WelcomeMsg &msg) {
  return reader.u8(msg.id) && reader.done();
}