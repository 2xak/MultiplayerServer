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
  assert(sequence_newer(1, 0));
  assert(sequence_newer(101, 100));
  assert(!sequence_newer(100, 101));
  assert(!sequence_newer(5, 5));    // duplicate
  assert(sequence_newer(0, 65535)); // wrapped
  assert(!sequence_newer(65535, 0));
  assert(sequence_newer(100, 40000)); // far apart: treated as wrapped
  assert(!sequence_newer(40000, 100));
  puts("all input validation tests passed");
}