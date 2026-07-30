# PocketWiki Archive Format (`.pwa`)

This document specifies version 1 of the PocketWiki Archive format. An archive
is a single file, `something.pwa`, that lives under `/pocketwiki/` on the SD
card. The companion tool writes archives; the firmware only ever reads them.

The format is designed around three constraints of the Xteink X4/X3 hardware
(ESP32-C3, roughly 380 KB usable RAM):

1. **Everything streams from the SD card.** The device never loads a whole
   archive, a whole article, or the whole title index into RAM.
2. **The title index is small and seek friendly.** Title search is a binary
   search followed by a short forward scan, so the device only ever holds a few
   small records at a time.
3. **Article bodies are pre-tokenised.** The device never parses HTML or
   Markdown. The companion converts each article into a compact binary
   render stream (see below) and compresses it in bounded blocks.

All multi byte integers are **little endian**, matching the ESP32 and the wider
CrossPoint ecosystem. Offsets are absolute byte offsets from the start of the
file unless stated otherwise.

## 1. Overall layout

```
+------------------+  offset 0
| Header (72 B)    |
+------------------+
| Title table      |   articleCount x TitleRecord (10 B each), sorted by fold key
+------------------+
| Key heap         |   folded (lower-cased, ASCII folded) title bytes
+------------------+
| Article directory|   articleCount x ArticleEntry (18 B each), indexed by articleId
+------------------+
| Title heap       |   display title bytes
+------------------+
| Block region     |   per article: block index table + compressed blocks
+------------------+
```

The header stores the offset of each region, so a reader seeks directly to the
region it needs.

## 2. Header (72 bytes)

| Field | Type | Notes |
| --- | --- | --- |
| `magic` | `char[4]` | ASCII `PWA1` |
| `formatVersion` | `u16` | `1` |
| `flags` | `u16` | reserved, `0` |
| `articleCount` | `u32` | number of articles |
| `titleTableOffset` | `u32` | start of the title table |
| `keyHeapOffset` | `u32` | start of the fold key heap |
| `dirOffset` | `u32` | start of the article directory |
| `titleHeapOffset` | `u32` | start of the display title heap |
| `blockRegionOffset` | `u32` | start of the block region |
| `maxArticleRaw` | `u32` | largest single article, decompressed (sizing hint) |
| `maxBlockRaw` | `u32` | largest single block, decompressed (device block buffer) |
| `titleTableBytes` | `u32` | size of the title table plus key heap (index budget) |
| `collectionName` | `char[24]` | UTF-8, null padded |
| `reserved` | `u32` | `0` |

`maxBlockRaw` tells the firmware exactly how large its single reusable inflate
buffer must be. `titleTableBytes` is the number the companion checks against the
on device index budget (see section 7).

## 3. Title table and key heap

The title table drives search. It is an array of `articleCount` fixed size
records, **sorted ascending by the fold key bytes** (`memcmp` order):

```
TitleRecord (10 bytes):
  u32 keyOffset     // absolute offset into the key heap
  u16 keyLen        // length of the fold key in bytes
  u32 articleId     // index into the article directory
```

The **fold key** is the article title lower cased and ASCII folded (see section
6). Because the table is sorted, a prefix search is:

1. Binary search for the first record whose fold key is greater than or equal to
   the folded query.
2. Scan forward while the fold key starts with the folded query, collecting up
   to _k_ matches.

Each comparison seeks to the record, reads its `keyOffset`/`keyLen`, seeks to the
key heap and reads at most `keyLen` bytes. No allocation is proportional to the
archive size.

## 4. Article directory and title heap

The directory is an array of `articleCount` entries, indexed directly by
`articleId`:

```
ArticleEntry (18 bytes):
  u32 titleOffset       // absolute offset into the title heap
  u16 titleLen          // display title length in bytes
  u32 blockIndexOffset  // absolute offset of this article's block index
  u16 blockCount        // number of blocks
  u32 rawSize           // total decompressed render stream size
  u16 reserved
```

The **title heap** holds the display titles (original capitalisation, UTF-8),
referenced by `titleOffset`/`titleLen`.

## 5. Block region and render stream

Each article body is a **render stream** (section 5.1) split into blocks at token
boundaries. Every block is compressed independently with **raw DEFLATE**
(RFC 1951, no zlib or gzip wrapper), so the device inflates one block at a time
into a single reusable buffer of `maxBlockRaw` bytes.

An article's block index lives at `blockIndexOffset` and is an array of
`blockCount` records:

```
BlockRecord (12 bytes):
  u32 compOffset   // absolute offset of the compressed block
  u32 compLen      // compressed length in bytes
  u32 rawLen       // decompressed length in bytes (<= maxBlockRaw)
```

To read an article the firmware walks the block records in order, inflates each
into the shared buffer, and feeds the bytes to the render stream decoder. The
decoder is resumable across block boundaries because blocks are split only
between whole tokens.

### 5.1 Render stream tokens

The render stream is a flat sequence of tokens. Block level tokens open a new
visual block; inline tokens add content to the current block. Text is always
UTF-8. `u16` lengths are byte counts.

Block level tokens:

| Byte | Name | Operands | Meaning |
| --- | --- | --- | --- |
| `0x10` | `PARAGRAPH` | none | start a normal paragraph |
| `0x11` | `HEADING` | `u8 level` (1..3) | start a heading |
| `0x12` | `LIST_ITEM` | `u8 depth` (0..3), `u8 ordered` (0/1) | start a list item |
| `0x15` | `RULE` | none | horizontal rule, no inline content |

Inline tokens (valid inside a block):

| Byte | Name | Operands | Meaning |
| --- | --- | --- | --- |
| `0x01` | `TEXT` | `u16 len`, `len` bytes | UTF-8 text run in the current style |
| `0x02` | `STYLE` | `u8 mask` | set inline style: bit0 bold, bit1 italic |
| `0x04` | `LINK` | `u32 targetId`, `u16 len`, `len` bytes | internal link with visible label |
| `0x00` | `END` | none | end of the render stream |

Rules for a decoder:

- `STYLE` sets the absolute style for following `TEXT` runs until the next
  `STYLE` or the next block level token (which resets style to normal).
- `LINK` carries the resolved target `articleId` directly, so the device never
  resolves links at runtime. A link whose target was outside the collection is
  emitted at build time as plain `TEXT`, never as `LINK`.
- Any byte the decoder does not recognise is a hard error in a well formed
  archive; the firmware skips to the next block on error rather than crashing.

## 6. Fold key normalisation

The fold key makes search case and accent insensitive without any Unicode
tables on the device. The companion computes it and the firmware only ever
compares bytes, so the two must agree exactly. Folding is:

1. Decode the title as UTF-8 code points.
2. Map Latin-1 and Latin Extended-A accented letters to their unaccented ASCII
   base (for example `é`, `ü`, `ñ` become `e`, `u`, `n`). The mapping table is
   listed in `companion/pocketwiki/fold.py` and mirrored in
   `core/pocketwiki/Fold.h`.
3. Lower case ASCII `A`..`Z`.
4. Collapse any run of whitespace to a single space and trim the ends.
5. Re encode the surviving code points as UTF-8. Code points with no ASCII base
   are kept as is, so non Latin scripts still sort and match by their own bytes.

## 7. Index memory budget

`titleTableBytes` (title table plus key heap) is the figure the companion checks
against a safe on device budget, because it is the part a reader may want to scan
quickly. The default budget is **64 KB**. `pocketwiki build` and
`pocketwiki inspect` warn when an archive exceeds it and suggest splitting the
collection into several `.pwa` files. The budget is configurable with
`--index-budget`.

## 8. Versioning

The `formatVersion` field gates compatibility. A reader must refuse an archive
whose `formatVersion` it does not implement. Additive changes that keep the
region layout may reuse `flags` bits; any change to a record layout requires a
new `formatVersion` and a new magic suffix (`PWA2`, ...).
