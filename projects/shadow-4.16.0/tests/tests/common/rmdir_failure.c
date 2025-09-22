/* 
 * gcc rmdir_failure.c -o rmdir_failure.so -shared -ldl 
 * LD_PRELOAD=./rmdir_failure.so FAILURE_PATH=/etc/shadow ./test /etc/shadow
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


typedef int (*rmdir_type) (const char *path);
static rmdir_type next_rmdir;

static const char *failure_path = NULL;

int rmdir (const char *path)
{
	if (NULL == next_rmdir)
	{
		next_rmdir = dlsym (RTLD_NEXT, "rmdir");
		assert (NULL != next_rmdir);
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
		fprintf (stderr, "rmdir FAILURE %s\n", path);
		errno = EBUSY;
		return -1;
	}

	return next_rmdir (path);
}

