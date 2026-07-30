// The application: owns the current archive, the screen state machine and the
// persisted library, and drives rendering through inkkit's Display.
//
// Device only. The heavy lifting (archive reading, search, article layout) lives
// in the portable core and is tested on the host; this class wires that core to
// the panel, buttons and SD card via inkkit.
#pragma once

#ifdef ARDUINO

#include <memory>
#include <string>
#include <vector>

#include "Input.h"
#include "PocketWikiConfig.h"
#include "inkkit/Display.h"
#include "inkkit/Power.h"
#include "inkkit/SdStream.h"
#include "inkkit/Storage.h"
#include "pocketwiki/ArticleLayout.h"
#include "pocketwiki/Canvas.h"
#include "pocketwiki/Library.h"
#include "pocketwiki/PwaReader.h"
#include "ui/Keyboard.h"
#include "ui/ListMenu.h"

namespace pocketwiki {

class PocketWikiApp {
 public:
  PocketWikiApp(inkkit::Display& display, Input& input, inkkit::Power& power)
      : display_(display), input_(input), power_(power) {}

  // Discover archives, load state, open the first (or the remembered) archive
  // and paint the first screen. Call once from setup().
  void begin();

  // Poll input, update state and refresh the panel when needed. Call from loop().
  void tick();

 private:
  enum class Screen : uint8_t {
    Picker,
    Home,
    Browse,
    Search,
    Reader,
    ReaderMenu,
    LinkMenu,
    History,
    Bookmarks,
    About,
    Error,
  };

  enum class SearchFocus : uint8_t { Keyboard, Results };

  // Archive management.
  void scanArchives();
  bool openArchive(const std::string& path);

  // State persistence.
  void loadLibrary();
  void saveHistory();
  void saveBookmarks();

  // Navigation helpers.
  void go(Screen screen, bool full = true);
  void openArticle(uint32_t articleId, uint32_t startPage = 0);
  void runSearch();
  void refreshBrowseList();
  void rebuildReaderMenu();

  // Input handling per screen.
  void handleInput(Button b);
  void handlePicker(Button b);
  void handleHome(Button b);
  void handleBrowse(Button b);
  void handleSearch(Button b);
  void handleReader(Button b);
  void handleReaderMenu(Button b);
  void handleLinkMenu(Button b);
  void handleLibraryList(Button b, bool bookmarks);

  // Rendering.
  Canvas canvas();
  void render();
  void drawHeader(Canvas& c, const std::string& title);
  void drawFooter(Canvas& c, const std::string& hint);
  int contentTop() const { return config::kHeaderHeight; }
  int contentHeight() const;

  inkkit::Display& display_;
  Input& input_;
  inkkit::Power& power_;

  // Archive.
  std::vector<std::string> archivePaths_;
  std::string currentArchivePath_;
  HalFile archiveFile_;
  std::unique_ptr<inkkit::SdFileReader> byteReader_;
  std::unique_ptr<PwaReader> reader_;

  // Library.
  HistoryStore history_;
  BookmarkStore bookmarks_;

  // Screen state.
  Screen screen_ = Screen::Home;
  Screen previousContentScreen_ = Screen::Home;  // where menus overlay from
  ListMenu homeMenu_;
  ListMenu list_;  // reused for browse / history / bookmarks / picker
  ListMenu overlayMenu_;  // reader menu / link menu
  Keyboard keyboard_;
  SearchFocus searchFocus_ = SearchFocus::Keyboard;
  std::vector<SearchHit> searchHits_;

  // Reader state.
  std::unique_ptr<ArticleLayout> layout_;
  uint32_t currentArticleId_ = 0;
  std::string currentTitle_;
  size_t readerPage_ = 0;
  std::vector<LinkRef> pageLinks_;
  std::vector<uint32_t> linkTargets_;  // targets shown in the link chooser

  std::string errorText_;

  bool dirty_ = true;
  bool needFull_ = true;
  uint32_t lastInputMs_ = 0;
};

}  // namespace pocketwiki

#endif  // ARDUINO
