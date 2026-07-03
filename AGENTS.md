# AGENTS

Orientation for anyone (human or agent) working on this library.

## What this is

An EINTR-safe POSIX file/socket layer extracted verbatim from Xapiand. One
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
examples/bench.cc Runnable throughput benchmark (io:: vs raw ::syscall).
```

## Dependencies

**None**, standalone. The three host couplings (logging, fault injection, the fd
tracker) are optional compile-time header seams that default to no-ops. A host
only pulls in an errno-names / traceback library if it opts into real logging or
`IO_CHECK_FDES`, and then it is the host's own trace header that includes them.

## Conventions / invariants

- **Verbatim behavior.** The engine logic is byte-for-byte from Xapiand's in-tree
  `io.*`. Do not change retry conditions, short-count rules, the `open()`
  minimum-fd / `O_CLOEXEC` handling, or `pread`'s single-read semantics without a
  deliberate reason.
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

## Build / test

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build && ./build/io_bench
# sanitizers (Homebrew LLVM):
cmake -B build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address -g" && cmake --build build-asan && ./build-asan/io_test
```

## Host integration (how Xapiand uses it)

Xapiand does **not** link `libio.a`. It compiles `io.cc` into its own object list
with `-D IO_TRACE_HEADER=<xapiand_io_trace.h>` and
`-D IO_RANDOM_ERRORS_HEADER=<xapiand_io_random_errors.h>`, so `io.cc`'s logging
lands in Xapiand's real logger and every op becomes a fault-injection point under
`XAPIAND_RANDOM_ERRORS`. The header comes from this repo's include dir via
FetchContent. Linking the static target instead (with no-op logging) is the simple
path for a host that doesn't need either seam.
