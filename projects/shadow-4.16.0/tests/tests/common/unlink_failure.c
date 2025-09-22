/* 
 * gcc unlink_failure.c -o unlink_failure.so -shared -ldl 
 * LD_PRELOAD=./unlink_failure.so FAILURE_PATH=/etc/shadow ./test /etc/shadow
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


typedef int (*unlink_type) (const char *path);
static unlink_type next_unlink;

static const char *failure_path = NULL;

int unlink (const char *path)
{
	if (NULL == next_unlink)
	{
		next_unlink = dlsym (RTLD_NEXT, "unlink");
		assert (NULL != next_unlink);
	}
	if (NULL == failure_path) {
		failure_path = getenv ("FAILURE_PATH");
		if (NULL == failure_path) {
			fputs ("No FAILURE_PATH defined\n", stderr);
		}
	}

	if (   (NULL != path)
	    && (NULL != failure_path)
	    && (strcmp (path, failure_path) == 0))
	{
		fprintf (stderr, "unlink FAILURE %s\n", path);
		errno = EBUSY;
		return -1;
	}

	return next_unlink (path);
}

