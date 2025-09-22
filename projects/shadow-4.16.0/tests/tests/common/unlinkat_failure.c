/* 
 * gcc unlinkat_failure.c -o unlinkat_failure.so -shared -ldl 
 * LD_PRELOAD=./unlinkat_failure.so FAILURE_PATH=/etc/shadow ./test /etc/shadow
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
#include <unistd.h>


typedef int (*unlinkat_type) (int dirfd, const char *pathname, int flags);
static unlinkat_type next_unlinkat;

static const char *failure_path = NULL;
static dev_t failure_dev = -1;
static ino_t failure_ino = -1;

int unlinkat (int dirfd, const char *pathname, int flags)
{
	if (NULL == next_unlinkat)
	{
		next_unlinkat = dlsym (RTLD_NEXT, "unlinkat");
		assert (NULL != next_unlinkat);
	}
	if (NULL == failure_path) {
		struct stat sb;
		failure_path = getenv ("FAILURE_PATH");
		if (NULL == failure_path) {
			fputs ("No FAILURE_PATH defined\n", stderr);
		}
		if (lstat (failure_path, &sb) != 0) {
			fputs ("Can't lstat FAILURE_PATH\n", stderr);
		}
		failure_dev = sb.st_dev;
		failure_ino = sb.st_ino;
	}

	if (   (NULL != pathname)
	    && (NULL != failure_path)) {
		struct stat sb;
		if (   (fstatat (dirfd, pathname, &sb, flags) == 0)
		    && (sb.st_dev == failure_dev)
		    && (sb.st_ino == failure_ino)) {
			fprintf (stderr, "unlinkat FAILURE %s\n", failure_path);
			errno = EBUSY;
			return -1;
		}
	}

	return next_unlinkat (dirfd, pathname, flags);
}

