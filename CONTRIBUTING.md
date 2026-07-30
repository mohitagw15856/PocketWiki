# Contributing to PocketWiki

Thanks for your interest in PocketWiki. This project has two halves that can be
worked on independently: the Python companion that builds archives, and the C++
firmware and its portable core.

## Ground rules

- Documentation is written in British English and avoids em dashes.
- Keep the memory discipline: the device streams from the SD card and does heavy
  work on the desktop. New on device features should not load whole files or make
  allocations proportional to the collection size.
- Prefer putting logic in the portable core (`core/pocketwiki/`, free of Arduino
  and the SDK) so it can be unit tested on the host. Keep `src/` thin.

## Development setup

You need a C++17 compiler, CMake and Python 3.9 or newer. PlatformIO is needed
only to build the firmware for the device.

### Run the C++ core tests

```bash
cmake -S test -B test/build
cmake --build test/build -j
ctest --test-dir test/build --output-on-failure
```

The test build fetches inkkit from GitHub. To use a local checkout instead:

```bash
cmake -S test -B test/build -DINKKIT_SRC_DIR=/path/to/inkkit/src
```

### Run the companion tests

```bash
cd companion
python -m pip install pytest
python -m pytest -q
```

### Build the firmware

```bash
pio run -e xteink_x4     # or xteink_x3
```

## Making changes

- **Archive format.** The format is specified in `docs/ARCHIVE_FORMAT.md` and
  implemented twice: the writer and reference reader in
  `companion/pocketwiki/archive.py`, and the device reader in
  `core/pocketwiki/PwaReader.*`. Any format change must update the spec and both
  implementations, and bump `FORMAT_VERSION`. The interop test in
  `test/test_reader.cpp` reads a companion built archive through the C++ reader
  and must stay green.
- **Fold key.** `companion/pocketwiki/fold.py` and `core/pocketwiki/Fold.h` must
  stay byte for byte identical. Both have tests covering the same cases.
- **Font.** Do not edit `core/pocketwiki/Font5x7.h` by hand. Edit the glyph art in
  `tools/genfont.py` and regenerate.
- **Hardware assumptions.** Anything you cannot verify without the device should
  be marked with a `TODO(hardware-test)` comment and listed in
  `docs/HARDWARE_TESTING.md`.

## Tests are required

Both test suites run in CI and must pass. New behaviour in the core, the widgets
or the companion should come with tests. The firmware build job runs too; it is
currently non blocking while the on device SDK build is being confirmed.

## Commits and pull requests

- Keep commits focused and write clear messages.
- Describe what changed and why in the pull request, and note any
  `TODO(hardware-test)` items you added.
