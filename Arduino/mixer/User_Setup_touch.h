// Provisional TFT_eSPI profile for an ESP32-2432S028-family board.
// -----------------------------------------------------------------------
// Copy this file to your TFT_eSPI library folder, replacing the default
// User_Setup.h there (it overrides the whole file, so fonts must be
// listed here — without LOAD_GLCD, drawString() silently does nothing).
//
// Candidate controller: ILI9341. The bare ESP32-2432S028 PCB marking does not
// confirm the display IC. Verify this profile on the physical board first.
// -----------------------------------------------------------------------

#define ILI9341_2_DRIVER

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH

// The built-in panel is reliable around 55 MHz. Lower this to 40 MHz if a board
// revision shows SPI noise or intermittent drawing.
#define SPI_FREQUENCY 55000000
#define SPI_READ_FREQUENCY 20000000

// Fonts — required; without these drawString() renders nothing
#define LOAD_GLCD  // Font 1: default 5x7 pixel font (used by setTextSize)
#define LOAD_FONT2 // Font 2: 16px
#define LOAD_FONT4 // Font 4: 26px
#define LOAD_GFXFF // Adafruit GFX free fonts
#define SMOOTH_FONT
