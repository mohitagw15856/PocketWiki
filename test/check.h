// A tiny, dependency free test harness.
//
// PocketWiki's core tests are deliberately hermetic: they need no network fetch
// of a test framework, which keeps the CI build fast and reproducible. Each test
// file defines a set of TEST functions and lists them in RUN_ALL in its main.
#pragma once

#include <cstdio>
#include <cstring>
#include <string>

namespace pwtest {
inline int& failures() {
  static int f = 0;
  return f;
}
inline int& checks() {
  static int c = 0;
  return c;
}
}  // namespace pwtest

#define CHECK(cond)                                                       \
  do {                                                                    \
    ++pwtest::checks();                                                   \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "  FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
      ++pwtest::failures();                                              \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                    \
  do {                                                                    \
    ++pwtest::checks();                                                   \
    if (!((a) == (b))) {                                                  \
      std::fprintf(stderr, "  FAIL %s:%d: CHECK_EQ(%s, %s)\n", __FILE__, __LINE__, #a, #b); \
      ++pwtest::failures();                                              \
    }                                                                     \
  } while (0)

#define CHECK_STR_EQ(a, b)                                               \
  do {                                                                    \
    ++pwtest::checks();                                                   \
    std::string _va = (a);                                               \
    std::string _vb = (b);                                               \
    if (_va != _vb) {                                                     \
      std::fprintf(stderr, "  FAIL %s:%d: CHECK_STR_EQ: \"%s\" != \"%s\"\n", __FILE__, __LINE__, _va.c_str(), _vb.c_str()); \
      ++pwtest::failures();                                              \
    }                                                                     \
  } while (0)

#define RUN(fn)                          \
  do {                                   \
    std::fprintf(stderr, "- %s\n", #fn); \
    fn();                                \
  } while (0)

#define REPORT()                                                                       \
  do {                                                                                 \
    std::fprintf(stderr, "%d checks, %d failures\n", pwtest::checks(), pwtest::failures()); \
    return pwtest::failures() == 0 ? 0 : 1;                                            \
  } while (0)
