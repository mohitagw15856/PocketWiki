// Definitions of the HAL singletons inkkit and the firmware reference. Keeping
// them in one translation unit means main.cpp only has to declare them extern.
//
// This is the compile shim described in the HAL headers. Replacing these globals
// with freeink-sdk backed implementations is the on-device work; the interface
// they present is exactly what inkkit expects.
#include "HalDisplay.h"
#include "HalGPIO.h"
#include "HalPowerManager.h"
#include "HalStorage.h"

HalStorage Storage;
HalDisplay display;
HalGPIO gpio;
HalPowerManager powerManager;
