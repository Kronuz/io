// A fault hook for pread_test: this executable's own `pread` stands in for the C library's, so the
// calls io.cc makes pass through it. Each call can transfer fewer bytes than asked (a short read),
// fail with EINTR, or fail with EIO; anything else is forwarded to the C library's pread.
//
// Only the headers that do not declare pread are included: a declaration from <unistd.h> would
// clash with the definition below (glibc marks it noexcept).

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

extern "C" {

int fault_chunk = 0;       // > 0: transfer at most this many bytes per call
int fault_eintr_every = 0; // > 0: calls number k-1, 2k-1, ... fail with EINTR (k = 1: every call)
int fault_eio_at = -1;     // >= 0: the call with this number fails with EIO
long fault_calls = 0;      // calls seen since the test last reset it

ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset) {
	using real_t = ssize_t (*)(int, void*, size_t, off_t);
	static const real_t real = reinterpret_cast<real_t>(dlsym(RTLD_NEXT, "pread"));
	const long call = fault_calls++;
	if (fault_eio_at >= 0 && call == fault_eio_at) {
		errno = EIO;
		return -1;
	}
	if (fault_eintr_every > 0 && call % fault_eintr_every == fault_eintr_every - 1) {
		errno = EINTR;
		return -1;
	}
	if (fault_chunk > 0 && nbyte > static_cast<size_t>(fault_chunk)) {
		nbyte = static_cast<size_t>(fault_chunk);
	}
	return real(fd, buf, nbyte, offset);
}

}
