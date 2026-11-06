#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "idlegif_touch.h"
#include "display_touch.h"
#include <TFT_eSPI.h>
#include "storage_touch.h"
#include <limits.h>
#include "idle_media_profile.h"

// ── Limits ──────────────────────────────────────────────────────────────────
// MAX_GIF_FRAMES must stay >= the app's IdleGifFrameCap (MainViewModel).
static const int MAX_GIF_FRAMES = 64;
static const int PANEL_W = 320;
static const int PANEL_H = 240;
static const int BAND_ROWS = IDLE_MEDIA_BAND_ROWS;
static const char *DATA_PATH = "/idle.dat";
static const char *TMP_PATH = "/idle.tmp";

// On-disk layout of DATA_PATH:
//   [0..3]  magic 'G','I','D','1'
//   [4..5]  frameCount (uint16 LE)
//   [6..7]  width      (uint16 LE)
//   [8..9]  height     (uint16 LE)
//   [10..]  delays     (frameCount x uint16 LE, ms)
//   [..]    pixels     (frameCount x width*height*2, little-endian RGB565)
static const int HEADER_BYTES = 10;

// ── State ───────────────────────────────────────────────────────────────────
static TFT_eSPI *tft = nullptr;
static bool fsOk = false;

static bool available = false;
static uint16_t frameCount = 0;
static uint16_t gifW = 0, gifH = 0;
static uint16_t delays[MAX_GIF_FRAMES];
static uint32_t dataOffset = 0;
static uint32_t frameBytes = 0;

// Upload (receive) state
static bool uploading = false;
static StorageFile tmpFile;
static uint32_t expectedPixelBytes = 0;
static uint32_t receivedPixelBytes = 0;

// Playback state
static StorageFile playFile;
static bool playing = false;
static int frameIndex = 0;
static unsigned long lastFrameMs = 0;
static bool framePending = false;
static int nextRow = 0;

// Scratch buffers (static so they don't fragment the heap)
static uint16_t bandBuf[IDLE_MEDIA_BUFFER_PIXELS];
static uint8_t decodeBuf[6200];

// ── base64 (same alphabet as assignments.cpp) ───────────────────────────────
static int b64Val(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;
  if (c >= '0' && c <= '9')
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

static int base64Decode(const char *src, uint8_t *dst, int dstLen) {
  int out = 0;
  while (src[0] && src[1] && src[2] && src[3]) {
    int v0 = b64Val(src[0]), v1 = b64Val(src[1]);
    int v2 = b64Val(src[2]), v3 = b64Val(src[3]);
    if (v0 < 0 || v1 < 0)
      break;
    if (out < dstLen)
      dst[out++] = (uint8_t)((v0 << 2) | (v1 >> 4));
    if (src[2] != '=' && v2 >= 0 && out < dstLen)
      dst[out++] = (uint8_t)(((v1 & 0xF) << 4) | (v2 >> 2));
    if (src[3] != '=' && v3 >= 0 && out < dstLen)
      dst[out++] = (uint8_t)(((v2 & 0x3) << 6) | v3);
    src += 4;
  }
  return out;
}

// ── Header load ─────────────────────────────────────────────────────────────
static bool loadHeader() {
  available = false;
  if (!fsOk || !mediaStorage.exists(DATA_PATH))
    return false;

  StorageFile f = mediaStorage.open(DATA_PATH, "r");
  if (!f)
    return false;

  uint8_t hdr[HEADER_BYTES];
  bool ok = f.read(hdr, HEADER_BYTES) == HEADER_BYTES && hdr[0] == 'G' && hdr[1] == 'I' &&
            hdr[2] == 'D' && hdr[3] == '1';
  if (ok) {
    frameCount = (uint16_t)(hdr[4] | (hdr[5] << 8));
    gifW = (uint16_t)(hdr[6] | (hdr[7] << 8));
    gifH = (uint16_t)(hdr[8] | (hdr[9] << 8));
    ok =
        frameCount >= 1 && frameCount <= MAX_GIF_FRAMES && idleMediaDimensionsSupported(gifW, gifH);
  }
  if (ok) {
    for (int i = 0; i < frameCount; i++) {
      uint8_t d[2];
      if (f.read(d, 2) != 2) {
        ok = false;
        break;
      }
      delays[i] = (uint16_t)(d[0] | (d[1] << 8));
    }
  }
  if (ok) {
    frameBytes = (uint32_t)gifW * gifH * 2;
    dataOffset = HEADER_BYTES + (uint32_t)frameCount * 2;
    // The file must be large enough to hold every frame's pixels.
    ok = f.size() >= dataOffset + (uint32_t)frameCount * frameBytes;
  }
  f.close();

  available = ok;
  return ok;
}

// ── Public: init ────────────────────────────────────────────────────────────
void idleGifInit(TFT_eSPI *display) {
  tft = display;
  fsOk = mediaStorage.begin(); // SD preferred; never automatically format
  if (fsOk)
    loadHeader();
}

// ── Upload protocol ─────────────────────────────────────────────────────────
static void abortUpload() {
  if (tmpFile)
    tmpFile.close();
  if (fsOk && mediaStorage.exists(TMP_PATH))
    mediaStorage.remove(TMP_PATH);
  uploading = false;
}

static void handleSpaceQuery() {
  if (fsOk && !mediaStorage.freeBytesKnown()) {
    Serial.println(mediaStorage.freeSpaceFailed() ? "gif:space:-1" : "gif:space:pending");
    return;
  }
  uint64_t free = 0;
  if (fsOk) {
    free = mediaStorage.freeBytes();
    // The existing GIF will be overwritten, so its bytes are effectively free.
    if (mediaStorage.exists(DATA_PATH)) {
      StorageFile f = mediaStorage.open(DATA_PATH, "r");
      if (f) {
        free += f.size();
        f.close();
      }
    }
  }
  Serial.print("gif:space:");
  Serial.println((long)min(free, uint64_t(LONG_MAX)));
}

static void handleBegin(const char *line) {
  if (!fsOk) {
    Serial.println("gif:err");
    return;
  }
  abortUpload(); // discard any half-finished previous attempt
  idleGifStop(); // don't play while we overwrite

  // Free the old GIF up front so the temp file can use its space (flash is
  // tight). The space query already counts these bytes as available. The
  // trade-off: a failed upload leaves no idle GIF until the next attempt.
  available = false;
  if (mediaStorage.exists(DATA_PATH))
    mediaStorage.remove(DATA_PATH);

  const char *p = line + 10; // past "gif:begin:"
  char *end;
  long count = strtol(p, &end, 10);
  if (*end != ':') {
    Serial.println("gif:err");
    return;
  }
  p = end + 1;
  long w = strtol(p, &end, 10);
  if (*end != ':') {
    Serial.println("gif:err");
    return;
  }
  p = end + 1;
  long h = strtol(p, &end, 10);
  if (*end != ':') {
    Serial.println("gif:err");
    return;
  }
  p = end + 1;

  if (count < 1 || count > MAX_GIF_FRAMES || !idleMediaDimensionsSupported(w, h)) {
    Serial.println("gif:err");
    return;
  }

  uint16_t parsedDelays[MAX_GIF_FRAMES];
  for (int i = 0; i < count; i++) {
    long d = strtol(p, &end, 10);
    if (end == p) {
      Serial.println("gif:err");
      return;
    }
    if (d < 1)
      d = 1;
    if (d > 65535)
      d = 65535;
    parsedDelays[i] = (uint16_t)d;
    p = (*end == ',') ? end + 1 : end;
  }

  uint32_t fb = (uint32_t)w * h * 2;
  expectedPixelBytes = (uint32_t)count * fb;

  tmpFile = mediaStorage.open(TMP_PATH, "w");
  if (!tmpFile) {
    Serial.println("gif:err");
    return;
  }

  uint8_t hdr[HEADER_BYTES] = {
      'G',
      'I',
      'D',
      '1',
      (uint8_t)(count & 0xFF),
      (uint8_t)(count >> 8),
      (uint8_t)(w & 0xFF),
      (uint8_t)(w >> 8),
      (uint8_t)(h & 0xFF),
      (uint8_t)(h >> 8),
  };
  bool ok = tmpFile.write(hdr, HEADER_BYTES) == HEADER_BYTES;
  for (int i = 0; ok && i < count; i++) {
    uint8_t d[2] = {(uint8_t)(parsedDelays[i] & 0xFF), (uint8_t)(parsedDelays[i] >> 8)};
    ok = tmpFile.write(d, 2) == 2;
  }
  if (!ok) {
    abortUpload();
    Serial.println("gif:err");
    return;
  }

  receivedPixelBytes = 0;
  uploading = true;
  displayUploadBegin();
  Serial.println("gif:rdy");
}

static void handleData(const char *line) {
  if (!uploading) {
    Serial.println("gif:err");
    return;
  }
  int n = base64Decode(line + 6, decodeBuf, sizeof(decodeBuf)); // past "gif:d:"
  if (n <= 0 || receivedPixelBytes + n > expectedPixelBytes) {
    abortUpload();
    displayUploadEnd(false);
    Serial.println("gif:err");
    return;
  }
  if (tmpFile.write(decodeBuf, n) != (size_t)n) {
    abortUpload();
    displayUploadEnd(false);
    Serial.println("gif:err");
    return;
  }
  receivedPixelBytes += n;
  displayUploadProgress(expectedPixelBytes ? (float)receivedPixelBytes / expectedPixelBytes : 1.0f);
  Serial.println("gif:ack");
}

static void handleEnd() {
  if (!uploading) {
    Serial.println("gif:err");
    return;
  }
  tmpFile.close();
  uploading = false;

  if (receivedPixelBytes != expectedPixelBytes) {
    if (mediaStorage.exists(TMP_PATH))
      mediaStorage.remove(TMP_PATH);
    Serial.println("gif:err");
    displayUploadEnd(false);
    return;
  }

  if (mediaStorage.exists(DATA_PATH))
    mediaStorage.remove(DATA_PATH);
  if (!mediaStorage.rename(TMP_PATH, DATA_PATH)) {
    if (mediaStorage.exists(TMP_PATH))
      mediaStorage.remove(TMP_PATH);
    Serial.println("gif:err");
    displayUploadEnd(false);
    return;
  }

  bool ok = loadHeader();
  Serial.println(ok ? "gif:done" : "gif:err");
  displayUploadEnd(ok);
}

static void handleClear() {
  idleGifStop();
  abortUpload();
  if (fsOk && mediaStorage.exists(DATA_PATH))
    mediaStorage.remove(DATA_PATH);
  available = false;
  Serial.println("gif:cleared");
}

bool idleGifHandleLine(const char *line) {
  if (strncmp(line, "gif:", 4) != 0)
    return false;

  if (strcmp(line, "gif:space?") == 0)
    handleSpaceQuery();
  else if (strncmp(line, "gif:begin:", 10) == 0)
    handleBegin(line);
  else if (strncmp(line, "gif:d:", 6) == 0)
    handleData(line);
  else if (strcmp(line, "gif:end") == 0)
    handleEnd();
  else if (strcmp(line, "gif:clear") == 0)
    handleClear();
  // Unknown gif:* line: consumed but ignored.
  return true;
}

// ── Playback ────────────────────────────────────────────────────────────────
bool idleGifAvailable() {
  return available;
}
bool idleGifRendering() {
  return playing && framePending;
}

static void beginFrame(int index) {
  if (!tft || !playFile)
    return;
  uint32_t base = dataOffset + (uint32_t)index * frameBytes;
  framePending = playFile.seek(base);
  nextRow = 0;
}

static void drawFrameBand() {
  if (!framePending || !playing || !tft || !playFile)
    return;
  int x0 = (PANEL_W - gifW) / 2;
  int y0 = (PANEL_H - gifH) / 2;
  int rows = (gifH - nextRow < BAND_ROWS) ? gifH - nextRow : BAND_ROWS;
  size_t bytes = (size_t)rows * gifW * 2;
  if (playFile.read((uint8_t *)bandBuf, bytes) != int(bytes)) {
    framePending = false;
    return;
  }
  // One band per loop: serial input and touch can interrupt between bands.
  tft->setSwapBytes(true); // stored little-endian; bus wants high-byte-first
  tft->pushImage(x0, y0 + nextRow, gifW, rows, bandBuf);
  tft->setSwapBytes(false);
  nextRow += rows;
  framePending = nextRow < gifH;
}

void idleGifStart(unsigned long now) {
  if (!available || !tft)
    return;
  playFile = mediaStorage.open(DATA_PATH, "r");
  if (!playFile)
    return;

  if (tft)
    tft->fillScreen(TFT_WHITE);
  frameIndex = 0;
  beginFrame(0);
  lastFrameMs = now;
  playing = true;
}

void idleGifStop() {
  if (playFile)
    playFile.close();
  playing = false;
  framePending = false;
}

void idleGifTick(unsigned long now) {
  if (!playing)
    return;
  if (framePending) {
    drawFrameBand();
    return;
  }
  if (frameCount <= 1)
    return; // Single-frame media is complete; no repeated reads.
  if (now - lastFrameMs < delays[frameIndex])
    return;

  frameIndex = (frameIndex + 1) % frameCount;
  beginFrame(frameIndex);
  drawFrameBand();
  lastFrameMs = now;
}
#endif
