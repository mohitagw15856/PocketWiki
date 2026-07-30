"""Render stream model and serialiser.

An article is a list of blocks; each block carries inline content. The
serialiser turns that model into the tokenised byte stream described in
docs/ARCHIVE_FORMAT.md section 5.1. Link targets are stored as slugs here and
resolved to article ids by the archive writer, which knows the whole
collection.
"""

import struct
from dataclasses import dataclass, field
from typing import Callable, List, Optional, Union

# Block level tokens.
TOK_END = 0x00
TOK_PARAGRAPH = 0x10
TOK_HEADING = 0x11
TOK_LIST_ITEM = 0x12
TOK_RULE = 0x15

# Inline tokens.
TOK_TEXT = 0x01
TOK_STYLE = 0x02
TOK_LINK = 0x04

# Inline style bits.
STYLE_BOLD = 0x01
STYLE_ITALIC = 0x02

# A single UTF-8 text run is chunked to at most this many bytes so that no
# individual token can exceed a block buffer.
MAX_TEXT_RUN = 480


@dataclass
class TextRun:
    text: str
    bold: bool = False
    italic: bool = False

    @property
    def style_mask(self) -> int:
        mask = 0
        if self.bold:
            mask |= STYLE_BOLD
        if self.italic:
            mask |= STYLE_ITALIC
        return mask


@dataclass
class Link:
    target_slug: str
    text: str


Inline = Union[TextRun, Link]


@dataclass
class Block:
    kind: str  # "paragraph" | "heading" | "list_item" | "rule"
    inlines: List[Inline] = field(default_factory=list)
    level: int = 0  # heading level 1..3
    depth: int = 0  # list nesting depth 0..3
    ordered: bool = False


def _utf8_chunks(text: str, limit: int = MAX_TEXT_RUN) -> List[bytes]:
    """Split text into UTF-8 byte chunks no longer than ``limit`` bytes without
    splitting a multi byte code point."""
    data = text.encode("utf-8")
    if len(data) <= limit:
        return [data] if data else []
    chunks = []
    start = 0
    while start < len(data):
        end = min(start + limit, len(data))
        # Back off to a code point boundary (bytes 0x80..0xBF are continuations).
        while end < len(data) and (data[end] & 0xC0) == 0x80:
            end -= 1
        chunks.append(data[start:end])
        start = end
    return chunks


def token_chunks(
    blocks: List[Block],
    resolve_link: Callable[[str], Optional[int]],
    max_text_run: int = MAX_TEXT_RUN,
) -> List[bytes]:
    """Serialise ``blocks`` into a list of whole token byte chunks.

    ``resolve_link(slug)`` returns the target article id, or ``None`` when the
    target is outside the collection (in which case the link is emitted as plain
    text). Keeping tokens as separate chunks lets the archive writer pack them
    into blocks without ever splitting a token. ``max_text_run`` bounds a single
    text run so no token can exceed the caller's block budget.
    """
    run_limit = max(16, min(MAX_TEXT_RUN, max_text_run))
    chunks: List[bytes] = []
    for block in blocks:
        if block.kind == "rule":
            chunks.append(bytes([TOK_RULE]))
            continue
        if block.kind == "paragraph":
            chunks.append(bytes([TOK_PARAGRAPH]))
        elif block.kind == "heading":
            level = max(1, min(3, block.level))
            chunks.append(bytes([TOK_HEADING, level]))
        elif block.kind == "list_item":
            depth = max(0, min(3, block.depth))
            chunks.append(bytes([TOK_LIST_ITEM, depth, 1 if block.ordered else 0]))
        else:
            raise ValueError(f"unknown block kind: {block.kind}")

        current_style = 0
        for node in block.inlines:
            if isinstance(node, TextRun):
                if not node.text:
                    continue
                if node.style_mask != current_style:
                    current_style = node.style_mask
                    chunks.append(bytes([TOK_STYLE, current_style]))
                for part in _utf8_chunks(node.text, run_limit):
                    chunks.append(bytes([TOK_TEXT]) + struct.pack("<H", len(part)) + part)
            elif isinstance(node, Link):
                target = resolve_link(node.target_slug)
                if target is None:
                    # Unresolved link: render the label as plain text.
                    if current_style != 0:
                        current_style = 0
                        chunks.append(bytes([TOK_STYLE, 0]))
                    for part in _utf8_chunks(node.text, run_limit):
                        chunks.append(bytes([TOK_TEXT]) + struct.pack("<H", len(part)) + part)
                else:
                    label = node.text.encode("utf-8")[:run_limit]
                    # Keep label on a code point boundary.
                    while label and (label[-1] & 0xC0) == 0x80:
                        label = label[:-1]
                    chunks.append(
                        bytes([TOK_LINK])
                        + struct.pack("<I", target)
                        + struct.pack("<H", len(label))
                        + label
                    )
                    # A link does not carry style; reset the tracked style so the
                    # next run re-emits its STYLE if needed.
                    current_style = 0
            else:
                raise TypeError(f"unknown inline node: {node!r}")

    chunks.append(bytes([TOK_END]))
    return chunks
