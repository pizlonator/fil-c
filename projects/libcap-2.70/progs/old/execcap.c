/*
 * This was written by Andrew G. Morgan <morgan@kernel.org>
 *
 * This is a program that is intended to exec a subsequent program.
 * The purpose of this 'execcap' wrapper is to limit the inheritable
 * capabilities of the exec()'d program.  All environment variables
 * are inherited.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/capability.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static void usage(void)
{
    fprintf(stderr,
"usage: execcap <caps> <command-path> [command-args...]\n\n"
"  This program is a wrapper that can be used to limit the Inheritable\n"
"  capabilities of a program to be executed.  Note, this wrapper is\n"
"  intended to assist in overcoming a lack of support for filesystem\n"
"  capability attributes and should be used to launch other files.\n"
"  This program should _NOT_ be made setuid-0.\n\n"
"[Copyright (c) 1998 Andrew G. Morgan <morgan@kernel.org>]\n");

    exit(1);
}

int main(int argc, char **argv)
{
    cap_t new_caps;

    /* this program should not be made setuid-0 */
    if (getuid() && !geteuid()) {
	usage();
    }

    /* check that we have at least 2 arguments */
    if (argc < 3) {
	usage();
    }

    /* parse the first argument to obtain a set of capabilities */
    new_caps = cap_from_text(argv[1]);
    if (new_caps == NULL) {
	fprintf(stderr, "requested capabilities were not recognized\n");
	usage();
    }

    /* set these capabilities for the current process */
    if (cap_set_proc(new_caps) != 0) {
	fprintf(stderr, "unable to set capabilities: %s\n", strerror(errno));
	usage();
    }

    /* exec the program indicated by args 2 ... */
    execvp(argv[2], argv+2);

    /* if we fall through to here, our exec failed -- announce the fact */
    fprintf(stderr, "Unable to execute command: %s\n", strerror(errno));

    usage();

    return 0;
}
