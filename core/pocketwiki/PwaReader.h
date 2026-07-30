// Streaming reader for a .pwa archive over an inkkit ByteReader.
//
// Everything streams from the SD card: the header, the title table (binary
// searched in place), the article directory, and the compressed article blocks
// (inflated one at a time into a single reusable buffer). No allocation grows
// with the number of articles. This is the device's canonical reader; it is
// exercised on the host against the same golden archive the companion writes.
//
// Arduino free: depends only on inkkit's ByteReader abstraction, which compiles
// on the host.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "inkkit/ByteStream.h"
#include "pocketwiki/RenderStream.h"

namespace pocketwiki {

struct PwaHeader {
  uint32_t articleCount = 0;
  uint32_t titleTableOffset = 0;
  uint32_t keyHeapOffset = 0;
  uint32_t dirOffset = 0;
  uint32_t titleHeapOffset = 0;
  uint32_t blockRegionOffset = 0;
  uint32_t maxArticleRaw = 0;
  uint32_t maxBlockRaw = 0;
  uint32_t indexBytes = 0;
  std::string collectionName;
};

struct DirEntry {
  std::string title;
  uint32_t blockIndexOffset = 0;
  uint16_t blockCount = 0;
  uint32_t rawSize = 0;
};

struct SearchHit {
  uint32_t articleId = 0;
  std::string title;
};

class PwaReader {
 public:
  explicit PwaReader(inkkit::ByteReader& src) : src_(src) {}

  // Read and validate the header. Returns false on a bad magic or version.
  bool open();

  const PwaHeader& header() const { return header_; }
  uint32_t articleCount() const { return header_.articleCount; }

  // Read the directory entry for an article id. Returns false if out of range.
  bool dirEntry(uint32_t articleId, DirEntry& out);

  // Prefix search over the fold key table. Calls sink(articleId, title) for up
  // to `limit` matches, in title order. Returns the number of hits.
  size_t search(const std::string& query, size_t limit,
                const std::function<void(uint32_t, const std::string&)>& sink);

  // Convenience wrapper collecting hits into a vector.
  std::vector<SearchHit> search(const std::string& query, size_t limit = 25);

  // Decode an article's render stream into `sink`. Returns false on a read or
  // decode error. Streams block by block through a single reusable buffer.
  bool readArticle(uint32_t articleId, RenderSink& sink);

 private:
  bool readAt(uint32_t offset, void* dst, size_t n);
  bool readU16At(uint32_t offset, uint16_t& out);
  bool readU32At(uint32_t offset, uint32_t& out);
  // Read the fold key for title-table record i into `key`.
  bool recordKey(uint32_t i, std::string& key, uint32_t& articleId);

  inkkit::ByteReader& src_;
  PwaHeader header_;
  std::vector<uint8_t> rawBuf_;   // reusable inflate output (sized to maxBlockRaw)
  std::vector<uint8_t> compBuf_;  // reusable compressed input
};

// A sink that concatenates all text (ignoring styling and structure). Handy for
// tests and for building a quick article preview.
struct TextCollector : RenderSink {
  std::string text;
  void onText(const char* utf8, size_t len, uint8_t) override { text.append(utf8, len); }
  void onLink(uint32_t, const char* label, size_t len) override { text.append(label, len); }
};

}  // namespace pocketwiki
