#include "controller_profile.h"
#if DIALED_CONTROLLER_TOUCH
#include "storage_touch.h"
#include "firmware_config_touch.h"
#include <LittleFS.h>
#include <limits.h>
#include "free_space_scan_touch.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#if SPI_DRIVER_SELECT != 2 || !MAINTAIN_FREE_CLUSTER_COUNT
#error Build with SPI_DRIVER_SELECT=2 and MAINTAIN_FREE_CLUSTER_COUNT=1 for SdFat
#endif
static SoftSpiDriver<DIALED_SD_MISO, DIALED_SD_MOSI, DIALED_SD_SCK> sdSPI;
static SdFat32 sd;
static SemaphoreHandle_t storageMutex = nullptr;
static FreeSpaceScan freeScan;
static bool scanTaskFailed = false;
StorageGuard::StorageGuard() {
  if (storageMutex)
    xSemaphoreTakeRecursive(storageMutex, portMAX_DELAY);
}
StorageGuard::~StorageGuard() {
  if (storageMutex)
    xSemaphoreGiveRecursive(storageMutex);
}
void storageWritten() {
  StorageGuard guard;
  freeScan.invalidate();
}
void storageYield() {
  // Give a waiting counter a turn between data sectors, not only between full
  // display bands. The worker also yields after each small FAT batch.
  taskYIELD();
}
static void countFreeSpace(void *) {
  for (;;) {
    bool pending;
    {
      StorageGuard guard;
      pending = freeScan.pending();
      for (int batch = 0; pending && batch < 2 && freeScan.pending(); ++batch)
        freeScan.step(
            [](uint32_t cluster, uint32_t *value) { return sd.vol()->dbgFat(cluster, value); });
    }
    // Release the shared filesystem after two FAT sectors; rendering, uploads and
    // serial queries never wait for the entire scan. All mutations restart it.
    vTaskDelay(pending ? 1 : pdMS_TO_TICKS(100));
  }
}
DialedStorage mediaStorage;
bool DialedStorage::begin() {
  if (!storageMutex) {
    storageMutex = xSemaphoreCreateRecursiveMutex();
    if (!storageMutex)
      return false;
  }
  StorageGuard guard;
  if (ready)
    return true;
  sdOK = sd.begin(SdSpiConfig(DIALED_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(0), &sdSPI));
  // Never autoformat either medium.
  ready = sdOK || LittleFS.begin(false);
  // Copy existing idle media once. Keep the flash copy for card-free boots.
  if (sdOK && !sd.exists("/flash-copy.done") && !sd.exists("/idle.dat") && LittleFS.begin(false) &&
      LittleFS.exists("/idle.dat")) {
    auto source = LittleFS.open("/idle.dat", "r");
    auto target = sd.open("/migrate.tmp", O_WRONLY | O_CREAT | O_TRUNC);
    uint8_t buffer[512];
    bool ok = bool(source) && bool(target);
    while (ok && source.available()) {
      size_t count = source.read(buffer, sizeof(buffer));
      ok = count > 0 && target.write(buffer, count) == count;
    }
    source.close();
    target.close();
    if (ok && sd.rename("/migrate.tmp", "/idle.dat")) {
      auto marker = sd.open("/flash-copy.done", O_WRONLY | O_CREAT);
      marker.close();
    } else
      sd.remove("/migrate.tmp");
  }
  if (sdOK) {
    freeScan.begin(sd.vol()->clusterCount(), sd.vol()->bytesPerClusterShift());
    scanTaskFailed = xTaskCreate(countFreeSpace, "sd-free", 3072, nullptr, 2, nullptr) != pdPASS;
  }
  return ready;
}
bool DialedStorage::exists(const char *p) {
  StorageGuard guard;
  return sdOK ? sd.exists(p) : LittleFS.exists(p);
}
bool DialedStorage::remove(const char *p) {
  StorageGuard guard;
  if (sdOK)
    storageWritten();
  return sdOK ? sd.remove(p) : LittleFS.remove(p);
}
bool DialedStorage::rename(const char *a, const char *b) {
  StorageGuard guard;
  if (sdOK)
    storageWritten();
  return sdOK ? sd.rename(a, b) : LittleFS.rename(a, b);
}
StorageFile DialedStorage::open(const char *p, const char *mode) {
  StorageGuard guard;
  if (sdOK && (mode[0] == 'w' || mode[0] == 'a'))
    storageWritten();
  StorageFile f;
  f.onSD = sdOK;
  if (sdOK)
    f.card = sd.open(p, mode[0] == 'w'   ? O_WRONLY | O_CREAT | O_TRUNC
                        : mode[0] == 'a' ? O_WRONLY | O_CREAT | O_APPEND
                                         : O_RDONLY);
  else
    f.flash = LittleFS.open(p, mode);
  return f;
}
uint64_t DialedStorage::freeBytes() {
  StorageGuard guard;
  if (!ready)
    return 0;
  if (!sdOK)
    return LittleFS.totalBytes() - LittleFS.usedBytes();
  return freeScan.bytes();
}
bool DialedStorage::freeBytesKnown() {
  StorageGuard guard;
  return ready && (!sdOK || (!scanTaskFailed && freeScan.ready()));
}
bool DialedStorage::freeSpaceFailed() {
  StorageGuard guard;
  return !ready || (sdOK && (scanTaskFailed || freeScan.failed()));
}
#endif
