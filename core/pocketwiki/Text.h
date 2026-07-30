// Text drawing, measuring and word wrapping over a Canvas using the 5x7 font.
//
// Kept portable (no Arduino, no inkkit) so the wrapping and measuring logic is
// unit tested on the host; the device draws with exactly the same code. Styling
// is emulated: bold thickens horizontally by one pixel, italic shears the upper
// rows to the right. Integer `scale` enlarges the font for headings.
#pragma once

#include <string>
#include <vector>

#include "pocketwiki/Canvas.h"
#include "pocketwiki/Font5x7.h"
#include "pocketwiki/RenderStream.h"  // kStyleBold, kStyleItalic

namespace pocketwiki {

// Horizontal advance of one character, including the inter-character gap.
inline int charAdvance(int scale, uint8_t style) {
  int adv = kFontWidth * scale + scale;  // glyph width plus a one-unit gap
  if (style & kStyleBold) adv += 1;
  return adv;
}

inline int measureText(const std::string& s, int scale, uint8_t style) {
  int w = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    // Skip UTF-8 continuation bytes so multi-byte code points advance once.
    if ((static_cast<uint8_t>(s[i]) & 0xC0) == 0x80) continue;
    w += charAdvance(scale, style);
  }
  return w;
}

inline int lineHeight(int scale) { return kFontHeight * scale + 2 * scale; }

// Draw one character; returns its advance. Characters outside the printable
// range fall back to '?' via font5x7Columns.
inline int drawChar(Canvas& c, int x, int y, char ch, int scale, uint8_t style) {
  const uint8_t* cols = font5x7Columns(ch);
  const bool bold = (style & kStyleBold) != 0;
  const bool italic = (style & kStyleItalic) != 0;
  for (int col = 0; col < kFontWidth; ++col) {
    const uint8_t bits = cols[col];
    for (int row = 0; row < kFontHeight; ++row) {
      if (!(bits & (1u << row))) continue;
      // Italic shear: rows nearer the top move right by up to ~1.5 cells.
      const int shear = italic ? ((kFontHeight - 1 - row) * scale) / 3 : 0;
      const int px = x + col * scale + shear;
      const int py = y + row * scale;
      c.fillRect(px, py, scale, scale, /*black=*/true);
      if (bold) c.fillRect(px + 1, py, scale, scale, /*black=*/true);
    }
  }
  return charAdvance(scale, style);
}

// Draw a string left to right; returns the x advance used.
inline int drawText(Canvas& c, int x, int y, const std::string& s, int scale, uint8_t style) {
  int cx = x;
  for (size_t i = 0; i < s.size(); ++i) {
    if ((static_cast<uint8_t>(s[i]) & 0xC0) == 0x80) continue;  // skip UTF-8 tails
    char ch = s[i];
    if (static_cast<uint8_t>(ch) > 0x7E || static_cast<uint8_t>(ch) < 0x20) ch = '?';
    cx += drawChar(c, cx, y, ch, scale, style);
  }
  return cx - x;
}

// Break `text` into lines no wider than maxWidth pixels. Words longer than the
// line are hard split. Existing spaces are the only break points.
inline std::vector<std::string> wrapText(const std::string& text, int maxWidth, int scale, uint8_t style) {
  std::vector<std::string> lines;
  const int space = charAdvance(scale, style);
  std::string line;
  int lineWidth = 0;

  size_t i = 0;
  const size_t n = text.size();
  while (i < n) {
    // Extract the next word (run of non-space).
    size_t j = i;
    while (j < n && text[j] != ' ') ++j;
    std::string word = text.substr(i, j - i);
    i = (j < n) ? j + 1 : j;  // skip the space
    if (word.empty()) continue;

    int wordWidth = measureText(word, scale, style);

    // Hard split a word that cannot fit on a line by itself.
    while (wordWidth > maxWidth && maxWidth > 0) {
      // Find the largest prefix that fits.
      std::string prefix;
      int pw = 0;
      size_t k = 0;
      while (k < word.size()) {
        if ((static_cast<uint8_t>(word[k]) & 0xC0) == 0x80) {
          prefix.push_back(word[k++]);
          continue;
        }
        int step = charAdvance(scale, style);
        if (pw + step > maxWidth && !prefix.empty()) break;
        prefix.push_back(word[k++]);
        pw += step;
      }
      if (!line.empty()) {
        lines.push_back(line);
        line.clear();
        lineWidth = 0;
      }
      lines.push_back(prefix);
      word = word.substr(prefix.size());
      wordWidth = measureText(word, scale, style);
    }

    const int add = (line.empty() ? 0 : space) + wordWidth;
    if (!line.empty() && lineWidth + add > maxWidth) {
      lines.push_back(line);
      line = word;
      lineWidth = wordWidth;
    } else {
      if (!line.empty()) {
        line.push_back(' ');
        lineWidth += space;
      }
      line += word;
      lineWidth += wordWidth;
    }
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}  // namespace pocketwiki
