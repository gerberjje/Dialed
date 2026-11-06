#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "storage_status_touch.h"
#include "storage_touch.h"
#include "status_led_touch.h"
namespace {
bool pcConnected = false, connectionDirty = true;
}
void setPcConnectionStatus(bool connected) {
  statusLedSetConnected(connected);
  if (pcConnected != connected) {
    pcConnected = connected;
    connectionDirty = true;
  }
}
void drawStorageStatus(TFT_eSPI& d, uint16_t bg, uint16_t fg, bool force) {
  static unsigned long updated = 0, painted = 0;
  static char text[54] = {};
  unsigned long now = millis();
  if (!text[0] || now - updated >= 5000) {
    if (mediaStorage.freeSpaceFailed())
      snprintf(text, sizeof(text), "Storage free space unavailable");
    else if (!mediaStorage.freeBytesKnown())
      snprintf(text, sizeof(text), "SD calculating free space");
    else {
      uint64_t free = mediaStorage.freeBytes();
      if (mediaStorage.isSD())
        snprintf(text, sizeof(text), "SD %.2f GiB free", double(free) / 1073741824.0);
      else
        snprintf(text, sizeof(text), "SD unavailable | Flash %.1f MiB free",
                 double(free) / 1048576.0);
    }
    updated = now;
    force = true;
  }
  if (!force && !connectionDirty && now - painted < 1000)
    return;
  // Idle-only footer, below the original ring and the READY subtitle.
  d.fillRect(0, 232, 320, 8, bg);
  d.setTextFont(1);
  d.setTextSize(1);
  d.setTextDatum(TL_DATUM);
  d.setTextColor(fg, bg);
  d.drawString(text, 4, 232, 1);
  d.setTextDatum(TR_DATUM);
  d.drawString(pcConnected ? "Connected" : "Disconnected", 316, 232, 1);
  connectionDirty = false;
  painted = now;
}
#endif
