#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SdFat.h>
#include <utility>
// The worker and foreground code share SdFat's caches and SPI bus.
class StorageGuard {
public:
  StorageGuard();
  ~StorageGuard();
  StorageGuard(const StorageGuard &) = delete;
  StorageGuard &operator=(const StorageGuard &) = delete;
};
void storageWritten();
void storageYield();
// One file interface for FAT32 SD and the internal LittleFS fallback.
class StorageFile {
public:
  fs::File flash;
  File32 card;
  bool onSD = false;
  StorageFile() = default;
  StorageFile(const StorageFile &) = delete;
  StorageFile &operator=(const StorageFile &) = delete;
  StorageFile(StorageFile &&other) {
    *this = std::move(other);
  }
  StorageFile &operator=(StorageFile &&other) {
    if (this == &other)
      return *this;
    StorageGuard guard;
    close();
    flash = std::move(other.flash);
    card = std::move(other.card);
    onSD = other.onSD;
    return *this;
  }
  ~StorageFile() {
    close();
  }
  explicit operator bool() {
    StorageGuard guard;
    return onSD ? bool(card) : bool(flash);
  }
  int read(uint8_t *b, size_t n) {
    if (!onSD) {
      StorageGuard guard;
      return flash.read(b, n);
    }
    size_t total = 0;
    while (total < n) {
      const size_t chunk = n - total < 512 ? n - total : 512;
      int count;
      {
        StorageGuard guard;
        count = card.read(b + total, chunk);
      }
      if (count < 0)
        return total ? int(total) : count;
      total += count;
      if (size_t(count) != chunk)
        break;
      storageYield();
    }
    return int(total);
  }
  size_t write(const uint8_t *b, size_t n) {
    StorageGuard guard;
    if (onSD)
      storageWritten();
    return onSD ? card.write(b, n) : flash.write(b, n);
  }
  bool seek(uint32_t p) {
    StorageGuard guard;
    return onSD ? card.seekSet(p) : flash.seek(p);
  }
  uint32_t size() {
    StorageGuard guard;
    return onSD ? card.fileSize() : flash.size();
  }
  void close() {
    StorageGuard guard;
    if (onSD)
      card.close();
    else
      flash.close();
  }
};
class DialedStorage {
public:
  bool begin();
  bool isSD() const {
    return sdOK;
  }
  bool exists(const char *path);
  bool remove(const char *path);
  bool rename(const char *from, const char *to);
  StorageFile open(const char *path, const char *mode);
  uint64_t freeBytes();
  bool freeBytesKnown();
  bool freeSpaceFailed();

private:
  bool sdOK = false, ready = false;
};
extern DialedStorage mediaStorage;
