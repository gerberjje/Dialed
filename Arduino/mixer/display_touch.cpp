#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "hardware_profile_touch.h"
#include "storage_status_touch.h"
#include "status_led_touch.h"
static bool blankMode =
    false; // Explicit PC suspend/shutdown; serial commands must not override it.
#include "display_touch.h"
#include "assignments_touch.h"
#include "idlegif_touch.h"
#include "touch_geometry.h"
#include <TFT_eSPI.h>
#include <math.h>

#if !defined(ILI9341_2_DRIVER) || TFT_MISO != 12 || TFT_MOSI != 13 || TFT_SCLK != 14 || \
    TFT_CS != 15 || TFT_DC != 2 || TFT_BL != 21
#error "Install mixer/User_Setup_touch.h in TFT_eSPI before compiling."
#endif

namespace {
TFT_eSPI tft;
TFT_eSprite body(&tft);
bool buffered = false;
constexpr uint16_t BG = 0x0000, SURFACE = 0x1106, TRACK = 0x1949;
constexpr uint16_t WHITE = 0xF7BE, CYAN = 0x073F, PINK = 0xF971;
constexpr uint16_t YELLOW = 0xEFEB, DIM = 0x7C92;
constexpr int BODY_Y = 38, BODY_H = 160;
constexpr unsigned long FRAME_MS = 33, TRANSITION_MS = 180;
constexpr float PI2 = 6.283185307f;
enum Screen { ACTIVE,
              IDLE,
              OUTPUT_CARD };
Screen screen = IDLE;
int selected = 0, pressed = 0, outputPos = 0;
uint8_t outputState = OUT_PENDING;
float volumes[MAX_KNOBS] = {}, shown = 0, target = 0;
bool muted[MAX_KNOBS] = {}, percent = false, powered = true;
int8_t discordMuted[MAX_KNOBS], discordDeafened[MAX_KNOBS];
unsigned long discordUpdated[MAX_KNOBS] = {};
bool discordReceived[MAX_KNOBS] = {}, renderedDiscordFresh = false;
bool discordFresh(int index, unsigned long now) {
  return discordReceived[index] && (unsigned long)(now - discordUpdated[index]) < 5000;
}
bool dirty = true, chromeDirty = true, playingGif = false;
bool uploading = false, uploadFinished = false, uploadOk = false;
int uploadPct = 0, renderedPct = -1;
unsigned long lastFrame = 0, pressUntil = 0, transitionStart = 0, uploadUntil = 0;
bool transitioning = false;

uint16_t mix(uint16_t color, float opacity) {
  const int a = constrain((int)(opacity * 256), 0, 256);
  int r = (((color >> 11) & 31) * a + ((BG >> 11) & 31) * (256 - a)) >> 8;
  int g = (((color >> 5) & 63) * a + ((BG >> 5) & 63) * (256 - a)) >> 8;
  int b = ((color & 31) * a + (BG & 31) * (256 - a)) >> 8;
  return (r << 11) | (g << 5) | b;
}

// One reusable body sprite, 102,400 bytes at 16-bit; try 8-bit if RAM is tight.
// Direct drawing is the last fallback. No full-screen clears during animation.
TFT_eSPI& beginBody() {
  if (buffered) {
    body.fillSprite(BG);
    return body;
  }
  tft.setViewport(0, BODY_Y, 320, BODY_H, true);
  tft.fillRect(0, 0, 320, BODY_H, BG);
  return tft;
}
void endBody() {
  if (buffered)
    body.pushSprite(0, BODY_Y);
  else
    tft.resetViewport();
}
void label(TFT_eSPI& d, const char* text, int x, int y, uint16_t color, int font = 2) {
  d.setTextFont(font);
  d.setTextSize(1);
  d.setTextDatum(MC_DATUM);
  d.setTextColor(color, BG);
  d.drawString(text, x, y);
}
void fitted(TFT_eSPI& d, const char* text, int x, int y, int width, uint16_t color) {
  d.setTextFont(2);
  d.setTextSize(1);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s", text);
  if (d.textWidth(buf) > width) {
    int n = strlen(buf);
    do {
      if (n <= 3)
        break;
      --n;
      buf[n - 3] = '.';
      buf[n - 2] = '.';
      buf[n - 1] = '.';
      buf[n] = '\0';
    } while (d.textWidth(buf) > width);
  }
  label(d, buf, x, y, color);
}
void speaker(TFT_eSPI& d, int x, int y, uint16_t color, bool slash) {
  d.fillRect(x - 18, y - 6, 10, 12, color);
  d.fillTriangle(x - 8, y - 6, x + 5, y - 17, x + 5, y + 17, color);
  if (slash) {
    d.drawLine(x - 20, y - 22, x + 23, y + 21, PINK);
    d.drawLine(x - 21, y - 21, x + 22, y + 22, PINK);
    d.drawLine(x - 22, y - 20, x + 21, y + 23, PINK);
  } else {
    d.drawLine(x + 11, y - 10, x + 17, y - 4, color);
    d.drawLine(x + 17, y - 4, x + 17, y + 4, color);
    d.drawLine(x + 17, y + 4, x + 11, y + 10, color);
  }
}
void channelIcon(TFT_eSPI& d, float opacity) {
  const int x = 24, y = 37, size = 48;
  if (knobHasIcon[selected]) {
    uint16_t row[size];
    d.setSwapBytes(true);
    for (int py = 0; py < size; ++py) {
      for (int px = 0; px < size; ++px)
        row[px] = mix(assignmentIcon(selected)[(py * ICON_H / size) * ICON_W + px * ICON_W / size],
                      opacity);
      // pushImage is not virtual in TFT_eSPI; dispatch explicitly to the sprite.
      if (buffered)
        body.pushImage(x, y + py, size, 1, row);
      else
        tft.pushImage(x, y + py, size, 1, row);
    }
    d.setSwapBytes(false);
  } else if (knobKind[selected] == KNOB_KIND_MIC) {
    uint16_t color = mix(CYAN, opacity);
    d.drawRoundRect(41, 40, 14, 25, 7, color);
    d.drawRoundRect(34, 48, 28, 26, 12, color);
    d.fillRect(33, 47, 30, 12, BG);
    d.drawRoundRect(41, 40, 14, 25, 7, color);
    d.drawLine(48, 74, 48, 83, color);
    d.drawLine(38, 83, 58, 83, color);
  } else
    speaker(d, 48, 62, mix(CYAN, opacity), false);
}
void arc(TFT_eSPI& d, int cx, int cy, int radius, int pct, uint16_t color, float opacity) {
  const int lit = (pct * 30 + 50) / 100;
  for (int i = 0; i < 30; ++i)
    d.drawSmoothArc(cx, cy, radius, radius - 9, 45 + i * 9, 52 + i * 9,
                    mix(i < lit ? color : TRACK, opacity), BG, false);
}
void chrome() {
  tft.fillRect(0, 0, 320, BODY_Y, BG);
  tft.fillRect(0, 198, 320, 42, BG);
  if (screen == ACTIVE && !uploading) {
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    for (int i = 0; i < 5; ++i) {
      const int x = i * 64 + 3;
      tft.fillRoundRect(x, 3, 58, 30, 5, SURFACE);
      tft.drawRoundRect(x, 3, 58, 30, 5, i == selected ? YELLOW : TRACK);
      tft.setTextColor(i == selected ? YELLOW : DIM, SURFACE);
      tft.drawNumber(i + 1, x + 29, 18);
    }
    const bool discord = knobDiscordMute[selected];
    const char* labels[] = {"-", muted[selected] ? "UNMUTE" : "MUTE", "+"};
    for (int i = 0; i < touchActionCount(discord); ++i) {
      const int action = touchActionId(i, discord);
      uint16_t col = action == 2   ? PINK
                     : action == 4 ? YELLOW
                                   : CYAN,
               fill = pressed == action ? col : SURFACE;
      const int x = touchActionX(i, discord), width = touchActionWidth(discord);
      tft.fillRoundRect(x, 204, width, 33, 6, fill);
      tft.drawRoundRect(x, 204, width, 33, 6, col);
      tft.setTextFont(action == 4 ? 1 : 2);
      tft.setTextColor(pressed == action ? BG : col, fill);
      tft.drawString(labels[action - 1], x + width / 2, 220);
    }
  } else if (uploading) {
    label(tft, "Idle GIF", 160, 19, DIM, 1);
  } else if (screen == OUTPUT_CARD) {
    label(tft, "AUDIO OUTPUT", 160, 19, DIM, 2);
    label(tft,
          outputState == OUT_PENDING ? "Pending"
          : outputState == OUT_OK    ? "Connected"
                                     : "Unavailable",
          160, 219,
          outputState == OUT_OK        ? CYAN
          : outputState == OUT_PENDING ? YELLOW
                                       : PINK,
          1);
  }
  chromeDirty = false;
}
void discordIndicators(TFT_eSPI& d, unsigned long now, float alpha) {
  const bool fresh = discordFresh(selected, now);
  const int states[] = {fresh ? discordMuted[selected] : -1,
                        fresh ? discordDeafened[selected] : -1};
  for (int i = 0; i < 2; ++i) {
    const int x = touchDiscordIconX(i), y = TOUCH_DISCORD_ICON_Y - BODY_Y;
    uint16_t color = mix(states[i] < 0 ? DIM : states[i] ? PINK
                                                         : WHITE,
                         alpha);
    d.fillRoundRect(x - 19, y - 16, 38, 32, 7, mix(SURFACE, alpha));
    d.drawRoundRect(x - 20, y - 17, 40, 34, 8, mix(pressed == 4 + i ? YELLOW : TRACK, alpha));
    if (i == 0) {
      d.drawRoundRect(x - 4, y - 10, 8, 13, 4, color);
      d.drawRoundRect(x - 8, y - 4, 16, 12, 7, color);
      d.fillRect(x - 9, y - 5, 18, 4, mix(SURFACE, alpha));
      d.drawLine(x, y + 8, x, y + 12, color);
      d.drawLine(x - 5, y + 12, x + 5, y + 12, color);
    } else {
      d.drawSmoothArc(x, y, 10, 8, 90, 270, color, mix(SURFACE, alpha), false);
      d.fillRoundRect(x - 11, y - 1, 5, 11, 2, color);
      d.fillRoundRect(x + 6, y - 1, 5, 11, 2, color);
    }
    if (states[i] > 0) {
      d.drawLine(x - 12, y - 12, x + 12, y + 12, color);
      d.drawLine(x - 11, y - 12, x + 13, y + 12, color);
    } else if (states[i] < 0) {
      label(d, "?", x + 14, y + 10, color, 1);
    }
  }
  renderedDiscordFresh = fresh;
}
void activeFrame(unsigned long now) {
  TFT_eSPI& d = beginBody();
  float alpha =
      transitioning ? constrain((now - transitionStart) / (float)TRANSITION_MS, 0.0f, 1.0f) : 1.0f;
  alpha = 1.0f - (1.0f - alpha) * (1.0f - alpha);
  int pct = (int)lroundf(shown * 100);
  channelIcon(d, alpha);
  fitted(d, knobLabel[selected][0] ? knobLabel[selected] : "---", 49, 108, 94, mix(WHITE, alpha));
  if (knobDiscordMute[selected])
    discordIndicators(d, now, alpha);
  else
    label(d, knobKind[selected] == KNOB_KIND_MIC ? "INPUT" : "CHANNEL", 49, 126, mix(DIM, alpha),
          1);
  arc(d, 200, 73, 65, muted[selected] ? 0 : pct, CYAN, alpha);
  if (muted[selected]) {
    d.drawCircle(200, 73, 43, mix(PINK, alpha));
    d.drawCircle(200, 73, 44, mix(PINK, alpha));
    speaker(d, 200, 73, mix(WHITE, alpha), true);
    label(d, "MUTED", 200, 132, mix(PINK, alpha), 2);
  } else {
    char value[4];
    snprintf(value, sizeof(value), "%d", pct);
    d.setFreeFont(&FreeSansBold24pt7b);
    d.setTextSize(1);
    d.setTextDatum(MC_DATUM);
    d.setTextColor(mix(WHITE, alpha), BG);
    d.drawString(value, 200, 76);
    label(d, "VOLUME", 200, 132, mix(DIM, alpha), 1);
    if (percent)
      label(d, "%", 282, 75, mix(DIM, alpha), 2);
  }
  endBody();
  renderedPct = pct;
  dirty = false;
  if (now - transitionStart >= TRANSITION_MS)
    transitioning = false;
}
// Original mixer idle graphic: fixed breathing ring, three drifting dots,
// and centered AudioMixer / READY text. Only the palette and panel center differ.
void idleFrame(unsigned long now) {
  constexpr int cx = 160, cy = 120;
  constexpr unsigned long breathMs = 1800;
  static int previousX[3] = {-100, -100, -100};
  static int previousY[3] = {-100, -100, -100};
  static const float speeds[3] = {0.28f, 0.19f, 0.23f};
  static const float phases[3] = {0.0f, 2.094f, 4.189f};
  static const float radii[3] = {70.0f, 80.0f, 74.0f};
  static const float drift[3] = {5.0f, 4.0f, 6.0f};
  static const uint16_t colors[3] = {CYAN, PINK, YELLOW};

  if (dirty) {
    tft.fillScreen(BG);
    label(tft, "AudioMixer", cx, cy - 8, WHITE, 2);
    label(tft, "READY", cx, cy + 12, DIM, 1);
    for (int i = 0; i < 3; ++i) {
      previousX[i] = -100;
      previousY[i] = -100;
    }
    dirty = false;
  }

  const float phase = (now % breathMs) / float(breathMs);
  const float breath = 0.5f + 0.5f * sinf(phase * PI2 - 1.5708f);
  const uint16_t ring = mix(CYAN, 0.18f + 0.38f * breath);
  tft.drawCircle(cx, cy, 105, ring);
  tft.drawCircle(cx, cy, 106, ring);

  const float seconds = now / 1000.0f;
  for (int i = 0; i < 3; ++i) {
    if (previousX[i] > -50)
      tft.fillCircle(previousX[i], previousY[i], 3, BG);
    const float angle = seconds * speeds[i] * PI2 + phases[i];
    const float radius = radii[i] + drift[i] * sinf(seconds * 0.7f + phases[i]);
    const int x = cx + int(radius * cosf(angle));
    const int y = cy + int(radius * sinf(angle));
    tft.fillCircle(x, y, 3, colors[i]);
    previousX[i] = x;
    previousY[i] = y;
  }
}
void outputFrame() {
  TFT_eSPI& d = beginBody();
  for (int i = 0; i < 2; ++i) {
    int x = 8 + i * 158;
    uint16_t color = i == outputPos ? (outputState == OUT_OK        ? CYAN
                                       : outputState == OUT_PENDING ? YELLOW
                                                                    : PINK)
                                    : TRACK;
    d.fillRoundRect(x, 6, 146, 146, 8, SURFACE);
    d.drawRoundRect(x, 6, 146, 146, 8, color);
    if (outKind[i] == OUT_KIND_HEADPHONE) {
      d.drawSmoothArc(x + 73, 65, 21, 18, 90, 270, color, SURFACE, false);
      d.fillRoundRect(x + 50, 61, 8, 20, 3, color);
      d.fillRoundRect(x + 88, 61, 8, 20, 3, color);
    } else
      speaker(d, x + 73, 65, color, false);
    fitted(d, outLabel[i][0] ? outLabel[i] : "---", x + 73, 107, 134, i == outputPos ? WHITE : DIM);
    label(d, i == 0 ? "OUTPUT A" : "OUTPUT B", x + 73, 129, color, 1);
  }
  endBody();
  dirty = false;
}
void uploadFrame() {
  TFT_eSPI& d = beginBody();
  arc(d, 160, 75, 65, uploadPct, uploadFinished && !uploadOk ? PINK : CYAN, 1);
  char value[5];
  snprintf(value, sizeof(value), "%d", uploadPct);
  d.setFreeFont(&FreeSansBold24pt7b);
  d.setTextDatum(MC_DATUM);
  d.setTextSize(1);
  d.setTextColor(WHITE, BG);
  d.drawString(value, 160, 75);
  label(d, uploadFinished ? (uploadOk ? "Done" : "Failed") : "Updating", 160, 133,
        uploadFinished && !uploadOk ? PINK : DIM, 2);
  endBody();
  dirty = false;
}
void stopGif() {
  if (playingGif) {
    idleGifStop();
    playingGif = false;
    dirty = true;
  }
}
void activate(int index) {
  if (screen != ACTIVE || selected != index) {
    selected = index;
    screen = ACTIVE;
    shown = volumes[index];
    target = shown;
    transitionStart = millis();
    transitioning = true;
    dirty = true;
    chromeDirty = true;
  }
  stopGif();
}
} // namespace

void displaySetup() {
  tft.init();
  tft.setRotation(3);
  // This panel displays complementary colors with the library default (INVOFF).
  // INVON corrects the panel globally: all primitives, sprites and raw RGB565 images.
  tft.invertDisplay(DIALED_PANEL_INVERSION);
  tft.fillScreen(BG);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
  idleGifInit(&tft);
  body.setColorDepth(16);
  buffered = body.createSprite(320, BODY_H) != nullptr;
  if (!buffered) {
    body.setColorDepth(8);
    buffered = body.createSprite(320, BODY_H) != nullptr;
  }
  dirty = chromeDirty = true;
}
int displayActiveKnob() {
  return selected;
}
void displaySelectKnob(int index) {
  if (index < 0 || index >= MAX_KNOBS)
    return;
  activate(index);
  target = volumes[index];
  dirty = chromeDirty = true;
}
void displayShowKnob(int index, float value) {
  if (index < 0 || index >= MAX_KNOBS)
    return;
  volumes[index] = constrain(value, 0.0f, 1.0f);
  activate(index);
  target = volumes[index];
}
void displayShowMute(int index, bool value) {
  if (index < 0 || index >= MAX_KNOBS)
    return;
  muted[index] = value;
  activate(index);
  target = volumes[index];
  dirty = chromeDirty = true;
}
void displaySyncKnobState(int index, float value, bool mute) {
  if (index < 0 || index >= MAX_KNOBS)
    return;
  volumes[index] = constrain(value, 0.0f, 1.0f);
  muted[index] = mute;
  if (screen == ACTIVE && selected == index) {
    target = volumes[index];
    dirty = chromeDirty = true;
  }
}
void displaySyncDiscordState(int index, int mute, int deaf) {
  if (index < 0 || index >= MAX_KNOBS)
    return;
  mute = (mute == 0 || mute == 1) ? mute : -1;
  deaf = (deaf == 0 || deaf == 1) ? deaf : -1;
  const bool changed = !discordFresh(index, millis()) || discordMuted[index] != mute ||
                       discordDeafened[index] != deaf;
  discordMuted[index] = mute;
  discordDeafened[index] = deaf;
  discordUpdated[index] = millis();
  discordReceived[index] = true;
  if (changed && screen == ACTIVE && selected == index)
    dirty = true;
}
void displaySetShowPercent(bool show) {
  if (percent != show) {
    percent = show;
    dirty = true;
  }
}
void displayAssignmentChanged() {
  dirty = chromeDirty = true;
}
void displayTouchFeedback(int button) {
  pressed = button;
  pressUntil = millis() + 130;
  chromeDirty = true;
  if (button >= 4)
    dirty = true;
}
void displayEnterIdle() {
  if (screen != IDLE) {
    screen = IDLE;
    dirty = chromeDirty = true;
    transitioning = false;
  }
}
void displaySetPower(bool on) {
  if (on == powered)
    return;
  powered = on;
  if (!on)
    statusLedSetScreenOn(false);
  if (on) {
    tft.writecommand(0x11); // SLPOUT
    delay(120);
    tft.writecommand(TFT_DISPON);
  } else {
    stopGif();
    tft.fillScreen(TFT_BLACK);
    tft.writecommand(TFT_DISPOFF);
    tft.writecommand(0x10); // SLPIN
    delay(120);
  }
#ifdef TFT_BL
  digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
#endif
  if (on) {
    statusLedSetScreenOn(true);
    dirty = chromeDirty = true;
  }
}
void displayShowOutput(int pos, uint8_t state) {
  if (pos < 0 || pos >= OUT_POSITIONS || uploading)
    return;
  stopGif();
  screen = OUTPUT_CARD;
  outputPos = pos;
  outputState = state;
  dirty = chromeDirty = true;
}
void displayUploadBegin() {
  stopGif();
  uploading = true;
  uploadFinished = false;
  uploadPct = 0;
  dirty = chromeDirty = true;
  chrome();
  uploadFrame();
}
void displayUploadProgress(float value) {
  if (!uploading)
    return;
  int pct = lroundf(constrain(value, 0.0f, 1.0f) * 100);
  if (pct != uploadPct) {
    uploadPct = pct;
    uploadFrame();
  }
}
void displayUploadEnd(bool ok) {
  if (!uploading)
    return;
  uploadFinished = true;
  uploadOk = ok;
  if (ok)
    uploadPct = 100;
  uploadUntil = millis() + 700;
  uploadFrame();
}
void displayTick() {
  if (blankMode || !powered)
    return;
  unsigned long now = millis();
  if (uploading) {
    if (!uploadFinished || (int32_t)(now - uploadUntil) < 0)
      return;
    uploading = false;
    uploadFinished = false;
    dirty = chromeDirty = true;
  }
  if (pressed && (int32_t)(now - pressUntil) >= 0) {
    if (pressed >= 4)
      dirty = true;
    pressed = 0;
    chromeDirty = true;
  }
  if (chromeDirty)
    chrome();
  if (now - lastFrame < FRAME_MS) {
    // Stream an in-flight media frame without imposing a 33 ms pause on every
    // small band. Active mixer transitions still use the regular frame cadence.
    if (screen == IDLE && !dirty && idleGifRendering()) {
      idleGifTick(now);
      drawStorageStatus(tft, BG, DIM, true);
    }
    return;
  }
  lastFrame = now;
  if (screen == ACTIVE) {
    if (knobDiscordMute[selected] && renderedDiscordFresh != discordFresh(selected, now))
      dirty = true;
    shown += (target - shown) * .45f;
    if (fabsf(target - shown) < .002f)
      shown = target;
    if (dirty || transitioning || renderedPct != (int)lroundf(shown * 100))
      activeFrame(now);
  } else if (screen == OUTPUT_CARD) {
    if (dirty)
      outputFrame();
  } else {
    bool available = idleGifAvailable();
    if (available) {
      if (!playingGif || dirty) {
        stopGif();
        idleGifStart(now);
        playingGif = true;
        dirty = false;
      }
      idleGifTick(now);
    } else {
      stopGif();
      idleFrame(now);
    }
    drawStorageStatus(tft, BG, DIM, true);
  }
}

// Explicit PC suspend/resume uses the same panel and backlight power handling.
void displayBlank(bool blank) {
  if (blankMode == blank)
    return;
  blankMode = blank;
  displaySetPower(!blank);
}
#endif
