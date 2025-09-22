/* 
 * gcc link_failure.c -o link_failure.so -shared -ldl 
 * LD_PRELOAD=./link_failure.so FAILURE_PATH=/etc/shadow ./test /etc/shadow
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


typedef int (*link_type) (const char *oldpath, const char *newpath);
static link_type next_link;

static const char *failure_path = NULL;

int link (const char *oldpath, const char *newpath)
{
	if (NULL == next_link)
	{
		next_link = dlsym (RTLD_NEXT, "link");
		assert (NULL != next_link);
	}
	if (NULL == failure_path) {
		failure_path = getenv ("FAILURE_PATH");
		if (NULL == failure_path) {
			fputs ("No FAILURE_PATH defined\n", stderr);
		}
	}

	if (   (NULL != newpath)
	    && (NULL != failure_path)
	    && (strcmp (newpath, failure_path) == 0))
	{
		fprintf (stderr, "link FAILURE %s %s\n", oldpath, newpath);
		errno = EIO;
		return -1;
	}

	return next_link (oldpath, newpath);
}

