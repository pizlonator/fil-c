/* 
 * gcc open_RDWR_failure.c -o open_RDWR_failure.so -shared -ldl 
 * LD_PRELOAD=./open_RDWR_failure.so FAILURE_PATH=/etc/shadow ./test /etc/shadow
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>


typedef int (*open_type) (const char *pathname, int flag, ...);
static open_type next_open64;

static const char *failure_path = NULL;

int open64 (const char *pathname, int flag, ...)
{
	if (NULL == next_open64)
	{
		next_open64 = dlsym (RTLD_NEXT, "open64");
		assert (NULL != next_open64);
	}
	if (NULL == failure_path) {
		failure_path = getenv ("FAILURE_PATH");
		if (NULL == failure_path) {
			fputs ("No FAILURE_PATH defined\n", stderr);
		}
	}

	if (   (NULL != pathname)
	    && ((flag & O_ACCMODE) == O_RDWR)
	    && (NULL != failure_path)
	    && (strcmp (pathname, failure_path) == 0))
	{
		fprintf (stderr, "open FAILURE %s %x ...\n", pathname, flag&O_ACCMODE);
		errno = EIO;
		return -1;
	}

	return next_open64 (pathname, flag);
}

