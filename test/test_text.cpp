// Tests the portable text measuring and word wrapping. Glyph pixels are not
// asserted here (they are a data asset verified on device); the geometry is.
#include <string>
#include <vector>

#include "check.h"
#include "pocketwiki/Text.h"

using namespace pocketwiki;

static void measure_counts_visible_chars() {
  const int adv = charAdvance(1, 0);
  CHECK_EQ(measureText("ab", 1, 0), 2 * adv);
  // A UTF-8 two byte code point advances once, not twice.
  CHECK_EQ(measureText("\xC3\xA9", 1, 0), adv);  // é
  // Bold is wider than regular.
  CHECK(measureText("abc", 1, kStyleBold) > measureText("abc", 1, 0));
}

static void wrap_breaks_on_spaces() {
  const int wordW = measureText("aaaa", 1, 0);
  // Budget fits one word but not two.
  const int maxWidth = wordW + 2;
  auto lines = wrapText("aaaa bbbb cccc", maxWidth, 1, 0);
  CHECK_EQ(lines.size(), static_cast<size_t>(3));
  CHECK_STR_EQ(lines[0], "aaaa");
  CHECK_STR_EQ(lines[2], "cccc");
}

static void wrap_packs_multiple_words() {
  const int maxWidth = measureText("aa aa aa", 1, 0) + 10;
  auto lines = wrapText("aa aa aa", maxWidth, 1, 0);
  CHECK_EQ(lines.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(lines[0], "aa aa aa");
}

static void wrap_hard_splits_long_word() {
  const int adv = charAdvance(1, 0);
  auto lines = wrapText(std::string(10, 'x'), adv * 5, 1, 0);
  CHECK(lines.size() >= 2);
  for (auto& l : lines) CHECK(measureText(l, 1, 0) <= adv * 5);
  std::string joined;
  for (auto& l : lines) joined += l;
  CHECK_STR_EQ(joined, std::string(10, 'x'));
}

static void no_line_exceeds_budget() {
  std::string para =
      "The quick brown fox jumps over the lazy dog while reading an offline "
      "encyclopaedia on a small e-ink display.";
  const int maxWidth = 120;
  for (auto& l : wrapText(para, maxWidth, 1, 0)) {
    CHECK(measureText(l, 1, 0) <= maxWidth);
  }
}

int main() {
  RUN(measure_counts_visible_chars);
  RUN(wrap_breaks_on_spaces);
  RUN(wrap_packs_multiple_words);
  RUN(wrap_hard_splits_long_word);
  RUN(no_line_exceeds_budget);
  REPORT();
}
