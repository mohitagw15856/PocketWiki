// Tests the pure UI widget logic: keyboard editing/navigation and list menu
// scrolling. Rendering pixels are not asserted; behaviour is.
#include <string>
#include <vector>

#include "check.h"
#include "ui/Keyboard.h"
#include "ui/ListMenu.h"

using namespace pocketwiki;

static void keyboard_types_and_deletes() {
  Keyboard kb;
  CHECK(kb.query().empty());
  // (0,0) is 'a'.
  CHECK(kb.handle(Button::Select) == Keyboard::Result::Changed);
  CHECK_STR_EQ(kb.query(), "a");
  // Move right to 'b' and type it.
  kb.handle(Button::Right);
  CHECK(kb.handle(Button::Select) == Keyboard::Result::Changed);
  CHECK_STR_EQ(kb.query(), "ab");
  // Hardware back deletes one character.
  CHECK(kb.handle(Button::Back) == Keyboard::Result::Changed);
  CHECK_STR_EQ(kb.query(), "a");
  CHECK(kb.handle(Button::Back) == Keyboard::Result::Changed);
  CHECK_STR_EQ(kb.query(), "");
  // Back on an empty query cancels.
  CHECK(kb.handle(Button::Back) == Keyboard::Result::Cancel);
}

static void keyboard_accepts_on_go() {
  Keyboard kb;
  kb.setQuery("newton");
  // Navigate to the action row (last) and to the GO key (index 2).
  while (kb.selectedRow() != 4) kb.handle(Button::Down);
  while (kb.selectedCol() != 0) kb.handle(Button::Left);
  kb.handle(Button::Right);  // SP -> DEL
  kb.handle(Button::Right);  // DEL -> GO
  CHECK(kb.handle(Button::Select) == Keyboard::Result::Accept);
  CHECK_STR_EQ(kb.query(), "newton");
}

static void listmenu_scrolls_and_activates() {
  ListMenu m;
  std::vector<std::string> items;
  for (int i = 0; i < 20; ++i) items.push_back("item " + std::to_string(i));
  m.setItems(items);
  m.setViewportRows(5);

  for (int i = 0; i < 6; ++i) m.handle(Button::Down);
  CHECK_EQ(m.selectedIndex(), 6);

  m.handle(Button::Right);  // page down by a viewport
  CHECK_EQ(m.selectedIndex(), 11);

  m.handle(Button::Left);  // page up
  CHECK_EQ(m.selectedIndex(), 6);

  CHECK(m.handle(Button::Select) == ListMenu::Result::Activated);
  CHECK(m.handle(Button::Back) == ListMenu::Result::Back);
}

static void listmenu_clamps_at_ends() {
  ListMenu m;
  m.setItems({"only"});
  m.setViewportRows(4);
  m.handle(Button::Down);  // cannot move past the single item
  CHECK_EQ(m.selectedIndex(), 0);
  m.handle(Button::Up);
  CHECK_EQ(m.selectedIndex(), 0);
}

int main() {
  RUN(keyboard_types_and_deletes);
  RUN(keyboard_accepts_on_go);
  RUN(listmenu_scrolls_and_activates);
  RUN(listmenu_clamps_at_ends);
  REPORT();
}
