# puff

`puff.c` and `puff.h` are the reference minimal DEFLATE (RFC 1951) decompressor
by Mark Adler, vendored unmodified from the zlib distribution
(`contrib/puff`, version 2.3).

PocketWiki uses puff to inflate one compressed article block at a time into a
single reusable buffer. It is small (a few kilobytes of code), allocates only a
couple of kilobytes on the stack, and needs no heap, which suits the ESP32-C3.

## Licence

puff is distributed under the zlib licence (see the header of `puff.h`), which
is compatible with PocketWiki's MIT licence. The files are unmodified; per the
zlib licence terms this notice records their origin and that they have not been
altered.

Source: https://github.com/madler/zlib/tree/master/contrib/puff
