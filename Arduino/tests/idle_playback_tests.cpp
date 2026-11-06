#include <iostream>
#include <map>
#define DIALED_CONTROLLER_TOUCH 1
#include "../mixer/idlegif_touch.cpp"

int checks = 0, lockDepth = 0, yields = 0;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
StorageGuard::StorageGuard() {
  ++lockDepth;
}
StorageGuard::~StorageGuard() {
  --lockDepth;
}
void storageYield() {
  check(lockDepth == 0, "file reads yield outside the storage lock");
  ++yields;
}
void storageWritten() {
}
std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> files;
DialedStorage mediaStorage;
bool DialedStorage::begin() {
  sdOK = ready = true;
  return true;
}
bool DialedStorage::exists(const char* path) {
  return files.count(path);
}
bool DialedStorage::remove(const char* path) {
  return files.erase(path);
}
bool DialedStorage::rename(const char* from, const char* to) {
  if (!files.count(from))
    return false;
  files[to] = files[from];
  files.erase(from);
  return true;
}
StorageFile DialedStorage::open(const char* path, const char* mode) {
  if (mode[0] == 'w')
    files[path] = std::make_shared<std::vector<uint8_t>>();
  StorageFile f;
  f.onSD = true;
  if (files.count(path))
    f.card = File32(files[path]);
  return f;
}
uint64_t DialedStorage::freeBytes() {
  return 10000000;
}
bool DialedStorage::freeBytesKnown() {
  return true;
}
bool DialedStorage::freeSpaceFailed() {
  return false;
}
void displayUploadBegin() {
}
void displayUploadProgress(float) {
}
void displayUploadEnd(bool) {
}

void seed(int width, int height, int count = 2) {
  idleGifStop();
  files.clear();
  auto bytes = std::make_shared<std::vector<uint8_t>>();
  auto word = [&](int value) {
    bytes->push_back(value & 255);
    bytes->push_back((value >> 8) & 255);
  };
  bytes->insert(bytes->end(), {'G', 'I', 'D', '1'});
  word(count);
  word(width);
  word(height);
  for (int i = 0; i < count; ++i)
    word(10);
  for (int i = 0; i < count; ++i)
    for (int pixel = 0; pixel < width * height; ++pixel)
      word(i ? 0x5678 : 0x1234);
  files["/idle.dat"] = bytes;
}
int main() {
  for (int width : {240, 320}) {
    seed(width, 240);
    TFT_eSPI display;
    idleGifInit(&display);
    check(idleGifAvailable(), "valid existing media loads");
    fs::bytesRead = fs::maxReadRequest = 0;
    idleGifStart(0);
    check(display.bands.empty() && fs::bytesRead == 0,
          "starting playback does not synchronously draw a full frame");
    idleGifTick(0);
    check(display.bands.size() == 1, "one tick draws at most one band");
    check(fs::bytesRead < size_t(width * 240 * 2),
          "tick leaves work for future serial/input loop iterations");
    int row = 0;
    while (row < 240) {
      const auto band = display.bands.back();
      check(band.x == (320 - width) / 2 && band.y == row,
            "native and legacy frames positioned correctly");
      check(band.width == width && band.height <= IDLE_MEDIA_BAND_ROWS && band.pixel == 0x1234 &&
                band.swapped,
            "band geometry and RGB565 byte order");
      row += band.height;
      if (row < 240)
        idleGifTick(0);
    }
    check(row == 240 && !display.swapped,
          "frame covers panel height exactly and restores TFT byte order");
    auto bands = display.bands.size();
    idleGifTick(9);
    check(display.bands.size() == bands, "next frame waits for its delay");
    idleGifTick(10);
    check(display.bands.size() == bands + 1 && display.bands.back().pixel == 0x5678,
          "next frame starts once its delay expires");
    check(fs::maxReadRequest <= 512 && yields > 0, "SD reads share the bus between sectors");
    idleGifStop();
    idleGifTick(20);
    check(display.bands.size() == bands + 1,
          "touch/serial interruption cancels incomplete rendering");
  }
  seed(320, 240, 1);
  TFT_eSPI display;
  idleGifInit(&display);
  idleGifStart(0);
  for (int i = 0; i < 240; ++i)
    idleGifTick(i);
  auto reads = fs::bytesRead;
  idleGifTick(10000);
  check(fs::bytesRead == reads, "static image is not reread after its bands complete");
  seed(321, 240);
  idleGifInit(&display);
  check(!idleGifAvailable(), "oversized persisted media rejected");
  std::cout << checks << " idle-playback checks passed\n";
}
