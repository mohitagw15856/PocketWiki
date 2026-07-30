// Firmware entry point: wire the freeink-sdk hardware singletons into inkkit's
// thin wrappers and hand them to PocketWikiApp.
//
// TODO(hardware-test): the freeink-sdk exposes the display, GPIO and power
// manager as global singletons. inkkit's Storage.cpp already uses the global
// `Storage`; the display/gpio/power instance names and types below follow the
// same ecosystem convention and must be confirmed against the pinned SDK on
// device. See docs/HARDWARE_TESTING.md. If a name differs, only this file and
// the matching inkkit wrapper change.
#include <Arduino.h>

#include "Input.h"
#include "PocketWikiApp.h"
#include "inkkit/inkkit.h"

// Provided by the freeink-sdk. Declared extern here so the linker binds them to
// the SDK's instances.
extern HalDisplay display;
extern HalGPIO gpio;
extern HalPowerManager powerManager;

using namespace pocketwiki;

namespace {
inkkit::Display g_display(display);
inkkit::Buttons g_buttons(gpio);
inkkit::Power g_power(powerManager, display, gpio);
Input g_input(g_buttons);
PocketWikiApp g_app(g_display, g_input, g_power);
}  // namespace

void setup() { g_app.begin(); }

void loop() {
  g_app.tick();
  delay(10);
}
