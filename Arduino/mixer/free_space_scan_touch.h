#pragma once
#include <stdint.h>

// Called under the storage lock. A step reads at most one FAT sector's entries;
// no caller ever needs a synchronous walk of the whole volume.
class FreeSpaceScan {
public:
  void begin(uint32_t clusters, uint8_t shift) {
    total = clusters;
    bytesShift = shift;
    invalidate();
  }
  void invalidate() {
    next = 2;
    free = 0;
    complete = false;
    error = false;
  }
  template <class Reader> void step(Reader read) {
    if (complete || error)
      return;
    const uint32_t end = total + 2;
    // Cluster 2 starts partway through the first 512-byte FAT32 sector.
    const uint32_t limit = next + (128 - next % 128);
    while (next < end && next < limit) {
      uint32_t value = 0;
      int result = read(next, &value);
      if (result < 0) {
        error = true;
        return;
      }
      if (result > 0 && value == 0)
        ++free;
      ++next;
    }
    complete = next == end;
  }
  bool ready() const {
    return complete;
  }
  bool failed() const {
    return error;
  }
  bool pending() const {
    return !complete && !error;
  }
  uint64_t bytes() const {
    return complete ? uint64_t(free) << bytesShift : 0;
  }

private:
  uint32_t total = 0, next = 2, free = 0;
  uint8_t bytesShift = 0;
  bool complete = false, error = false;
};
