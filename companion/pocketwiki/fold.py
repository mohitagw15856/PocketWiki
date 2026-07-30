"""Fold key normalisation.

The fold key makes title search case and accent insensitive without shipping
Unicode tables to the device. This module MUST stay byte for byte compatible
with ``core/pocketwiki/Fold.h``; the two are exercised against the same cases in
each test suite.

Folding steps (see docs/ARCHIVE_FORMAT.md section 6):

1. Map a fixed set of Latin-1 and Latin Extended-A accented letters to their
   unaccented ASCII base.
2. Lower case ASCII A..Z.
3. Collapse whitespace runs to a single space and trim the ends.
4. Keep any other code point unchanged.
"""

# Accented code point -> ASCII base. Kept intentionally small and explicit so it
# is trivial to mirror in C++. Covers the common Western European letters that
# appear in encyclopaedic titles.
_ACCENT_MAP = {}


def _add(bases_and_variants):
    for base, variants in bases_and_variants:
        for ch in variants:
            _ACCENT_MAP[ord(ch)] = base


_add(
    [
        ("a", "àáâãäåāăą"),
        ("e", "èéêëēĕėęě"),
        ("i", "ìíîïĩīĭįı"),
        ("o", "òóôõöøōŏő"),
        ("u", "ùúûüũūŭůűų"),
        ("y", "ýÿ"),
        ("n", "ñńņň"),
        ("c", "çćĉċč"),
        ("s", "śŝşš"),
        ("z", "źżž"),
        ("g", "ĝğġģ"),
        ("l", "ĺļľł"),
        ("r", "ŕŗř"),
        ("t", "ţťŧ"),
        ("d", "ďđ"),
        ("A", "ÀÁÂÃÄÅĀĂĄ"),
        ("E", "ÈÉÊËĒĔĖĘĚ"),
        ("I", "ÌÍÎÏĨĪĬĮİ"),
        ("O", "ÒÓÔÕÖØŌŎŐ"),
        ("U", "ÙÚÛÜŨŪŬŮŰŲ"),
        ("Y", "ÝŸ"),
        ("N", "ÑŃŅŇ"),
        ("C", "ÇĆĈĊČ"),
        ("S", "ŚŜŞŠ"),
        ("Z", "ŹŻŽ"),
    ]
)


def fold_key(title: str) -> bytes:
    """Return the UTF-8 encoded fold key for ``title``."""
    out_chars = []
    prev_space = False
    for ch in title:
        cp = ord(ch)
        mapped = _ACCENT_MAP.get(cp)
        if mapped is not None:
            ch = mapped
            cp = ord(ch)
        if ch.isspace():
            # Collapse whitespace runs to a single ASCII space.
            if not prev_space:
                out_chars.append(" ")
                prev_space = True
            continue
        prev_space = False
        if 0x41 <= cp <= 0x5A:  # ASCII A..Z
            ch = chr(cp + 0x20)
        out_chars.append(ch)
    return "".join(out_chars).strip().encode("utf-8")
