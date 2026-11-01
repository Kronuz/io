# io

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/io/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/io/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

EINTR-safe POSIX file and socket wrappers for C++20: full-count I/O, descriptors above stderr, portable durability.
Each `io::` function mirrors the syscall of the same name and applies one policy: retry on
`EINTR`, loop until the full count moves, keep data files off fds 0, 1 and 2. Reach for it in
storage and network code that must not lose a write to a signal or a short count.

## ✨ Highlights

- **`EINTR` retry.** `read`, `write`, `send`, `recv`, `sendto`, `recvfrom`, `accept`,
  `connect`, `fcntl`, `fsync` and `fallocate` retry while `io::ignore_eintr()` is set.
  `close` never retries, since a retry can close a reused fd.
- **Full-count I/O.** `read`, `write`, `pread` and `pwrite` loop until the whole buffer
  moves; they return short at the end of the file (the reads) or on an error, with the
  bytes moved so far.
- **Safe descriptors.** `open()` sets `O_CLOEXEC` and never returns stdin, stdout or
  stderr.
- **Durability.** `fsync` calls `fdatasync` where the platform has it, `full_fsync` uses
  `F_FULLFSYNC` on macOS, and `fallocate` falls back to `posix_fallocate` or
  `F_PREALLOCATE`.
- **No dependencies.** Logging, fault injection and fd tracking are compile-time seams that
  default to no-ops.

## 🚀 Quick start

```cpp
#include "io.hh"

#include <cstdio>
#include <fcntl.h>

int main() {
	int fd = io::open("data.bin", O_RDWR | O_CREAT | O_TRUNC, 0644);  // never 0, 1 or 2
	io::write(fd, "hello", 5);                       // all five bytes, or an error
	io::fsync(fd);
	char buf[5];
	ssize_t n = io::pread(fd, buf, sizeof buf, 0);   // all five bytes, or fewer at the end of the file
	std::printf("%d %.*s\n", fd > 2, int(n), buf);   // 1 hello
	io::close(fd);                                   // one ::close, never retried
}
```

## 📦 Installation

Requires a C++20 compiler, a POSIX system, and CMake 3.16 or newer; it compiles one source
file. CI builds it on Linux (x86_64, arm64), macOS (arm64, x86_64), and FreeBSD, plus ASan,
UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(io
  GIT_REPOSITORY https://github.com/Kronuz/io.git
  GIT_TAG        3893d47fb416ead9b52db604d8b148ef24a6fef6)  # or main, to track the tip
FetchContent_MakeAvailable(io)

target_link_libraries(my_app PRIVATE io::io)
```

```cpp
#include "io.hh"
```

A host wires its own facilities in with compile definitions:

| Define | Default | Purpose |
| --- | --- | --- |
| `IO_TRACE_HEADER` | no-op | route `L_CALL`, `L_ERRNO` and `L_ERR` to your logger |
| `IO_RANDOM_ERRORS_HEADER` | off | fail operations at random, for resilience tests |
| `IO_CHECK_FDES` | off | catch double-close and use-after-close |
| `IO_MINIMUM_FILE_DESCRIPTOR` | 3 | the smallest fd `open()` returns |

## 🎯 When to use it

Use it for storage and network code that must not lose writes to signals or short counts,
and that wants one place to hook logging and fault injection into every syscall.

Not for:

- **Non-POSIX targets.** There is no Windows backend.
- **Asynchronous I/O.** The wrappers are plain synchronous syscalls; readiness and
  completion belong to your event loop.

## 📚 Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md): each wrapper's semantics, the retry model and how the
  seams stay dependency-free.
- [CHANGELOG.md](CHANGELOG.md): dated changes.
- [AGENTS.md](AGENTS.md): repository map and invariants.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries): the catalog.

## 🛠️ Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

`./build/io_bench` times sequential writes and reads through `io::` and through the raw
syscalls.

## 🤝 Contributing

Read [AGENTS.md](AGENTS.md) and the [catalog's rules](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md)
first.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo and (c) 2015-2020 Dubalu LLC. See
[LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
