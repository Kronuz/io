# Changelog

Notable changes to `io`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no releases, so
entries are dated.

## 2026-11-01

### Changed

- The README follows the family README standard: the description as its first sentence,
  Quick start before Installation, a pinned FetchContent tag, and "Not for" limits. It
  names CMake 3.16, the version `CMakeLists.txt` requires, where it said 3.14, and states
  that `pread` returns after one positional read.
- The README no longer embeds `assets/architecture.svg`; the diagram stays in
  `ARCHITECTURE.md`.
