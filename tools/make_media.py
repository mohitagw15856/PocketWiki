#!/usr/bin/env python3
"""Turn the PGM frames from render_screens into README media.

Produces framed PNG screenshots and animated GIFs with a simple device bezel.
The pixels come from the real firmware rendering code (see render_screens.cpp);
this script only frames and animates them. British English; no em dashes.

Usage: python tools/make_media.py <frames_dir> <out_dir>
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

SCALE = 2
PAD = 26
CHIN = 52
BODY = "#22262c"
EDGE = "#3a4049"
SCREEN_EDGE = "#0e1013"
LABEL = "#aeb6c2"
ACCENT = "#5b6270"


def _font(size):
    for path in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    ):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


LABEL_FONT = _font(15)


def load_pgm(path):
    return Image.open(path).convert("L")


def bezel(screen_l, caption):
    screen = screen_l.resize((screen_l.width * SCALE, screen_l.height * SCALE), Image.NEAREST).convert("RGB")
    w = screen.width + 2 * PAD
    h = screen.height + PAD + CHIN
    dev = Image.new("RGB", (w, h), BODY)
    d = ImageDraw.Draw(dev)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=24, fill=BODY, outline=EDGE, width=2)
    sx, sy = PAD, PAD
    dev.paste(screen, (sx, sy))
    d.rectangle([sx - 2, sy - 2, sx + screen.width + 1, sy + screen.height + 1], outline=SCREEN_EDGE, width=2)
    cy = sy + screen.height + CHIN // 2
    d.ellipse([w // 2 - 9, cy - 9, w // 2 + 9, cy + 9], outline=ACCENT, width=3)
    d.text((sx + 2, cy - 8), caption, fill=LABEL, font=LABEL_FONT)
    return dev


def main():
    frames = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    stills = {
        "home.pgm": ("home.png", "Xteink X4  .  Home"),
        "browse.pgm": ("browse.png", "Xteink X4  .  Browse"),
        "search.pgm": ("search.png", "Xteink X4  .  Search"),
        "reader.pgm": ("reader.png", "Xteink X4  .  Reader"),
    }
    for src, (dst, caption) in stills.items():
        p = frames / src
        if p.exists():
            bezel(load_pgm(p), caption).save(out / dst)
            print("wrote", out / dst)

    # Search typing animation.
    search_frames = sorted(frames.glob("search_*.pgm"), key=lambda p: int(p.stem.split("_")[1]))
    if search_frames:
        imgs = [bezel(load_pgm(p), "Xteink X4  .  Search") for p in search_frames]
        imgs = imgs + [imgs[-1]]  # hold the final frame
        imgs[0].save(
            out / "demo-search.gif",
            save_all=True,
            append_images=imgs[1:],
            duration=[650] * (len(imgs) - 1) + [1400],
            loop=0,
            optimize=True,
        )
        print("wrote", out / "demo-search.gif")

    # Reader paging animation.
    reader_frames = sorted(frames.glob("reader_p*.pgm"), key=lambda p: int(p.stem.split("p")[-1]))
    if reader_frames:
        imgs = [bezel(load_pgm(p), "Xteink X4  .  Reader") for p in reader_frames]
        seq = imgs + list(reversed(imgs[1:-1])) if len(imgs) > 2 else imgs + imgs[::-1]
        seq[0].save(
            out / "demo-reader.gif",
            save_all=True,
            append_images=seq[1:],
            duration=1600,
            loop=0,
            optimize=True,
        )
        print("wrote", out / "demo-reader.gif")


if __name__ == "__main__":
    main()
