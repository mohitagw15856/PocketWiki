// A button-driven on-screen keyboard, following the pattern CrossPoint uses: a
// grid of keys navigated with the direction buttons and committed with Select.
// The query updates incrementally so the caller can re-run the title search on
// every change.
//
// Portable (Canvas + Text + Button) so the navigation and editing logic is unit
// tested on the host; the device draws the same grid.
#pragma once

#include <string>
#include <vector>

#include "pocketwiki/Canvas.h"
#include "pocketwiki/Text.h"
#include "ui/Button.h"

namespace pocketwiki {

class Keyboard {
 public:
  enum class Result { None, Changed, Accept, Cancel };

  Keyboard() { buildLayout(); }

  const std::string& query() const { return query_; }
  void setQuery(const std::string& q) { query_ = q; }
  void clear() { query_.clear(); }

  int selectedRow() const { return row_; }
  int selectedCol() const { return col_; }

  Result handle(Button b) {
    switch (b) {
      case Button::Up:
        row_ = (row_ + rowCount() - 1) % rowCount();
        clampCol();
        return Result::None;
      case Button::Down:
        row_ = (row_ + 1) % rowCount();
        clampCol();
        return Result::None;
      case Button::Left:
        col_ = (col_ + colCount(row_) - 1) % colCount(row_);
        return Result::None;
      case Button::Right:
        col_ = (col_ + 1) % colCount(row_);
        return Result::None;
      case Button::Select:
        return activate(rows_[row_][col_]);
      case Button::Back:
        // Hardware back deletes a character, or cancels on an empty query.
        if (query_.empty()) return Result::Cancel;
        popChar();
        return Result::Changed;
      default:
        return Result::None;
    }
  }

  void render(Canvas& c, int x, int y, int w, int h) const {
    c.drawRect(x, y, w, h, true);
    // Query line.
    const int pad = 3;
    std::string shown = query_.empty() ? std::string("(type to search)") : query_;
    drawText(c, x + pad, y + pad, shown, 1, 0);
    c.hLine(x, y + lineHeight(1) + pad, w, true);

    // Key grid. The action row keys carry short multi-letter labels, so the
    // cells are wide enough to keep them apart.
    const int gridTop = y + lineHeight(1) + pad + 4;
    const int cellW = 22;
    const int cellH = lineHeight(1) + 4;
    for (int r = 0; r < rowCount(); ++r) {
      for (int col = 0; col < colCount(r); ++col) {
        const Key& k = rows_[r][col];
        const int kx = x + pad + col * cellW;
        const int ky = gridTop + r * cellH;
        const bool sel = (r == row_ && col == col_);
        if (sel) c.fillRect(kx - 2, ky - 2, cellW, cellH, true);  // black key cap
        // Selected key draws its label in paper colour (white) so it stays
        // readable on the filled cap.
        drawText(c, kx, ky, k.label, 1, 0, !sel);
      }
    }
  }

 private:
  enum class Kind { Char, Space, Del, Done, Cancel };
  struct Key {
    std::string label;
    char ch = 0;
    Kind kind = Kind::Char;
  };

  void buildLayout() {
    addCharRow("abcdefghij");
    addCharRow("klmnopqrst");
    addCharRow("uvwxyz0123");
    addCharRow("456789");
    std::vector<Key> actions;
    actions.push_back(Key{"SP", ' ', Kind::Space});
    actions.push_back(Key{"DEL", 0, Kind::Del});
    actions.push_back(Key{"GO", 0, Kind::Done});
    actions.push_back(Key{"X", 0, Kind::Cancel});
    rows_.push_back(actions);
  }

  void addCharRow(const std::string& chars) {
    std::vector<Key> row;
    for (char ch : chars) {
      row.push_back(Key{std::string(1, ch), ch, Kind::Char});
    }
    rows_.push_back(row);
  }

  Result activate(const Key& k) {
    switch (k.kind) {
      case Kind::Char:
        query_.push_back(k.ch);
        return Result::Changed;
      case Kind::Space:
        query_.push_back(' ');
        return Result::Changed;
      case Kind::Del:
        if (query_.empty()) return Result::None;
        popChar();
        return Result::Changed;
      case Kind::Done:
        return Result::Accept;
      case Kind::Cancel:
        return Result::Cancel;
    }
    return Result::None;
  }

  void popChar() {
    if (query_.empty()) return;
    // Drop a whole UTF-8 code point.
    size_t i = query_.size();
    do {
      --i;
    } while (i > 0 && (static_cast<uint8_t>(query_[i]) & 0xC0) == 0x80);
    query_.erase(i);
  }

  int rowCount() const { return static_cast<int>(rows_.size()); }
  int colCount(int r) const { return static_cast<int>(rows_[r].size()); }
  void clampCol() {
    if (col_ >= colCount(row_)) col_ = colCount(row_) - 1;
  }

  std::vector<std::vector<Key>> rows_;
  std::string query_;
  int row_ = 0;
  int col_ = 0;
};

}  // namespace pocketwiki
