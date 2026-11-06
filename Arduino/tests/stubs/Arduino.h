#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iomanip>
inline unsigned long testTime = 0;
template <class T>
T min(T a, T b) {
  return a < b ? a : b;
}
inline unsigned long millis() {
  return testTime;
}
template <class T>
T constrain(T v, T lo, T hi) {
  return v < lo ? lo : v > hi ? hi
                              : v;
}
inline long map(long v, long a, long b, long c, long d) {
  return (v - a) * (d - c) / (b - a) + c;
}
struct TestSerial {
  std::string output;
  std::string input;
  size_t cursor = 0;
  int available() {
    return static_cast<int>(input.size() - cursor);
  }
  int read() {
    return cursor < input.size() ? input[cursor++] : -1;
  }
  void setRxBufferSize(int) {
  }
  void begin(int) {
  }
  template <class T>
  void print(T v) {
    std::ostringstream s;
    s << v;
    output += s.str();
  }
  void println(const char* v) {
    print(v);
    output += '\n';
  }
  template <class T>
  void println(T v) {
    print(v);
    output += '\n';
  }
  void println(float v, int digits) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(digits) << v;
    output += s.str() + "\n";
  }
};
inline TestSerial Serial;
