# tools

Developer tools. None of these run on the device.

- `genfont.py`: authors the 5x7 bitmap font and generates
  `core/pocketwiki/Font5x7.h`. Do not edit the header by hand; edit the glyph art
  here and regenerate.
- `render_screens.cpp`: renders genuine UI frames on the host using the real
  firmware drawing code (Canvas, font, ListMenu, Keyboard, ArticleLayout) against
  a `.pwa` archive, and writes them as PGM images.
- `make_media.py`: frames those PGMs into the PNG screenshots and animated GIFs
  in `docs/images/` using Pillow.

## Regenerating the README media

```bash
# From the repository root
gcc -O2 -c core/third_party/puff/puff.c -o /tmp/puff.o
g++ -std=c++17 -O2 -I core -I core/third_party/puff -I <path-to>/inkkit/src -I src \
    tools/render_screens.cpp core/pocketwiki/PwaReader.cpp /tmp/puff.o -o /tmp/render_screens
mkdir -p /tmp/frames
/tmp/render_screens companion/sample/foundations-of-science.pwa /tmp/frames
python -m pip install pillow
python tools/make_media.py /tmp/frames docs/images
```

The screenshots are illustrative renders of the firmware drawing code, not
photographs of hardware. On device capture is tracked in
`docs/HARDWARE_TESTING.md`.
