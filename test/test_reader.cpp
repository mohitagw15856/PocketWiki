// Interop test: reads a .pwa built by the Python companion and checks that the
// C++ reader agrees on structure, search, links and text. The mini archive is
// generated from test/fixtures/mini at build time; the full sample archive is
// read too when it is present.
#include <cstdio>
#include <string>
#include <vector>

#include "check.h"
#include "inkkit/ByteStream.h"
#include "pocketwiki/PwaReader.h"

using namespace pocketwiki;

namespace {
bool loadFile(const char* path, std::vector<uint8_t>& out) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  out.resize(n > 0 ? static_cast<size_t>(n) : 0);
  size_t got = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
  std::fclose(f);
  return got == out.size();
}

struct LinkCapture : RenderSink {
  std::string text;
  std::vector<std::pair<uint32_t, std::string>> links;
  void onText(const char* t, size_t n, uint8_t) override { text.append(t, n); }
  void onLink(uint32_t target, const char* t, size_t n) override {
    links.emplace_back(target, std::string(t, n));
    text.append(t, n);
  }
};

int findId(PwaReader& r, const std::string& query, const std::string& title) {
  for (auto& hit : r.search(query, 25)) {
    if (hit.title == title) return static_cast<int>(hit.articleId);
  }
  return -1;
}
}  // namespace

static void mini_archive() {
#ifdef POCKETWIKI_MINI_PWA
  std::vector<uint8_t> bytes;
  CHECK(loadFile(POCKETWIKI_MINI_PWA, bytes));
  if (bytes.empty()) return;
  inkkit::MemoryReader mr(bytes);
  PwaReader reader(mr);
  CHECK(reader.open());
  CHECK_EQ(reader.articleCount(), static_cast<uint32_t>(3));

  const int alpha = findId(reader, "alp", "Alpha");
  const int beta = findId(reader, "bet", "Beta");
  CHECK(alpha >= 0);
  CHECK(beta >= 0);

  // Accent insensitive search: "gamma e" folds and matches "Gamma Éclair".
  auto gammaHits = reader.search("gamma e", 5);
  CHECK(!gammaHits.empty());

  // Alpha's body: styled text present, and its link resolves to Beta.
  LinkCapture alphaSink;
  CHECK(reader.readArticle(static_cast<uint32_t>(alpha), alphaSink));
  CHECK(alphaSink.text.find("bold") != std::string::npos);
  CHECK(alphaSink.text.find("italic") != std::string::npos);
  bool linksToBeta = false;
  for (auto& l : alphaSink.links) {
    if (l.first == static_cast<uint32_t>(beta)) linksToBeta = true;
  }
  CHECK(linksToBeta);

  // Beta's external link is plain text (no LINK token to an outside target),
  // but its internal link to Alpha is a real link.
  LinkCapture betaSink;
  CHECK(reader.readArticle(static_cast<uint32_t>(beta), betaSink));
  CHECK(betaSink.text.find("site") != std::string::npos);
  bool linksToAlpha = false;
  for (auto& l : betaSink.links) {
    CHECK(l.first != 0xFFFFFFFFu);
    if (l.first == static_cast<uint32_t>(alpha)) linksToAlpha = true;
  }
  CHECK(linksToAlpha);
#endif
}

static void full_sample() {
#ifdef POCKETWIKI_SAMPLE_PWA
  std::vector<uint8_t> bytes;
  if (!loadFile(POCKETWIKI_SAMPLE_PWA, bytes) || bytes.empty()) {
    std::fprintf(stderr, "  (sample archive not present, skipping)\n");
    return;
  }
  inkkit::MemoryReader mr(bytes);
  PwaReader reader(mr);
  CHECK(reader.open());
  CHECK_EQ(reader.articleCount(), static_cast<uint32_t>(50));

  const int newton = findId(reader, "isaac", "Isaac Newton");
  CHECK(newton >= 0);
  LinkCapture sink;
  CHECK(reader.readArticle(static_cast<uint32_t>(newton), sink));
  CHECK(sink.text.find("Newton") != std::string::npos);
  CHECK(!sink.links.empty());
  // Every link target is a valid article id.
  for (auto& l : sink.links) {
    DirEntry e;
    CHECK(reader.dirEntry(l.first, e));
  }
#endif
}

int main() {
  RUN(mini_archive);
  RUN(full_sample);
  REPORT();
}
