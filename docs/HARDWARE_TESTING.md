# Hardware testing checklist

PocketWiki's portable core (archive reading, decompression, search, layout,
history and bookmarks) and the UI widget logic are covered by host tests. The
remaining items can only be confirmed on a real Xteink X4 or X3, because they
depend on the freeink-sdk build and on panel and button behaviour. Each item
below corresponds to a best guess implementation marked with a
`TODO(hardware-test)` comment in the code, or to behaviour that needs a human to
look at the screen.

Work through this list on device and tick items off. If a value is wrong, the fix
is almost always confined to the single file named.

## Build and SDK wiring

- [ ] **Firmware builds via PlatformIO.** `pio run -e xteink_x4` completes.
      inkkit and the firmware compile against the HAL interface in `hal/`. This
      is verified in CI; the shim makes it a real compile check.
- [ ] **Wire the HAL to the freeink-sdk (the main on-device task).** `hal/`
      currently provides safe no-op bodies so the firmware compiles and links.
      Replace them with implementations backed by the freeink-sdk libraries, and
      add those libraries to `platformio.ini` `lib_deps`:
      - `HalStorage` and `HalFile` to `SDCardManager` (read archives from SD).
      - `HalDisplay` to `FreeInkDisplay` (push the framebuffer to the panel).
      - `HalGPIO` to `InputManager` (real button edges and wake reason).
      - `HalPowerManager` to `PowerManager` (deep sleep).
      Each `hal/*.h` method carries a `TODO(hardware-test)` naming its target.
- [ ] **Hardware singletons.** `src/main.cpp` and `hal/hal_shim.cpp` define the
      `display`, `gpio`, `powerManager` and `Storage` globals. When wiring the
      real HAL, confirm the freeink-sdk's own instance names and construction.
- [ ] **inkkit inherited SDK TODOs.** inkkit's own `TODO(hardware-test)` notes
      cover the display method and constant names, the Storage and `HalFile`
      API (including directory iteration and append flags), the GPIO enums and
      the power manager entry point. Confirm these hold; if not, they are fixed
      once in inkkit and PocketWiki inherits the fix.

## Display

- [ ] **Framebuffer bit order and polarity.** `core/pocketwiki/Canvas.h` assumes
      most significant bit is the leftmost pixel and that a set bit is white.
      Draw a known pattern and confirm text is not mirrored, inverted or
      byte swapped. Only `Canvas.h` changes if the panel differs.
- [ ] **Panel dimensions.** The UI reads width and height from `inkkit::Display`
      at runtime, so the X4 (larger) and X3 (smaller) should both lay out
      correctly. Confirm margins, the header and footer bars and the reader
      content area look right on each panel.
- [ ] **Full vs fast refresh.** Screen changes request a full refresh and in
      screen updates request a fast refresh (`Display::flush(full)`). Confirm
      ghosting is acceptable and that a periodic full refresh is not needed while
      paging.

## Fonts and rendering

- [ ] **Glyph legibility.** The 5x7 font (`core/pocketwiki/Font5x7.h`, generated
      by `tools/genfont.py`) is legible at scale 1 for body text and scale 2 for
      headings. Tune the glyph art in `tools/genfont.py` and regenerate if any
      character is hard to read.
- [ ] **Bold and italic.** Bold thickens by one pixel and italic shears the upper
      rows. Confirm both are distinguishable from regular text at reading size.
- [ ] **Word wrap and pagination.** Open a long article and page through it.
      Confirm no line overflows the content width and that the last line of a
      page is not clipped.

## Input

- [ ] **Button indices.** `src/Input.h` maps logical directions to physical
      button indices 0..5 as a guess. Confirm each physical button triggers the
      intended action; correct the index constants in `Input.h` only.
- [ ] **Power button hold.** Holding power for `kPowerOffHoldMs` (800 ms) saves
      state and enters deep sleep. Confirm the hold duration feels right and does
      not clash with the wake tap.
- [ ] **Idle sleep.** After `kIdleSleepMs` (60 s) with no input the device saves
      state and sleeps. Confirm it wakes cleanly and repaints.

## Storage and state

- [ ] **Archive discovery.** Place one or more `.pwa` files in `/pocketwiki/` and
      confirm they are listed; with several, the picker appears and remembers the
      last opened archive via `/pocketwiki/.state/last.txt`.
- [ ] **History and bookmarks persist.** Read a few articles, add a bookmark,
      power cycle, and confirm history and bookmarks survive (stored under
      `/pocketwiki/.state/`).
- [ ] **Large article memory.** The reader lays out one article at a time. Very
      large articles are avoided by the build tool, but confirm a worst case
      article from a big collection does not exhaust RAM. If it does, page the
      layout instead of building it whole.

## Search

- [ ] **Incremental search.** Typing on the keyboard updates results live and
      accent folding matches (for example, typing without the accent finds an
      accented title). Confirm latency is acceptable when the collection is
      large, since each keystroke does a binary search plus a short scan on SD.
