import io

import pytest

from pocketwiki.archive import ArticleInput, PwaReader, build_archive
from pocketwiki.markdown import article_from_markdown
from pocketwiki.render import Block, Link, TextRun


def _article(slug, title, blocks):
    return ArticleInput(slug=slug, title=title, blocks=blocks)


def _sample_articles():
    a = _article(
        "isaac-newton",
        "Isaac Newton",
        [
            Block(kind="heading", level=2, inlines=[TextRun("Life")]),
            Block(
                kind="paragraph",
                inlines=[
                    TextRun("Newton described "),
                    Link(target_slug="theory-of-gravity", text="gravity"),
                    TextRun(" in his work."),
                ],
            ),
            Block(kind="list_item", depth=0, ordered=False, inlines=[TextRun("Optics")]),
        ],
    )
    b = _article(
        "theory-of-gravity",
        "Theory of Gravity",
        [Block(kind="paragraph", inlines=[TextRun("Bodies attract each other.", bold=True)])],
    )
    c = _article(
        "eclair-science",
        "Éclair Science",
        [Block(kind="paragraph", inlines=[TextRun("An accented title for folding.")])],
    )
    return [a, b, c]


def test_roundtrip_and_header():
    result = build_archive(_sample_articles(), collection_name="Test")
    reader = PwaReader(io.BytesIO(result.data))
    assert reader.header.article_count == 3
    assert reader.header.collection_name == "Test"
    titles = sorted(t for _, t in reader.titles())
    assert titles == ["Isaac Newton", "Theory of Gravity", "Éclair Science"]


def test_link_resolution():
    result = build_archive(_sample_articles())
    reader = PwaReader(io.BytesIO(result.data))
    # find Isaac Newton's id
    ids = {t: i for i, t in reader.titles()}
    gravity_id = ids["Theory of Gravity"]
    tokens = reader.iter_tokens(ids["Isaac Newton"])
    links = [t for t in tokens if t[0] == "LINK"]
    assert links and links[0][1]["target"] == gravity_id
    assert links[0][1]["text"] == "gravity"


def test_style_tokens():
    result = build_archive(_sample_articles())
    reader = PwaReader(io.BytesIO(result.data))
    ids = {t: i for i, t in reader.titles()}
    tokens = reader.iter_tokens(ids["Theory of Gravity"])
    assert ("STYLE", {"mask": 1}) in tokens  # bold


def test_prefix_search():
    result = build_archive(_sample_articles())
    reader = PwaReader(io.BytesIO(result.data))
    matches = reader.search("isa")
    assert any(t == "Isaac Newton" for _, t in matches)
    # accent-insensitive
    assert any(t == "Éclair Science" for _, t in reader.search("ecl"))
    # empty prefix returns everything up to limit
    assert len(reader.search("", limit=10)) == 3


def test_search_no_match():
    result = build_archive(_sample_articles())
    reader = PwaReader(io.BytesIO(result.data))
    assert reader.search("zzz") == []


def test_end_token_present():
    result = build_archive(_sample_articles())
    reader = PwaReader(io.BytesIO(result.data))
    ids = {t: i for i, t in reader.titles()}
    tokens = reader.iter_tokens(ids["Isaac Newton"])
    assert tokens[-1] == ("END", None)


def test_multi_block_article():
    # Force several blocks with a tiny block size and a long body.
    big = [Block(kind="paragraph", inlines=[TextRun("word " * 100)]) for _ in range(5)]
    art = _article("big", "Big Article", big)
    result = build_archive([art], max_block_raw=256)
    reader = PwaReader(io.BytesIO(result.data))
    entry = reader.dir_entry(0)
    assert entry.block_count > 1
    tokens = reader.iter_tokens(0)
    # All text preserved across block boundaries. Runs concatenate with no
    # separator, exactly as the device renderer joins them.
    text = "".join(t[1]["text"] for t in tokens if t[0] == "TEXT")
    assert text.count("word") == 500


def test_duplicate_slug_rejected():
    arts = [_article("dup", "One", []), _article("dup", "Two", [])]
    with pytest.raises(ValueError):
        build_archive(arts)


def test_index_budget_flag():
    result = build_archive(_sample_articles(), index_budget=1)
    assert result.over_budget is True
    result2 = build_archive(_sample_articles(), index_budget=1_000_000)
    assert result2.over_budget is False


def test_full_markdown_pipeline():
    md = "---\ntitle: Alpha\n---\n\n## Head\n\nText with a [Beta](beta.md) link.\n"
    title, blocks = article_from_markdown(md)
    a = ArticleInput(slug="alpha", title=title, blocks=blocks)
    b = ArticleInput(
        slug="beta",
        title="Beta",
        blocks=[Block(kind="paragraph", inlines=[TextRun("Beta body.")])],
    )
    result = build_archive([a, b])
    reader = PwaReader(io.BytesIO(result.data))
    ids = {t: i for i, t in reader.titles()}
    tokens = reader.iter_tokens(ids["Alpha"])
    link = [t for t in tokens if t[0] == "LINK"][0]
    assert link[1]["target"] == ids["Beta"]
