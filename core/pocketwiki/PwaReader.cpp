#include "pocketwiki/PwaReader.h"

#include "pocketwiki/Fold.h"

extern "C" {
#include "puff.h"
}

namespace pocketwiki {

namespace {
constexpr uint32_t kHeaderSize = 72;
constexpr uint32_t kTitleRecordSize = 10;
constexpr uint32_t kDirEntrySize = 18;
constexpr uint32_t kBlockRecordSize = 12;
constexpr uint8_t kMagic[4] = {'P', 'W', 'A', '1'};
constexpr uint16_t kFormatVersion = 1;

uint16_t le16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(p[1]) << 8;
}
uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
}  // namespace

bool PwaReader::readAt(uint32_t offset, void* dst, size_t n) {
  if (!src_.seek(offset)) return false;
  return src_.read(dst, n) == n;
}

bool PwaReader::readU16At(uint32_t offset, uint16_t& out) {
  uint8_t b[2];
  if (!readAt(offset, b, 2)) return false;
  out = le16(b);
  return true;
}

bool PwaReader::readU32At(uint32_t offset, uint32_t& out) {
  uint8_t b[4];
  if (!readAt(offset, b, 4)) return false;
  out = le32(b);
  return true;
}

bool PwaReader::open() {
  uint8_t h[kHeaderSize];
  if (!readAt(0, h, kHeaderSize)) return false;
  for (int i = 0; i < 4; ++i) {
    if (h[i] != kMagic[i]) return false;
  }
  if (le16(h + 4) != kFormatVersion) return false;

  header_.articleCount = le32(h + 8);
  header_.titleTableOffset = le32(h + 12);
  header_.keyHeapOffset = le32(h + 16);
  header_.dirOffset = le32(h + 20);
  header_.titleHeapOffset = le32(h + 24);
  header_.blockRegionOffset = le32(h + 28);
  header_.maxArticleRaw = le32(h + 32);
  header_.maxBlockRaw = le32(h + 36);
  header_.indexBytes = le32(h + 40);
  const char* name = reinterpret_cast<const char*>(h + 44);
  size_t nameLen = 0;
  while (nameLen < 24 && name[nameLen] != '\0') ++nameLen;
  header_.collectionName.assign(name, nameLen);

  rawBuf_.assign(header_.maxBlockRaw ? header_.maxBlockRaw : 1, 0);
  return true;
}

bool PwaReader::dirEntry(uint32_t articleId, DirEntry& out) {
  if (articleId >= header_.articleCount) return false;
  const uint32_t off = header_.dirOffset + articleId * kDirEntrySize;
  uint8_t rec[kDirEntrySize];
  if (!readAt(off, rec, kDirEntrySize)) return false;
  const uint32_t titleOffset = le32(rec + 0);
  const uint16_t titleLen = le16(rec + 4);
  out.blockIndexOffset = le32(rec + 6);
  out.blockCount = le16(rec + 10);
  out.rawSize = le32(rec + 12);
  out.title.resize(titleLen);
  if (titleLen && !readAt(titleOffset, &out.title[0], titleLen)) return false;
  return true;
}

bool PwaReader::recordKey(uint32_t i, std::string& key, uint32_t& articleId) {
  const uint32_t off = header_.titleTableOffset + i * kTitleRecordSize;
  uint8_t rec[kTitleRecordSize];
  if (!readAt(off, rec, kTitleRecordSize)) return false;
  const uint32_t keyOffset = le32(rec + 0);
  const uint16_t keyLen = le16(rec + 4);
  articleId = le32(rec + 6);
  key.resize(keyLen);
  if (keyLen && !readAt(keyOffset, &key[0], keyLen)) return false;
  return true;
}

size_t PwaReader::search(const std::string& query, size_t limit,
                         const std::function<void(uint32_t, const std::string&)>& sink) {
  const std::string prefix = foldKey(query);
  const uint32_t n = header_.articleCount;

  // Lower bound: first record whose key >= prefix.
  uint32_t lo = 0, hi = n;
  std::string key;
  uint32_t id = 0;
  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2;
    if (!recordKey(mid, key, id)) return 0;
    if (key < prefix) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  size_t hits = 0;
  for (uint32_t i = lo; i < n && hits < limit; ++i) {
    if (!recordKey(i, key, id)) break;
    if (key.size() < prefix.size() || key.compare(0, prefix.size(), prefix) != 0) break;
    DirEntry entry;
    if (!dirEntry(id, entry)) break;
    sink(id, entry.title);
    ++hits;
  }
  return hits;
}

std::vector<SearchHit> PwaReader::search(const std::string& query, size_t limit) {
  std::vector<SearchHit> out;
  search(query, limit, [&out](uint32_t id, const std::string& title) {
    out.push_back(SearchHit{id, title});
  });
  return out;
}

bool PwaReader::readArticle(uint32_t articleId, RenderSink& sink) {
  DirEntry entry;
  if (!dirEntry(articleId, entry)) return false;

  RenderDecoder decoder(sink);
  for (uint16_t b = 0; b < entry.blockCount; ++b) {
    const uint32_t recOff = entry.blockIndexOffset + b * kBlockRecordSize;
    uint8_t rec[kBlockRecordSize];
    if (!readAt(recOff, rec, kBlockRecordSize)) return false;
    const uint32_t compOffset = le32(rec + 0);
    const uint32_t compLen = le32(rec + 4);
    const uint32_t rawLen = le32(rec + 8);
    if (rawLen > rawBuf_.size()) rawBuf_.resize(rawLen);
    if (compBuf_.size() < compLen) compBuf_.resize(compLen);
    if (!readAt(compOffset, compBuf_.data(), compLen)) return false;

    unsigned long destLen = rawLen;
    unsigned long srcLen = compLen;
    const int rc = puff(rawBuf_.data(), &destLen, compBuf_.data(), &srcLen);
    if (rc != 0 || destLen != rawLen) return false;

    if (!decoder.feed(rawBuf_.data(), rawLen)) return false;
    if (decoder.ended()) break;
  }
  return true;
}

}  // namespace pocketwiki
