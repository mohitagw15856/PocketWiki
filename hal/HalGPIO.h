// Minimal HAL GPIO/buttons interface that satisfies inkkit. See HalStorage.h for
// why this shim exists.
//
// TODO(hardware-test): back this with the freeink-sdk InputManager so real
// button edges and the wake reason are reported.
#pragma once

#include <cstdint>

class HalGPIO {
 public:
  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };

  void update() {}
  bool wasPressed(uint8_t) { return false; }
  bool wasReleased(uint8_t) { return false; }
  bool isPressed(uint8_t) { return false; }
  uint32_t getPowerButtonHeldTime() { return 0; }
  WakeupReason getWakeupReason() { return WakeupReason::AfterFlash; }
};

extern HalGPIO gpio;
