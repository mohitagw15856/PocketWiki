# Architecture

PocketWiki is an offline reference reader for the Xteink X4 and X3 pocket e-ink
devices (ESP32-C3, roughly 380 KB usable RAM, 4.2 inch panel on the X4, SD card,
physical buttons, no touchscreen). It serves compressed article collections from
the SD card, with browse, incremental title search, an article renderer, history
and bookmarks.

This document records the main structural decisions, most importantly whether
PocketWiki should be a fork of CrossPoint Reader, a module offered upstream, or a
standalone application, and why the chosen dependencies are licence compatible.

## The decision: a standalone application on top of inkkit

We evaluated three options against the CrossPoint Reader codebase (studied under
`/reference`, kept out of this repository by `.gitignore`).

**(a) Fork CrossPoint Reader.** CrossPoint is a capable EPUB and text reader.
Forking it would inherit its EPUB pipeline, its font system and its activity
framework. It would also inherit a large surface we do not need: EPUB and ZIP
parsing, OPDS network browsing, a Wi-Fi web server, dictionaries and a
sophisticated multi font renderer. Our content model is different in kind. We do
not read books, we serve a pre-indexed reference corpus where every article is
tokenised at build time so the device never parses markup. Carrying the whole
reader to add a different content type would be a poor fit and a maintenance
burden, and it would couple us tightly to CrossPoint's internal churn.

**(b) A module or patch for upstream.** PocketWiki could in principle be a new
"activity" inside CrossPoint. But an offline encyclopaedia with its own archive
format, its own on device index and its own search UI is a whole application, not
a feature that slots cleanly into the book reader's model. Upstreaming it would
force CrossPoint to take on our archive format and build tooling, which is not a
reasonable ask. A future integration (for example, opening a `.pwa` from
CrossPoint's home screen) remains possible without our code living inside it.

**(c) A standalone application. Chosen.** PocketWiki is its own firmware built on
the shared device layer, **inkkit**. inkkit provides thin, single source wrappers
over the freeink-sdk (display framebuffer, SD storage and byte streaming,
buttons, power) that the ecosystem firmwares share. Building on inkkit gives us a
small, well scoped device layer without reimplementing hardware access, while
leaving us free to design an archive format and reader tuned for streaming a
reference corpus within a tight RAM budget.

We reuse CrossPoint's **patterns**, not its code:

- an activity or screen state machine with a back stack;
- a button driven on screen keyboard for text entry, the same interaction model
  CrossPoint uses on its keyboard;
- a hidden per collection cache directory on the SD card, mirroring CrossPoint's
  `/.crosspoint` convention (we use `/pocketwiki/.state`);
- streaming everything from the SD card rather than loading whole files.

No CrossPoint source is copied into this repository.

## Licence compatibility

- CrossPoint Reader is MIT licensed (Copyright Dave Allie). We reuse only ideas,
  so no licence obligation attaches, but MIT would be compatible in any case.
- inkkit is MIT licensed.
- puff, the small DEFLATE decompressor we vendor for on device inflate, is under
  the zlib licence, which is compatible with MIT. It is vendored unmodified with
  its notice under `core/third_party/puff/`.
- PocketWiki itself is MIT licensed.

All dependencies are therefore licence compatible with this project and with
each other.

## Component map

```
companion/                Python CLI: build and inspect .pwa archives, plus tests
  pocketwiki/             fold key, markdown/html parsing, render stream, archive
  sample_content/         ~50 public-domain Markdown articles (CC0)
  sample/                 the built sample archive (foundations-of-science.pwa)

core/                     Portable, host-tested C++ (no Arduino, no hardware)
  pocketwiki/             PwaReader, RenderStream, Fold, Text, Canvas,
                          ArticleLayout, Library (history/bookmarks), Font5x7
  third_party/puff/       vendored DEFLATE decompressor

src/                      Device firmware (compiled only for the target)
  PocketWikiApp           screen state machine wiring the core to inkkit
  ui/                     Keyboard and ListMenu widgets (portable, host-tested)
  Input, main.cpp         inkkit and freeink-sdk wiring

test/                     Native test build (CMake) exercising core + widgets
docs/                     Format spec, gaps, hardware testing checklist
```

## The split that makes this testable

The hard, correctness sensitive logic lives in `core/` and is free of any Arduino
or SDK dependency: the archive reader, the DEFLATE inflate, the render stream
decoder, the fold key, text wrapping, article pagination and the history and
bookmark models. All of it is unit tested on the host, and an interop test reads
an archive built by the Python companion through the C++ reader, so the two
implementations of the format are checked against each other on every build.

The `src/` layer is deliberately thin: it maps buttons to screens and draws the
core's output through inkkit's framebuffer. The parts of it that carry logic (the
keyboard and the list menu) are also Arduino free and host tested. Only the
final wiring to the panel, GPIO and SD singletons is device specific, and that is
where the remaining on device verification is focused (see
`docs/HARDWARE_TESTING.md`).

## Memory discipline

- The archive never loads whole. The header, title table, directory and article
  blocks are all read by seeking and streaming from the SD card.
- Article bodies are compressed in bounded blocks; the device inflates one block
  at a time into a single reusable buffer sized by the archive header
  (`maxBlockRaw`, 4 KB by default).
- The title index is a sorted table binary searched in place; search touches only
  a handful of small records per query, never an allocation proportional to the
  collection.
- All heavy processing (fetching, HTML stripping, tokenising, compression) is
  done on the desktop by the companion, never on the device.

See `docs/ARCHIVE_FORMAT.md` for the on disk format and `docs/INKKIT_GAPS.md` for
capabilities PocketWiki needed that inkkit does not yet provide.
