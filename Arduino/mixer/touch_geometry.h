#pragma once
#include <math.h>

// Bottom controls keep the same three positions on every channel.
inline int touchActionCount(bool) {
  return 3;
}
inline int touchActionX(int index, bool) {
  return index * 110 + 3;
}
inline int touchActionWidth(bool) {
  return 94;
}
inline int touchActionId(int index, bool) {
  return index + 1;
}

// Existing Discord icon centers in screen coordinates. IDs 4 and 5 are
// microphone mute and headset deafen. Targets never intersect the volume arc.
inline int touchDiscordIconX(int index) {
  return 29 + index * 44;
}
static const int TOUCH_DISCORD_ICON_Y = 176;
inline int touchDiscordActionAt(int x, int y, bool discord) {
  if (!discord || y < TOUCH_DISCORD_ICON_Y - 18 || y >= TOUCH_DISCORD_ICON_Y + 18)
    return 0;
  for (int i = 0; i < 2; i++) {
    int cx = touchDiscordIconX(i);
    if (x >= cx - 21 && x < cx + 21)
      return 4 + i;
  }
  return 0;
}
inline int touchActionAt(int x, int y, bool discord) {
  int icon = touchDiscordActionAt(x, y, discord);
  if (icon)
    return icon;
  if (y < 204 || y >= 237)
    return 0;
  for (int i = 0; i < 3; i++) {
    int left = touchActionX(i, discord);
    if (x >= left && x < left + touchActionWidth(discord))
      return i + 1;
  }
  return 0;
}

// Same center/sweep as display.cpp: 0% bottom-left, 50% top, 100% bottom-right.
// The interior and bottom gap are not volume controls.
inline bool touchArcValue(int x, int y, float &value) {
  const float dx = x - 200.0f, dy = y - 111.0f;
  const float radiusSquared = dx * dx + dy * dy;
  if (radiusSquared < 45 * 45 || radiusSquared > 82 * 82)
    return false;
  const float degrees = atan2f(dx, -dy) * 57.295779513f;
  if (degrees < -135.0f || degrees > 135.0f)
    return false;
  value = (degrees + 135.0f) / 270.0f;
  return true;
}
