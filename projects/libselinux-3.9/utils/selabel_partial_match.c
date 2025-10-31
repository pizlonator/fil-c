#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <stdbool.h>
#include <selinux/selinux.h>
#include <selinux/label.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] -p <path> [-f file]\n\n"
		"Where:\n\t"
		"-v  Validate file_contxts entries against loaded policy.\n\t"
		"-p  Path to check if a match or partial match is possible\n\t"
		"    against a regex entry in the file_contexts file.\n\t"
		"-f  Optional file_contexts file (defaults to current policy).\n\n"
		"Example:\n\t"
		"%s -p /sys/devices/system/cpu/online\n\t"
		"   Check if a match or partial match is possible against\n\t"
		"   the path \"/sys/devices/system/cpu/online\", returning\n\t"
		"   TRUE or FALSE.\n\n", progname, progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt;
	bool partial_match;
	const char *validate = NULL, *path = NULL, *file = NULL;

	struct selabel_handle *hnd;
	struct selinux_opt selabel_option[] = {
		{ SELABEL_OPT_PATH, file },
		{ SELABEL_OPT_VALIDATE, validate }
	};

	if (argc < 2)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "f:vp:")) > 0) {
		switch (opt) {
		case 'f':
			file = optarg;
			break;
		case 'v':
			validate = (char *)1;
			break;
		case 'p':
			path = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (!path || optind != argc)
		usage(argv[0]);

	selabel_option[0].value = file;
	selabel_option[1].value = validate;

	hnd = selabel_open(SELABEL_CTX_FILE, selabel_option, 2);
	if (!hnd) {
		fprintf(stderr, "ERROR: selabel_open - Could not obtain "
							     "handle:  %s\n",
							     strerror(errno));
		return -1;
	}

	partial_match = selabel_partial_match(hnd, path);

	printf("Match or Partial match: %s\n",
		    partial_match ? "TRUE" : "FALSE");

	selabel_close(hnd);
	return partial_match;
}
