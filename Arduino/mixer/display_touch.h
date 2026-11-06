#pragma once
#include <Arduino.h>

void displaySetup();
void displaySelectKnob(int knobIndex);
void displayShowKnob(int knobIndex, float value);
void displayShowMute(int knobIndex, bool muted);
void displaySyncKnobState(int knobIndex, float value, bool muted);
// -1 = unknown; 0 = off; 1 = on. Silent update; never wakes the screen.
void displaySyncDiscordState(int knobIndex, int muted, int deafened);
void displaySetShowPercent(bool show);
void displayAssignmentChanged();
void displayEnterIdle();
void displaySetPower(bool powered);

// Output-switch card. pos: 0 = A, 1 = B.
// OUT_PENDING = drawn locally the moment the toggle moves, before the PC has
// confirmed. The other three are what an "out:" line reports.
enum OutState : uint8_t { OUT_PENDING = 0,
                          OUT_OK = 1,
                          OUT_NONE = 2,
                          OUT_FAIL = 3 };
void displayShowOutput(int pos, uint8_t state);

void displayBlank(bool blank);
void displayTick();

// GIF-upload progress screen (driven by idlegif.cpp while a new GIF flashes).
void displayUploadBegin();
void displayUploadProgress(float frac); // 0..1
void displayUploadEnd(bool ok);

// Button 1 = minus, 2 = playback mute, 3 = plus, 4 = Discord mic icon, 5 = headset icon.
void displayTouchFeedback(int button);

int displayActiveKnob();
