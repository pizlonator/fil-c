/* test-schedbatch.c - Create a process using SCHED_BATCH scheduler
 * policy and nice value.
 * Compile: gcc -o test-schedbatch -Wall test-schedbatch.c
 * Usage:   ./test-schedbatch [ <NICE> ]
 *
 * Author: Mike Fleetwood
 * https://bugzilla.redhat.com/show_bug.cgi?id=741090
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>

/* Defined in Linux headers only */
#ifndef SCHED_BATCH
#define SCHED_BATCH 3
#endif

int main(int argc, const char *argv[])
{
	int nice = 19;
	struct sched_param sp;
	char msg[50];

	if (argc >= 2) {
		nice = atoi(argv[1]);
	}
	sp.sched_priority = 0;
#ifdef SCHED_BATCH
	if (sched_setscheduler(0, SCHED_BATCH, &sp)) {
		perror("sched_setscheduler(0,SCHED_BATCH,{.sched_priority=0}");
	}
#endif /* SCHED_BATCH */
	if (setpriority(PRIO_PROCESS, 0, nice) || errno) {
		(void)snprintf(msg, sizeof(msg),
			       "setpriority(PRIO_PROCESS, 0, %d)", nice);
		perror(msg);
	}
	while (1) {
		getchar();
	}
}
