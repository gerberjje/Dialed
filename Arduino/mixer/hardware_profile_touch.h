#pragma once

// Provisional profile for the common resistive-touch ESP32-2432S028 variant.
// The PCB family marking alone does not confirm these controller ICs or pins.
// Verify on the physical board before treating this profile as final.
#define DIALED_HW_PROFILE "cyd-ili9341-xpt2046-candidate"

static const int TOUCH_CLK = 25;
static const int TOUCH_MISO = 39;
static const int TOUCH_MOSI = 32;
static const int TOUCH_CS = 33;
static const int TOUCH_IRQ = 36;

// Typical raw bounds for the candidate XPT2046 profile. Tune after confirming
// the touch controller and checking all four screen edges.
static const int TOUCH_X_MIN = 200;
static const int TOUCH_X_MAX = 3900;
static const int TOUCH_Y_MIN = 200;
static const int TOUCH_Y_MAX = 3900;
// Panel-specific: the photographed board requires INVON for normal RGB565 colors.
// Applied in displaySetup, independent of the global TFT_eSPI User_Setup.
#define DIALED_PANEL_INVERSION true

// Common CYD onboard RGB LED (Espressif jczn_2432s028r variant), active low.
static const int STATUS_LED_RED = 4;
static const int STATUS_LED_GREEN = 16;
static const int STATUS_LED_BLUE = 17;
static const bool STATUS_LED_ACTIVE_LOW = true;
