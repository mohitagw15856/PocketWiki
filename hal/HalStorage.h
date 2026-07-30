// Minimal HAL storage interface that satisfies inkkit.
//
// Why this exists: inkkit is written against an ecosystem HAL (HalStorage,
// HalDisplay, HalGPIO, HalPowerManager) that it neither ships nor declares as a
// dependency. In the CrossPoint firmware that HAL lives in lib/hal and wraps the
// freeink-sdk hardware libraries (FreeInkDisplay, SDCardManager, InputManager,
// PowerManager, ...). Without it, any inkkit based firmware fails to compile.
//
// This header provides that interface so the firmware compiles and links for the
// real ESP32-C3 target, which is what the CI build verifies. The method bodies
// here are safe no-ops: a compile shim, not working drivers. Wiring each method
// to the matching freeink-sdk library is the remaining on-device work, tracked in
// docs/HARDWARE_TESTING.md and docs/INKKIT_GAPS.md.
//
// TODO(hardware-test): back HalStorage and HalFile with the freeink-sdk
// SDCardManager so archives are read from the SD card on device.
#pragma once

#include <Arduino.h>  // Arduino String, used by inkkit's Storage helpers

#include <cstddef>
#include <cstdint>
#include <functional>

// SdFat style open flags, defined only if the real SD library has not.
#ifndef O_RDONLY
#define O_RDONLY 0x00
#define O_WRONLY 0x01
#define O_RDWR 0x02
#define O_APPEND 0x08
#define O_CREAT 0x100
#endif

// A seekable file handle. inkkit's SdFileReader/Writer drive these methods.
class HalFile {
 public:
  explicit operator bool() const { return valid_; }

  uint32_t size() const { return size_; }
  bool seekSet(uint32_t pos) {
    if (pos > size_) return false;
    pos_ = pos;
    return true;
  }
  uint32_t position() const { return pos_; }

  // Reads up to n bytes; returns bytes read (0 at end of file). Stub returns 0.
  int read(void* dst, size_t n) {
    (void)dst;
    (void)n;
    return 0;
  }
  // Writes n bytes; returns bytes written. Stub reports success.
  size_t write(const void* src, size_t n) {
    (void)src;
    return n;
  }

  // Set by the storage backend when a real file is opened.
  bool valid_ = false;
  uint32_t size_ = 0;
  uint32_t pos_ = 0;
};

// The SD storage singleton inkkit talks to.
class HalStorage {
 public:
  bool exists(const char*) { return false; }
  void ensureDirectoryExists(const char*) {}
  bool openFileForRead(const char* /*tag*/, const char* /*path*/, HalFile& /*out*/) { return false; }
  bool openFileForWrite(const char* /*tag*/, const char* /*path*/, HalFile& /*out*/) { return false; }
  HalFile open(const char* /*path*/, int /*flags*/) { return HalFile{}; }
  String readFile(const char*) { return String(); }
  bool writeFile(const char*, String) { return true; }
  void listDir(const char* /*dir*/, const std::function<void(const char*, bool, size_t)>& /*cb*/) {}
};

extern HalStorage Storage;
