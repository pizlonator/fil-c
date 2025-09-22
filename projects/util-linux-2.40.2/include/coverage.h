/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_COVERAGE_H
#define UTIL_LINUX_COVERAGE_H

/* When built with --coverage (gcov) we need to explicitly call __gcov_dump()
 * in places where we use _exit(), since _exit() skips at-exit hooks resulting
 * in lost coverage.
 *
 * To make sure we don't miss any _exit() calls, this header file is included
 * explicitly on the compiler command line via the -include directive (only
 * when built with --coverage/-Db_coverage=true)
 */
void __gcov_dump(void);
__attribute__((noreturn)) void _exit(int);

__attribute__((noreturn)) static inline void _coverage__exit(int status) {
        __gcov_dump();
        _exit(status);
}
#define _exit(x) _coverage__exit(x)

#endif
