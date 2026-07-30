#include "check.h"
#include "pocketwiki/Library.h"

using namespace pocketwiki;

static void history_dedups_and_orders() {
  HistoryStore h(3);
  h.add(1, "One");
  h.add(2, "Two");
  h.add(3, "Three");
  h.add(1, "One again");  // moves 1 to front, updates title
  CHECK_EQ(h.size(), static_cast<size_t>(3));
  CHECK_EQ(h.entries()[0].articleId, static_cast<uint32_t>(1));
  CHECK_STR_EQ(h.entries()[0].title, "One again");
  h.add(4, "Four");  // evicts the oldest (2)
  CHECK_EQ(h.size(), static_cast<size_t>(3));
  for (auto& e : h.entries()) CHECK(e.articleId != 2);
}

static void history_roundtrips() {
  HistoryStore h;
  h.add(10, "Ten\twith tab");  // tab is sanitised
  h.add(20, "Twenty");
  std::string s = h.serialise();
  HistoryStore h2;
  h2.parse(s);
  CHECK_EQ(h2.size(), static_cast<size_t>(2));
  CHECK_EQ(h2.entries()[0].articleId, static_cast<uint32_t>(20));
  CHECK(h2.entries()[1].title.find('\t') == std::string::npos);
}

static void bookmarks_toggle_and_roundtrip() {
  BookmarkStore b;
  CHECK(!b.contains(5));
  CHECK(b.toggle(5, 2, "Five"));  // now added
  CHECK(b.contains(5));
  CHECK(!b.toggle(5, 2, "Five"));  // now removed
  CHECK(!b.contains(5));
  b.set(6, 3, "Six");
  b.set(6, 9, "Six moved");  // updates position
  CHECK_EQ(b.all()[0].position, static_cast<uint32_t>(9));
  std::string s = b.serialise();
  BookmarkStore b2;
  b2.parse(s);
  CHECK_EQ(b2.size(), static_cast<size_t>(1));
  CHECK_EQ(b2.all()[0].articleId, static_cast<uint32_t>(6));
  CHECK_STR_EQ(b2.all()[0].title, "Six moved");
}

int main() {
  RUN(history_dedups_and_orders);
  RUN(history_roundtrips);
  RUN(bookmarks_toggle_and_roundtrip);
  REPORT();
}
