// A scrolling, selectable list used by the home menu, the browse and library
// screens, the archive picker and the on-page link chooser. Portable so its
// scrolling maths is unit tested on the host.
#pragma once

#include <string>
#include <vector>

#include "PocketWikiConfig.h"
#include "pocketwiki/Canvas.h"
#include "pocketwiki/Text.h"
#include "ui/Button.h"

namespace pocketwiki {

class ListMenu {
 public:
  enum class Result { None, Activated, Back };

  void setItems(std::vector<std::string> items) {
    items_ = std::move(items);
    selected_ = 0;
    top_ = 0;
  }
  void setViewportRows(int rows) { viewportRows_ = rows > 0 ? rows : 1; }

  size_t size() const { return items_.size(); }
  int selectedIndex() const { return selected_; }
  void setSelectedIndex(int i) {
    if (i >= 0 && i < static_cast<int>(items_.size())) {
      selected_ = i;
      ensureVisible();
    }
  }

  Result handle(Button b) {
    if (items_.empty()) {
      if (b == Button::Back) return Result::Back;
      return Result::None;
    }
    switch (b) {
      case Button::Up:
        if (selected_ > 0) --selected_;
        ensureVisible();
        return Result::None;
      case Button::Down:
        if (selected_ + 1 < static_cast<int>(items_.size())) ++selected_;
        ensureVisible();
        return Result::None;
      case Button::Left:  // page up
        selected_ = selected_ - viewportRows_ < 0 ? 0 : selected_ - viewportRows_;
        ensureVisible();
        return Result::None;
      case Button::Right:  // page down
        selected_ = selected_ + viewportRows_ >= static_cast<int>(items_.size())
                        ? static_cast<int>(items_.size()) - 1
                        : selected_ + viewportRows_;
        ensureVisible();
        return Result::None;
      case Button::Select:
        return Result::Activated;
      case Button::Back:
        return Result::Back;
      default:
        return Result::None;
    }
  }

  void render(Canvas& c, int x, int y, int w, int h) const {
    const int rowH = config::kListRowHeight;
    const int rows = h / rowH;
    for (int i = 0; i < rows; ++i) {
      const int idx = top_ + i;
      if (idx >= static_cast<int>(items_.size())) break;
      const int ry = y + i * rowH;
      const bool sel = (idx == selected_);
      if (sel) c.fillRect(x, ry, w, rowH, true);  // black bar behind the row
      // Selected rows draw in paper colour (white) so the label stays legible on
      // the filled bar; unselected rows draw normal ink.
      const int textY = ry + (rowH - kFontHeight) / 2;
      const bool ink = !sel;
      if (sel) drawText(c, x + 3, textY, std::string(">"), 1, 0, ink);
      drawText(c, x + 14, textY, items_[idx], 1, sel ? kStyleBold : 0, ink);
    }
    // Scrollbar hint.
    if (static_cast<int>(items_.size()) > rows) {
      drawText(c, x + w - 40, y + h - kFontHeight,
               std::to_string(selected_ + 1) + "/" + std::to_string(items_.size()), 1, 0);
    }
  }

 private:
  void ensureVisible() {
    const int rows = viewportRows_;
    if (selected_ < top_) top_ = selected_;
    if (selected_ >= top_ + rows) top_ = selected_ - rows + 1;
    if (top_ < 0) top_ = 0;
  }

  std::vector<std::string> items_;
  int selected_ = 0;
  int top_ = 0;
  int viewportRows_ = 8;
};

}  // namespace pocketwiki
