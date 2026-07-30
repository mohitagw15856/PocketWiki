import os

from pocketwiki import sources
from pocketwiki.render import TextRun


def test_slugify():
    assert sources.slugify("Isaac Newton!") == "isaac-newton"
    assert sources.slugify("  The Solar System  ") == "the-solar-system"


def test_plaintext_to_blocks():
    text = "Intro paragraph.\n\n== History ==\n\nMore text here.\n"
    blocks = sources.plaintext_to_blocks(text)
    kinds = [b.kind for b in blocks]
    assert kinds == ["paragraph", "heading", "paragraph"]
    assert blocks[1].level == 1


def test_load_markdown_folder(tmp_path):
    (tmp_path / "isaac-newton.md").write_text(
        "---\ntitle: Isaac Newton\n---\n\nHe studied [Gravity](gravity.md).\n", encoding="utf-8"
    )
    (tmp_path / "gravity.md").write_text("---\ntitle: Gravity\n---\n\nA force.\n", encoding="utf-8")
    (tmp_path / "README.md").write_text("ignore me", encoding="utf-8")
    arts = sources.load_markdown_folder(str(tmp_path))
    slugs = sorted(a.slug for a in arts)
    assert slugs == ["gravity", "isaac-newton"]


class _FakeResponse:
    def __init__(self, payload):
        self._payload = payload

    def raise_for_status(self):
        pass

    def json(self):
        return self._payload


class _FakeSession:
    def __init__(self, payload):
        self._payload = payload
        self.calls = 0

    def get(self, url, params=None, timeout=None):
        self.calls += 1
        return _FakeResponse(self._payload)


def test_fetch_wikipedia_with_mock():
    payload = {
        "query": {
            "pages": {
                "1": {"title": "Isaac Newton", "extract": "Intro.\n\n== Life ==\n\nDetails."},
                "2": {"title": "Missing", "missing": ""},
            }
        }
    }
    session = _FakeSession(payload)
    arts = sources.fetch_wikipedia(["Isaac Newton", "Missing"], session=session)
    assert len(arts) == 1
    assert arts[0].title == "Isaac Newton"
    assert arts[0].slug == "isaac-newton"
    assert any(b.kind == "heading" for b in arts[0].blocks)
