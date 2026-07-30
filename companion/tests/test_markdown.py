from pocketwiki.markdown import (
    article_from_markdown,
    parse_inlines,
    parse_markdown,
    split_front_matter,
)
from pocketwiki.render import Block, Link, TextRun


def test_front_matter():
    meta, body = split_front_matter("---\ntitle: Hello World\n---\n\nBody text.\n")
    assert meta["title"] == "Hello World"
    assert body.strip() == "Body text."


def test_headings_and_paragraphs():
    blocks = parse_markdown("## Heading\n\nA paragraph that\nwraps two lines.\n")
    assert blocks[0].kind == "heading" and blocks[0].level == 2
    assert blocks[1].kind == "paragraph"
    text = "".join(n.text for n in blocks[1].inlines)
    assert text == "A paragraph that wraps two lines."


def test_lists():
    blocks = parse_markdown("- one\n- two\n\n1. first\n2. second\n")
    kinds = [(b.kind, b.ordered) for b in blocks]
    assert kinds == [
        ("list_item", False),
        ("list_item", False),
        ("list_item", True),
        ("list_item", True),
    ]


def test_rule():
    blocks = parse_markdown("Above\n\n---\n\nBelow\n")
    assert [b.kind for b in blocks] == ["paragraph", "rule", "paragraph"]


def test_emphasis():
    nodes = parse_inlines("plain **bold** and *italic* end")
    styles = [(n.text, n.bold, n.italic) for n in nodes if isinstance(n, TextRun)]
    assert ("bold", True, False) in styles
    assert ("italic", False, True) in styles


def test_links():
    nodes = parse_inlines("See [Isaac Newton](isaac-newton.md) now")
    links = [n for n in nodes if isinstance(n, Link)]
    assert links and links[0].target_slug == "isaac-newton"
    assert links[0].text == "Isaac Newton"


def test_external_link_slug_is_url():
    nodes = parse_inlines("[home](https://example.com/page)")
    links = [n for n in nodes if isinstance(n, Link)]
    # target keeps last path segment, which will not resolve to an article
    assert links[0].target_slug == "page"


def test_title_from_front_matter_or_heading():
    title, _ = article_from_markdown("---\ntitle: From Meta\n---\n\n## Other\n")
    assert title == "From Meta"
    title2, _ = article_from_markdown("## First Heading\n\nbody\n", default_title="x")
    assert title2 == "First Heading"
