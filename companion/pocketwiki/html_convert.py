"""Convert a small HTML subset into the render block model.

Used by the ZIM source (and available to any source that has HTML). Only the
tags the device can render are kept; everything else becomes plain text or is
dropped. Links are handed to a ``resolve_href`` callback that returns a target
slug (for an internal article) or ``None`` (render the label as plain text).
"""

from html.parser import HTMLParser
from typing import Callable, List, Optional

from .render import Block, Link, TextRun

_BLOCK_TAGS = {"p", "h1", "h2", "h3", "ul", "ol", "li", "hr", "div"}
_SKIP_CONTENT = {"script", "style", "table", "sup", "sub", "figure", "figcaption"}


class _Converter(HTMLParser):
    def __init__(self, resolve_href: Callable[[str], Optional[str]]):
        super().__init__(convert_charrefs=True)
        self.resolve_href = resolve_href
        self.blocks: List[Block] = []
        self._cur: Optional[Block] = None
        self._bold = 0
        self._italic = 0
        self._skip_depth = 0
        self._list_depth = -1
        self._ordered = [False]
        self._link_href: Optional[str] = None
        self._link_text: List[str] = []

    def _open_block(self, block: Block):
        self._flush_block()
        self._cur = block

    def _flush_block(self):
        if self._cur is not None:
            if self._cur.kind == "rule" or any(
                isinstance(n, (TextRun, Link)) and (getattr(n, "text", "").strip())
                for n in self._cur.inlines
            ):
                self.blocks.append(self._cur)
            self._cur = None

    def _append_text(self, text: str):
        if self._skip_depth or not text:
            return
        if self._link_href is not None:
            self._link_text.append(text)
            return
        if self._cur is None:
            self._cur = Block(kind="paragraph")
        self._cur.inlines.append(TextRun(text, bold=self._bold > 0, italic=self._italic > 0))

    def handle_starttag(self, tag, attrs):
        if tag in _SKIP_CONTENT:
            self._skip_depth += 1
            return
        if self._skip_depth:
            return
        if tag in ("b", "strong"):
            self._bold += 1
        elif tag in ("i", "em"):
            self._italic += 1
        elif tag in ("h1", "h2", "h3"):
            self._open_block(Block(kind="heading", level=int(tag[1])))
        elif tag == "p" or tag == "div":
            self._open_block(Block(kind="paragraph"))
        elif tag in ("ul", "ol"):
            self._list_depth += 1
            if len(self._ordered) <= self._list_depth:
                self._ordered.append(tag == "ol")
            else:
                self._ordered[self._list_depth] = tag == "ol"
        elif tag == "li":
            depth = max(0, self._list_depth)
            self._open_block(
                Block(kind="list_item", depth=min(3, depth), ordered=self._ordered[min(depth, len(self._ordered) - 1)])
            )
        elif tag == "hr":
            self._open_block(Block(kind="rule"))
            self._flush_block()
        elif tag == "a":
            href = dict(attrs).get("href", "")
            self._link_href = href
            self._link_text = []
        elif tag == "br":
            self._append_text(" ")

    def handle_endtag(self, tag):
        if tag in _SKIP_CONTENT:
            if self._skip_depth:
                self._skip_depth -= 1
            return
        if self._skip_depth:
            return
        if tag in ("b", "strong"):
            self._bold = max(0, self._bold - 1)
        elif tag in ("i", "em"):
            self._italic = max(0, self._italic - 1)
        elif tag in ("h1", "h2", "h3", "p", "div", "li"):
            self._flush_block()
        elif tag in ("ul", "ol"):
            self._list_depth -= 1
        elif tag == "a":
            label = "".join(self._link_text).strip()
            href = self._link_href
            self._link_href = None
            self._link_text = []
            if not label:
                return
            slug = self.resolve_href(href) if href else None
            if self._cur is None:
                self._cur = Block(kind="paragraph")
            if slug:
                self._cur.inlines.append(Link(target_slug=slug, text=label))
            else:
                self._cur.inlines.append(TextRun(label, bold=self._bold > 0, italic=self._italic > 0))

    def handle_data(self, data):
        # Collapse internal whitespace but keep single spaces between words.
        if not data.strip() and self._cur is None and self._link_href is None:
            return
        self._append_text(" ".join(data.split()) + (" " if data.endswith((" ", "\n", "\t")) else ""))

    def finish(self) -> List[Block]:
        self._flush_block()
        return self.blocks


def html_to_blocks(html: str, resolve_href: Callable[[str], Optional[str]]) -> List[Block]:
    conv = _Converter(resolve_href)
    conv.feed(html)
    return conv.finish()
