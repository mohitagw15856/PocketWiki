// Logical button values shared by the input mapper and the UI widgets. Kept
// free of any Arduino dependency so the widgets (keyboard, list menu) can be
// unit tested on the host.
#pragma once

#include <cstdint>

namespace pocketwiki {

enum class Button : uint8_t { None, Up, Down, Left, Right, Select, Back };

}  // namespace pocketwiki
