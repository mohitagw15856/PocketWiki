// A 1-bit drawing surface over a raw framebuffer.
//
// This wraps the buffer inkkit's Display exposes (width x height, `stride` bytes
// per row). Following the SDK convention a *set* bit is white (paper) and a
// *cleared* bit is black (ink), so drawing ink clears bits. Bits are packed most
// significant first: pixel x lives in byte x/8 at bit 7-(x&7).
//
// Canvas holds no memory of its own; it draws into a buffer the caller owns, so
// there is no heap cost. Arduino free for host testing.
//
// TODO(hardware-test): the bit order (MSB first) and the set-bit-is-white
// polarity are taken from the inkkit Display notes and must be confirmed against
// the panel on device. Only this file changes if the panel differs.
#pragma once

#include <cstdint>

namespace pocketwiki {

class Canvas {
 public:
  Canvas(uint8_t* buffer, int width, int height, int stride)
      : buf_(buffer), width_(width), height_(height), stride_(stride) {}

  int width() const { return width_; }
  int height() const { return height_; }

  // Fill the whole surface. white = paper, otherwise ink.
  void clear(bool white = true) {
    const uint8_t v = white ? 0xFF : 0x00;
    for (int i = 0; i < stride_ * height_; ++i) buf_[i] = v;
  }

  void setPixel(int x, int y, bool black) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    uint8_t& byte = buf_[y * stride_ + (x >> 3)];
    const uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
    if (black) {
      byte &= static_cast<uint8_t>(~mask);
    } else {
      byte |= mask;
    }
  }

  void fillRect(int x, int y, int w, int h, bool black) {
    for (int yy = y; yy < y + h; ++yy) {
      for (int xx = x; xx < x + w; ++xx) setPixel(xx, yy, black);
    }
  }

  void hLine(int x, int y, int w, bool black) { fillRect(x, y, w, 1, black); }
  void vLine(int x, int y, int h, bool black) { fillRect(x, y, 1, h, black); }

  // Draw a 1px border rectangle.
  void drawRect(int x, int y, int w, int h, bool black) {
    hLine(x, y, w, black);
    hLine(x, y + h - 1, w, black);
    vLine(x, y, h, black);
    vLine(x + w - 1, y, h, black);
  }

 private:
  uint8_t* buf_;
  int width_;
  int height_;
  int stride_;
};

}  // namespace pocketwiki
