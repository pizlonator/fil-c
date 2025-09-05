/* This file is in the public domain. */
#include <sys/time.h>
#include "config.h"

int
futimens(int fd, const struct timespec times[2])
{
	struct timeval timevals[2];

	TIMESPEC_TO_TIMEVAL(&timevals[0], &times[0]);
	TIMESPEC_TO_TIMEVAL(&timevals[1], &times[1]);

	return futimes(fd, timevals);
}
