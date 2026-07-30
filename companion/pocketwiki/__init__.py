"""PocketWiki companion: build and inspect .pwa reference archives.

The package is deliberately dependency light. Building from a folder of
Markdown files and inspecting an archive need only the Python standard library.
The Wikipedia source needs ``requests`` and the ZIM source needs ``libzim``;
both are optional extras imported lazily so the core commands work offline.
"""

__version__ = "1.0.0"

FORMAT_VERSION = 1
MAGIC = b"PWA1"

# Default safe on device index budget in bytes (title table + key heap).
DEFAULT_INDEX_BUDGET = 64 * 1024

# Default maximum decompressed size of a single block. The firmware allocates a
# single reusable buffer of this size, so it is a direct RAM cost on device.
DEFAULT_BLOCK_RAW = 4096
