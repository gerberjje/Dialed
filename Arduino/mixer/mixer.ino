#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "knobs_touch.h"
#include "display_touch.h"
#include "assignments_touch.h"
#include "idlegif_touch.h"
#include "storage_touch.h"
#include "storage_status_touch.h"
#include "status_led_touch.h"
#else
#include "knobs.h"
#include "display.h"
#include "assignments.h"
#include "idlegif.h"
#endif
#include "version.h"
#include "idle_media_profile.h"

// Runtime-configurable: the PC pushes the user's setting via "cfg:idle:<ms>"
// (see handleConfigLine). Seeded to the previous hardcoded default for the
// window between boot and the first sync.
static unsigned long idleTimeoutMs = 3000;
static unsigned long lastKnobActivity = 0;
static bool isIdle = true;

#if DIALED_CONTROLLER_TOUCH
static bool displayPowered = true;
static bool screenOffRequested = false;
#endif

// Buffer for lines arriving from the PC (icon lines are ~11 KB)
static char  inLine[12000];
static int   inPos = 0;

static void handleVolumeLine(const char* line) {
  if (strncmp(line, "vol:", 4) != 0) return;
  const char* rest = line + 4;                // "knob1:0.42"
  if (strncmp(rest, "knob", 4) != 0) return;
  const char* colon = strchr(rest, ':');
  if (!colon) return;
  int idx = atoi(rest + 4) - 1;
  if (idx < 0 || idx >= MAX_KNOBS) return;
  float v = atof(colon + 1);
  displayShowKnob(idx, v);
  lastKnobActivity = millis();
  isIdle = false;
}

static void handleMuteLine(const char* line) {
  if (strncmp(line, "mute:", 5) != 0) return;
  const char* rest = line + 5;                // "knob1:1"
  if (strncmp(rest, "knob", 4) != 0) return;
  const char* colon = strchr(rest, ':');
  if (!colon) return;
  int idx = atoi(rest + 4) - 1;
  if (idx < 0 || idx >= MAX_KNOBS) return;
  bool muted = atoi(colon + 1) != 0;
  displayShowMute(idx, muted);
  lastKnobActivity = millis();
  isIdle = false;
}

#if DIALED_CONTROLLER_TOUCH
static void handleStateLine(const char *line) {
  if (strncmp(line, "state:", 6) != 0)
    return;
  const char *rest = line + 6; // "knob1:0.42:0"
  if (strncmp(rest, "knob", 4) != 0)
    return;
  const char *volumeSep = strchr(rest, ':');
  if (!volumeSep)
    return;
  const char *muteSep = strchr(volumeSep + 1, ':');
  if (!muteSep)
    return;

  int idx = atoi(rest + 4) - 1;
  if (idx < 0 || idx >= MAX_KNOBS)
    return;
  displaySyncKnobState(idx, atof(volumeSep + 1), atoi(muteSep + 1) != 0);
}

static void handleDiscordLine(const char *line) {
  if (strncmp(line, "discord:knob", 12) != 0)
    return;
  char *end = nullptr;
  long knob = strtol(line + 12, &end, 10);
  if (end == line + 12 || *end != ':' || knob < 1 || knob > MAX_KNOBS)
    return;
  const char *value = end + 1;
  long mute = strtol(value, &end, 10);
  if (end == value || *end != ':' || mute < -1 || mute > 1)
    return;
  value = end + 1;
  long deaf = strtol(value, &end, 10);
  if (end == value || *end != '\0' || deaf < -1 || deaf > 1)
    return;
  displaySyncDiscordState((int)knob - 1, (int)mute, (int)deaf);
}

#endif

static void handleConfigLine(const char* line) {
  if (strncmp(line, "cfg:idle:", 9) == 0) {
    long ms = atol(line + 9);
    if (ms >= 0) idleTimeoutMs = (unsigned long)ms;

  } else if (strncmp(line, "config:pct:", 11) == 0) {
    displaySetShowPercent(atoi(line + 11) != 0);

#if DIALED_CONTROLLER_TOUCH
  } else if (strncmp(line, "cfg:discord:knob", 16) == 0) {
    const char *value = strchr(line + 16, ':');
    int index = atoi(line + 16) - 1;
    if (value && index >= 0 && index < MAX_KNOBS) {
      bool enabled = atoi(value + 1) != 0;
      if (enabled != knobDiscordMute[index])
        displaySyncDiscordState(index, -1, -1);
      knobDiscordMute[index] = enabled;
      displayAssignmentChanged();
    }

#endif
  } else if (strncmp(line, "cfg:knobs:", 10) == 0) {
    knobsSetActiveCount(atoi(line + 10));
  }
}

static void handleOutLine(const char* line) {
  if (strncmp(line, "out:", 4) != 0) return;
  const char* p = line + 4;                       // "<a|b>:<state>"

  int pos;
  if      (p[0] == 'a' || p[0] == 'A') pos = 0;
  else if (p[0] == 'b' || p[0] == 'B') pos = 1;
  else return;
  if (p[1] != ':') return;

  const char* s = p + 2;
  uint8_t state;
  if      (strcmp(s, "ok")   == 0) state = OUT_OK;
  else if (strcmp(s, "none") == 0) state = OUT_NONE;
  else if (strcmp(s, "fail") == 0) state = OUT_FAIL;
  else return;                                    // unknown state: ignore the line

  displayShowOutput(pos, state);
  lastKnobActivity = millis();
  isIdle = false;
}

static void handleScreenLine(const char* line) {
  if (strcmp(line, "screen:off") == 0) {
    displayBlank(true);
#if DIALED_CONTROLLER_TOUCH
    screenOffRequested = true;
    displayPowered = false;
    setPcConnectionStatus(false);
#endif
  } else if (strcmp(line, "screen:on") == 0) {
    displayBlank(false);
#if DIALED_CONTROLLER_TOUCH
    screenOffRequested = false;
    displayPowered = true;
#endif
  }
}

static void sendFirmwareVersion() {
  // Symmetric with SerialManager: "fw:<board>:<version>". Does NOT touch the
  // display/idle state — it's metadata, not a knob event.
  Serial.print("fw:");
  Serial.print(FW_BOARD);
  Serial.print(':');
  Serial.println(FW_VERSION);
}

static void handleVerLine(const char* line) {
  if (strcmp(line, "ver?") == 0)
    sendFirmwareVersion();
  else if (strcmp(line, "media?") == 0) {
    Serial.print("media:");
    Serial.print(IDLE_MEDIA_MAX_WIDTH);
    Serial.print(':');
    Serial.println(IDLE_MEDIA_MAX_HEIGHT);
  }
}

void readIncomingSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (inPos > 0) {
        inLine[inPos] = '\0';
#if DIALED_CONTROLLER_TOUCH
        setPcConnectionStatus(true);
        if (!displayPowered && !screenOffRequested) {
          displaySetPower(true);
          displayPowered = true;
        }
#endif
        // GIF upload lines are the hot path during a transfer; handle them first
        // and skip the other parsers when consumed.
        if (!idleGifHandleLine(inLine)) {
#if DIALED_CONTROLLER_TOUCH
          if (assignmentsHandleProfile(inLine))
            displayAssignmentChanged();
          if (handleAssignLine(inLine))
            displayAssignmentChanged();
          if (handleIconLine(inLine))
            displayAssignmentChanged();
#else
          handleAssignLine(inLine);
          handleIconLine(inLine);
#endif
          handleVolumeLine(inLine);
          handleMuteLine(inLine);
#if DIALED_CONTROLLER_TOUCH
          handleStateLine(inLine);
          handleDiscordLine(inLine);
#endif
          handleConfigLine(inLine);
          handleOutSetLine(inLine);
          handleOutLine(inLine);
          handleScreenLine(inLine);
          handleVerLine(inLine);
#if DIALED_CONTROLLER_TOUCH
          if (strcmp(inLine, "storage?") == 0) {
            Serial.print("storage:");
            Serial.print(mediaStorage.isSD() ? "sd:" : "flash:");
            if (mediaStorage.freeSpaceFailed())
              Serial.println("unavailable");
            else if (!mediaStorage.freeBytesKnown())
              Serial.println("pending");
            else
              Serial.println((unsigned long long)mediaStorage.freeBytes());
          }
#endif
        }
        inPos = 0;
      }
    } else if (inPos < (int)sizeof(inLine) - 1) {
      inLine[inPos++] = c;
    }
  }
}

void onKnobChange(const char* id, float value) {
  int idx = -1;
  if (strncmp(id, "knob", 4) == 0)
    idx = atoi(id + 4) - 1;   // "knob1" -> 0

  if (idx >= 0 && idx < MAX_KNOBS)
    displayShowKnob(idx, value);

  lastKnobActivity = millis();
  isIdle = false;
}

#if DIALED_CONTROLLER_TOUCH
bool onTouchWake() {
  lastKnobActivity = millis();
  if (!isIdle)
    return false;
  isIdle = false;
  return true;
}

#endif

// The toggle moved. Draw the card immediately from stored assignment data — the
// PC's "out:" confirmation may never arrive (Dialed closed) and must not be
// waited on. Counts as activity, so the card dwells for cfg:idle:<ms> and then
// falls to the idle screen through the normal loop() check.
void onSwitchChange(int pos) {
  displayShowOutput(pos, OUT_PENDING);
  lastKnobActivity = millis();
  isIdle = false;
}

void setup() {
#if DIALED_CONTROLLER_TOUCH
  statusLedInit();
  assignmentsInit();
#endif
  displaySetup();
#if DIALED_CONTROLLER_TOUCH
  statusLedSetScreenOn(true);
#endif
  knobsSetup(onKnobChange);   // knobsSetup calls Serial.begin(921600)
#if DIALED_CONTROLLER_TOUCH
  knobsSetTouchWakeCallback(onTouchWake);
#endif
  knobsSetSwitchCallback(onSwitchChange);
  sendFirmwareVersion();      // announce firmware version to the PC on boot
  lastKnobActivity = millis();
}

void loop() {
  readIncomingSerial();
  knobsLoop();

  if (!isIdle && millis() - lastKnobActivity > idleTimeoutMs) {
    displayEnterIdle();
    isIdle = true;
  }

  displayTick();
}
