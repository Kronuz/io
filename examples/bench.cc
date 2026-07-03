/*
 * A runnable benchmark for the io library (not a ctest). Measures sequential
 * write + read throughput through the io:: wrappers and, for comparison, through
 * the raw ::syscalls -- so you can see the EINTR-retry / full-count wrapper adds
 * no meaningful overhead. Build: cmake --build build --target io_bench; run: ./build/io_bench
 */

#include "io.hh"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

static double mb_per_s(size_t bytes, double seconds) {
	return (static_cast<double>(bytes) / (1024.0 * 1024.0)) / seconds;
}

// Write `total` bytes in `chunk`-sized calls, fsync, then read them all back.
template <typename WriteFn, typename ReadFn, typename FsyncFn>
static void run(const char* label, const std::string& path, size_t total, size_t chunk,
                WriteFn&& wr, ReadFn&& rd, FsyncFn&& sync) {
	std::vector<char> block(chunk, 'x');
	std::vector<char> in(chunk, 0);

	int fd = io::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) { std::perror("open"); std::exit(1); }

	auto t0 = Clock::now();
	size_t left = total;
	while (left != 0) {
		size_t n = left < chunk ? left : chunk;
		wr(fd, block.data(), n);
		left -= n;
	}
	sync(fd);
	auto t1 = Clock::now();

	io::lseek(fd, 0, SEEK_SET);
	left = total;
	while (left != 0) {
		size_t n = left < chunk ? left : chunk;
		rd(fd, in.data(), n);
		left -= n;
	}
	auto t2 = Clock::now();

	io::close(fd);
	io::unlink(path.c_str());

	double w = std::chrono::duration<double>(t1 - t0).count();
	double r = std::chrono::duration<double>(t2 - t1).count();
	std::printf("  %-12s write %7.1f MB/s   read %7.1f MB/s\n",
	            label, mb_per_s(total, w), mb_per_s(total, r));
}

int main(int argc, char** argv) {
	size_t total = 128 * 1024 * 1024;  // 128 MiB
	size_t chunk = 64 * 1024;          // 64 KiB
	if (argc > 1) { total = std::strtoull(argv[1], nullptr, 10) * 1024 * 1024; }
	if (argc > 2) { chunk = std::strtoull(argv[2], nullptr, 10) * 1024; }

	std::string path = "/tmp/io_bench.dat";
	std::printf("io benchmark: %zu MiB in %zu KiB chunks\n", total >> 20, chunk >> 10);

	run("io::",
	    path, total, chunk,
	    [](int fd, const void* b, size_t n) { io::write(fd, b, n); },
	    [](int fd, void* b, size_t n) { io::read(fd, b, n); },
	    [](int fd) { io::fsync(fd); });

	run("raw ::",
	    path, total, chunk,
	    [](int fd, const void* b, size_t n) { ssize_t r = ::write(fd, b, n); (void)r; },
	    [](int fd, void* b, size_t n) { ssize_t r = ::read(fd, b, n); (void)r; },
	    [](int fd) { ::fsync(fd); });

	return 0;
}
