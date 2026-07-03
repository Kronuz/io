# io

An EINTR-safe POSIX file and socket layer, extracted verbatim from
[Xapiand](https://github.com/Kronuz/Xapiand).

Every function in the `io::` namespace mirrors a `::syscall` of the same name but
adds the retry and short-count handling a storage/network engine actually needs,
so call sites never have to open-code an `EINTR` loop or a partial-write loop
again.

```cpp
#include "io.hh"

int fd = io::open("data.bin", O_RDWR | O_CREAT, 0644);   // never returns fd 0/1/2
io::write(fd, buf, n);          // loops until all n bytes are written
io::pread(fd, buf, n, offset);  // positional read
io::fsync(fd);                  // durability (fdatasync where available)
io::close(fd);                  // deliberately does NOT retry on EINTR
```

## Why not just call the syscalls?

- **`EINTR` retry.** `read`/`write`/`send`/`recv`/`fcntl`/`accept`/`connect`/`fsync`
  and friends run through `RetryAfterSignal`, which retries while `errno == EINTR`
  and the process-wide `io::ignore_eintr()` flag is set. `close()` is the one
  deliberate exception: retrying `close` on `EINTR` can double-close a reused fd,
  so it never does.
- **Full-count I/O.** `write`/`pwrite`/`read` loop until the whole buffer is
  transferred, returning a short count only on a genuine error (matching the
  in-tree engine's semantics). `pread` returns after one read (positional reads
  don't have to fill the buffer).
- **Safe fds.** `open()` refuses any descriptor below `IO_MINIMUM_FILE_DESCRIPTOR`
  (default `STDERR_FILENO + 1`), so a data file never lands on stdin/stdout/stderr,
  and sets `O_CLOEXEC`.
- **Portable durability.** `fsync` uses `fdatasync` on Linux; `full_fsync` uses
  `F_FULLFSYNC` on macOS for real on-platter durability. `fallocate` falls back to
  `posix_fallocate` / `F_PREALLOCATE` / a ftruncate-and-touch emulation.

## Standalone, with zero dependencies

Feature detection is portable (no generated `config.h`), and the three host
couplings are **optional header seams** that default to no-ops, so the library
builds and links against nothing:

| Seam | `-D` knob | Default | Purpose |
| --- | --- | --- | --- |
| Logging | `IO_TRACE_HEADER` | no-op (`io_trace.h`) | route `L_CALL`/`L_ERRNO`/`L_ERR` to your logger |
| Fault injection | `IO_RANDOM_ERRORS_HEADER` | off | make ops randomly fail (resiliency testing) |
| fd-state tracker | `IO_CHECK_FDES` | off | catch double-close / use-after-close in debug |

Because `error::name(errno)`, `error::description(errno)`, `DEBUG_COL` and
`traceback::` appear **only inside** those macros' arguments, the no-op expansion
drops them unevaluated. That is what keeps the standalone build dependency-free:
you only need an errno-names / traceback library if you opt into real logging or
the fd tracker, and then it is *your* trace header that pulls them in.

## Build / test

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build          # round-trip, offsets, durability, sockets, EINTR flag
./build/io_bench                # io:: vs raw ::syscall throughput
```

## Using it

Via CMake FetchContent, link the target (default no-op logging):

```cmake
FetchContent_Declare(io GIT_REPOSITORY https://github.com/Kronuz/io.git GIT_TAG main)
FetchContent_MakeAvailable(io)
target_link_libraries(your_app PRIVATE io::io)
```

To route logging to your own logger, point `IO_TRACE_HEADER` at a header that
defines `L_CALL` / `L_ERRNO` / `L_ERR` (and whatever `error::` / `traceback::` they
reference), and compile `io.cc` with that define. See `ARCHITECTURE.md`.
