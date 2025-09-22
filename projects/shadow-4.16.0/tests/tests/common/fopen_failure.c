/* 
 * gcc fopen_failure.c -o fopen_failure.so -shared -ldl 
 * LD_PRELOAD=./fopen_failure.so FAILURE_PATH=/etc/shadow ./test /etc/shadow
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>


typedef FILE * (*fopen_type) (const char *path, const char *mode);
static fopen_type next_fopen;

static const char *failure_path = NULL;

FILE *fopen64 (const char *path, const char *mode)
{
printf ("fopen64(%s, %s)\n", path, mode);
	if (NULL == next_fopen)
	{
		next_fopen = dlsym (RTLD_NEXT, "fopen64");
		assert (NULL != next_fopen);
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
		fprintf (stderr, "fopen64 FAILURE %s %s ...\n", path, mode);
		errno = EIO;
		return NULL;
	}

	return next_fopen (path, mode);
}

