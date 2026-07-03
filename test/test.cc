/*
 * Tests for the io library. Plain ctest executable: exits non-zero on the first
 * failed check. Exercises the round-trip, offset, durability, allocation, socket,
 * and EINTR-flag surfaces of the io:: layer -- with the default (no-op) logging,
 * so this file also proves the library builds and links with zero dependencies.
 */

#include "io.hh"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

static int failures = 0;

#define CHECK(cond)                                                            \
	do {                                                                       \
		if (!(cond)) {                                                         \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                        \
		}                                                                      \
	} while (0)


// A scratch path in the system temp dir, unlinked at scope exit.
struct TempFile {
	std::string path;
	int fd = -1;
	TempFile() {
		char tmpl[] = "/tmp/io_test_XXXXXX";
		fd = io::mkstemp(tmpl);
		path = tmpl;
	}
	~TempFile() {
		if (fd != -1) { io::close(fd); }
		io::unlink(path.c_str());
	}
};


static void test_open_write_read() {
	TempFile t;
	CHECK(t.fd >= IO_MINIMUM_FILE_DESCRIPTOR);  // never lands on 0/1/2

	const std::string payload = "the quick brown fox jumps over the lazy dog";
	ssize_t w = io::write(t.fd, payload.data(), payload.size());
	CHECK(w == static_cast<ssize_t>(payload.size()));  // full count, not short

	CHECK(io::lseek(t.fd, 0, SEEK_SET) == 0);

	char buf[128] = {0};
	ssize_t r = io::read(t.fd, buf, payload.size());
	CHECK(r == static_cast<ssize_t>(payload.size()));
	CHECK(std::memcmp(buf, payload.data(), payload.size()) == 0);
}


static void test_pwrite_pread_offset() {
	TempFile t;
	const char* a = "AAAA";
	const char* b = "BBBB";
	// Lay down 4 bytes at offset 0 and 4 bytes at offset 8 (leaving a hole).
	CHECK(io::pwrite(t.fd, a, 4, 0) == 4);
	CHECK(io::pwrite(t.fd, b, 4, 8) == 4);

	char buf[4] = {0};
	CHECK(io::pread(t.fd, buf, 4, 8) == 4);
	CHECK(std::memcmp(buf, b, 4) == 0);

	CHECK(io::pread(t.fd, buf, 4, 0) == 4);
	CHECK(std::memcmp(buf, a, 4) == 0);
}


static void test_durability() {
	TempFile t;
	CHECK(io::write(t.fd, "durable", 7) == 7);
	CHECK(io::fsync(t.fd) == 0);
	CHECK(io::full_fsync(t.fd) == 0);
	CHECK(io::unchecked_fsync(t.fd) == 0);
}


static void test_fstat_and_fallocate() {
	TempFile t;
	CHECK(io::write(t.fd, "0123456789", 10) == 10);

	struct stat st{};
	CHECK(io::fstat(t.fd, &st) == 0);
	CHECK(st.st_size == 10);

	// Grow the file to at least 4096 bytes.
	io::fallocate(t.fd, 0, 0, 4096);
	CHECK(io::fstat(t.fd, &st) == 0);
	CHECK(st.st_size >= 10);  // never shrinks the existing data
}


static void test_fadvise_and_fcntl() {
	TempFile t;
	CHECK(io::write(t.fd, "hint", 4) == 4);
	// fadvise is a hint: portable no-op on platforms without posix_fadvise; must not error.
	CHECK(io::fadvise(t.fd, 0, 0, POSIX_FADV_SEQUENTIAL) >= 0);
	// fcntl passthrough: fetch and set flags.
	int flags = io::fcntl(t.fd, F_GETFL, 0);
	CHECK(flags != -1);
	CHECK(io::fcntl(t.fd, F_SETFL, flags) != -1);
}


static void test_socketpair_send_recv() {
	int sv[2];
	CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	const char* msg = "ping";
	CHECK(io::send(sv[0], msg, 4, 0) == 4);

	char buf[4] = {0};
	CHECK(io::recv(sv[1], buf, 4, 0) == 4);
	CHECK(std::memcmp(buf, msg, 4) == 0);

	CHECK(io::shutdown(sv[0], SHUT_RDWR) == 0);
	::close(sv[0]);
	::close(sv[1]);
}


static void test_ignore_eintr_flag() {
	// Process-wide, defaults to true; togglable and observed by the retry loop.
	CHECK(io::ignore_eintr().load() == true);
	io::ignore_eintr().store(false);
	CHECK(io::ignore_eintr().load() == false);
	io::ignore_eintr().store(true);
	CHECK(io::ignore_eintr().load() == true);
}


static void test_ignored_errno() {
	CHECK(io::ignored_errno(EAGAIN, /*again=*/true, false, false) == true);
	CHECK(io::ignored_errno(EAGAIN, /*again=*/false, false, false) == false);
	CHECK(io::ignored_errno(EINPROGRESS, false, /*tcp=*/true, false) == true);
	CHECK(io::ignored_errno(ECONNRESET, false, false, /*udp=*/true) == true);
	CHECK(io::ignored_errno(EIO, true, true, true) == false);  // real error, never ignored
}


int main() {
	test_open_write_read();
	test_pwrite_pread_offset();
	test_durability();
	test_fstat_and_fallocate();
	test_fadvise_and_fcntl();
	test_socketpair_send_recv();
	test_ignore_eintr_flag();
	test_ignored_errno();

	if (failures != 0) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	std::puts("all io tests passed");
	return EXIT_SUCCESS;
}
