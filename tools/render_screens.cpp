// Render genuine PocketWiki UI frames on the host using the real rendering code
// (Canvas, 5x7 font, ListMenu, Keyboard, ArticleLayout) against the real sample
// archive, and write them as PGM images. tools/make_media.py turns these into
// the PNG screenshots and animated GIFs used in the README.
//
// These are true renders of the firmware's drawing code at an illustrative panel
// resolution; they are not photographs of hardware and the panel bit order is
// still pending on-device confirmation (see docs/HARDWARE_TESTING.md).
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "inkkit/ByteStream.h"
#include "pocketwiki/ArticleLayout.h"
#include "pocketwiki/Canvas.h"
#include "pocketwiki/PwaReader.h"
#include "pocketwiki/Text.h"
#include "ui/Keyboard.h"
#include "ui/ListMenu.h"

using namespace pocketwiki;

namespace {

constexpr int kW = 400;   // illustrative X4 panel size
constexpr int kH = 300;
constexpr int kStride = (kW + 7) / 8;
constexpr int kHeader = 22;
constexpr int kFooter = 16;
constexpr int kMargin = 6;
constexpr int kRow = 18;

struct Frame {
  std::vector<uint8_t> fb = std::vector<uint8_t>(kStride * kH, 0xFF);
  Canvas canvas{fb.data(), kW, kH, kStride};
  void reset() {
    fb.assign(kStride * kH, 0xFF);
    canvas.clear(true);
  }
};

void savePgm(const Frame& f, const std::string& path) {
  std::FILE* fp = std::fopen(path.c_str(), "wb");
  if (!fp) {
    std::fprintf(stderr, "cannot write %s\n", path.c_str());
    return;
  }
  std::fprintf(fp, "P5\n%d %d\n255\n", kW, kH);
  std::vector<uint8_t> row(kW);
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      const uint8_t byte = f.fb[y * kStride + (x >> 3)];
      const bool white = (byte & (0x80 >> (x & 7))) != 0;
      row[x] = white ? 255 : 0;
    }
    std::fwrite(row.data(), 1, kW, fp);
  }
  std::fclose(fp);
}

void header(Canvas& c, const std::string& title) {
  c.fillRect(0, 0, kW, kHeader, false);
  drawText(c, kMargin, 6, title, 1, kStyleBold);
  c.hLine(0, kHeader - 1, kW, true);
}

void footer(Canvas& c, const std::string& hint) {
  const int y = kH - kFooter;
  c.hLine(0, y, kW, true);
  drawText(c, kMargin, y + 4, hint, 1, 0);
}

int contentTop() { return kHeader + 2; }
int contentH() { return kH - kHeader - kFooter - 2; }
int contentW() { return kW - 2 * kMargin; }

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: render_screens <sample.pwa> <outdir>\n");
    return 2;
  }
  const std::string outdir = argv[2];

  std::vector<uint8_t> bytes;
  {
    std::FILE* fp = std::fopen(argv[1], "rb");
    if (!fp) {
      std::fprintf(stderr, "cannot open %s\n", argv[1]);
      return 1;
    }
    std::fseek(fp, 0, SEEK_END);
    long n = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    bytes.resize(n);
    if (std::fread(bytes.data(), 1, n, fp) != static_cast<size_t>(n)) return 1;
    std::fclose(fp);
  }

  inkkit::MemoryReader mr(bytes);
  PwaReader reader(mr);
  if (!reader.open()) {
    std::fprintf(stderr, "not a valid archive\n");
    return 1;
  }

  auto findId = [&](const std::string& q, const std::string& title) -> int {
    for (auto& h : reader.search(q, 30))
      if (h.title == title) return static_cast<int>(h.articleId);
    return -1;
  };

  // --- Home menu ---
  {
    Frame f;
    header(f.canvas, "PocketWiki");
    ListMenu m;
    m.setItems({"Browse all articles", "Search", "History", "Bookmarks", "About"});
    m.setViewportRows(contentH() / kRow);
    m.render(f.canvas, kMargin, contentTop(), contentW(), contentH());
    footer(f.canvas, "Select: open   Hold power: off");
    savePgm(f, outdir + "/home.pgm");
  }

  // --- Browse list ---
  {
    Frame f;
    header(f.canvas, "All articles");
    std::vector<std::string> titles;
    for (uint32_t id = 0; id < reader.articleCount(); ++id) {
      DirEntry e;
      if (reader.dirEntry(id, e)) titles.push_back(e.title);
    }
    ListMenu m;
    m.setItems(titles);
    m.setViewportRows(contentH() / kRow);
    int nid = findId("isaac", "Isaac Newton");
    if (nid >= 0) m.setSelectedIndex(nid);
    m.render(f.canvas, kMargin, contentTop(), contentW(), contentH());
    footer(f.canvas, "Select: read   Back: home");
    savePgm(f, outdir + "/browse.pgm");
  }

  // --- Search (keyboard + live results), plus typing frames for a GIF ---
  {
    const std::vector<std::string> steps = {"m", "ma", "mar", "mars"};
    int frame = 0;
    for (const auto& q : steps) {
      Frame f;
      header(f.canvas, "Search");
      const int kbH = contentH() / 2;
      Keyboard kb;
      kb.setQuery(q);
      kb.render(f.canvas, kMargin, contentTop(), contentW(), kbH);
      std::vector<std::string> names;
      for (auto& h : reader.search(q, 30)) names.push_back(h.title);
      ListMenu m;
      m.setItems(names);
      m.setViewportRows((contentH() - kbH) / kRow);
      m.render(f.canvas, kMargin, contentTop() + kbH + 2, contentW(), contentH() - kbH - 2);
      footer(f.canvas, "GO: results   Back: erase");
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%s/search_%d.pgm", outdir.c_str(), frame++);
      savePgm(f, buf);
      if (q == "mar") savePgm(f, outdir + "/search.pgm");  // the still used in the README
    }
  }

  // --- Reader (Isaac Newton), plus paging frames for a GIF ---
  {
    int nid = findId("isaac", "Isaac Newton");
    if (nid >= 0) {
      ArticleLayout layout(contentW(), contentH());
      reader.readArticle(static_cast<uint32_t>(nid), layout);
      layout.finish();
      DirEntry e;
      reader.dirEntry(static_cast<uint32_t>(nid), e);
      const size_t pages = layout.pageCount();
      for (size_t p = 0; p < pages; ++p) {
        Frame f;
        header(f.canvas, e.title);
        layout.drawPage(f.canvas, kMargin, contentTop(), p);
        char foot[64];
        std::snprintf(foot, sizeof(foot), "Page %zu/%zu   Select: menu", p + 1, pages);
        footer(f.canvas, foot);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s/reader_p%zu.pgm", outdir.c_str(), p);
        savePgm(f, buf);
        if (p == 0) savePgm(f, outdir + "/reader.pgm");
      }
      std::printf("reader pages: %zu\n", pages);
    }
  }

  std::printf("rendered frames into %s\n", outdir.c_str());
  return 0;
}
