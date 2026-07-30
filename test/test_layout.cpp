// Exercises article pagination and link capture end to end: build a token
// stream, decode it into an ArticleLayout, and check lines, pages and links.
#include <vector>

#include "check.h"
#include "pocketwiki/ArticleLayout.h"
#include "pocketwiki/RenderStream.h"

using namespace pocketwiki;

namespace {
void putU16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(x & 0xFF);
  v.push_back((x >> 8) & 0xFF);
}
void putU32(std::vector<uint8_t>& v, uint32_t x) {
  for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 0xFF);
}
void text(std::vector<uint8_t>& v, const std::string& s) {
  v.push_back(tok::TEXT);
  putU16(v, static_cast<uint16_t>(s.size()));
  v.insert(v.end(), s.begin(), s.end());
}
void link(std::vector<uint8_t>& v, uint32_t target, const std::string& s) {
  v.push_back(tok::LINK);
  putU32(v, target);
  putU16(v, static_cast<uint16_t>(s.size()));
  v.insert(v.end(), s.begin(), s.end());
}
}  // namespace

static void lays_out_and_captures_links() {
  std::vector<uint8_t> v;
  v.push_back(tok::HEADING);
  v.push_back(2);
  text(v, "A Title");
  v.push_back(tok::PARAGRAPH);
  text(v, "Some words before a ");
  link(v, 42, "target");
  text(v, " and words after it to force wrapping onto several lines.");
  v.push_back(tok::LIST_ITEM);
  v.push_back(0);
  v.push_back(0);
  text(v, "A bullet point");
  v.push_back(tok::END);

  ArticleLayout layout(120, 200);
  RenderDecoder dec(layout);
  CHECK(dec.feed(v.data(), v.size()));
  layout.finish();

  CHECK(layout.lines().size() > 2);
  CHECK(layout.pageCount() >= 1);

  bool found = false;
  for (auto& l : layout.links()) {
    if (l.target == 42) found = true;
  }
  CHECK(found);
}

static void paginates_long_article() {
  std::vector<uint8_t> v;
  for (int p = 0; p < 40; ++p) {
    v.push_back(tok::PARAGRAPH);
    text(v, "This is paragraph number filler with several words to occupy vertical space.");
  }
  v.push_back(tok::END);

  ArticleLayout layout(120, 80);  // small page height forces multiple pages
  RenderDecoder dec(layout);
  CHECK(dec.feed(v.data(), v.size()));
  layout.finish();
  CHECK(layout.pageCount() > 1);

  // Draw every page into a scratch framebuffer without crashing.
  const int w = 128, h = 96, stride = w / 8;
  std::vector<uint8_t> fb(stride * h, 0xFF);
  Canvas canvas(fb.data(), w, h, stride);
  for (size_t page = 0; page < layout.pageCount(); ++page) {
    canvas.clear(true);
    layout.drawPage(canvas, 0, 0, page);
  }

  // pageOfLine is monotonic and within range.
  CHECK_EQ(layout.pageOfLine(0), static_cast<size_t>(0));
  CHECK(layout.pageOfLine(layout.lines().size() - 1) == layout.pageCount() - 1);
}

int main() {
  RUN(lays_out_and_captures_links);
  RUN(paginates_long_article);
  REPORT();
}
