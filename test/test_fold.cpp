#include "check.h"
#include "pocketwiki/Fold.h"

using pocketwiki::foldKey;

static void lowercases_ascii() { CHECK_STR_EQ(foldKey("Isaac Newton"), "isaac newton"); }

static void strips_accents() {
  CHECK_STR_EQ(foldKey("\xC3\x89" "douard"), "edouard");        // Édouard
  CHECK_STR_EQ(foldKey("Se\xC3\xB1or"), "senor");                // Señor
  CHECK_STR_EQ(foldKey("\xC3\x85ngstr\xC3\xB6m"), "angstrom");  // Ångström
}

static void collapses_whitespace() {
  CHECK_STR_EQ(foldKey("  The   Solar\tSystem  "), "the solar system");
}

static void non_latin_kept() {
  // Greek alpha survives; ASCII lower cased.
  std::string k = foldKey("Alpha \xCE\xB1");
  CHECK(k.rfind("alpha ", 0) == 0);
  CHECK(k.find("\xCE\xB1") != std::string::npos);
}

int main() {
  RUN(lowercases_ascii);
  RUN(strips_accents);
  RUN(collapses_whitespace);
  RUN(non_latin_kept);
  REPORT();
}
