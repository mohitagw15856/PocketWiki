// Lays a decoded article out into styled lines and pages for the reader screen.
//
// ArticleLayout is a RenderSink: feed it an article through PwaReader::readArticle
// and it accumulates wrapped lines, each a sequence of positioned segments that
// carry their own style and (for links) a target article id. Pages are computed
// by packing whole lines into the content height. It is portable (Canvas + Text
// only) so pagination and link capture are unit tested on the host; the device
// draws the same lines.
//
// Memory: one article is laid out at a time. Articles are kept small by the
// build tool (heavy processing happens on the desktop), so the line vector is
// modest; the reader never holds more than one article's layout.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "pocketwiki/Canvas.h"
#include "pocketwiki/RenderStream.h"
#include "pocketwiki/Text.h"

namespace pocketwiki {

struct Segment {
  std::string text;
  int x = 0;       // pixel x within the content area
  int scale = 1;
  uint8_t style = 0;
  bool link = false;
  uint32_t target = 0;
};

struct LayoutLine {
  std::vector<Segment> segments;
  int height = 0;      // pixel height of the line
  bool rule = false;   // draw a horizontal rule instead of text
};

struct LinkRef {
  size_t lineIndex = 0;
  uint32_t target = 0;
  std::string label;
};

class ArticleLayout : public RenderSink {
 public:
  // contentWidth/contentHeight are the drawable article area in pixels.
  ArticleLayout(int contentWidth, int contentHeight)
      : contentWidth_(contentWidth), contentHeight_(contentHeight) {}

  const std::vector<LayoutLine>& lines() const { return lines_; }
  const std::vector<LinkRef>& links() const { return links_; }

  // Number of pages once layout is finished (call finish() first).
  size_t pageCount() const { return pageStarts_.size(); }

  // Index of the first line on a page.
  size_t pageStartLine(size_t page) const {
    return page < pageStarts_.size() ? pageStarts_[page] : lines_.size();
  }
  size_t pageEndLine(size_t page) const {
    return (page + 1 < pageStarts_.size()) ? pageStarts_[page + 1] : lines_.size();
  }

  // The page a given line falls on (used to jump to a bookmarked position).
  size_t pageOfLine(size_t line) const {
    for (size_t p = pageStarts_.size(); p-- > 0;) {
      if (pageStarts_[p] <= line) return p;
    }
    return 0;
  }

  // --- RenderSink ---
  void onBlockStart(const BlockInfo& b) override {
    flushLine();
    switch (b.kind) {
      case BlockKind::Heading:
        blockScale_ = b.level <= 1 ? 2 : (b.level == 2 ? 2 : 1);
        blockStyle_ = kStyleBold;
        indent_ = 0;
        pendingBullet_.clear();
        blankBefore_ = true;
        break;
      case BlockKind::ListItem:
        blockScale_ = 1;
        blockStyle_ = 0;
        indent_ = (b.depth + 1) * (kFontWidth + 1) * 1;
        pendingBullet_ = b.ordered ? std::string("- ") : std::string("- ");
        blankBefore_ = false;
        break;
      case BlockKind::Rule:
        flushLine();
        addRuleLine();
        blockScale_ = 1;
        blockStyle_ = 0;
        indent_ = 0;
        pendingBullet_.clear();
        break;
      case BlockKind::Paragraph:
      default:
        blockScale_ = 1;
        blockStyle_ = 0;
        indent_ = 0;
        pendingBullet_.clear();
        blankBefore_ = true;
        break;
    }
    cursorX_ = indent_;
    lineStarted_ = false;
    // A blank spacer line between blocks improves readability.
    if (blankBefore_ && !lines_.empty()) addSpacer();
  }

  void onText(const char* utf8, size_t len, uint8_t style) override {
    appendRun(std::string(utf8, len), style, false, 0);
  }

  void onLink(uint32_t target, const char* label, size_t len) override {
    appendRun(std::string(label, len), 0, true, target);
  }

  void onEnd() override { flushLine(); }

  // Call after readArticle returns to compute page boundaries.
  void finish() {
    flushLine();
    computePages();
  }

  // Draw one page into the canvas content area starting at (originX, originY).
  // Returns the links visible on this page (their target and a label).
  std::vector<LinkRef> drawPage(Canvas& canvas, int originX, int originY, size_t page) const {
    std::vector<LinkRef> visible;
    int y = originY;
    const size_t begin = pageStartLine(page);
    const size_t end = pageEndLine(page);
    for (size_t i = begin; i < end; ++i) {
      const LayoutLine& line = lines_[i];
      if (line.rule) {
        canvas.hLine(originX, y + line.height / 2, contentWidth_, true);
      } else {
        for (const Segment& s : line.segments) {
          drawText(canvas, originX + s.x, y, s.text, s.scale, s.style);
          if (s.link) {
            const int w = measureText(s.text, s.scale, s.style);
            canvas.hLine(originX + s.x, y + kFontHeight * s.scale + 1, w, true);
          }
        }
      }
      for (const LinkRef& l : links_) {
        if (l.lineIndex == i) visible.push_back(l);
      }
      y += line.height;
    }
    return visible;
  }

 private:
  void appendRun(const std::string& text, uint8_t style, bool link, uint32_t target) {
    const uint8_t effStyle = static_cast<uint8_t>(style | blockStyle_);
    // Split into words on spaces; each word is placed with wrapping.
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
      size_t j = i;
      while (j < n && text[j] != ' ') ++j;
      if (j > i) {
        placeWord(text.substr(i, j - i), effStyle, link, target);
      }
      i = (j < n) ? j + 1 : j;
    }
  }

  void placeWord(const std::string& word, uint8_t style, bool link, uint32_t target) {
    const int space = charAdvance(blockScale_, style);
    int wordWidth = measureText(word, blockScale_, style);

    if (!lineStarted_ && !pendingBullet_.empty()) {
      // Emit the bullet first.
      Segment b;
      b.text = pendingBullet_;
      b.x = 0;
      b.scale = blockScale_;
      b.style = 0;
      current_.push_back(b);
      cursorX_ = measureText(pendingBullet_, blockScale_, 0);
      pendingBullet_.clear();
      lineStarted_ = true;
    }

    const int needed = (lineStarted_ && cursorX_ > indent_ ? space : 0) + wordWidth;
    if (lineStarted_ && cursorX_ + needed > contentWidth_) {
      flushLine();
      cursorX_ = indent_;
    }
    if (cursorX_ > indent_) cursorX_ += space;

    Segment s;
    s.text = word;
    s.x = cursorX_;
    s.scale = blockScale_;
    s.style = style;
    s.link = link;
    s.target = target;
    current_.push_back(s);
    if (link) {
      links_.push_back(LinkRef{lines_.size(), target, word});
    }
    cursorX_ += wordWidth;
    lineStarted_ = true;
  }

  void flushLine() {
    if (current_.empty()) {
      lineStarted_ = false;
      return;
    }
    LayoutLine line;
    line.segments = current_;
    line.height = lineHeight(blockScale_);
    // Fix up the recorded link line indices (they were pushed with lines_.size()
    // which is the index this line will occupy).
    lines_.push_back(std::move(line));
    current_.clear();
    lineStarted_ = false;
    cursorX_ = indent_;
  }

  void addSpacer() {
    LayoutLine line;
    line.height = lineHeight(1) / 2;
    lines_.push_back(line);
  }

  void addRuleLine() {
    LayoutLine line;
    line.rule = true;
    line.height = lineHeight(1);
    lines_.push_back(line);
  }

  void computePages() {
    pageStarts_.clear();
    if (lines_.empty()) {
      pageStarts_.push_back(0);
      return;
    }
    pageStarts_.push_back(0);
    int y = 0;
    for (size_t i = 0; i < lines_.size(); ++i) {
      if (y + lines_[i].height > contentHeight_ && y > 0) {
        pageStarts_.push_back(i);
        y = 0;
      }
      y += lines_[i].height;
    }
  }

  int contentWidth_;
  int contentHeight_;
  std::vector<LayoutLine> lines_;
  std::vector<LinkRef> links_;
  std::vector<size_t> pageStarts_;

  std::vector<Segment> current_;
  int cursorX_ = 0;
  int indent_ = 0;
  int blockScale_ = 1;
  uint8_t blockStyle_ = 0;
  bool lineStarted_ = false;
  bool blankBefore_ = false;
  std::string pendingBullet_;
};

}  // namespace pocketwiki
