#pragma once

// Retained as the input boundary. Touch events use the same wire protocol as
// rotary encoders and are echoed back by the PC.
typedef void (*KnobCallback)(const char *id, float value);

void knobsSetup(KnobCallback cb);
void knobsLoop();

// Called on a new touch. Return true when the touch woke the idle screen and
// should be consumed until release instead of activating a control.
typedef bool (*TouchWakeCallback)();
void knobsSetTouchWakeCallback(TouchWakeCallback cb);

// Kept for protocol compatibility. This touch build has no output toggle, so
// the callback is accepted but never fired.
typedef void (*SwitchCallback)(int pos);
void knobsSetSwitchCallback(SwitchCallback cb);

// Number of virtual controls exposed by the PC, clamped to five touch tabs.
void knobsSetActiveCount(int n);
