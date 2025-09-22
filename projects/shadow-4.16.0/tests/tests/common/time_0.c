/* 
 * gcc time_0.c -o time_0.so -shared
 * LD_PRELOAD=./time_0.so ./test
 */

#include <stdio.h>
#include <time.h>


time_t time (time_t *t)
{
	fprintf (stderr, "time 0\n");

	return (time_t)0;
}

