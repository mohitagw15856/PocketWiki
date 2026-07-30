// Render stream token decoder (see docs/ARCHIVE_FORMAT.md section 5.1).
//
// The decoder is fed one decompressed storage block at a time. Because the
// companion never splits a token across a storage block, each fed buffer holds
// whole tokens, so text pointers handed to the sink stay valid for the duration
// of the callback with no copying. Inline style persists across storage blocks
// and is reset by every block level token.
//
// Arduino free: compiles in the host test build.
#pragma once

#include <cstddef>
#include <cstdint>

namespace pocketwiki {

namespace tok {
constexpr uint8_t END = 0x00;
constexpr uint8_t TEXT = 0x01;
constexpr uint8_t STYLE = 0x02;
constexpr uint8_t LINK = 0x04;
constexpr uint8_t PARAGRAPH = 0x10;
constexpr uint8_t HEADING = 0x11;
constexpr uint8_t LIST_ITEM = 0x12;
constexpr uint8_t RULE = 0x15;
}  // namespace tok

constexpr uint8_t kStyleBold = 0x01;
constexpr uint8_t kStyleItalic = 0x02;

enum class BlockKind : uint8_t { Paragraph, Heading, ListItem, Rule };

struct BlockInfo {
  BlockKind kind = BlockKind::Paragraph;
  uint8_t level = 0;    // heading level 1..3
  uint8_t depth = 0;    // list nesting depth 0..3
  bool ordered = false;  // ordered list item
};

// Sink for decoded tokens. A renderer or a text extractor implements this.
struct RenderSink {
  virtual ~RenderSink() = default;
  virtual void onBlockStart(const BlockInfo&) {}
  virtual void onText(const char* /*utf8*/, size_t /*len*/, uint8_t /*style*/) {}
  virtual void onLink(uint32_t /*targetId*/, const char* /*label*/, size_t /*len*/) {}
  virtual void onEnd() {}
};

class RenderDecoder {
 public:
  explicit RenderDecoder(RenderSink& sink) : sink_(sink) {}

  bool ended() const { return ended_; }

  // Decode the whole buffer. Returns false on a malformed token, in which case
  // the caller should stop decoding this article.
  bool feed(const uint8_t* data, size_t n) {
    size_t i = 0;
    while (i < n && !ended_) {
      const uint8_t op = data[i++];
      switch (op) {
        case tok::END:
          ended_ = true;
          sink_.onEnd();
          return true;
        case tok::PARAGRAPH:
          style_ = 0;
          sink_.onBlockStart(BlockInfo{BlockKind::Paragraph, 0, 0, false});
          break;
        case tok::HEADING: {
          if (i >= n) return false;
          const uint8_t level = data[i++];
          style_ = 0;
          sink_.onBlockStart(BlockInfo{BlockKind::Heading, level, 0, false});
          break;
        }
        case tok::LIST_ITEM: {
          if (i + 1 >= n) return false;
          const uint8_t depth = data[i++];
          const bool ordered = data[i++] != 0;
          style_ = 0;
          sink_.onBlockStart(BlockInfo{BlockKind::ListItem, 0, depth, ordered});
          break;
        }
        case tok::RULE:
          style_ = 0;
          sink_.onBlockStart(BlockInfo{BlockKind::Rule, 0, 0, false});
          break;
        case tok::STYLE:
          if (i >= n) return false;
          style_ = data[i++];
          break;
        case tok::TEXT: {
          if (i + 2 > n) return false;
          const uint16_t len = readU16(data + i);
          i += 2;
          if (i + len > n) return false;
          sink_.onText(reinterpret_cast<const char*>(data + i), len, style_);
          i += len;
          break;
        }
        case tok::LINK: {
          if (i + 6 > n) return false;
          const uint32_t target = readU32(data + i);
          i += 4;
          const uint16_t len = readU16(data + i);
          i += 2;
          if (i + len > n) return false;
          sink_.onLink(target, reinterpret_cast<const char*>(data + i), len);
          i += len;
          style_ = 0;
          break;
        }
        default:
          return false;  // unknown token in a well formed archive is an error
      }
    }
    return true;
  }

 private:
  static uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(p[1]) << 8;
  }
  static uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
  }

  RenderSink& sink_;
  uint8_t style_ = 0;
  bool ended_ = false;
};

}  // namespace pocketwiki
