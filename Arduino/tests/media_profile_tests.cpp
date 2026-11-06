#include <cstdlib>
#include <iostream>
#include "../mixer/idle_media_profile.h"
void check(bool ok, const char *name) {
  if (!ok) {
    std::cerr << name << '\n';
    std::exit(1);
  }
}
int main() {
  check(IDLE_MEDIA_MAX_WIDTH == (DIALED_CONTROLLER_TOUCH ? 320 : 240), "profile width");
  check(IDLE_MEDIA_MAX_HEIGHT == 240, "profile height");
  check(idleMediaDimensionsSupported(240, 240), "existing square media still supported");
  check(idleMediaDimensionsSupported(320, 240) == bool(DIALED_CONTROLLER_TOUCH),
        "wide frames only on touch");
  check(!idleMediaDimensionsSupported(321, 240), "oversized width rejected");
  check(!idleMediaDimensionsSupported(240, 241), "oversized height rejected");
  check(!idleMediaDimensionsSupported(0, 240) && !idleMediaDimensionsSupported(-1, 240),
        "invalid width");
  check(!idleMediaDimensionsSupported(240, 0), "invalid height");
  check(IDLE_MEDIA_MAX_WIDTH * IDLE_MEDIA_BAND_ROWS <= IDLE_MEDIA_BUFFER_PIXELS,
        "maximum band fits scratch buffer");
  std::cout << (DIALED_CONTROLLER_TOUCH ? "touch" : "mixer") << ": 9 media-profile checks passed\n";
}
