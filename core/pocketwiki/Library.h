// History and bookmark models, persisted to the SD card.
//
// The logic here is pure (parse/serialise strings and maintain bounded lists) so
// it is fully host-testable. The firmware persists the serialised strings with
// inkkit's Storage helpers under /pocketwiki/.state/; see the device layer.
//
// Serialised form is one record per line, tab separated, which is trivial to
// stream with inkkit::readLines and cheap to write.
//
// Arduino free.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace pocketwiki {

inline std::string sanitiseField(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\t' || c == '\n' || c == '\r') {
      out.push_back(' ');
    } else {
      out.push_back(c);
    }
  }
  return out;
}

// Recently opened articles, most recent first, deduplicated, bounded.
class HistoryStore {
 public:
  struct Entry {
    uint32_t articleId = 0;
    std::string title;
  };

  explicit HistoryStore(size_t maxEntries = 32) : maxEntries_(maxEntries) {}

  const std::vector<Entry>& entries() const { return entries_; }
  size_t size() const { return entries_.size(); }

  void add(uint32_t articleId, const std::string& title) {
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].articleId == articleId) {
        entries_.erase(entries_.begin() + i);
        break;
      }
    }
    entries_.insert(entries_.begin(), Entry{articleId, title});
    if (entries_.size() > maxEntries_) entries_.resize(maxEntries_);
  }

  void clear() { entries_.clear(); }

  std::string serialise() const {
    std::string out;
    for (const auto& e : entries_) {
      out += std::to_string(e.articleId);
      out.push_back('\t');
      out += sanitiseField(e.title);
      out.push_back('\n');
    }
    return out;
  }

  void parse(const std::string& text) {
    entries_.clear();
    size_t start = 0;
    while (start < text.size() && entries_.size() < maxEntries_) {
      size_t nl = text.find('\n', start);
      std::string line = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
      start = (nl == std::string::npos) ? text.size() : nl + 1;
      if (line.empty()) continue;
      size_t tab = line.find('\t');
      if (tab == std::string::npos) continue;
      Entry e;
      e.articleId = static_cast<uint32_t>(std::strtoul(line.substr(0, tab).c_str(), nullptr, 10));
      e.title = line.substr(tab + 1);
      entries_.push_back(e);
    }
  }

 private:
  size_t maxEntries_;
  std::vector<Entry> entries_;
};

// Bookmarks: an article plus a reading position (block index). Toggling adds or
// removes; the set is kept in insertion order.
class BookmarkStore {
 public:
  struct Bookmark {
    uint32_t articleId = 0;
    uint32_t position = 0;  // block index within the article
    std::string title;
  };

  const std::vector<Bookmark>& all() const { return marks_; }
  size_t size() const { return marks_.size(); }

  bool contains(uint32_t articleId) const {
    for (const auto& m : marks_) {
      if (m.articleId == articleId) return true;
    }
    return false;
  }

  void set(uint32_t articleId, uint32_t position, const std::string& title) {
    for (auto& m : marks_) {
      if (m.articleId == articleId) {
        m.position = position;
        m.title = title;
        return;
      }
    }
    marks_.push_back(Bookmark{articleId, position, title});
  }

  void remove(uint32_t articleId) {
    for (size_t i = 0; i < marks_.size(); ++i) {
      if (marks_[i].articleId == articleId) {
        marks_.erase(marks_.begin() + i);
        return;
      }
    }
  }

  // Returns true if a bookmark now exists after the toggle.
  bool toggle(uint32_t articleId, uint32_t position, const std::string& title) {
    if (contains(articleId)) {
      remove(articleId);
      return false;
    }
    set(articleId, position, title);
    return true;
  }

  std::string serialise() const {
    std::string out;
    for (const auto& m : marks_) {
      out += std::to_string(m.articleId);
      out.push_back('\t');
      out += std::to_string(m.position);
      out.push_back('\t');
      out += sanitiseField(m.title);
      out.push_back('\n');
    }
    return out;
  }

  void parse(const std::string& text) {
    marks_.clear();
    size_t start = 0;
    while (start < text.size()) {
      size_t nl = text.find('\n', start);
      std::string line = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
      start = (nl == std::string::npos) ? text.size() : nl + 1;
      if (line.empty()) continue;
      size_t t1 = line.find('\t');
      if (t1 == std::string::npos) continue;
      size_t t2 = line.find('\t', t1 + 1);
      if (t2 == std::string::npos) continue;
      Bookmark m;
      m.articleId = static_cast<uint32_t>(std::strtoul(line.substr(0, t1).c_str(), nullptr, 10));
      m.position = static_cast<uint32_t>(std::strtoul(line.substr(t1 + 1, t2 - t1 - 1).c_str(), nullptr, 10));
      m.title = line.substr(t2 + 1);
      marks_.push_back(m);
    }
  }

 private:
  std::vector<Bookmark> marks_;
};

}  // namespace pocketwiki
