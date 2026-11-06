#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "../mixer/free_space_scan_touch.h"

int checks = 0;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
int main() {
  FreeSpaceScan scan;
  std::vector<uint32_t> fat(301, 0);
  fat[0] = fat[1] = 0; // Reserved entries must never be counted.
  fat[2] = 0xFFFFFFFF; // End-of-chain is allocated, even if reader returns no value.
  fat[150] = 200;
  int calls = 0;
  auto read = [&](uint32_t cluster, uint32_t* value) {
    ++calls;
    if (fat[cluster] == 0xFFFFFFFF)
      return 0;
    *value = fat[cluster];
    return 1;
  };
  scan.begin(299, 15);
  check(scan.pending() && !scan.ready() && scan.bytes() == 0, "count initially unknown");
  scan.step(read);
  check(calls == 126 && scan.pending(), "first step stops at FAT sector boundary");
  check(scan.bytes() == 0, "partial totals are not published");
  scan.step(read);
  check(calls == 254 && scan.pending(), "later step reads one sector at most");
  scan.step(read);
  check(scan.ready() && scan.bytes() == uint64_t(297) * 32768,
        "exact final count excludes reserved and allocated entries");
  int previous = calls;
  scan.step(read);
  check(calls == previous, "cached completed query performs no disk reads");

  scan.invalidate();
  scan.step(read);
  fat[3] = 4; // A foreground write changed an already-scanned sector.
  scan.invalidate();
  check(!scan.ready(), "mutations invalidate the cached count");
  while (scan.pending())
    scan.step(read);
  check(scan.bytes() == uint64_t(296) * 32768, "restart does not publish mixed generations");

  scan.begin(299, 15);
  scan.step([](uint32_t, uint32_t*) { return -1; });
  check(scan.failed() && !scan.ready() && !scan.pending(),
        "read error is distinct from full volume");
  previous = calls;
  scan.step(read);
  check(calls == previous, "failed scan does not spin or retry indefinitely");
  scan.invalidate();
  while (scan.pending())
    scan.step(read);
  check(scan.ready(), "invalidation permits recovery after a failed scan");

  scan.begin(0, 15);
  scan.step(read);
  check(scan.ready() && scan.bytes() == 0, "empty volume boundary");
  scan.begin(128, 15);
  while (scan.pending())
    scan.step([](uint32_t, uint32_t* v) {
      *v = 1;
      return 1;
    });
  check(scan.ready() && scan.bytes() == 0, "full volume is a known zero count");
  scan.begin(262144, 15);
  while (scan.pending())
    scan.step([](uint32_t, uint32_t* v) {
      *v = 0;
      return 1;
    });
  check(scan.bytes() == 8589934592ULL, "byte totals exceed 32 bits safely");
  std::cout << checks << " free-space checks passed\n";
}
