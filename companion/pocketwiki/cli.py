"""Command line interface: ``pocketwiki build`` and ``pocketwiki inspect``."""

import argparse
import sys
from typing import List

from . import DEFAULT_BLOCK_RAW, DEFAULT_INDEX_BUDGET, __version__
from .archive import ArticleInput, PwaReader, build_archive


def _human(n: int) -> str:
    f = float(n)
    for unit in ("B", "KB", "MB", "GB"):
        if f < 1024.0 or unit == "GB":
            return f"{int(f)} {unit}" if unit == "B" else f"{f:.1f} {unit}"
        f /= 1024.0
    return f"{n} B"


def _collect_articles(args) -> List[ArticleInput]:
    from . import sources

    if args.source == "markdown":
        return sources.load_markdown_folder(args.input)
    if args.source == "wikipedia":
        with open(args.input, "r", encoding="utf-8") as fh:
            titles = [ln.strip() for ln in fh if ln.strip() and not ln.startswith("#")]
        return sources.fetch_wikipedia(titles, lang=args.lang)
    if args.source == "zim":
        titles = None
        if args.titles:
            with open(args.titles, "r", encoding="utf-8") as fh:
                titles = [ln.strip() for ln in fh if ln.strip()]
        return sources.load_zim_subset(args.input, limit=args.limit, titles=titles)
    raise ValueError(f"unknown source {args.source}")


def cmd_build(args) -> int:
    articles = _collect_articles(args)
    result = build_archive(
        articles,
        collection_name=args.name or "",
        max_block_raw=args.block_size,
        index_budget=args.index_budget,
    )
    with open(args.output, "wb") as fh:
        fh.write(result.data)

    print(f"Built {args.output}")
    print(f"  Articles:          {result.article_count}")
    print(f"  Archive size:      {_human(len(result.data))}")
    print(f"  Body (raw):        {_human(result.total_raw)}")
    print(f"  Body (compressed): {_human(result.total_comp)}")
    ratio = (result.total_comp / result.total_raw * 100) if result.total_raw else 0
    print(f"  Compression:       {ratio:.0f}% of raw")
    print(f"  Largest block raw: {_human(result.max_block_raw)} (device inflate buffer)")
    print(f"  Index size:        {_human(result.index_bytes)} of {_human(result.index_budget)} budget")
    if result.over_budget:
        print(
            "  WARNING: the title index exceeds the on device budget. Consider "
            "splitting this collection into several .pwa files or raising "
            "--index-budget only if you know the target device has the RAM.",
            file=sys.stderr,
        )
        if args.strict:
            return 2
    return 0


def cmd_inspect(args) -> int:
    with PwaReader.open(args.archive) as reader:
        h = reader.header
        print(f"Archive:      {args.archive}")
        print(f"Collection:   {h.collection_name or '(unnamed)'}")
        print(f"Articles:     {h.article_count}")
        print(f"Index size:   {_human(h.index_bytes)} of {_human(args.index_budget)} budget")
        print(f"Max block:    {_human(h.max_block_raw)} (device inflate buffer)")
        print(f"Max article:  {_human(h.max_article_raw)} decompressed")
        if h.index_bytes > args.index_budget:
            print("  WARNING: index exceeds the on device budget.", file=sys.stderr)
        if args.list:
            print("\nArticles (id: title):")
            for article_id, title in reader.titles():
                entry = reader.dir_entry(article_id)
                print(f"  {article_id:4d}: {title}  [{entry.block_count} block(s), {_human(entry.raw_size)}]")
        if args.search:
            print(f"\nSearch '{args.search}':")
            for article_id, title in reader.search(args.search, limit=args.limit):
                print(f"  {article_id:4d}: {title}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="pocketwiki", description="Build and inspect PocketWiki archives")
    parser.add_argument("--version", action="version", version=f"pocketwiki {__version__}")
    sub = parser.add_subparsers(dest="command", required=True)

    b = sub.add_parser("build", help="build a .pwa archive")
    b.add_argument("source", choices=["markdown", "wikipedia", "zim"], help="input source type")
    b.add_argument("input", help="folder of Markdown, a titles list, or a .zim file")
    b.add_argument("-o", "--output", required=True, help="output .pwa path")
    b.add_argument("-n", "--name", default="", help="collection name (max 24 bytes)")
    b.add_argument("--lang", default="en", help="Wikipedia language code (wikipedia source)")
    b.add_argument("--titles", help="optional titles list to select from a ZIM file")
    b.add_argument("--limit", type=int, help="max articles to take from a ZIM file")
    b.add_argument("--block-size", type=int, default=DEFAULT_BLOCK_RAW, help="max decompressed block size")
    b.add_argument("--index-budget", type=int, default=DEFAULT_INDEX_BUDGET, help="index memory budget in bytes")
    b.add_argument("--strict", action="store_true", help="fail the build if the index exceeds the budget")
    b.set_defaults(func=cmd_build)

    i = sub.add_parser("inspect", help="list contents and size budget of an archive")
    i.add_argument("archive", help="path to a .pwa archive")
    i.add_argument("--list", action="store_true", help="list every article")
    i.add_argument("--search", help="run a prefix search and show matches")
    i.add_argument("--limit", type=int, default=25, help="max search results")
    i.add_argument("--index-budget", type=int, default=DEFAULT_INDEX_BUDGET, help="index budget for the warning")
    i.set_defaults(func=cmd_inspect)

    return parser


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except (ValueError, RuntimeError, FileNotFoundError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
