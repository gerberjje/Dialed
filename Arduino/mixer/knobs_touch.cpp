#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "knobs_touch.h"
#include "display_touch.h"
#include "assignments_touch.h"
#include "hardware_profile_touch.h"
#include "touch_geometry.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const int TAB_H = 36;
static const int ACTION_Y = 202;
static const unsigned long REPEAT_DELAY_MS = 350;
static const unsigned long REPEAT_RATE_MS = 110;

enum TouchAction {
  TOUCH_NONE,
  TOUCH_DOWN,
  TOUCH_MUTE,
  TOUCH_UP,
  TOUCH_DISCORD_MUTE,
  TOUCH_DISCORD_DEAFEN
};

static SPIClass touchSpi(HSPI);
static XPT2046_Touchscreen touchscreen(TOUCH_CS, TOUCH_IRQ);
static int activeCount = 5;
static int selectedKnob = 0;
static TouchAction heldAction = TOUCH_NONE;
static unsigned long pressedAt = 0;
static unsigned long lastRepeatAt = 0;
static TouchWakeCallback touchWakeCallback = nullptr;
static bool touchWasDown = false;
static bool suppressUntilRelease = false;
static bool centerDragActive = false;
static float dragLastValue = -1;

void knobsSetTouchWakeCallback(TouchWakeCallback cb) {
  touchWakeCallback = cb;
}

void knobsSetSwitchCallback(SwitchCallback cb) {
  (void)cb;
}

void knobsSetActiveCount(int count) {
  activeCount = constrain(count, 0, 5);
  if (activeCount > 0 && selectedKnob >= activeCount) {
    selectedKnob = activeCount - 1;
    displaySelectKnob(selectedKnob);
  }
}

static void sendKnobEvent(const char *event) {
  Serial.print("knob");
  Serial.print(selectedKnob + 1);
  Serial.print(':');
  Serial.println(event);
}

static void sendKnobValue(float value) {
  Serial.print("knob");
  Serial.print(selectedKnob + 1);
  Serial.print(':');
  Serial.println(value, 3);
}

static bool readTouch(int &x, int &y) {
  if (!touchscreen.touched())
    return false;

  TS_Point point = touchscreen.getPoint();
  if (point.z < 200)
    return false;

  // XPT2046_Touchscreen applies rotation 3 before returning this point.
  x = map(point.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
  y = map(point.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);
  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);
  return true;
}

static TouchAction actionAt(int x, int y) {
  return (TouchAction)touchActionAt(x, y, knobDiscordMute[selectedKnob]);
}

void knobsSetup(KnobCallback cb) {
  (void)cb;

  Serial.setRxBufferSize(16384);
  Serial.begin(921600);

  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touchscreen.begin(touchSpi);
  touchscreen.setRotation(3);
}

void knobsLoop() {
  int x, y;
  unsigned long now = millis();
  if (!readTouch(x, y)) {
    heldAction = TOUCH_NONE;
    touchWasDown = false;
    suppressUntilRelease = false;
    centerDragActive = false;
    return;
  }

  if (!touchWasDown) {
    touchWasDown = true;
    // A host volume event may have changed the visible channel since last touch.
    selectedKnob = activeCount > 0 ? constrain(displayActiveKnob(), 0, activeCount - 1) : 0;
    if (touchWakeCallback && touchWakeCallback()) {
      displaySelectKnob(selectedKnob);
      heldAction = TOUCH_NONE;
      suppressUntilRelease = true;
      return;
    }
    float initialValue;
    centerDragActive =
        activeCount > 0 && y >= TAB_H && y < ACTION_Y && touchArcValue(x, y, initialValue);
    dragLastValue = -1;
  } else if (touchWakeCallback) {
    // Touch holds and drags keep the local activity timer alive.
    touchWakeCallback();
  }

  if (suppressUntilRelease)
    return;

  if (centerDragActive) {
    float value;
    if (touchArcValue(x, y, value) && fabsf(value - dragLastValue) >= 0.005f) {
      sendKnobValue(value);
      displayShowKnob(selectedKnob, value);
      dragLastValue = value;
    }
    heldAction = TOUCH_NONE;
    return;
  }

  if (y < TAB_H && activeCount > 0) {
    int knob = constrain(x / (SCREEN_W / 5), 0, 4);
    if (knob < activeCount && knob != selectedKnob) {
      selectedKnob = knob;
      displaySelectKnob(selectedKnob);
    }
    heldAction = TOUCH_NONE;
    return;
  }

  TouchAction action = actionAt(x, y);
  if (action == TOUCH_NONE || activeCount == 0) {
    heldAction = TOUCH_NONE;
    return;
  }

  if (action != heldAction) {
    heldAction = action;
    displayTouchFeedback((int)action);
    pressedAt = now;
    lastRepeatAt = now;
    if (action == TOUCH_DOWN)
      sendKnobEvent("down");
    else if (action == TOUCH_UP)
      sendKnobEvent("up");
    else if (action == TOUCH_DISCORD_MUTE || action == TOUCH_DISCORD_DEAFEN) {
      sendKnobEvent(action == TOUCH_DISCORD_MUTE ? "discordmute" : "discorddeafen");
      suppressUntilRelease = true; // one Discord action per contact, even if the finger slides
    } else
      sendKnobEvent("press");
    return;
  }

  if ((action == TOUCH_DOWN || action == TOUCH_UP) && now - pressedAt >= REPEAT_DELAY_MS &&
      now - lastRepeatAt >= REPEAT_RATE_MS) {
    displayTouchFeedback((int)action);
    sendKnobEvent(action == TOUCH_DOWN ? "down" : "up");
    lastRepeatAt = now;
  }
}
#endif
