// Exercises the render stream decoder with hand built token buffers, including
// inline style persisting across a storage block boundary.
#include <vector>

#include "check.h"
#include "pocketwiki/RenderStream.h"

using namespace pocketwiki;

namespace {
void putU16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(x & 0xFF);
  v.push_back((x >> 8) & 0xFF);
}
void putU32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x & 0xFF);
  v.push_back((x >> 8) & 0xFF);
  v.push_back((x >> 16) & 0xFF);
  v.push_back((x >> 24) & 0xFF);
}
void putText(std::vector<uint8_t>& v, const std::string& s) {
  v.push_back(tok::TEXT);
  putU16(v, static_cast<uint16_t>(s.size()));
  v.insert(v.end(), s.begin(), s.end());
}

struct RecordingSink : RenderSink {
  struct Item {
    std::string kind;
    std::string text;
    uint8_t style = 0;
    uint32_t target = 0;
    BlockInfo block;
  };
  std::vector<Item> items;
  void onBlockStart(const BlockInfo& b) override {
    Item it;
    it.kind = "block";
    it.block = b;
    items.push_back(it);
  }
  void onText(const char* t, size_t n, uint8_t style) override {
    items.push_back(Item{"text", std::string(t, n), style, 0, {}});
  }
  void onLink(uint32_t target, const char* t, size_t n) override {
    items.push_back(Item{"link", std::string(t, n), 0, target, {}});
  }
  void onEnd() override { items.push_back(Item{"end", "", 0, 0, {}}); }
};
}  // namespace

static void decodes_blocks_and_inline() {
  std::vector<uint8_t> v;
  v.push_back(tok::HEADING);
  v.push_back(2);
  putText(v, "Title");
  v.push_back(tok::PARAGRAPH);
  v.push_back(tok::STYLE);
  v.push_back(kStyleBold);
  putText(v, "bold");
  v.push_back(tok::LINK);
  putU32(v, 7);
  putU16(v, 4);
  const char* label = "link";
  v.insert(v.end(), label, label + 4);
  v.push_back(tok::END);

  RecordingSink sink;
  RenderDecoder dec(sink);
  CHECK(dec.feed(v.data(), v.size()));
  CHECK(dec.ended());
  CHECK_EQ(sink.items.size(), static_cast<size_t>(6));
  CHECK_STR_EQ(sink.items[0].kind, "block");
  CHECK_EQ(sink.items[0].block.kind, BlockKind::Heading);
  CHECK_EQ(sink.items[0].block.level, static_cast<uint8_t>(2));
  CHECK_STR_EQ(sink.items[1].text, "Title");
  CHECK_EQ(sink.items[3].style, kStyleBold);
  CHECK_STR_EQ(sink.items[4].kind, "link");
  CHECK_EQ(sink.items[4].target, static_cast<uint32_t>(7));
  CHECK_STR_EQ(sink.items.back().kind, "end");
}

static void style_persists_across_blocks() {
  // Block 1 sets bold style then text; block 2 continues text (same paragraph).
  std::vector<uint8_t> b1;
  b1.push_back(tok::PARAGRAPH);
  b1.push_back(tok::STYLE);
  b1.push_back(kStyleItalic);
  putText(b1, "one");

  std::vector<uint8_t> b2;
  putText(b2, "two");
  b2.push_back(tok::END);

  RecordingSink sink;
  RenderDecoder dec(sink);
  CHECK(dec.feed(b1.data(), b1.size()));
  CHECK(!dec.ended());
  CHECK(dec.feed(b2.data(), b2.size()));
  CHECK(dec.ended());
  // Both text runs carry italic even though the second is in a later block.
  int textRuns = 0;
  for (auto& it : sink.items) {
    if (it.kind == "text") {
      CHECK_EQ(it.style, kStyleItalic);
      ++textRuns;
    }
  }
  CHECK_EQ(textRuns, 2);
}

static void rejects_truncated_token() {
  std::vector<uint8_t> v;
  v.push_back(tok::TEXT);
  putU16(v, 10);  // claims 10 bytes but provides none
  RecordingSink sink;
  RenderDecoder dec(sink);
  CHECK(!dec.feed(v.data(), v.size()));
}

int main() {
  RUN(decodes_blocks_and_inline);
  RUN(style_persists_across_blocks);
  RUN(rejects_truncated_token);
  REPORT();
}
