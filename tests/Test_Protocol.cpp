#include "Protocol.hpp"
#include <cassert>
#include <cstdio>

static bool roundtrip(InputMsg in) {
  uint8_t buf[MAX_PACKET];
  Writer writer{buf, sizeof buf};
  write_input(writer, in); // encode, with no checks on the way out
  Reader reader{buf, writer.position};
  uint8_t type;
  read_type(reader, type);
  InputMsg out{};
  return read_input(reader, out); // does the receiver accept it?
}

int main() {
  assert(roundtrip({1, 0, 0}));
  assert(roundtrip({1, 1, -1}));
  assert(roundtrip({65535, -1, 1}));
  assert(!roundtrip({1, 2, 0}));
  assert(!roundtrip({1, 0, -2}));
  assert(!roundtrip({1, 100, 0}));
  assert(!roundtrip({1, 0, -128}));
  puts("all input validation tests passed");
}