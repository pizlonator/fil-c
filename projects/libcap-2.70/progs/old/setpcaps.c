/*
 * Copyright (c) 1997-8 Andrew G. Morgan  <morgan@kernel.org>
 *
 * This sets the capabilities of a given process.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#undef _POSIX_SOURCE
#include <sys/capability.h>
#include <unistd.h>

static void usage(void)
{
    fprintf(stderr,
"usage: setcap [-q] (-|<caps>) <pid> [ ... (-|<capsN>) <pid> ]\n\n"
"  This program can be used to set the process capabilities of running\n"
"  processes.  In order to work, it needs to be executing with CAP_SETPCAP\n"
"  raised, and the only capabilities that this program can bestow on others\n"
"  are a subset of its effective set.  This program is mostly intended as an\n"
"  example -- a safe use of CAP_SETPCAP has yet to be demonstrated!\n\n"
"[Copyright (c) 1997-8 Andrew G. Morgan  <morgan@kernel.org>]\n"
	);
    exit(1);
}

#define MAXCAP  2048

static int read_caps(int quiet, const char *filename, char *buffer)
{
    int i=MAXCAP;

    if (!quiet) {
	fprintf(stderr,	"Please enter caps for file [empty line to end]:\n");
    }
    while (i > 0) {
	int j = read(STDIN_FILENO, buffer, i);

	if (j < 0) {
	    fprintf(stderr, "\n[Error - aborting]\n");
	    exit(1);
	}

	if (j==0 || buffer[0] == '\n') {
	    /* we're done */
	    break;
	}

	/* move on... */

	i -= j;
	buffer += j;
    }

    /* <NUL> terminate */
    buffer[0] = '\0';

    return (i < MAXCAP ? 0:-1);
}

int main(int argc, char **argv)
{
    char buffer[MAXCAP+1];
    int retval, quiet=0;
    cap_t cap_d;

    if (argc < 3) {
	usage();
    }

    while (--argc > 0) {
	const char *text;
	pid_t pid;

	if (!strcmp(*++argv,"-q")) {
	    quiet = 1;
	    continue;
	}
	if (!strcmp(*argv,"-")) {
	    retval = read_caps(quiet, *argv, buffer);
	    if (retval)
		usage();
	    text = buffer;
	} else
	    text = *argv;

	cap_d = cap_from_text(text);
	if (cap_d == NULL) {
	    perror("fatal error");
	    usage();
	}
#ifndef DEBUG
	{
	    ssize_t length;
	    char *result;

	    result = cap_to_text(cap_d, &length);
	    fprintf(stderr, "[caps set to:\n%s\n]\n", result);
	    cap_free(result);
	    result = NULL;
	}
#endif

	if (--argc <= 0)
	    usage();

	pid = atoi(*++argv);
	retval = capsetp(pid, cap_d);

	if (retval != 0) {
	    fprintf(stderr, "Failed to set cap's on process `%d': (%s)\n",
		    pid, strerror(errno));
	    usage();
	}
#ifndef DEBUG
	fprintf(stderr, "[caps set on %d]\n", pid);
#endif
    }

    return 0;
}
