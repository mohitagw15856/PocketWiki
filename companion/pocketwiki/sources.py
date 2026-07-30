"""Article sources: a folder of Markdown files, Wikipedia titles, or a ZIM
subset. Each source returns a list of ``ArticleInput`` ready for the archive
writer."""

import os
import re
from typing import List, Optional

from .archive import ArticleInput
from .markdown import article_from_markdown
from .render import Block, TextRun

_SLUG_RE = re.compile(r"[^a-z0-9]+")


def slugify(text: str) -> str:
    slug = _SLUG_RE.sub("-", text.strip().lower()).strip("-")
    return slug or "article"


# ---------------------------------------------------------------------------
# Markdown folder
# ---------------------------------------------------------------------------


def load_markdown_folder(path: str) -> List[ArticleInput]:
    """Load every ``*.md`` file in ``path`` (non recursive) as an article. The
    slug is the file name without extension; ``README.md`` and files starting
    with ``_`` are skipped."""
    articles: List[ArticleInput] = []
    for name in sorted(os.listdir(path)):
        if not name.lower().endswith(".md"):
            continue
        if name.lower() == "readme.md" or name.startswith("_"):
            continue
        slug = name[:-3]
        with open(os.path.join(path, name), "r", encoding="utf-8") as fh:
            text = fh.read()
        title, blocks = article_from_markdown(text, default_title=slug.replace("-", " ").title())
        articles.append(ArticleInput(slug=slug, title=title, blocks=blocks))
    if not articles:
        raise ValueError(f"no Markdown articles found in {path}")
    return articles


# ---------------------------------------------------------------------------
# Wikipedia
# ---------------------------------------------------------------------------


def plaintext_to_blocks(text: str) -> List[Block]:
    """Structure a plain text extract (as returned by the Wikipedia extracts
    API) into blocks. Lines like ``== Heading ==`` become headings; blank line
    separated runs become paragraphs."""
    blocks: List[Block] = []
    para: List[str] = []

    def flush():
        if para:
            joined = " ".join(s.strip() for s in para).strip()
            para.clear()
            if joined:
                blocks.append(Block(kind="paragraph", inlines=[TextRun(joined)]))

    for line in text.replace("\r\n", "\n").split("\n"):
        stripped = line.strip()
        m = re.match(r"^(={2,6})\s*(.*?)\s*\1$", stripped)
        if m:
            flush()
            level = min(3, len(m.group(1)) - 1)
            blocks.append(Block(kind="heading", level=max(1, level), inlines=[TextRun(m.group(2))]))
        elif not stripped:
            flush()
        else:
            para.append(stripped)
    flush()
    return blocks


def fetch_wikipedia(
    titles: List[str],
    lang: str = "en",
    session=None,
) -> List[ArticleInput]:
    """Fetch plain text extracts for the given Wikipedia article titles.

    Requires ``requests``. Internal links are not synthesised for this source
    because the extracts API returns no link information; use the Markdown or
    ZIM source when cross linking matters. ``session`` is injectable for tests.
    """
    if session is None:
        try:
            import requests
        except ImportError as exc:  # pragma: no cover - exercised via message
            raise RuntimeError(
                "the Wikipedia source needs the 'requests' package: pip install requests"
            ) from exc
        session = requests.Session()
        session.headers.update({"User-Agent": "PocketWiki/1.0 (companion build tool)"})

    api = f"https://{lang}.wikipedia.org/w/api.php"
    articles: List[ArticleInput] = []
    # The API accepts up to 20 titles per request for extracts.
    for i in range(0, len(titles), 20):
        batch = titles[i:i + 20]
        params = {
            "action": "query",
            "format": "json",
            "prop": "extracts",
            "explaintext": 1,
            "exsectionformat": "wiki",
            "redirects": 1,
            "titles": "|".join(batch),
        }
        resp = session.get(api, params=params, timeout=30)
        resp.raise_for_status()
        pages = resp.json().get("query", {}).get("pages", {})
        for page in pages.values():
            if "missing" in page:
                continue
            title = page.get("title", "Untitled")
            extract = page.get("extract", "") or ""
            blocks = plaintext_to_blocks(extract)
            if not blocks:
                continue
            articles.append(ArticleInput(slug=slugify(title), title=title, blocks=blocks))
    if not articles:
        raise ValueError("no articles were fetched from Wikipedia")
    return articles


# ---------------------------------------------------------------------------
# ZIM subset
# ---------------------------------------------------------------------------


def load_zim_subset(
    path: str,
    limit: Optional[int] = None,
    titles: Optional[List[str]] = None,
) -> List[ArticleInput]:
    """Load a subset of articles from a Kiwix ZIM file.

    Requires ``libzim``. When ``titles`` is given only those entries are taken;
    otherwise up to ``limit`` article entries are read in order. Internal links
    that point at other included articles are preserved.
    """
    try:
        from libzim.reader import Archive  # type: ignore
    except ImportError as exc:  # pragma: no cover - exercised via message
        raise RuntimeError(
            "the ZIM source needs the 'libzim' package: pip install libzim"
        ) from exc

    from .html_convert import html_to_blocks

    archive = Archive(path)

    # First pass: decide which paths are included and map them to slugs.
    selected = []  # (path, title, html)
    if titles:
        for t in titles:
            try:
                entry = archive.get_entry_by_path(t)
            except KeyError:
                continue
            selected.append(entry)
    else:
        count = archive.all_entry_count
        taken = 0
        for i in range(count):
            if limit is not None and taken >= limit:
                break
            entry = archive._get_entry_by_id(i) if hasattr(archive, "_get_entry_by_id") else None
            if entry is None:
                break
            if entry.is_redirect:
                continue
            selected.append(entry)
            taken += 1

    path_to_slug = {}
    for entry in selected:
        path_to_slug[entry.path] = slugify(entry.title or entry.path)

    def resolve_href(href: str) -> Optional[str]:
        target = href.split("#", 1)[0]
        target = target.split("?", 1)[0]
        target = target.lstrip("./")
        if target.startswith("/"):
            target = target[1:]
        return path_to_slug.get(target)

    articles: List[ArticleInput] = []
    for entry in selected:
        item = entry.get_item()
        html = bytes(item.content).decode("utf-8", "replace")
        blocks = html_to_blocks(html, resolve_href)
        if not blocks:
            continue
        articles.append(
            ArticleInput(slug=path_to_slug[entry.path], title=entry.title or entry.path, blocks=blocks)
        )
    if not articles:
        raise ValueError("no articles were extracted from the ZIM file")
    return articles
