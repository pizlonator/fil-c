/* 
 * gcc time_past.c -o time_past.so -shared -ldl 
 * LD_PRELOAD=./time_past.so PAST_DAYS=2 ./test
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


typedef time_t (*time_type) (time_t *t);
static time_type next_time;

static int time_past = 0;
static char *past = NULL;

time_t time (time_t *t)
{
	time_t res;

	if (NULL == next_time)
	{
		next_time = dlsym (RTLD_NEXT, "time");
		assert (NULL != next_time);
	}
	if (NULL == past) {
		const char *past = getenv ("PAST_DAYS");
		if (NULL == past) {
			fputs ("No PAST_DAYS defined\n", stderr);
		}
		time_past = atoi (past);
	}

	res = next_time (t);
	res -= 24*60*60*time_past;

	if (NULL != t) {
		*t = res;
	}

	return res;
}

