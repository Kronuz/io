# Architecture

`io` is a thin layer over POSIX file and socket calls. There is no state beyond a
single process-wide flag; the value is entirely in the retry / short-count / safety
policy each wrapper applies. Read this before changing a wrapper.

![io: the wrapper core and its three optional host seams](assets/architecture.svg)

<!-- Diagram: assets/architecture.svg. Edit the D2 source below and re-render with:
     d2 --theme 0 --pad 20 <this-source>.d2 assets/architecture.svg

```d2
# io: the wrapper core and its three optional host seams.
direction: down
caller: "caller\n(io::read / write / open / socket ...)" { style.fill: "#e8f5ee" }
core: "io wrapper" {
  style.fill: "#eef2f7"
  retry: "RetryAfterSignal\n(loop while EINTR && ignore_eintr())"
  sys: "::syscall\n(full-count read/write, safe fds, O_CLOEXEC)"
  retry -> sys
}
seams: "optional host seams (compile-time -D; no-op / off by default)" {
  style.fill: "#faf3e6"
  grid-columns: 3
  trace: "IO_TRACE_HEADER\n(L_* logging + error::)"
  rnd: "IO_RANDOM_ERRORS_HEADER\n(fault injection)"
  chk: "IO_CHECK_FDES\n(fd-state tracker)"
}
caller -> core
core.retry -> seams.rnd: "before each op" { style.stroke-dash: 3 }
core -> seams.trace: "on error" { style.stroke-dash: 3 }
core -> seams.chk: "on open/close" { style.stroke-dash: 3 }
```
-->

## Dependencies

**None** when built standalone — `io` compiles against nothing but the C++ standard
library and the POSIX headers. `likely.h` is vendored. The three seams are the only way
a host facility enters the picture, and only on opt-in: `IO_TRACE_HEADER` pulls the
host's logger + `error::` strings, `IO_RANDOM_ERRORS_HEADER` its PRNG/config, and
`IO_CHECK_FDES` a traceback library. With the defaults, none of those are referenced.

## Files

```
io.hh        The API: the io:: namespace. Inline wrappers for the syscalls that
             only need retry/fault-injection framing, plus declarations for the
             six out-of-line ops. Portable feature detection lives at the top.
io.cc        The out-of-line ops: open, close, read, write, pread, pwrite,
             fallocate (fallback), and the optional check() fd tracker. This is
             the only compiled unit; it is where logging is emitted.
io_trace.h   No-op L_* logging defaults (redirect via IO_TRACE_HEADER).
likely.h     Vendored likely()/unlikely() branch hints.
```

## The retry model

`RetryAfterSignal(F, args...)` (in `io.hh`) calls `F(args...)` and retries while it
returns `-1` with `errno == EINTR` **and** `io::ignore_eintr()` is set. That flag is
a single `std::atomic_bool`, default `true`, returned by reference so a host can
clear it (Xapiand clears it during shutdown so a blocked syscall unblocks and stays
unblocked instead of looping). Every blocking wrapper (`read`, `write`, `send`,
`recv`, `sendto`, `recvfrom`, `accept`, `connect`, `fcntl`, `fsync`, `fallocate`)
goes through it.

`close()` is the exception. POSIX leaves the fd state undefined after `close`
returns `EINTR`; on Linux the fd is already closed, so retrying can close a
*different* fd that was concurrently opened onto the same number. So `close()`
issues exactly one `::close` and never retries.

## Full-count vs positional

`write` / `pwrite` / `read` loop until the whole buffer is transferred and return
the total; a mid-transfer error returns the bytes done so far (or `-1` if nothing
was written/read). `pread` intentionally returns after a single `::pread` -- a
positional read is allowed to come up short, and the storage engine that uses it
handles that.

## The three seams

Everything host-specific is an optional, compile-time seam. Nothing is a runtime
hook or a virtual call.

1. **Logging (`IO_TRACE_HEADER`).** `io.cc` includes `IO_TRACE_HEADER` if defined,
   else `io_trace.h`, which defines `L_CALL` / `L_ERRNO` / `L_ERR` as no-ops. The
   error-string helpers (`error::name` / `error::description`), `DEBUG_COL` and
   `traceback::` are referenced *only inside* those macros' arguments, so a no-op
   expansion never evaluates them -- which is why the standalone build needs no
   errno-names or traceback dependency. A host trace header both defines the L_*
   macros and `#include`s whatever they reference.

2. **Fault injection (`IO_RANDOM_ERRORS_HEADER`).** `io.hh` includes it if defined,
   then falls back to no-op `RANDOM_ERRORS_IO_ERRNO_RETURN` /
   `RANDOM_ERRORS_NET_ERRNO_RETURN`. A host defines these to (probabilistically)
   set `errno` and `return -1` (Xapiand wires them to `opts.random_errors_io/net`
   and a PRNG), turning every op into a fault-injection point for resiliency tests.

3. **fd-state tracker (`IO_CHECK_FDES`).** When defined, the `CHECK_*` macros call
   `io::check()`, which tracks each fd's opened/closed/socket state in a bitset and
   logs a traceback on a violation (double-close, use-after-close, socket/file
   mix-up). Off by default: the `CHECK_*` macros expand to nothing.

## Portable feature detection

`io.hh` replaces Xapiand's generated `HAVE_*` config macros with direct platform
checks:

| Capability | Detected by | Fallback when absent |
| --- | --- | --- |
| `pwrite`/`pread` | `__unix__` / `__APPLE__` | `lseek` + `read`/`write` |
| `fdatasync` | `__linux__` | `fsync` |
| `F_FULLFSYNC` | `F_FULLFSYNC` macro (macOS) | `fsync`/`fdatasync` |
| `fallocate` | `__linux__` | `posix_fallocate` / `F_PREALLOCATE` / ftruncate emulation |
| `posix_fadvise` | `__linux__` | no-op returning success |

A host may pre-define any `IO_HAVE_*` to override the platform guess.

## Invariants

- **Verbatim behavior.** The wrapper logic is byte-for-byte from Xapiand's in-tree
  `io.cc`/`io.hh`. Do not change the retry conditions, the short-count rules, or
  the `open()` minimum-fd / `O_CLOEXEC` handling without a deliberate reason.
- **No new hard dependencies.** Keep the standalone build dependency-free; if a
  wrapper needs a host facility, add (or extend) a seam, don't `#include` the host.
- **`close()` never retries.** See above.
