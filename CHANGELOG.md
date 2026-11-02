# Changelog

Notable changes to `io`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no releases, so
entries are dated.

Upgrading a pin from before 2026-10-30: [the consumer upgrade note](https://github.com/Kronuz/cpp-libraries/blob/main/docs/UPGRADING-2026-10.md#io) lists what changes for callers, how to find affected inputs, the commits not to pin, and the speedup.

## 2026-11-01

### Fixed

- `io::pread` returned after one `::pread`, though `io.hh` documents that `read`, `write`,
  `pread` and `pwrite` loop to the full byte count. It now loops, advancing the offset by
  each count it gets back, until the buffer is full, a read returns 0 (the end of the file:
  the count stops there, `0` at or past it) or a read fails (the bytes read so far, `-1`
  when none). The commented-out loop that the single read replaced had no end-of-file test
  and did not advance the offset. `README.md`, `ARCHITECTURE.md` and `AGENTS.md`, which said
  `pread` reads once, say it fills the buffer. Callers: journal, storage, completion,
  residency and strata do not call `io::pread` (completion uses `::pread`; storage's
  `DefaultIO::pread` is its own single read). Xapiand's `Storage` policy forwards to it
  and reads headers and a block: it compares the count with the header size (a short count
  is an incomplete header) or accepts any count but `-1`, so a full-count read changes
  nothing for it except a second `pread` at the end of a file.
- `io.hh` said that every wrapper returns short only on an error. The reads also return
  short at the end of the file; the header says so.

### Tests

- `io_pread` (`test/pread_test.cc`, `test/pread_interpose.cc`) runs `io::pread` over a real
  file through a `pread` that replaces the C library's in the test executable: 1, 7, 100
  and 999 bytes per call, an offset in the middle of the file, a buffer longer than the
  file (11 calls, ends at 1000 bytes, no loop on the 0), an offset at and past the end,
  `EINTR` retried and not retried, and `EIO` after some bytes and on the first call. With
  the old `io.cc` it fails on the first short read (`pread(fd, buf, 1000, 0)` returns 1,
  7, 100 or 999) and on every case after; it passes with the loop. A loop that does not
  advance the offset fails the offset checks.

### Changed

- The README follows the family README standard: the description as its first sentence,
  Quick start before Installation, a pinned FetchContent tag, and "Not for" limits. It
  names CMake 3.16, the version `CMakeLists.txt` requires, where it said 3.14, and states
  that `pread` returns after one positional read.
- The README no longer embeds `assets/architecture.svg`; the diagram stays in
  `ARCHITECTURE.md`.
