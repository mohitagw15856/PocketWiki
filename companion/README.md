# PocketWiki companion

A dependency light Python CLI that builds and inspects PocketWiki `.pwa`
archives. All of the heavy work (fetching, HTML stripping, tokenising and
compression) happens here on the desktop so the device only ever streams a
compact, pre indexed binary.

## Install

The core commands (building from Markdown and inspecting) need only the Python
standard library. Two sources have optional extras:

```bash
# From the companion/ directory
python -m pip install -e .            # core only
python -m pip install -e .[wikipedia] # adds requests for the Wikipedia source
python -m pip install -e .[zim]       # adds libzim for the ZIM source
python -m pip install -e .[dev]       # pytest and requests for development
```

You can also run it without installing, from this directory:

```bash
python -m pocketwiki ...
```

## Build an archive

```bash
# From a folder of Markdown files (internal links resolve between files)
pocketwiki build markdown sample_content -o foundations-of-science.pwa \
    -n "Foundations of Science"

# From a list of Wikipedia article titles (one per line)
pocketwiki build wikipedia titles.txt -o science.pwa --lang en

# From a subset of a Kiwix ZIM file
pocketwiki build zim wikipedia.zim -o subset.pwa --limit 500
```

Useful options:

- `-n, --name` sets the collection name shown on the device (max 24 bytes).
- `--block-size` sets the maximum decompressed block size, which is the device's
  reusable inflate buffer (default 4096 bytes).
- `--index-budget` sets the safe on device index budget (default 64 KB).
- `--strict` fails the build if the index exceeds the budget.

The build prints a size report and warns if the title index would exceed the
budget, suggesting you split a very large collection into several archives.

## Inspect an archive

```bash
pocketwiki inspect foundations-of-science.pwa --list
pocketwiki inspect foundations-of-science.pwa --search newton
```

## Markdown support

The Markdown parser recognises exactly what the device can render: level 1 to 3
headings, paragraphs, unordered and ordered lists, horizontal rules, bold and
italic, and internal links written as `[label](slug.md)` where `slug` is another
article's file name. Anything else is treated as plain text. Front matter is a
leading `---` fenced block; set `title:` there.

## Tests

```bash
python -m pytest -q
```

## Sample content

`sample_content/` holds about 50 original, public domain (CC0) articles about the
foundations of science, cross linked to each other. `sample/` holds the archive
built from them.
