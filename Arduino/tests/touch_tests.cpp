// Compile the actual input implementation against simulated hardware boundaries.
#include <cstdlib>
#include <iostream>
#define DIALED_CONTROLLER_TOUCH 1
#include "../mixer/knobs_touch.cpp"

bool knobDiscordMute[MAX_KNOBS] = {};
int visibleChannel = 0;
bool wakeNext = false;
void displaySelectKnob(int n) {
  visibleChannel = n;
}
int displayActiveKnob() {
  return visibleChannel;
}
void displayShowKnob(int n, float) {
  visibleChannel = n;
}
void displayTouchFeedback(int) {
}
bool wake() {
  bool result = wakeNext;
  wakeNext = false;
  return result;
}
int checks = 0;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
void release() {
  testContact = false;
  knobsLoop();
}
void touch(int x, int y, unsigned long time = 0, int pressure = 500) {
  // Choose raw coordinates that map exactly to the requested screen pixel.
  testPoint = {200 + (x * 3700 + 318) / 319, 200 + (y * 3700 + 238) / 239, pressure};
  testContact = true;
  testTime = time;
  knobsLoop();
}
void reset() {
  release();
  Serial.output.clear();
  visibleChannel = 0;
  knobsSetActiveCount(1);
  knobsSetActiveCount(5);
  knobsSetTouchWakeCallback(wake);
  for (bool& enabled : knobDiscordMute)
    enabled = false;
}
int main() {
  knobsSetup(nullptr);
  float value = -1;
  check(touchArcValue(200, 45, value) && fabsf(value - .5f) < .001f, "arc midpoint");
  check(touchArcValue(153, 158, value) && fabsf(value) < .001f, "arc minimum");
  check(touchArcValue(247, 158, value) && fabsf(value - 1) < .001f, "arc maximum");
  check(!touchArcValue(200, 111, value), "arc interior ignored");
  check(!touchArcValue(200, 180, value), "arc bottom gap ignored");
  check(!touchArcValue(200, 28, value), "outside arc ignored");
  for (bool discord : {false, true}) {
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 320; ++x) {
        int expected = 0;
        if (y >= 204 && y < 237) {
          if (x >= 3 && x < 97)
            expected = 1;
          if (x >= 113 && x < 207)
            expected = 2;
          if (x >= 223 && x < 317)
            expected = 3;
        }
        if (discord && y >= 158 && y < 194) {
          if (x >= 8 && x < 50)
            expected = 4;
          if (x >= 52 && x < 94)
            expected = 5;
        }
        check(touchActionAt(x, y, discord) == expected, "touch target boundaries");
        if (expected >= 4)
          check(!touchArcValue(x, y, value), "Discord target clear of volume arc");
      }
  }
  reset();
  touch(250, 220, 0);
  touch(250, 220, 349);
  check(Serial.output == "knob1:up\n", "no early repeat");
  touch(250, 220, 350);
  touch(250, 220, 459);
  touch(250, 220, 460);
  check(Serial.output == "knob1:up\nknob1:up\nknob1:up\n", "repeat delay and cadence");
  reset();
  touch(20, 220);
  check(Serial.output == "knob1:down\n", "minus protocol");
  reset();
  touch(150, 220);
  touch(150, 220, 1000);
  check(Serial.output == "knob1:press\n", "playback mute does not repeat while held");
  reset();
  knobDiscordMute[0] = true;
  touch(29, 176);
  touch(73, 176, 400);
  touch(250, 220, 800);
  check(Serial.output == "knob1:discordmute\n", "Discord contact suppresses sliding actions");
  release();
  touch(73, 176);
  check(Serial.output == "knob1:discordmute\nknob1:discorddeafen\n", "release rearms Discord");
  reset();
  touch(29, 176);
  check(Serial.output.empty(), "Discord controls disabled on ordinary channels");
  reset();
  wakeNext = true;
  touch(150, 220);
  touch(250, 220, 400);
  check(Serial.output.empty(), "wake contact consumed until release");
  release();
  touch(150, 220);
  check(Serial.output == "knob1:press\n", "input resumes after wake contact");
  reset();
  touch(200, 45);
  touch(200, 45, 100);
  check(Serial.output == "knob1:0.500\n", "absolute volume and duplicate filtering");
  touch(29, 176, 200);
  check(Serial.output == "knob1:0.500\n", "arc drag cannot trigger buttons");
  reset();
  touch(280, 10);
  release();
  touch(250, 220);
  check(Serial.output == "knob5:up\n", "fifth tab routes channel");
  reset();
  visibleChannel = 3;
  touch(250, 220);
  check(Serial.output == "knob4:up\n", "host-selected channel respected");
  reset();
  knobsSetActiveCount(2);
  touch(280, 10);
  release();
  touch(250, 220);
  check(Serial.output == "knob1:up\n", "inactive tabs ignored");
  reset();
  knobsSetActiveCount(0);
  touch(250, 220);
  release();
  touch(200, 45);
  check(Serial.output.empty(), "zero channels disables input");
  reset();
  touch(250, 220, 0, 199);
  check(Serial.output.empty(), "low pressure ignored");
  std::cout << checks << " checks passed\n";
}
