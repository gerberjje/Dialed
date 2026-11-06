#include <iostream>
#include "../mixer/mixer.ino"

int checks = 0, volumeChannel = -1, muteChannel = -1, outputPosition = -1;
float lastVolume = -1;
bool lastMute = false, blanked = false;
int configuredCount = -1, gifTicks = 0, assignmentUpdates = 0;
bool gifConsumed = false;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
void displaySetup() {
}
void displayShowKnob(int n, float v) {
  volumeChannel = n;
  lastVolume = v;
}
void displayShowMute(int n, bool v) {
  muteChannel = n;
  lastMute = v;
}
void displaySetShowPercent(bool) {
}
void displayShowOutput(int pos, uint8_t) {
  outputPosition = pos;
}
void displayBlank(bool v) {
  blanked = v;
}
void displayTick() {
  ++gifTicks;
}
void displayEnterIdle() {
}
void knobsSetup(KnobCallback) {
}
void knobsLoop() {
}
void knobsSetActiveCount(int n) {
  configuredCount = n;
}
void knobsSetSwitchCallback(SwitchCallback) {
}
bool handleAssignLine(const char*) {
  return false;
}
bool handleIconLine(const char*) {
  return false;
}
bool handleOutSetLine(const char*) {
  return false;
}
bool idleGifHandleLine(const char*) {
  return gifConsumed;
}

#if DIALED_CONTROLLER_TOUCH
bool knobDiscordMute[MAX_KNOBS] = {};
int stateChannel = -1, discordChannel = -1, discordMuted = -2, discordDeafened = -2;
bool connected = false, powered = true;
DialedStorage mediaStorage;
uint64_t DialedStorage::freeBytes() {
  return 123456789;
}
bool testSpaceKnown = true, testSpaceFailed = false;
bool DialedStorage::freeBytesKnown() {
  return testSpaceKnown;
}
bool DialedStorage::freeSpaceFailed() {
  return testSpaceFailed;
}
void assignmentsInit() {
}
bool assignmentsHandleProfile(const char*) {
  return false;
}
void displayAssignmentChanged() {
  ++assignmentUpdates;
}
void displaySyncKnobState(int n, float, bool) {
  stateChannel = n;
}
void displaySyncDiscordState(int n, int m, int d) {
  discordChannel = n;
  discordMuted = m;
  discordDeafened = d;
}
void displaySetPower(bool v) {
  powered = v;
}
void setPcConnectionStatus(bool v) {
  connected = v;
}
void statusLedInit() {
}
void statusLedSetScreenOn(bool) {
}
void knobsSetTouchWakeCallback(TouchWakeCallback) {
}
#endif

void send(const std::string& line) {
  Serial.input = line;
  Serial.cursor = 0;
  readIncomingSerial();
}
int main() {
  setup();
  check(Serial.output == std::string("fw:") + FW_BOARD + ":" + FW_VERSION + "\n", "boot identity");
#if DIALED_CONTROLLER_TOUCH
  check(std::string(FW_BOARD) == "esp32-mixer-touch", "touch identity");
  check(MAX_KNOBS == 5, "touch channel limit");
#else
  check(std::string(FW_BOARD) == "esp32-mixer", "default mixer identity");
  check(MAX_KNOBS == 4, "mixer channel limit");
#endif
  Serial.output.clear();
  send("ver?\r\n");
  check(Serial.output == std::string("fw:") + FW_BOARD + ":" + FW_VERSION + "\n",
        "version reply once for CRLF");
  Serial.output.clear();
  send("media?\n");
  check(Serial.output == (DIALED_CONTROLLER_TOUCH ? "media:320:240\n" : "media:240:240\n"),
        "media capabilities match the selected controller");
  send("vol:knob2:0.");
  check(volumeChannel == -1, "partial lines buffered");
  send("75\nmute:knob2:1\ncfg:knobs:3\nout:b:ok\n");
  check(volumeChannel == 1 && lastVolume == .75f, "volume routing after fragmented input");
  check(muteChannel == 1 && lastMute, "mute routing");
  check(configuredCount == 3 && outputPosition == 1, "configuration and output routing");
  send("vol:knob0:0.5\nvol:knob99:0.5\n");
  check(volumeChannel == 1 && lastVolume == .75f, "out of range channels ignored");
  send("screen:off\nver?\n");
  check(blanked, "metadata does not wake suspended display");
  send("screen:on\n");
  check(!blanked, "explicit resume");
  gifConsumed = true;
  send("vol:knob1:0.1\n");
  gifConsumed = false;
  check(volumeChannel == 1, "consumed GIF input skips other parsers");
  Serial.output.clear();
  send("unknown\n\r\n");
  check(Serial.output.empty(), "unknown and empty commands produce no replies");
#if DIALED_CONTROLLER_TOUCH
  isIdle = true;
  lastKnobActivity = 17;
  send("state:knob5:0.4:1\ndiscord:knob5:-1:1\n");
  check(stateChannel == 4 && discordChannel == 4 && discordMuted == -1 && discordDeafened == 1,
        "touch state synchronization");
  check(isIdle && lastKnobActivity == 17, "silent state does not reset idle activity");
  send("discord:knob0:0:0\ndiscord:knob6:0:0\ndiscord:knob1:2:0\n"
       "discord:knob1:0:1junk\ndiscord:knob1::1\n");
  check(discordChannel == 4 && discordMuted == -1, "invalid Discord states ignored");
  send("cfg:discord:knob5:1\n");
  check(knobDiscordMute[4] && discordMuted == -1 && discordDeafened == -1,
        "enabling Discord clears stale state");
  Serial.output.clear();
  send("storage?\n");
  check(Serial.output == "storage:flash:123456789\n", "touch storage reply");
  Serial.output.clear();
  testSpaceKnown = false;
  send("storage?\nver?\n");
  check(Serial.output ==
            std::string("storage:flash:pending\nfw:") + FW_BOARD + ":" + FW_VERSION + "\n",
        "pending storage never reports zero or blocks serial queries");
  Serial.output.clear();
  testSpaceFailed = true;
  send("storage?\n");
  check(Serial.output == "storage:flash:unavailable\n", "storage scan errors are explicit");
  testSpaceKnown = true;
  testSpaceFailed = false;
  send("screen:off\nvol:knob1:0.5\n");
  check(screenOffRequested && !displayPowered && blanked,
        "host traffic respects screen-off request");
  send("screen:on\n");
  check(!screenOffRequested && displayPowered, "touch power resumes");
#else
  send("state:knob5:0.4:1\ndiscord:knob5:-1:1\ncfg:discord:knob5:1\nstorage?\n");
  check(Serial.output.empty(), "mixer ignores unsupported touch extensions");
#endif
  std::cout << (DIALED_CONTROLLER_TOUCH ? "touch" : "mixer") << " protocol: " << checks
            << " checks passed\n";
}
