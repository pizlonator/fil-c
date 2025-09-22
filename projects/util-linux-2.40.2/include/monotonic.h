/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_MONOTONIC_H
#define UTIL_LINUX_MONOTONIC_H

# ifdef CLOCK_MONOTONIC_RAW
#  define UL_CLOCK_MONOTONIC	CLOCK_MONOTONIC_RAW
# else
#  define UL_CLOCK_MONOTONIC	CLOCK_MONOTONIC
# endif

#include <sys/time.h>

#include "timeutils.h"

extern int get_boot_time(struct timeval *boot_time);

extern usec_t get_suspended_time(void);

extern int gettime_monotonic(struct timeval *tv);

#endif /* UTIL_LINUX_MONOTONIC_H */
