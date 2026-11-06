#pragma once
#include "SPI.h"
struct TS_Point {
  int x = 0, y = 0, z = 0;
};
inline TS_Point testPoint;
inline bool testContact = false;
struct XPT2046_Touchscreen {
  XPT2046_Touchscreen(int, int) {
  }
  void begin(SPIClass &) {
  }
  void setRotation(int) {
  }
  bool touched() {
    return testContact;
  }
  TS_Point getPoint() {
    return testPoint;
  }
};
