# PocketWiki

PocketWiki is an offline reference reader for the Xteink X4 and X3 pocket e-ink
devices. It serves compressed article collections from the SD card: browse an
indexed archive, search titles as you type, and read articles with headings,
emphasis, lists and internal links, all rendered from a compact binary that the
device never has to parse as HTML.

It is a standalone firmware built on the shared
[inkkit](https://github.com/mohitagw15856/inkkit) device layer. It reuses
interaction patterns from CrossPoint Reader (the on screen keyboard, the SD card
cache convention, streaming from storage) but shares no code with it. See
[ARCHITECTURE.md](ARCHITECTURE.md) for the reasoning.

## Features

- Browse an indexed archive of articles from `/pocketwiki/*.pwa`.
- Incremental, accent insensitive title search on a button driven on screen
  keyboard.
- An article renderer with headings, bold and italic, lists, horizontal rules
  and internal links between articles.
- History and bookmarks, persisted to the SD card.
- A Python companion that builds archives from Markdown, Wikipedia titles or a
  Kiwix ZIM subset, and warns when an archive's index would exceed a safe on
  device memory budget.

## Screenshots

<!-- Screenshots pending on device capture. Add images to docs/images/ and link
them here once hardware testing is complete. -->

- Home menu: _placeholder_
- Browse and search: _placeholder_
- Article reader with internal links: _placeholder_

## Compatibility

- Built for the Xteink X4 and X3 (ESP32-C3). The panel size is read at runtime,
  so one firmware serves both.
- Works with the CrossPoint Reader ecosystem as of **CrossPoint v1.5.0**: it uses
  the same freeink-sdk device layer through inkkit and follows the same SD card
  conventions. PocketWiki is standalone and does **not** require CrossPoint to be
  installed; the two can coexist on the same SD card, PocketWiki under
  `/pocketwiki/` and CrossPoint under its own directories.

## Installing the firmware

PocketWiki builds with [PlatformIO](https://platformio.org/).

```bash
# Install PlatformIO Core (pioarduino flavour, matching the ecosystem)
python -m pip install -U https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip

# Build for the X4 (or xteink_x3 for the X3)
pio run -e xteink_x4

# Flash over USB
pio run -e xteink_x4 -t upload
```

The firmware depends on inkkit and the freeink-sdk, which PlatformIO fetches
automatically as library dependencies (see `platformio.ini`).

> On device verification is in progress. The parts that depend on the SDK build
> and on panel and button behaviour are tracked in
> [docs/HARDWARE_TESTING.md](docs/HARDWARE_TESTING.md).

## Putting content on the device

1. Build an archive with the companion (see `companion/README.md`):

   ```bash
   cd companion
   python -m pocketwiki build markdown sample_content -o foundations-of-science.pwa \
       -n "Foundations of Science"
   ```

2. Copy the `.pwa` file into a `/pocketwiki/` folder on the SD card.
3. Insert the card and power on. If several archives are present, PocketWiki
   shows a picker and remembers your last choice.

A ready made sample archive built from about 50 public domain articles ships in
`companion/sample/foundations-of-science.pwa`.

## Building and testing from source

Two independent test suites cover the project, and both run in CI:

```bash
# Portable C++ core and UI widget logic (host build, no hardware needed)
cmake -S test -B test/build
cmake --build test/build -j
ctest --test-dir test/build --output-on-failure

# Companion Python tool
cd companion
python -m pip install pytest
python -m pytest -q
```

The core test build also generates a small archive with the companion and reads
it back through the C++ reader, so the two implementations of the `.pwa` format
are checked against each other on every build.

## Repository layout

| Path | What |
| --- | --- |
| `companion/` | Python CLI to build and inspect archives, plus its tests |
| `core/` | Portable, host tested C++ (archive reader, decoder, layout, models) |
| `src/` | Device firmware (screens, keyboard, inkkit wiring) |
| `test/` | Native test build (CMake) |
| `docs/` | Format specification, gaps, hardware testing checklist |

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md): design decisions and licence compatibility.
- [docs/ARCHIVE_FORMAT.md](docs/ARCHIVE_FORMAT.md): the `.pwa` on disk format.
- [docs/INKKIT_GAPS.md](docs/INKKIT_GAPS.md): what PocketWiki needed beyond inkkit.
- [docs/HARDWARE_TESTING.md](docs/HARDWARE_TESTING.md): the on device checklist.
- [CONTRIBUTING.md](CONTRIBUTING.md): how to contribute.

## Licence

MIT, see [LICENSE](LICENSE). The vendored `puff` decompressor is under the zlib
licence (`core/third_party/puff/`). The sample article text is dedicated to the
public domain (CC0).
