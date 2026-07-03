/*
 * Default (no-op) logging hooks for the standalone `io` library.
 *
 * io.cc instruments a few operations (open/read/write/pread/pwrite, and the
 * optional fd-state check()) through a small L_* family that is no-op by default,
 * so the library builds with zero dependency on any logging, error-string, or
 * traceback header. The engine's behavior does not depend on them: they only emit
 * diagnostics a host may want to surface.
 *
 * The macros used by io.cc:
 *
 *   - L_CALL(...)   — entry trace for the out-of-line ops (open/read/write/...).
 *   - L_ERRNO(...)  — a failed syscall, with errno name/description in its args.
 *   - L_ERR(...)    — an fd-state violation from check() (only under IO_CHECK_FDES).
 *   - L_NOTHING(...)— explicit no-op (the default target for the others).
 *
 * Because error::name(errno) / error::description(errno) / DEBUG_COL / traceback::
 * appear ONLY inside these macros' arguments, a no-op expansion drops them
 * unevaluated -- so the standalone library needs neither an errno-names nor a
 * traceback dependency. A host that wants real logging provides its own versions,
 * two ways:
 *
 *   1. Point IO_TRACE_HEADER at a header that defines them (that header is then
 *      responsible for pulling in whatever error::/traceback:: it references), e.g.
 *        c++ -DIO_TRACE_HEADER='"my_io_trace.h"' ...
 *      io.cc includes that instead of this file.
 *
 *   2. Define the macros directly before io.cc includes io.hh.
 *
 * Each macro is `#ifndef`-guarded, so defining any subset is fine; the rest fall
 * back to the no-op defaults here.
 */

#pragma once

#ifndef L_NOTHING
#define L_NOTHING(...)
#endif

#ifndef L_CALL
#define L_CALL L_NOTHING
#endif

#ifndef L_ERRNO
#define L_ERRNO L_NOTHING
#endif

#ifndef L_ERR
#define L_ERR L_NOTHING
#endif
