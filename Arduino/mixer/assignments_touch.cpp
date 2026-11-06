#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "assignments_touch.h"
#include "storage_touch.h"

char knobLabel[MAX_KNOBS][32] = {};
static uint16_t iconCache[ICON_PIXELS] = {};
static int cachedIcon = -1;
static bool storageOK = false;
bool knobHasIcon[MAX_KNOBS] = {};
uint16_t knobColor[MAX_KNOBS] = {0x065F, 0x065F, 0x065F, 0x065F, 0x065F}; // default accent
uint8_t knobKind[MAX_KNOBS] = {};
bool knobDiscordMute[MAX_KNOBS] = {};

char outLabel[OUT_POSITIONS][32] = {};
uint8_t outKind[OUT_POSITIONS] = {OUT_KIND_SPEAKER, OUT_KIND_SPEAKER};

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

// Parse "assign:knob1:RRGGBB:<a|s|m>:AppName". The older format without a
// kind field remains accepted for compatibility.
bool handleAssignLine(const char *line) {
  if (strncmp(line, "assign:", 7) != 0)
    return false;
  const char *p = line + 7; // "knob1:RRGGBB:AppName"
  if (strncmp(p, "knob", 4) != 0)
    return false;

  int idx = atoi(p + 4) - 1; // 1-based -> 0-based
  if (idx < 0 || idx >= MAX_KNOBS)
    return false;

  const char *c1 = strchr(p, ':'); // after "knobN"
  if (!c1)
    return false;
  const char *colorStr = c1 + 1;          // "RRGGBB:AppName"
  const char *c2 = strchr(colorStr, ':'); // after color
  if (!c2)
    return false;

  if (c2 - colorStr >= 6) {
    char hex[7];
    memcpy(hex, colorStr, 6);
    hex[6] = '\0';
    long rgb = strtol(hex, nullptr, 16);
    uint8_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    knobColor[idx] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  const char *label = c2 + 1;
  if (label[0] && label[1] == ':') {
    knobKind[idx] = label[0] == 'm'   ? KNOB_KIND_MIC
                    : label[0] == 's' ? KNOB_KIND_SYSTEM
                                      : KNOB_KIND_APP;
    label += 2;
  } else {
    knobKind[idx] = KNOB_KIND_APP;
  }

  // An assignment without a following icon (system/microphone) must not keep
  // the bitmap from the app previously assigned to this knob.
  knobHasIcon[idx] = false;
  if (cachedIcon == idx)
    cachedIcon = -1;

  strncpy(knobLabel[idx], label, 31);
  knobLabel[idx][31] = '\0';
  // Also recognize current localized names when paired with an older host that
  // does not send the channel-kind field.
  if (strcmp(knobLabel[idx], "Microphone") == 0 || strcmp(knobLabel[idx], "Microfone") == 0)
    knobKind[idx] = KNOB_KIND_MIC;
  return true;
}

// Parse "icon:knob1:<base64>"
bool handleIconLine(const char *line) {
  if (strncmp(line, "icon:", 5) != 0)
    return false;
  const char *rest = line + 5; // "knob1:<base64>"
  const char *colon = strchr(rest, ':');
  if (!colon || strncmp(rest, "knob", 4) != 0)
    return false;

  int idx = atoi(rest + 4) - 1;
  if (idx < 0 || idx >= MAX_KNOBS)
    return false;

  int decoded = base64Decode(colon + 1, (uint8_t *)iconCache, ICON_PIXELS * 2);
  knobHasIcon[idx] = false;
  cachedIcon = -1;
  if (decoded == ICON_PIXELS * 2 && storageOK) {
    char path[24];
    snprintf(path, sizeof(path), "/icon%d.bin", idx);
    auto f = mediaStorage.open(path, "w");
    if (f)
      knobHasIcon[idx] = f.write((uint8_t *)iconCache, sizeof(iconCache)) == sizeof(iconCache);
    f.close();
    if (knobHasIcon[idx])
      cachedIcon = idx;
  }
  return true;
}

// Parse "outset:<a|b>:<h|s>:<DeviceName>"
// Note the guard order: "out:" cannot reach here because its 4th char is ':'
// while this line's is 's', so the two prefixes never collide.
bool handleOutSetLine(const char *line) {
  if (strncmp(line, "outset:", 7) != 0)
    return false;
  const char *p = line + 7; // "<a|b>:<h|s>:<Name>"

  int pos;
  if (p[0] == 'a' || p[0] == 'A')
    pos = 0;
  else if (p[0] == 'b' || p[0] == 'B')
    pos = 1;
  else
    return false;
  if (p[1] != ':')
    return false;

  const char *kindStr = p + 2; // "<h|s>:<Name>"
  if (kindStr[0] == '\0' || kindStr[1] != ':')
    return false;
  outKind[pos] = (kindStr[0] == 'h' || kindStr[0] == 'H') ? OUT_KIND_HEADPHONE : OUT_KIND_SPEAKER;

  strncpy(outLabel[pos], kindStr + 2, 31);
  outLabel[pos][31] = '\0';
  return true;
}

const uint16_t *assignmentIcon(int index) {
  if (index < 0 || index >= MAX_KNOBS)
    return iconCache;
  if (cachedIcon != index) {
    memset(iconCache, 0, sizeof(iconCache));
    char path[24];
    snprintf(path, sizeof(path), "/icon%d.bin", index);
    auto f = mediaStorage.open(path, "r");
    if (!f || f.read((uint8_t *)iconCache, sizeof(iconCache)) != sizeof(iconCache)) {
      memset(iconCache, 0, sizeof(iconCache));
      knobHasIcon[index] = false;
    }
    f.close();
    cachedIcon = index;
  }
  return iconCache;
}
void assignmentsInit() {
  storageOK = mediaStorage.begin();
}
// A named device snapshot; PC application/session mappings remain owned by Windows.
// Icons are included so changing assignments cannot corrupt saved snapshots.
bool assignmentsHandleProfile(const char *line) {
  bool save = strncmp(line, "profile:save:", 13) == 0;
  bool load = strncmp(line, "profile:load:", 13) == 0;
  if (!save && !load)
    return false;
  const char *name = line + 13;
  size_t n = strlen(name);
  if (!storageOK || n == 0 || n > 24) {
    Serial.println("profile:err");
    return true;
  }
  for (size_t i = 0; i < n; i++)
    if (!isalnum((unsigned char)name[i]) && name[i] != '_' && name[i] != '-') {
      Serial.println("profile:err");
      return true;
    }
  char path[48], temp[48];
  snprintf(path, sizeof(path), "/p_%s.bin", name);
  snprintf(temp, sizeof(temp), "/p_%s.tmp", name);
  auto f = mediaStorage.open(save ? temp : path, save ? "w" : "r");
  bool ok = bool(f);
  uint8_t magic[4] = {'D', 'P', 'R', '1'};
  if (save)
    ok = ok && f.write(magic, 4) == 4;
  else {
    uint8_t h[4];
    ok = ok && f.read(h, 4) == 4 && memcmp(h, magic, 4) == 0;
  }
  // Load validates the entire fixed-size file before changing live assignments.
  const size_t expected = 4 + sizeof(knobLabel) + sizeof(knobColor) + sizeof(knobKind) +
                          sizeof(knobDiscordMute) + sizeof(outLabel) + sizeof(outKind) +
                          MAX_KNOBS * (1 + sizeof(iconCache)) + 4;
  if (load) {
    ok = ok && f.size() == expected;
    // Validate the complete snapshot before mutating live state.
    uint32_t hash = 2166136261u, stored = 0;
    uint8_t bytes[256];
    size_t remaining = expected - 4;
    ok = ok && f.seek(0);
    while (ok && remaining) {
      size_t count = remaining > sizeof(bytes) ? sizeof(bytes) : remaining;
      ok = f.read(bytes, count) == int(count);
      for (size_t j = 0; j < count; j++)
        hash = (hash ^ bytes[j]) * 16777619u;
      remaining -= count;
    }
    ok = ok && f.read((uint8_t *)&stored, 4) == 4 && hash == stored && f.seek(4);
  }
#define FIELD(x)                                                                                   \
  if (ok) {                                                                                        \
    if (save)                                                                                      \
      ok = f.write((const uint8_t *)x, sizeof(x)) == sizeof(x);                                    \
    else                                                                                           \
      ok = f.read((uint8_t *)x, sizeof(x)) == sizeof(x);                                           \
  }
  FIELD(knobLabel);
  FIELD(knobColor);
  FIELD(knobKind);
  FIELD(knobDiscordMute);
  FIELD(outLabel);
  FIELD(outKind);
#undef FIELD
  for (int i = 0; ok && i < MAX_KNOBS; i++) {
    uint8_t present = knobHasIcon[i] ? 1 : 0;
    if (save) {
      const uint16_t *pixels = assignmentIcon(i);
      ok = f.write(&present, 1) == 1 &&
           f.write((const uint8_t *)pixels, sizeof(iconCache)) == sizeof(iconCache);
    } else {
      ok = f.read(&present, 1) == 1 &&
           f.read((uint8_t *)iconCache, sizeof(iconCache)) == sizeof(iconCache);
      char iconPath[24];
      snprintf(iconPath, sizeof(iconPath), "/icon%d.bin", i);
      auto icon = mediaStorage.open(iconPath, "w");
      ok = ok && bool(icon) &&
           icon.write((uint8_t *)iconCache, sizeof(iconCache)) == sizeof(iconCache);
      icon.close();
      knobHasIcon[i] = ok && present;
    }
  }
  f.close();
  cachedIcon = -1;
  if (save && ok) {
    auto check = mediaStorage.open(temp, "r");
    uint32_t hash = 2166136261u;
    uint8_t bytes[256];
    size_t remaining = expected - 4;
    ok = bool(check) && check.size() == remaining;
    while (ok && remaining) {
      size_t count = remaining > sizeof(bytes) ? sizeof(bytes) : remaining;
      ok = check.read(bytes, count) == int(count);
      for (size_t j = 0; j < count; j++)
        hash = (hash ^ bytes[j]) * 16777619u;
      remaining -= count;
    }
    check.close();
    auto append = mediaStorage.open(temp, "a");
    ok = ok && bool(append) && append.write((uint8_t *)&hash, 4) == 4;
    append.close();
    if (!ok) {
      Serial.println("profile:err");
      return true;
    }
    if (mediaStorage.exists(path))
      mediaStorage.remove(path);
    ok = mediaStorage.rename(temp, path);
  }
  for (int i = 0; i < MAX_KNOBS; i++)
    knobLabel[i][31] = 0;
  for (int i = 0; i < OUT_POSITIONS; i++)
    outLabel[i][31] = 0;
  Serial.println(ok ? "profile:ok" : "profile:err");
  return true;
}
#endif
