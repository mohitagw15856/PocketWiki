"""Read and write ``.pwa`` archives.

The writer takes a collection of articles (slug, title, block model), resolves
internal links, tokenises and block compresses each body, and lays out the file
exactly as docs/ARCHIVE_FORMAT.md describes. The reader is used by
``pocketwiki inspect`` and by the test suite; the firmware has an independent C++
reader, and the two are cross checked against a shared golden archive.
"""

import struct
import zlib
from dataclasses import dataclass, field
from typing import BinaryIO, Callable, Dict, List, Optional, Tuple

from . import DEFAULT_BLOCK_RAW, DEFAULT_INDEX_BUDGET, FORMAT_VERSION, MAGIC
from .fold import fold_key
from .render import (
    Block,
    TOK_END,
    TOK_HEADING,
    TOK_LINK,
    TOK_LIST_ITEM,
    TOK_PARAGRAPH,
    TOK_RULE,
    TOK_STYLE,
    TOK_TEXT,
    token_chunks,
)

HEADER_SIZE = 72
TITLE_RECORD_SIZE = 10
DIR_ENTRY_SIZE = 18
BLOCK_RECORD_SIZE = 12


@dataclass
class ArticleInput:
    slug: str
    title: str
    blocks: List[Block] = field(default_factory=list)


@dataclass
class BuildResult:
    data: bytes
    article_count: int
    index_bytes: int
    max_article_raw: int
    max_block_raw: int
    total_raw: int
    total_comp: int
    index_budget: int

    @property
    def over_budget(self) -> bool:
        return self.index_bytes > self.index_budget


def _pack_blocks(chunks: List[bytes], max_block_raw: int) -> List[bytes]:
    """Greedily pack whole token chunks into raw blocks no larger than
    ``max_block_raw`` bytes."""
    blocks: List[bytes] = []
    current = bytearray()
    for chunk in chunks:
        if len(chunk) > max_block_raw:
            raise ValueError(
                f"token of {len(chunk)} bytes exceeds block budget {max_block_raw}"
            )
        if current and len(current) + len(chunk) > max_block_raw:
            blocks.append(bytes(current))
            current = bytearray()
        current += chunk
    if current or not blocks:
        blocks.append(bytes(current))
    return blocks


def build_archive(
    articles: List[ArticleInput],
    collection_name: str = "",
    max_block_raw: int = DEFAULT_BLOCK_RAW,
    index_budget: int = DEFAULT_INDEX_BUDGET,
    compress_level: int = 9,
) -> BuildResult:
    """Build a ``.pwa`` archive from ``articles`` and return the bytes plus
    statistics."""
    if not articles:
        raise ValueError("cannot build an archive with no articles")

    # Deterministic order: by fold key, then title, then slug. articleId is the
    # position in this order, so the directory is title sorted too.
    ordered = sorted(articles, key=lambda a: (fold_key(a.title), a.title, a.slug))
    slug_to_id: Dict[str, int] = {}
    for i, art in enumerate(ordered):
        if art.slug in slug_to_id:
            raise ValueError(f"duplicate slug: {art.slug}")
        slug_to_id[art.slug] = i

    def resolve_link(slug: str) -> Optional[int]:
        return slug_to_id.get(slug)

    # Compress every article body up front so we know all sizes.
    per_article_blocks: List[List[Tuple[int, bytes]]] = []  # (rawLen, compData)
    max_article_raw = 0
    max_seen_block_raw = 0
    total_raw = 0
    total_comp = 0
    text_run_limit = max(16, max_block_raw - 8)
    for art in ordered:
        chunks = token_chunks(art.blocks, resolve_link, max_text_run=text_run_limit)
        raw_blocks = _pack_blocks(chunks, max_block_raw)
        recs: List[Tuple[int, bytes]] = []
        art_raw = 0
        for rb in raw_blocks:
            comp = _deflate(rb, compress_level)
            recs.append((len(rb), comp))
            art_raw += len(rb)
            max_seen_block_raw = max(max_seen_block_raw, len(rb))
            total_comp += len(comp)
        per_article_blocks.append(recs)
        max_article_raw = max(max_article_raw, art_raw)
        total_raw += art_raw

    n = len(ordered)

    # Region sizes.
    title_table_offset = HEADER_SIZE
    title_table_size = TITLE_RECORD_SIZE * n
    key_heap_offset = title_table_offset + title_table_size
    key_bytes = [fold_key(a.title) for a in ordered]
    key_heap_size = sum(len(k) for k in key_bytes)
    dir_offset = key_heap_offset + key_heap_size
    dir_size = DIR_ENTRY_SIZE * n
    title_heap_offset = dir_offset + dir_size
    title_bytes = [a.title.encode("utf-8") for a in ordered]
    title_heap_size = sum(len(t) for t in title_bytes)
    block_region_offset = title_heap_offset + title_heap_size

    # Key heap and offsets (absolute).
    key_heap = bytearray()
    key_offsets = []
    for k in key_bytes:
        key_offsets.append(key_heap_offset + len(key_heap))
        key_heap += k

    # Title heap and offsets (absolute).
    title_heap = bytearray()
    title_offsets = []
    for t in title_bytes:
        title_offsets.append(title_heap_offset + len(title_heap))
        title_heap += t

    # Block region: per article index table followed by its compressed blocks.
    block_region = bytearray()
    block_index_offsets = []  # absolute, per article
    for recs in per_article_blocks:
        index_offset = block_region_offset + len(block_region)
        block_index_offsets.append(index_offset)
        index_table_size = BLOCK_RECORD_SIZE * len(recs)
        payload_cursor = index_offset + index_table_size
        index_table = bytearray()
        payload = bytearray()
        for raw_len, comp in recs:
            comp_offset = payload_cursor + len(payload)
            index_table += struct.pack("<III", comp_offset, len(comp), raw_len)
            payload += comp
        block_region += index_table
        block_region += payload

    # Title table sorted by fold key (ordered already is sorted by fold key).
    title_table = bytearray()
    for i in range(n):
        title_table += struct.pack("<IHI", key_offsets[i], len(key_bytes[i]), i)

    # Directory indexed by articleId.
    directory = bytearray()
    for i in range(n):
        directory += struct.pack(
            "<IHIHIH",
            title_offsets[i],
            len(title_bytes[i]),
            block_index_offsets[i],
            len(per_article_blocks[i]),
            sum(r[0] for r in per_article_blocks[i]),
            0,
        )

    index_bytes = title_table_size + key_heap_size

    header = bytearray(HEADER_SIZE)
    struct.pack_into(
        "<4sHHIIIIIIIII24sI",
        header,
        0,
        MAGIC,
        FORMAT_VERSION,
        0,
        n,
        title_table_offset,
        key_heap_offset,
        dir_offset,
        title_heap_offset,
        block_region_offset,
        max_article_raw,
        max_seen_block_raw,
        index_bytes,
        collection_name.encode("utf-8")[:24].ljust(24, b"\x00"),
        0,
    )

    data = bytes(header) + bytes(title_table) + bytes(key_heap) + bytes(directory) + bytes(title_heap) + bytes(block_region)

    return BuildResult(
        data=data,
        article_count=n,
        index_bytes=index_bytes,
        max_article_raw=max_article_raw,
        max_block_raw=max_seen_block_raw,
        total_raw=total_raw,
        total_comp=total_comp,
        index_budget=index_budget,
    )


def _deflate(data: bytes, level: int) -> bytes:
    comp = zlib.compressobj(level, zlib.DEFLATED, -15)
    out = comp.compress(data) + comp.flush()
    return out


def _inflate(data: bytes, raw_len: int) -> bytes:
    out = zlib.decompress(data, -15)
    if len(out) != raw_len:
        raise ValueError(f"inflate size mismatch: got {len(out)}, expected {raw_len}")
    return out


# ---------------------------------------------------------------------------
# Reader (used by inspect and tests).
# ---------------------------------------------------------------------------


@dataclass
class Header:
    article_count: int
    title_table_offset: int
    key_heap_offset: int
    dir_offset: int
    title_heap_offset: int
    block_region_offset: int
    max_article_raw: int
    max_block_raw: int
    index_bytes: int
    collection_name: str


@dataclass
class DirEntry:
    title: str
    block_index_offset: int
    block_count: int
    raw_size: int


class PwaReader:
    """A streaming reader over a ``.pwa`` archive."""

    def __init__(self, stream: BinaryIO):
        self.f = stream
        self.header = self._read_header()

    @classmethod
    def open(cls, path: str) -> "PwaReader":
        return cls(open(path, "rb"))

    def close(self):
        self.f.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _read_at(self, offset: int, length: int) -> bytes:
        self.f.seek(offset)
        return self.f.read(length)

    def _read_header(self) -> Header:
        raw = self._read_at(0, HEADER_SIZE)
        if len(raw) < HEADER_SIZE:
            raise ValueError("not a PocketWiki archive (file too small)")
        (
            magic,
            version,
            _flags,
            count,
            tt_off,
            key_off,
            dir_off,
            title_off,
            block_off,
            max_article,
            max_block,
            index_bytes,
            name,
            _reserved,
        ) = struct.unpack("<4sHHIIIIIIIII24sI", raw)
        if magic != MAGIC:
            raise ValueError("not a PocketWiki archive (bad magic)")
        if version != FORMAT_VERSION:
            raise ValueError(f"unsupported format version {version}")
        return Header(
            article_count=count,
            title_table_offset=tt_off,
            key_heap_offset=key_off,
            dir_offset=dir_off,
            title_heap_offset=title_off,
            block_region_offset=block_off,
            max_article_raw=max_article,
            max_block_raw=max_block,
            index_bytes=index_bytes,
            collection_name=name.rstrip(b"\x00").decode("utf-8", "replace"),
        )

    def dir_entry(self, article_id: int) -> DirEntry:
        if not 0 <= article_id < self.header.article_count:
            raise IndexError(article_id)
        off = self.header.dir_offset + article_id * DIR_ENTRY_SIZE
        title_off, title_len, block_index_off, block_count, raw_size, _res = struct.unpack(
            "<IHIHIH", self._read_at(off, DIR_ENTRY_SIZE)
        )
        title = self._read_at(title_off, title_len).decode("utf-8", "replace")
        return DirEntry(
            title=title,
            block_index_offset=block_index_off,
            block_count=block_count,
            raw_size=raw_size,
        )

    def _title_record(self, i: int) -> Tuple[bytes, int]:
        off = self.header.title_table_offset + i * TITLE_RECORD_SIZE
        key_off, key_len, article_id = struct.unpack("<IHI", self._read_at(off, TITLE_RECORD_SIZE))
        key = self._read_at(key_off, key_len)
        return key, article_id

    def search(self, query: str, limit: int = 25) -> List[Tuple[int, str]]:
        """Prefix search over fold keys. Returns (articleId, displayTitle)."""
        prefix = fold_key(query)
        n = self.header.article_count
        # Binary search for the first key >= prefix.
        lo, hi = 0, n
        while lo < hi:
            mid = (lo + hi) // 2
            key, _ = self._title_record(mid)
            if key < prefix:
                lo = mid + 1
            else:
                hi = mid
        results = []
        i = lo
        while i < n and len(results) < limit:
            key, article_id = self._title_record(i)
            if not key.startswith(prefix):
                break
            results.append((article_id, self.dir_entry(article_id).title))
            i += 1
        return results

    def read_render_stream(self, article_id: int) -> bytes:
        """Return the concatenated decompressed render stream for an article."""
        entry = self.dir_entry(article_id)
        out = bytearray()
        for b in range(entry.block_count):
            rec_off = entry.block_index_offset + b * BLOCK_RECORD_SIZE
            comp_off, comp_len, raw_len = struct.unpack("<III", self._read_at(rec_off, BLOCK_RECORD_SIZE))
            comp = self._read_at(comp_off, comp_len)
            out += _inflate(comp, raw_len)
        return bytes(out)

    def iter_tokens(self, article_id: int):
        """Decode the render stream into (token_name, payload) tuples."""
        data = self.read_render_stream(article_id)
        return decode_tokens(data)

    def titles(self):
        for i in range(self.header.article_count):
            yield i, self.dir_entry(i).title


def decode_tokens(data: bytes):
    """Decode a render stream into a list of (name, payload) tuples. Used by the
    inspector and tests; mirrors the C++ decoder."""
    tokens = []
    i, n = 0, len(data)
    while i < n:
        op = data[i]
        i += 1
        if op == TOK_END:
            tokens.append(("END", None))
            break
        elif op == TOK_PARAGRAPH:
            tokens.append(("PARAGRAPH", None))
        elif op == TOK_HEADING:
            tokens.append(("HEADING", {"level": data[i]}))
            i += 1
        elif op == TOK_LIST_ITEM:
            tokens.append(("LIST_ITEM", {"depth": data[i], "ordered": bool(data[i + 1])}))
            i += 2
        elif op == TOK_RULE:
            tokens.append(("RULE", None))
        elif op == TOK_TEXT:
            (length,) = struct.unpack_from("<H", data, i)
            i += 2
            text = data[i:i + length].decode("utf-8", "replace")
            i += length
            tokens.append(("TEXT", {"text": text}))
        elif op == TOK_STYLE:
            tokens.append(("STYLE", {"mask": data[i]}))
            i += 1
        elif op == TOK_LINK:
            (target,) = struct.unpack_from("<I", data, i)
            i += 4
            (length,) = struct.unpack_from("<H", data, i)
            i += 2
            text = data[i:i + length].decode("utf-8", "replace")
            i += length
            tokens.append(("LINK", {"target": target, "text": text}))
        else:
            raise ValueError(f"unknown token byte 0x{op:02x} at {i - 1}")
    return tokens
