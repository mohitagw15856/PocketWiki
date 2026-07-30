// Minimal HAL power manager interface that satisfies inkkit. See HalStorage.h for
// why this shim exists.
//
// TODO(hardware-test): back this with the freeink-sdk PowerManager so the device
// actually enters deep sleep.
#pragma once

class HalGPIO;

class HalPowerManager {
 public:
  void startDeepSleep(HalGPIO&) {}
};

extern HalPowerManager powerManager;
