# io: an EINTR-safe POSIX file/socket layer extracted from Xapiand.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build
#
# A compiled static library (io.cc) plus a header (io.hh). Standalone it has NO
# dependencies: feature detection is portable, and the logging / fault-injection /
# fd-check couplings are optional header seams that default to no-ops. A host with
# its own logger points IO_TRACE_HEADER at a header defining the L_* family; a host
# that wants random IO/net errors points IO_RANDOM_ERRORS_HEADER at one defining the
# RANDOM_ERRORS_* macros. (Xapiand compiles io.cc into its own object with those -D's
# rather than linking this target, so io.cc's logging lands in its real logger.)

cmake_minimum_required(VERSION 3.16)
project(io LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# --- Library target ---
add_library(io STATIC io.cc)
target_include_directories(io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(io PUBLIC cxx_std_20)
add_library(io::io ALIAS io)

# Build the test + bench only when this repo is the top-level project, so
# consumers (e.g. via FetchContent) don't build them.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	enable_testing()
	add_executable(io_test test/test.cc)
	target_link_libraries(io_test PRIVATE io)
	add_test(NAME io COMMAND io_test)

	# A runnable benchmark (not a test): ./build/io_bench
	add_executable(io_bench examples/bench.cc)
	target_link_libraries(io_bench PRIVATE io)
endif()
