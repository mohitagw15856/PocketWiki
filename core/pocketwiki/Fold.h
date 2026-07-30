// Fold key normalisation, mirroring companion/pocketwiki/fold.py byte for byte.
//
// The device only ever compares fold keys produced by the companion, so it does
// not strictly need to fold at runtime. It does need to fold the user's *query*
// so a search matches. Keeping the same algorithm on both sides is what makes
// search case and accent insensitive without any Unicode tables in flash.
//
// This header is Arduino free so it compiles in the host test build.
#pragma once

#include <cstdint>
#include <string>

namespace pocketwiki {

// Map a Latin-1 / Latin Extended-A accented code point to its ASCII base, or 0
// if there is no mapping. The set mirrors _ACCENT_MAP in fold.py.
inline char foldAccent(uint32_t cp) {
  switch (cp) {
    case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
    case 0x0101: case 0x0103: case 0x0105:
      return 'a';
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
    case 0x0113: case 0x0115: case 0x0117: case 0x0119: case 0x011B:
      return 'e';
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
    case 0x0129: case 0x012B: case 0x012D: case 0x012F: case 0x0131:
      return 'i';
    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
    case 0x014D: case 0x014F: case 0x0151:
      return 'o';
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
    case 0x0169: case 0x016B: case 0x016D: case 0x016F: case 0x0171: case 0x0173:
      return 'u';
    case 0x00FD: case 0x00FF:
      return 'y';
    case 0x00F1: case 0x0144: case 0x0146: case 0x0148:
      return 'n';
    case 0x00E7: case 0x0107: case 0x0109: case 0x010B: case 0x010D:
      return 'c';
    case 0x015B: case 0x015D: case 0x015F: case 0x0161:
      return 's';
    case 0x017A: case 0x017C: case 0x017E:
      return 'z';
    case 0x011D: case 0x011F: case 0x0121: case 0x0123:
      return 'g';
    case 0x013A: case 0x013C: case 0x013E: case 0x0142:
      return 'l';
    case 0x0155: case 0x0157: case 0x0159:
      return 'r';
    case 0x0163: case 0x0165: case 0x0167:
      return 't';
    case 0x010F: case 0x0111:
      return 'd';
    // Upper case variants map to the same lower case base (folding also lower cases).
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
    case 0x0100: case 0x0102: case 0x0104:
      return 'a';
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
    case 0x0112: case 0x0114: case 0x0116: case 0x0118: case 0x011A:
      return 'e';
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
    case 0x0128: case 0x012A: case 0x012C: case 0x012E: case 0x0130:
      return 'i';
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8:
    case 0x014C: case 0x014E: case 0x0150:
      return 'o';
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
    case 0x0168: case 0x016A: case 0x016C: case 0x016E: case 0x0170: case 0x0172:
      return 'u';
    case 0x00DD: case 0x0178:
      return 'y';
    case 0x00D1: case 0x0143: case 0x0145: case 0x0147:
      return 'n';
    case 0x00C7: case 0x0106: case 0x0108: case 0x010A: case 0x010C:
      return 'c';
    case 0x015A: case 0x015C: case 0x015E: case 0x0160:
      return 's';
    case 0x0179: case 0x017B: case 0x017D:
      return 'z';
    default:
      return 0;
  }
}

// Decode one UTF-8 code point starting at s[i]; advance i past it. Invalid bytes
// are passed through as a single Latin-1 style code point so we never loop.
inline uint32_t utf8Decode(const std::string& s, size_t& i) {
  const auto n = s.size();
  uint8_t c = static_cast<uint8_t>(s[i]);
  if (c < 0x80) {
    i += 1;
    return c;
  }
  int extra;
  uint32_t cp;
  if ((c & 0xE0) == 0xC0) {
    extra = 1;
    cp = c & 0x1F;
  } else if ((c & 0xF0) == 0xE0) {
    extra = 2;
    cp = c & 0x0F;
  } else if ((c & 0xF8) == 0xF0) {
    extra = 3;
    cp = c & 0x07;
  } else {
    i += 1;
    return c;
  }
  if (i + extra >= n + 0 && i + 1 + extra > n) {
    i += 1;
    return c;
  }
  for (int k = 1; k <= extra; ++k) {
    uint8_t cc = static_cast<uint8_t>(s[i + k]);
    if ((cc & 0xC0) != 0x80) {
      i += 1;
      return c;
    }
    cp = (cp << 6) | (cc & 0x3F);
  }
  i += (extra + 1);
  return cp;
}

// Append a code point as UTF-8.
inline void utf8Append(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

inline bool isSpace(uint32_t cp) {
  return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == '\v';
}

// Return the UTF-8 fold key for a title/query. See docs/ARCHIVE_FORMAT.md s.6.
inline std::string foldKey(const std::string& title) {
  std::string out;
  out.reserve(title.size());
  bool prevSpace = false;
  size_t i = 0;
  while (i < title.size()) {
    uint32_t cp = utf8Decode(title, i);
    char base = foldAccent(cp);
    if (base) cp = static_cast<uint32_t>(static_cast<uint8_t>(base));
    if (isSpace(cp)) {
      if (!prevSpace) {
        out.push_back(' ');
        prevSpace = true;
      }
      continue;
    }
    prevSpace = false;
    if (cp >= 'A' && cp <= 'Z') cp += 0x20;
    utf8Append(out, cp);
  }
  // Trim trailing space (leading spaces are already suppressed by prevSpace).
  while (!out.empty() && out.back() == ' ') out.pop_back();
  size_t start = 0;
  while (start < out.size() && out[start] == ' ') ++start;
  return out.substr(start);
}

}  // namespace pocketwiki
