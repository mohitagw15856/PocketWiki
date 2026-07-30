from pocketwiki.html_convert import html_to_blocks
from pocketwiki.render import Link, TextRun


def resolve(href):
    mapping = {"Newton.html": "newton", "Gravity.html": "gravity"}
    return mapping.get(href)


def test_headings_paragraphs_lists():
    html = "<h2>Title</h2><p>A <b>bold</b> word.</p><ul><li>one</li><li>two</li></ul>"
    blocks = html_to_blocks(html, resolve)
    kinds = [b.kind for b in blocks]
    assert kinds == ["heading", "paragraph", "list_item", "list_item"]
    assert blocks[0].level == 2
    bold = [n for n in blocks[1].inlines if isinstance(n, TextRun) and n.bold]
    assert bold and bold[0].text.strip() == "bold"


def test_internal_and_external_links():
    html = '<p>See <a href="Newton.html">Newton</a> and <a href="http://x">out</a>.</p>'
    blocks = html_to_blocks(html, resolve)
    inlines = blocks[0].inlines
    links = [n for n in inlines if isinstance(n, Link)]
    assert len(links) == 1
    assert links[0].target_slug == "newton"
    # external became plain text
    assert any(isinstance(n, TextRun) and "out" in n.text for n in inlines)


def test_skips_script_and_style():
    html = "<p>keep</p><script>var x=1;</script><style>.a{}</style>"
    blocks = html_to_blocks(html, resolve)
    assert len(blocks) == 1
    assert "keep" in "".join(n.text for n in blocks[0].inlines)


def test_hr():
    html = "<p>a</p><hr><p>b</p>"
    kinds = [b.kind for b in html_to_blocks(html, resolve)]
    assert kinds == ["paragraph", "rule", "paragraph"]
