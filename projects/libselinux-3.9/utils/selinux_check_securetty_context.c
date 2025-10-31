#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <selinux/selinux.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr, "usage:  %s tty_context...\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int i;
	if (argc < 2)
		usage(argv[0]);

	for (i = 1; i < argc; i++) {
		switch (selinux_check_securetty_context(argv[i])) {
		case 0:
			printf("%s securetty.\n", argv[i]);
			break;
		default:
			printf("%s not securetty.\n", argv[i]);
			break;
		}
	}
	return 0;
}
