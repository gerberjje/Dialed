#pragma once
#include <Arduino.h>
#include <algorithm>
#include <memory>
#include <vector>
namespace fs {
inline size_t maxReadRequest = 0, bytesRead = 0;
struct File {
  std::shared_ptr<std::vector<uint8_t>> data;
  size_t position = 0;
  File() = default;
  explicit File(std::shared_ptr<std::vector<uint8_t>> bytes) : data(std::move(bytes)) {
  }
  explicit operator bool() {
    return bool(data);
  }
  int read(uint8_t* buffer, size_t length) {
    maxReadRequest = std::max(maxReadRequest, length);
    if (!data)
      return -1;
    size_t count = std::min(length, data->size() - std::min(position, data->size()));
    memcpy(buffer, data->data() + position, count);
    position += count;
    bytesRead += count;
    return int(count);
  }
  size_t write(const uint8_t* buffer, size_t length) {
    if (!data)
      return 0;
    data->resize(std::max(data->size(), position + length));
    memcpy(data->data() + position, buffer, length);
    position += length;
    return length;
  }
  bool seek(uint32_t offset) {
    if (!data || offset > data->size())
      return false;
    position = offset;
    return true;
  }
  uint32_t size() {
    return data ? uint32_t(data->size()) : 0;
  }
  void close() {
    data.reset();
  }
};
} // namespace fs
