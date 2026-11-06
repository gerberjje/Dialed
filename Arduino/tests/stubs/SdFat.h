#pragma once
#include <FS.h>
struct File32 : fs::File {
  File32() = default;
  explicit File32(std::shared_ptr<std::vector<uint8_t>> bytes) : fs::File(std::move(bytes)) {
  }
  bool seekSet(uint32_t offset) {
    return seek(offset);
  }
  uint32_t fileSize() {
    return size();
  }
};
