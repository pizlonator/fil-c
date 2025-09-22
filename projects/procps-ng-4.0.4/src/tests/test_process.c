/*
 * test_procps -- program to create a process to test procps
 *
 * Copyright 2015 Craig Small <csmall@dropbear.xyz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include "c.h"

#define DEFAULT_SLEEPTIME 300
#define MY_NAME "spcorp"

static void usage(void)
{
    fprintf(stderr, " %s [options]\n", program_invocation_short_name);
    fprintf(stderr, " -s <seconds>\n");
    exit(EXIT_FAILURE);
}


void
signal_handler(int signum, siginfo_t *siginfo, void *ucontext)
{
    char *signame = NULL;

    switch(signum) {
	case SIGUSR1:
	    signame = strdup("SIGUSR1");
	    break;
	case SIGUSR2:
	    signame = strdup("SIGUSR2");
	    break;
	default:
	    printf("SIG unknown\n");
	    exit(EXIT_FAILURE);
    }
    if (signame == NULL) {
	printf("SIG malloc error\n");
	exit(EXIT_FAILURE);
    }
    switch (siginfo->si_code) {
	case SI_USER:
	    printf("SIG %s\n", signame);
	    break;
	case SI_QUEUE:
#ifdef HAVE_SIGINFO_T_SI_INT
	        printf("SIG %s value=%d\n", signame, siginfo->si_int);
#else
	        printf("case SI_QUEUE: SIG %s siginfo->si_int undefined\n", signame);
#endif
	    break;
	default:
	    printf("Unknown si_code %d\n", siginfo->si_code);
	    exit(EXIT_FAILURE);
    }

    free(signame);
}

int main(int argc, char *argv[])
{
    int sleep_time, opt;
    struct sigaction signal_action;

    sleep_time = DEFAULT_SLEEPTIME;
    while ((opt = getopt(argc, argv, "s:")) != -1) {
	switch(opt) {
	    case 's':
		sleep_time = atoi(optarg);
		if (sleep_time < 1) {
		    fprintf(stderr, "sleep time must be 1 second or more\n");
		    usage();
		}
		break;
	    default:
		usage();
	}
    }

    /* Setup signal handling */
    signal_action.sa_sigaction = signal_handler;
    sigemptyset (&signal_action.sa_mask);
    signal_action.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &signal_action, NULL);
    sigaction(SIGUSR2, &signal_action, NULL);

#ifdef __linux__
    /* set process name */
    prctl(PR_SET_NAME, MY_NAME, NULL, NULL, NULL);
#endif

    while (sleep_time > 0) {
	sleep_time = sleep(sleep_time);
    }
    return EXIT_SUCCESS;
}
