"""A small Markdown subset parser.

Only the features the device renderer supports are recognised: level 1..3
headings, paragraphs, unordered and ordered lists, horizontal rules, bold and
italic emphasis, and internal links whose target is another article slug.
Anything else is treated as plain text. Parsing happens on the desktop side so
the device never sees Markdown.
"""

import re
from typing import List, Optional, Tuple

from .render import Block, Link, TextRun

_HEADING = re.compile(r"^(#{1,3})\s+(.*?)\s*#*\s*$")
_RULE = re.compile(r"^\s*([-*_])(?:\s*\1){2,}\s*$")
_ULIST = re.compile(r"^(\s*)[-*+]\s+(.*)$")
_OLIST = re.compile(r"^(\s*)\d+[.)]\s+(.*)$")
_LINK = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")

_FRONT_MATTER = re.compile(r"^---\s*\n(.*?)\n---\s*\n?", re.DOTALL)


def split_front_matter(text: str) -> Tuple[dict, str]:
    """Return (metadata, body). Front matter is a leading ``---`` fenced block of
    simple ``key: value`` lines."""
    meta: dict = {}
    m = _FRONT_MATTER.match(text)
    if not m:
        return meta, text
    for line in m.group(1).splitlines():
        if ":" in line:
            key, _, value = line.partition(":")
            meta[key.strip()] = value.strip()
    return meta, text[m.end():]


def _target_to_slug(target: str) -> str:
    """Normalise a link target into a slug. Article links point at ``slug.md``;
    anything else (an external URL) is returned unchanged so it fails to resolve
    and renders as plain text."""
    target = target.strip()
    if target.lower().endswith(".md"):
        target = target[:-3]
    # Drop any fragment or leading path.
    target = target.split("#", 1)[0]
    target = target.rsplit("/", 1)[-1]
    return target


def _parse_emphasis(text: str, bold: bool = False, italic: bool = False) -> List[TextRun]:
    """Turn a run of plain text with ``**bold**`` and ``*italic*`` markers into
    styled text runs. Backslash escapes a following marker character."""
    runs: List[TextRun] = []
    buf: List[str] = []
    i, n = 0, len(text)

    def flush():
        if buf:
            runs.append(TextRun("".join(buf), bold, italic))
            buf.clear()

    while i < n:
        c = text[i]
        if c == "\\" and i + 1 < n and text[i + 1] in "*_[]\\":
            buf.append(text[i + 1])
            i += 2
            continue
        if text.startswith("**", i) or text.startswith("__", i):
            flush()
            bold = not bold
            i += 2
            continue
        if c in "*_":
            flush()
            italic = not italic
            i += 1
            continue
        buf.append(c)
        i += 1
    flush()
    return runs


def parse_inlines(text: str):
    """Parse inline content into a list of TextRun and Link nodes."""
    nodes = []
    pos = 0
    for m in _LINK.finditer(text):
        if m.start() > pos:
            nodes.extend(_parse_emphasis(text[pos:m.start()]))
        label = m.group(1)
        slug = _target_to_slug(m.group(2))
        nodes.append(Link(target_slug=slug, text=label))
        pos = m.end()
    if pos < len(text):
        nodes.extend(_parse_emphasis(text[pos:]))
    # Drop empty runs.
    return [n for n in nodes if not (isinstance(n, TextRun) and not n.text.strip() and n.text == "")]


def parse_markdown(body: str) -> List[Block]:
    """Parse a Markdown body (front matter already removed) into blocks."""
    blocks: List[Block] = []
    lines = body.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    para: List[str] = []

    def flush_para():
        if para:
            text = " ".join(s.strip() for s in para).strip()
            para.clear()
            if text:
                blocks.append(Block(kind="paragraph", inlines=parse_inlines(text)))

    for raw in lines:
        line = raw.rstrip()
        if not line.strip():
            flush_para()
            continue
        if _RULE.match(line):
            flush_para()
            blocks.append(Block(kind="rule"))
            continue
        m = _HEADING.match(line)
        if m:
            flush_para()
            level = len(m.group(1))
            blocks.append(Block(kind="heading", level=level, inlines=parse_inlines(m.group(2))))
            continue
        m = _OLIST.match(line)
        if m:
            flush_para()
            depth = min(3, len(m.group(1)) // 2)
            blocks.append(
                Block(kind="list_item", depth=depth, ordered=True, inlines=parse_inlines(m.group(2)))
            )
            continue
        m = _ULIST.match(line)
        if m:
            flush_para()
            depth = min(3, len(m.group(1)) // 2)
            blocks.append(
                Block(kind="list_item", depth=depth, ordered=False, inlines=parse_inlines(m.group(2)))
            )
            continue
        para.append(line)

    flush_para()
    return blocks


def article_from_markdown(text: str, default_title: Optional[str] = None):
    """Return (title, blocks) for a full Markdown document with optional front
    matter. Falls back to the first heading or ``default_title`` for the title."""
    meta, body = split_front_matter(text)
    title = meta.get("title")
    blocks = parse_markdown(body)
    if not title:
        for b in blocks:
            if b.kind == "heading":
                title = "".join(n.text for n in b.inlines if isinstance(n, TextRun))
                break
    if not title:
        title = default_title or "Untitled"
    return title, blocks
