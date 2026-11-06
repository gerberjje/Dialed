#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "status_led_touch.h"
#include "hardware_profile_touch.h"
#include <Arduino.h>

namespace {
enum class Color { Off,
                   Red,
                   Green };
bool initialized = false;
bool connected = false;
bool screenOn = false;
Color shown = Color::Off;

void update() {
  if (!initialized)
    return;

  const Color next = !screenOn ? Color::Off : connected ? Color::Green
                                                        : Color::Red;
  if (next == shown)
    return;

  const int on = STATUS_LED_ACTIVE_LOW ? LOW : HIGH;
  const int off = STATUS_LED_ACTIVE_LOW ? HIGH : LOW;
  // Clear every channel before switching, avoiding mixed colors.
  digitalWrite(STATUS_LED_RED, off);
  digitalWrite(STATUS_LED_GREEN, off);
  digitalWrite(STATUS_LED_BLUE, off);
  if (next == Color::Red)
    digitalWrite(STATUS_LED_RED, on);
  else if (next == Color::Green)
    digitalWrite(STATUS_LED_GREEN, on);
  shown = next;
}
} // namespace

void statusLedInit() {
  const int off = STATUS_LED_ACTIVE_LOW ? HIGH : LOW;
  const int pins[] = {STATUS_LED_RED, STATUS_LED_GREEN, STATUS_LED_BLUE};
  for (int pin : pins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, off);
  }
  connected = false;
  screenOn = false;
  shown = Color::Off;
  initialized = true;
}

void statusLedSetConnected(bool value) {
  connected = value;
  update();
}

void statusLedSetScreenOn(bool value) {
  screenOn = value;
  update();
}
#endif
