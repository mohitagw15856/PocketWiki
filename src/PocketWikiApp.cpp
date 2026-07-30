#include "PocketWikiApp.h"

#ifdef ARDUINO

namespace pocketwiki {

namespace {
const char* kHomeItems[] = {"Browse all articles", "Search", "History", "Bookmarks", "About"};
enum HomeIndex { kBrowse = 0, kSearch, kHistory, kBookmarks, kAbout };
}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void PocketWikiApp::begin() {
  display_.begin();
  lastInputMs_ = power_.millis();

  inkkit::sd::ensureDir(config::kArchiveDir);
  inkkit::sd::ensureDir(config::kStateDir);
  loadLibrary();
  scanArchives();

  if (archivePaths_.empty()) {
    errorText_ = "No .pwa archives found in /pocketwiki. Build one with the companion tool.";
    go(Screen::Error);
    return;
  }

  // Remember the last opened archive if possible.
  std::string last;
  if (inkkit::sd::readWholeFile(config::kLastArchivePath, last)) {
    while (!last.empty() && (last.back() == '\n' || last.back() == '\r')) last.pop_back();
  }

  if (archivePaths_.size() == 1) {
    if (openArchive(archivePaths_[0])) {
      go(Screen::Home);
    }
    return;
  }

  // Several archives: if the remembered one is present, open it, else pick.
  for (const auto& p : archivePaths_) {
    if (p == last && openArchive(p)) {
      go(Screen::Home);
      return;
    }
  }
  std::vector<std::string> names;
  for (const auto& p : archivePaths_) {
    names.push_back(p.substr(p.find_last_of('/') + 1));
  }
  list_.setItems(names);
  go(Screen::Picker);
}

void PocketWikiApp::scanArchives() {
  archivePaths_.clear();
  inkkit::sd::listFiles(config::kArchiveDir, config::kArchiveExt,
                        [this](const std::string& path) { archivePaths_.push_back(path); });
}

bool PocketWikiApp::openArchive(const std::string& path) {
  reader_.reset();
  byteReader_.reset();
  if (!inkkit::sd::openRead(config::kStorageTag, path.c_str(), archiveFile_)) {
    errorText_ = "Could not open " + path;
    go(Screen::Error);
    return false;
  }
  byteReader_.reset(new inkkit::SdFileReader(archiveFile_));
  reader_.reset(new PwaReader(*byteReader_));
  if (!reader_->open()) {
    errorText_ = "Not a valid PocketWiki archive: " + path;
    reader_.reset();
    go(Screen::Error);
    return false;
  }
  currentArchivePath_ = path;
  inkkit::sd::writeWholeFile(config::kLastArchivePath, path + "\n");
  homeMenu_.setItems({kHomeItems[0], kHomeItems[1], kHomeItems[2], kHomeItems[3], kHomeItems[4]});
  return true;
}

// ---------------------------------------------------------------------------
// Library persistence
// ---------------------------------------------------------------------------

void PocketWikiApp::loadLibrary() {
  std::string text;
  if (inkkit::sd::readWholeFile(config::kHistoryPath, text)) history_.parse(text);
  if (inkkit::sd::readWholeFile(config::kBookmarksPath, text)) bookmarks_.parse(text);
}

void PocketWikiApp::saveHistory() {
  inkkit::sd::writeWholeFile(config::kHistoryPath, history_.serialise());
}

void PocketWikiApp::saveBookmarks() {
  inkkit::sd::writeWholeFile(config::kBookmarksPath, bookmarks_.serialise());
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void PocketWikiApp::go(Screen screen, bool full) {
  screen_ = screen;
  dirty_ = true;
  needFull_ = needFull_ || full;
}

void PocketWikiApp::refreshBrowseList() {
  std::vector<std::string> titles;
  const uint32_t n = reader_->articleCount();
  titles.reserve(n);
  for (uint32_t id = 0; id < n; ++id) {
    DirEntry e;
    if (reader_->dirEntry(id, e)) titles.push_back(e.title);
  }
  list_.setItems(titles);
}

void PocketWikiApp::openArticle(uint32_t articleId, uint32_t startPage) {
  DirEntry entry;
  if (!reader_->dirEntry(articleId, entry)) return;
  layout_.reset(new ArticleLayout(display_.width() - 2 * config::kMarginX, contentHeight()));
  if (!reader_->readArticle(articleId, *layout_)) {
    errorText_ = "Failed to read article";
    go(Screen::Error);
    return;
  }
  layout_->finish();
  currentArticleId_ = articleId;
  currentTitle_ = entry.title;
  readerPage_ = startPage < layout_->pageCount() ? startPage : 0;

  history_.add(articleId, entry.title);
  saveHistory();

  go(Screen::Reader);
}

void PocketWikiApp::runSearch() {
  searchHits_ = reader_->search(keyboard_.query(), config::kMaxSearchResults);
  std::vector<std::string> names;
  for (const auto& hit : searchHits_) names.push_back(hit.title);
  list_.setItems(names);
}

void PocketWikiApp::rebuildReaderMenu() {
  std::vector<std::string> items;
  if (!pageLinks_.empty()) items.push_back("Follow a link on this page");
  items.push_back(bookmarks_.contains(currentArticleId_) ? "Remove bookmark" : "Add bookmark");
  items.push_back("Back to article");
  overlayMenu_.setItems(items);
}

// ---------------------------------------------------------------------------
// Main tick
// ---------------------------------------------------------------------------

void PocketWikiApp::tick() {
  input_.update();

  // Power button hold powers the device down.
  if (input_.powerHeldMs() >= config::kPowerOffHoldMs) {
    saveHistory();
    saveBookmarks();
    power_.deepSleep();  // does not return
    return;
  }

  const Button b = input_.poll();
  if (b != Button::None) {
    lastInputMs_ = power_.millis();
    handleInput(b);
  } else if (power_.millis() - lastInputMs_ > config::kIdleSleepMs) {
    saveHistory();
    saveBookmarks();
    power_.deepSleep();  // does not return
    return;
  }

  if (dirty_) render();
}

void PocketWikiApp::handleInput(Button b) {
  switch (screen_) {
    case Screen::Picker: handlePicker(b); break;
    case Screen::Home: handleHome(b); break;
    case Screen::Browse: handleBrowse(b); break;
    case Screen::Search: handleSearch(b); break;
    case Screen::Reader: handleReader(b); break;
    case Screen::ReaderMenu: handleReaderMenu(b); break;
    case Screen::LinkMenu: handleLinkMenu(b); break;
    case Screen::History: handleLibraryList(b, /*bookmarks=*/false); break;
    case Screen::Bookmarks: handleLibraryList(b, /*bookmarks=*/true); break;
    case Screen::About:
    case Screen::Error:
      if (b == Button::Back || b == Button::Select) go(Screen::Home);
      break;
  }
}

void PocketWikiApp::handlePicker(Button b) {
  auto r = list_.handle(b);
  if (r == ListMenu::Result::Activated) {
    if (openArchive(archivePaths_[list_.selectedIndex()])) go(Screen::Home);
  } else if (r != ListMenu::Result::None) {
    dirty_ = true;
  } else {
    dirty_ = true;
  }
}

void PocketWikiApp::handleHome(Button b) {
  auto r = homeMenu_.handle(b);
  if (r == ListMenu::Result::Activated) {
    switch (homeMenu_.selectedIndex()) {
      case kBrowse:
        refreshBrowseList();
        go(Screen::Browse);
        break;
      case kSearch:
        keyboard_.clear();
        searchFocus_ = SearchFocus::Keyboard;
        runSearch();
        go(Screen::Search);
        break;
      case kHistory: {
        std::vector<std::string> names;
        for (const auto& e : history_.entries()) names.push_back(e.title);
        list_.setItems(names);
        go(Screen::History);
        break;
      }
      case kBookmarks: {
        std::vector<std::string> names;
        for (const auto& m : bookmarks_.all()) names.push_back(m.title);
        list_.setItems(names);
        go(Screen::Bookmarks);
        break;
      }
      case kAbout:
        go(Screen::About);
        break;
    }
  } else if (r == ListMenu::Result::Back) {
    // Already at the top level; nothing to do.
  } else {
    dirty_ = true;
  }
}

void PocketWikiApp::handleBrowse(Button b) {
  auto r = list_.handle(b);
  if (r == ListMenu::Result::Activated) {
    openArticle(static_cast<uint32_t>(list_.selectedIndex()));
  } else if (r == ListMenu::Result::Back) {
    go(Screen::Home);
  } else {
    dirty_ = true;
  }
}

void PocketWikiApp::handleSearch(Button b) {
  if (searchFocus_ == SearchFocus::Keyboard) {
    auto r = keyboard_.handle(b);
    if (r == Keyboard::Result::Changed) {
      runSearch();
      dirty_ = true;
    } else if (r == Keyboard::Result::Accept) {
      if (!searchHits_.empty()) {
        searchFocus_ = SearchFocus::Results;
        list_.setSelectedIndex(0);
      }
      dirty_ = true;
    } else if (r == Keyboard::Result::Cancel) {
      go(Screen::Home);
    } else {
      dirty_ = true;
    }
  } else {  // Results focus
    if (b == Button::Back) {
      searchFocus_ = SearchFocus::Keyboard;
      dirty_ = true;
      return;
    }
    auto r = list_.handle(b);
    if (r == ListMenu::Result::Activated && !searchHits_.empty()) {
      openArticle(searchHits_[list_.selectedIndex()].articleId);
    } else {
      dirty_ = true;
    }
  }
}

void PocketWikiApp::handleReader(Button b) {
  const size_t pages = layout_ ? layout_->pageCount() : 0;
  switch (b) {
    case Button::Down:
    case Button::Right:
      if (readerPage_ + 1 < pages) {
        ++readerPage_;
        dirty_ = true;
      }
      break;
    case Button::Up:
    case Button::Left:
      if (readerPage_ > 0) {
        --readerPage_;
        dirty_ = true;
      }
      break;
    case Button::Select:
      previousContentScreen_ = Screen::Reader;
      rebuildReaderMenu();
      go(Screen::ReaderMenu, /*full=*/false);
      break;
    case Button::Back:
      go(Screen::Browse);
      break;
    default:
      break;
  }
}

void PocketWikiApp::handleReaderMenu(Button b) {
  auto r = overlayMenu_.handle(b);
  if (r == ListMenu::Result::Back) {
    go(Screen::Reader);
    return;
  }
  if (r != ListMenu::Result::Activated) {
    dirty_ = true;
    return;
  }
  const std::string& choice = "";  // determined by index below
  (void)choice;
  int idx = overlayMenu_.selectedIndex();
  const bool hasLinks = !pageLinks_.empty();
  if (hasLinks && idx == 0) {
    // Build the link chooser (dedupe by target, label with article title).
    std::vector<std::string> labels;
    linkTargets_.clear();
    for (const auto& l : pageLinks_) {
      bool seen = false;
      for (auto t : linkTargets_) {
        if (t == l.target) seen = true;
      }
      if (seen) continue;
      DirEntry e;
      if (reader_->dirEntry(l.target, e)) {
        linkTargets_.push_back(l.target);
        labels.push_back(e.title);
      }
    }
    overlayMenu_.setItems(labels);
    go(Screen::LinkMenu, /*full=*/false);
    return;
  }
  const int bookmarkIdx = hasLinks ? 1 : 0;
  const int backIdx = hasLinks ? 2 : 1;
  if (idx == bookmarkIdx) {
    bookmarks_.toggle(currentArticleId_, static_cast<uint32_t>(readerPage_), currentTitle_);
    saveBookmarks();
    go(Screen::Reader);
  } else if (idx == backIdx) {
    go(Screen::Reader);
  }
}

void PocketWikiApp::handleLinkMenu(Button b) {
  auto r = overlayMenu_.handle(b);
  if (r == ListMenu::Result::Back) {
    rebuildReaderMenu();
    go(Screen::ReaderMenu, /*full=*/false);
  } else if (r == ListMenu::Result::Activated) {
    const int i = overlayMenu_.selectedIndex();
    if (i >= 0 && i < static_cast<int>(linkTargets_.size())) {
      openArticle(linkTargets_[i]);
    }
  } else {
    dirty_ = true;
  }
}

void PocketWikiApp::handleLibraryList(Button b, bool bookmarks) {
  auto r = list_.handle(b);
  if (r == ListMenu::Result::Activated) {
    const int i = list_.selectedIndex();
    if (bookmarks) {
      if (i >= 0 && i < static_cast<int>(bookmarks_.all().size())) {
        const auto& m = bookmarks_.all()[i];
        openArticle(m.articleId, m.position);
      }
    } else {
      if (i >= 0 && i < static_cast<int>(history_.entries().size())) {
        openArticle(history_.entries()[i].articleId);
      }
    }
  } else if (r == ListMenu::Result::Back) {
    go(Screen::Home);
  } else {
    dirty_ = true;
  }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

Canvas PocketWikiApp::canvas() {
  return Canvas(display_.framebuffer(), display_.width(), display_.height(), display_.stride());
}

int PocketWikiApp::contentHeight() const {
  return display_.height() - config::kHeaderHeight - config::kFooterHeight;
}

void PocketWikiApp::drawHeader(Canvas& c, const std::string& title) {
  c.fillRect(0, 0, c.width(), config::kHeaderHeight, false);  // paper
  drawText(c, config::kMarginX, 5, title, 1, kStyleBold);
  c.hLine(0, config::kHeaderHeight - 1, c.width(), true);
}

void PocketWikiApp::drawFooter(Canvas& c, const std::string& hint) {
  const int y = c.height() - config::kFooterHeight;
  c.hLine(0, y, c.width(), true);
  drawText(c, config::kMarginX, y + 3, hint, 1, 0);
}

void PocketWikiApp::render() {
  Canvas c = canvas();
  c.clear(true);

  const int cx = config::kMarginX;
  const int cy = contentTop() + 2;
  const int cw = c.width() - 2 * config::kMarginX;
  const int ch = contentHeight();
  const int rows = ch / config::kListRowHeight;

  switch (screen_) {
    case Screen::Picker:
      drawHeader(c, "Choose an archive");
      list_.setViewportRows(rows);
      list_.render(c, cx, cy, cw, ch);
      drawFooter(c, "Select: open   Up/Down: move");
      break;
    case Screen::Home:
      drawHeader(c, "PocketWiki");
      homeMenu_.setViewportRows(rows);
      homeMenu_.render(c, cx, cy, cw, ch);
      drawFooter(c, "Select: open   Hold power: off");
      break;
    case Screen::Browse:
      drawHeader(c, "All articles");
      list_.setViewportRows(rows);
      list_.render(c, cx, cy, cw, ch);
      drawFooter(c, "Select: read   Back: home");
      break;
    case Screen::Search: {
      drawHeader(c, "Search");
      const int kbH = ch / 2;
      keyboard_.render(c, cx, cy, cw, kbH);
      list_.setViewportRows((ch - kbH) / config::kListRowHeight);
      list_.render(c, cx, cy + kbH + 2, cw, ch - kbH - 2);
      drawFooter(c, searchFocus_ == SearchFocus::Keyboard ? "GO: results   Back: erase"
                                                          : "Select: read   Back: keyboard");
      break;
    }
    case Screen::Reader: {
      drawHeader(c, currentTitle_);
      pageLinks_.clear();
      if (layout_) pageLinks_ = layout_->drawPage(c, cx, cy, readerPage_);
      const size_t pages = layout_ ? layout_->pageCount() : 1;
      drawFooter(c, "Page " + std::to_string(readerPage_ + 1) + "/" + std::to_string(pages) +
                        "   Select: menu");
      break;
    }
    case Screen::ReaderMenu:
      drawHeader(c, currentTitle_);
      if (layout_) layout_->drawPage(c, cx, cy, readerPage_);
      overlayMenu_.setViewportRows(6);
      overlayMenu_.render(c, cx + cw / 6, cy + ch / 4, cw * 2 / 3, ch / 2);
      drawFooter(c, "Select: choose   Back: close");
      break;
    case Screen::LinkMenu:
      drawHeader(c, "Follow a link");
      overlayMenu_.setViewportRows(rows);
      overlayMenu_.render(c, cx, cy, cw, ch);
      drawFooter(c, "Select: open   Back: menu");
      break;
    case Screen::History:
      drawHeader(c, "History");
      list_.setViewportRows(rows);
      list_.render(c, cx, cy, cw, ch);
      drawFooter(c, "Select: read   Back: home");
      break;
    case Screen::Bookmarks:
      drawHeader(c, "Bookmarks");
      list_.setViewportRows(rows);
      list_.render(c, cx, cy, cw, ch);
      drawFooter(c, "Select: read   Back: home");
      break;
    case Screen::About:
      drawHeader(c, "About PocketWiki");
      drawText(c, cx, cy, std::string("PocketWiki offline reader"), 1, kStyleBold);
      drawText(c, cx, cy + lineHeight(1) * 2, "Collection:", 1, 0);
      drawText(c, cx, cy + lineHeight(1) * 3, reader_ ? reader_->header().collectionName : "", 1, 0);
      drawText(c, cx, cy + lineHeight(1) * 5,
               std::to_string(reader_ ? reader_->articleCount() : 0) + " articles", 1, 0);
      drawFooter(c, "Back: home");
      break;
    case Screen::Error:
      drawHeader(c, "PocketWiki");
      for (size_t i = 0; i < wrapText(errorText_, cw, 1, 0).size(); ++i) {
        drawText(c, cx, cy + static_cast<int>(i) * lineHeight(1), wrapText(errorText_, cw, 1, 0)[i], 1, 0);
      }
      drawFooter(c, "Back: retry");
      break;
  }

  display_.flush(needFull_);
  needFull_ = false;
  dirty_ = false;
}

}  // namespace pocketwiki

#endif  // ARDUINO
