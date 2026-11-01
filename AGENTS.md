# AGENTS

Orientation for anyone (human or agent) working on this library.

## What this is

An EINTR-safe POSIX file/socket layer extracted from Xapiand. One
compiled unit (`io.cc`) plus a header (`io.hh`). It is pure syscall-wrapping
policy: retry-on-`EINTR`, full-count read/write, safe fds, portable durability.
Read `ARCHITECTURE.md` before touching a wrapper.

## File map

```
io.hh        The io:: API + portable feature detection + the fault-injection /
             fd-check seams (with no-op defaults).
io.cc        The out-of-line ops (open/close/read/write/pread/pwrite/fallocate/
             check) -- the only compiled unit, and where logging is emitted.
io_trace.h   No-op L_* logging defaults (redirect via IO_TRACE_HEADER).
likely.h     Vendored likely()/unlikely().
test/test.cc      ctest: round-trip, offsets, durability, fallocate size,
                  socketpair send/recv, the ignore_eintr flag, ignored_errno.
test/pread_test.cc, test/pread_interpose.cc
                  ctest "io_pread": io::pread over a pread that returns a few bytes at a
                  time, is interrupted, or fails (the interposer replaces the C library's).
examples/bench.cc Runnable throughput benchmark (io:: vs raw ::syscall).
```

## Dependencies

**None**, standalone. The three host couplings (logging, fault injection, the fd
tracker) are optional compile-time header seams that default to no-ops. A host
only pulls in an errno-names / traceback library if it opts into real logging or
`IO_CHECK_FDES`, and then it is the host's own trace header that includes them.

## Conventions / invariants

- **Behavior from Xapiand.** The engine logic comes from Xapiand's in-tree `io.*`. Do not
  change retry conditions, short-count rules, the `open()` minimum-fd / `O_CLOEXEC`
  handling, or `pread`'s loop to the full count without a deliberate reason;
  `test/pread_test.cc` replaces the C library's `pread` with one that returns a few
  bytes at a time.
- **`close()` never retries on `EINTR`** (a retried close can hit a reused fd).
  Leave it that way.
- **Three seams, nothing else host-specific.** `IO_TRACE_HEADER` (logging),
  `IO_RANDOM_ERRORS_HEADER` (fault injection), `IO_CHECK_FDES` (fd tracker).
  Resist adding a direct dependency to a wrapper; extend a seam instead.
- **`error::`/`traceback::`/`DEBUG_COL` may appear only inside L_* macro
  arguments.** That is what lets the no-op logging default keep the standalone
  build dependency-free. If you reference one outside a L_* macro, you have added
  a hard dependency -- don't.
- **Portable feature detection, no `config.h`.** Add capabilities with an
  `IO_HAVE_*` guarded by a platform/feature macro, mirroring the table in
  `ARCHITECTURE.md`.

## Verification and benchmarks

The shared method is the catalog's
[PERFORMANCE.md](https://github.com/Kronuz/cpp-libraries/blob/main/docs/PERFORMANCE.md). From a
fresh clone:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure
```

- **Tests.** `io` (`test/test.cc`): write/read round trips, offsets, durability calls, `fallocate`
  size, `send`/`recv` over a socketpair, the `ignore_eintr` flag and `ignored_errno`. `io_pread`
  (`test/pread_test.cc`) runs `io::pread` over a real file through a `pread` that
  `test/pread_interpose.cc` puts in place of the C library's: 1, 7, 100 and 999 bytes per call, an
  offset inside the file, a buffer past its end, `EINTR` retried and not, `EIO` early and late.
- **Revert and mutant.** With the `io.cc` from before the pread loop
  (`git show 6524ce0^:io.cc > io.cc`), `io_pread` fails on the first short read; deleting
  `offset += c;` from `io::pread` fails its offset checks. Restore with `git checkout io.cc`. No
  mutation script, differential test, fuzz target or `docs/perf` note lives here.
- **CI.** `.github/workflows/ci.yml` calls the catalog's shared
  [`cmake.yml`](https://github.com/Kronuz/cpp-libraries/blob/main/.github/workflows/cmake.yml):
  Linux x86_64 (GCC, Clang) and arm64 (GCC), macOS arm64 and x86_64, and FreeBSD, at `-O2 -g` with
  the standard library's assertions on, plus ASan+UBSan and TSan jobs. The ASan+UBSan job, on Linux:

  ```sh
  CC=clang CXX=clang++ cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS" \
    -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
  cmake --build build-asan && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
  ```

- **Benchmark.** `io_bench` (`examples/bench.cc`) builds with the tests, with no option and
  nothing fetched. `./build/io_bench [MiB] [KiB]` (default 128 MiB in 64 KiB chunks) writes,
  `fsync`s and reads `/tmp/io_bench.dat` through `io::` and through the raw syscalls, its
  comparator, in MB/s; the write column includes the disk. To compare two commits, build it in both
  checkouts and run them A/B/A with [cpp-libraries](https://github.com/Kronuz/cpp-libraries)'
  `tools/ab_compare.py` (`read` for the other column):

  ```sh
  python3 ab_compare.py run --rounds 7 --higher-better --regex '(?P<row>io::|raw ::) .*write +(?P<value>[0-9.]+) MB/s' build-a/io_bench build-b/io_bench
  ```

## Host integration (how Xapiand uses it)

Xapiand does **not** link `libio.a`. It compiles `io.cc` into its own object list
with `-D IO_TRACE_HEADER=<xapiand_io_trace.h>` and
`-D IO_RANDOM_ERRORS_HEADER=<xapiand_io_random_errors.h>`, so `io.cc`'s logging
lands in Xapiand's real logger and every op becomes a fault-injection point under
`XAPIAND_RANDOM_ERRORS`. The header comes from this repo's include dir via
FetchContent. Linking the static target instead (with no-op logging) is the simple
path for a host that doesn't need either seam.
