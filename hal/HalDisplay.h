// Minimal HAL display interface that satisfies inkkit. See HalStorage.h for why
// this shim exists.
//
// TODO(hardware-test): back this with the freeink-sdk FreeInkDisplay so the
// framebuffer is pushed to the panel. The dimensions below are illustrative for
// a 4.2 inch panel and must be confirmed on device.
#pragma once

#include <cstdint>

class HalDisplay {
 public:
  static constexpr int DISPLAY_WIDTH = 400;
  static constexpr int DISPLAY_HEIGHT = 300;
  static constexpr int DISPLAY_WIDTH_BYTES = (DISPLAY_WIDTH + 7) / 8;
  static constexpr uint32_t BUFFER_SIZE =
      static_cast<uint32_t>(DISPLAY_WIDTH_BYTES) * DISPLAY_HEIGHT;

  enum RefreshMode { FULL_REFRESH, FAST_REFRESH };

  void begin() {}
  uint8_t* getFrameBuffer() { return framebuffer_; }
  void displayBuffer(RefreshMode) {}
  void deepSleep() {}

 private:
  uint8_t framebuffer_[BUFFER_SIZE] = {};
};

extern HalDisplay display;
