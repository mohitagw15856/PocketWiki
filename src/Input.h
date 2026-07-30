// Logical buttons mapped from inkkit's raw GPIO edges.
//
// The X4/X3 has a small cluster of physical buttons and a power button. inkkit
// exposes per-index edge queries; this maps those indices onto the logical
// directions the UI uses. One edge is reported per poll so a single frame never
// triggers two actions.
//
// TODO(hardware-test): the physical button indices below are a best guess and
// must be confirmed on device. Only this table changes if the layout differs;
// every screen speaks in logical Button values. See docs/HARDWARE_TESTING.md.
#pragma once

#include <cstdint>

#include "ui/Button.h"

#ifdef ARDUINO

#include "inkkit/Buttons.h"

namespace pocketwiki {

class Input {
 public:
  explicit Input(inkkit::Buttons& buttons) : buttons_(buttons) {}

  // Physical button indices (best guess). Confirm on device.
  static constexpr uint8_t kIdxUp = 0;
  static constexpr uint8_t kIdxDown = 1;
  static constexpr uint8_t kIdxLeft = 2;
  static constexpr uint8_t kIdxRight = 3;
  static constexpr uint8_t kIdxSelect = 4;
  static constexpr uint8_t kIdxBack = 5;

  // Sample once per loop, then read the first pressed edge.
  void update() { buttons_.update(); }

  Button poll() const {
    if (buttons_.wasPressed(kIdxSelect)) return Button::Select;
    if (buttons_.wasPressed(kIdxBack)) return Button::Back;
    if (buttons_.wasPressed(kIdxUp)) return Button::Up;
    if (buttons_.wasPressed(kIdxDown)) return Button::Down;
    if (buttons_.wasPressed(kIdxLeft)) return Button::Left;
    if (buttons_.wasPressed(kIdxRight)) return Button::Right;
    return Button::None;
  }

  bool anyEdge() const {
    for (uint8_t i = 0; i <= kIdxBack; ++i) {
      if (buttons_.wasPressed(i)) return true;
    }
    return false;
  }

  uint32_t powerHeldMs() const { return buttons_.powerHeldMs(); }

 private:
  inkkit::Buttons& buttons_;
};

}  // namespace pocketwiki

#endif  // ARDUINO
