// io::pread transfers the whole count: it keeps reading, at the advancing offset, until the buffer
// is full, the file ends, or a read fails. The short reads are injected by pread_interpose.cc, which
// replaces the C library's pread for this executable, so every case below runs a real file through
// a pread that hands back a few bytes at a time.

#include "io.hh"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

extern "C" {
extern int fault_chunk;
extern int fault_eintr_every;
extern int fault_eio_at;
extern long fault_calls;
}

static int failures = 0;

#define CHECK(cond)                                                            \
	do {                                                                       \
		if (!(cond)) {                                                         \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                        \
		}                                                                      \
	} while (0)

static void faults(int chunk, int eintr_every = 0, int eio_at = -1) {
	fault_chunk = chunk;
	fault_eintr_every = eintr_every;
	fault_eio_at = eio_at;
	fault_calls = 0;
}

int main() {
	char path[] = "/tmp/io_pread_test_XXXXXX";
	int fd = io::mkstemp(path);
	CHECK(fd >= IO_MINIMUM_FILE_DESCRIPTOR);

	// 1000 bytes, each different from its neighbours, so a misplaced chunk shows.
	std::string data;
	for (int i = 0; i < 1000; ++i) { data.push_back(static_cast<char>('a' + (i * 7 + i / 26) % 26)); }
	CHECK(io::write(fd, data.data(), data.size()) == 1000);

	char buf[1500];

	// No injection: one call fills the buffer.
	faults(0);
	std::memset(buf, 0, sizeof(buf));
	CHECK(io::pread(fd, buf, 1000, 0) == 1000);
	CHECK(std::memcmp(buf, data.data(), 1000) == 0);
	CHECK(fault_calls == 1);

	// A pread that returns 1, 7 or 100 bytes per call still fills the buffer, from the right offsets.
	for (int chunk : {1, 7, 100, 999}) {
		faults(chunk);
		std::memset(buf, 0, sizeof(buf));
		CHECK(io::pread(fd, buf, 1000, 0) == 1000);
		CHECK(std::memcmp(buf, data.data(), 1000) == 0);
		CHECK(fault_calls == (1000 + chunk - 1) / chunk);

		faults(chunk);
		std::memset(buf, 0, sizeof(buf));
		CHECK(io::pread(fd, buf, 300, 411) == 300);
		CHECK(std::memcmp(buf, data.data() + 411, 300) == 0);
	}

	// The file ends before the buffer does: the count stops at the end of the file, with no further
	// call looping on a read that returns 0.
	faults(100);
	std::memset(buf, 0, sizeof(buf));
	CHECK(io::pread(fd, buf, 1500, 0) == 1000);
	CHECK(std::memcmp(buf, data.data(), 1000) == 0);
	CHECK(fault_calls == 11);
	faults(0);
	CHECK(io::pread(fd, buf, 100, 950) == 50);
	CHECK(std::memcmp(buf, data.data() + 950, 50) == 0);

	// At and past the end: nothing to read, not an error.
	faults(0);
	CHECK(io::pread(fd, buf, 10, 1000) == 0);
	CHECK(io::pread(fd, buf, 10, 5000) == 0);

	// Nothing asked, nothing read, no call.
	faults(0);
	CHECK(io::pread(fd, buf, 0, 0) == 0);
	CHECK(fault_calls == 0);

	// EINTR is retried while ignore_eintr() is set: every other call is interrupted and the count is whole.
	io::ignore_eintr().store(true);
	faults(10, 2);
	std::memset(buf, 0, sizeof(buf));
	CHECK(io::pread(fd, buf, 200, 100) == 200);
	CHECK(std::memcmp(buf, data.data() + 100, 200) == 0);

	// With ignore_eintr() clear the interruption is an error: -1 when nothing was read, the bytes read when some were.
	io::ignore_eintr().store(false);
	faults(10, 1);
	CHECK(io::pread(fd, buf, 200, 100) == -1);
	CHECK(errno == EINTR);
	faults(10, 3);  // two calls of 10 bytes, then the third is interrupted
	std::memset(buf, 0, sizeof(buf));
	CHECK(io::pread(fd, buf, 200, 100) == 20);
	CHECK(std::memcmp(buf, data.data() + 100, 20) == 0);
	io::ignore_eintr().store(true);

	// An error after some bytes came back returns those bytes; an error on the first read returns -1.
	faults(10, 0, 3);
	std::memset(buf, 0, sizeof(buf));
	CHECK(io::pread(fd, buf, 200, 100) == 30);
	CHECK(std::memcmp(buf, data.data() + 100, 30) == 0);
	faults(10, 0, 0);
	CHECK(io::pread(fd, buf, 200, 100) == -1);
	CHECK(errno == EIO);

	io::close(fd);
	io::unlink(path);

	if (failures == 0) { std::puts("all io pread tests passed"); }
	return failures == 0 ? 0 : 1;
}
