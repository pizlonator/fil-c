/* 
 * gcc rename_failure.c -o rename_failure.so -shared -ldl 
 * LD_PRELOAD=./rename_failure.so FAILURE_PATH=/etc/shadow ./test /etc/shadow
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


typedef int (*rename_type) (const char *old, const char *new);
static rename_type next_rename;

static const char *failure_path = NULL;

int rename (const char *old, const char *new)
{
	if (NULL == next_rename)
	{
		next_rename = dlsym (RTLD_NEXT, "rename");
		assert (NULL != next_rename);
	}
	if (NULL == failure_path) {
		failure_path = getenv ("FAILURE_PATH");
		if (NULL == failure_path) {
			fputs ("No FAILURE_PATH defined\n", stderr);
		}
	}

	if (   (NULL != new)
	    && (NULL != failure_path)
	    && (strcmp (new, failure_path) == 0))
	{
		fprintf (stderr, "rename FAILURE %s %s\n", old, new);
		errno = EIO;
		return -1;
	}

	return next_rename (old, new);
}

