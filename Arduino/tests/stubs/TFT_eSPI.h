#pragma once
#include <Arduino.h>
#include <vector>
static constexpr uint16_t TFT_WHITE = 0xFFFF;
class TFT_eSPI {
public:
  struct Band {
    int x, y, width, height;
    uint16_t pixel;
    bool swapped;
  };
  std::vector<Band> bands;
  bool swapped = false;
  void fillScreen(uint16_t) {
  }
  void setSwapBytes(bool value) {
    swapped = value;
  }
  void pushImage(int x, int y, int width, int height, const uint16_t *pixels) {
    bands.push_back({x, y, width, height, pixels[0], swapped});
  }
};
