#pragma once
#include "controller_profile.h"

#if DIALED_CONTROLLER_TOUCH
static constexpr int IDLE_MEDIA_MAX_WIDTH = 320;
static constexpr int IDLE_MEDIA_BAND_ROWS = 8;
#else
static constexpr int IDLE_MEDIA_MAX_WIDTH = 240;
static constexpr int IDLE_MEDIA_BAND_ROWS = 48;
#endif
static constexpr int IDLE_MEDIA_MAX_HEIGHT = 240;
static constexpr int IDLE_MEDIA_BUFFER_PIXELS = IDLE_MEDIA_MAX_WIDTH * IDLE_MEDIA_BAND_ROWS;
inline bool idleMediaDimensionsSupported(int width, int height) {
  return width > 0 && height > 0 && width <= IDLE_MEDIA_MAX_WIDTH &&
         height <= IDLE_MEDIA_MAX_HEIGHT;
}
