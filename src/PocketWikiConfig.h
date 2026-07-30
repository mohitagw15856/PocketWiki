// Firmware wide configuration: SD paths, layout metrics and timings.
//
// Screen dimensions are read from the panel at runtime (inkkit Display), so only
// margins and fixed metrics live here.
#pragma once

#include <cstdint>

namespace pocketwiki {
namespace config {

// SD card layout.
constexpr const char* kArchiveDir = "/pocketwiki";
constexpr const char* kArchiveExt = ".pwa";
constexpr const char* kStateDir = "/pocketwiki/.state";
constexpr const char* kHistoryPath = "/pocketwiki/.state/history.tsv";
constexpr const char* kBookmarksPath = "/pocketwiki/.state/bookmarks.tsv";
constexpr const char* kLastArchivePath = "/pocketwiki/.state/last.txt";

// Short owner tag passed to inkkit Storage for logging.
constexpr const char* kStorageTag = "PWK";

// Layout metrics in pixels.
constexpr int kMarginX = 6;
constexpr int kHeaderHeight = 22;
constexpr int kFooterHeight = 16;
constexpr int kListRowHeight = 18;

// Search: cap live results so a keystroke never scans an unbounded list.
constexpr int kMaxSearchResults = 30;

// Idle sleep after this many milliseconds without input.
constexpr uint32_t kIdleSleepMs = 60UL * 1000UL;

// Power button hold to power off (milliseconds).
constexpr uint32_t kPowerOffHoldMs = 800;

}  // namespace config
}  // namespace pocketwiki
