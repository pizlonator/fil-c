/*
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef UTIL_LINUX_RANDUTILS
#define UTIL_LINUX_RANDUTILS

#ifdef HAVE_SRANDOM
#define srand(x)	srandom(x)
#define rand()		random()
#endif

/* rand() based */
extern int rand_get_number(int low_n, int high_n);

/* /dev/urandom based with fallback to rand() */
extern int random_get_fd(void);
extern int ul_random_get_bytes(void *buf, size_t nbytes);
extern const char *random_tell_source(void);

#endif
