#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <selinux/selinux.h>
#include <selinux/label.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr,
		"usage: %s -b backend [-v] [-r] -k key [-t type] [-f file]\n\n"
		"Where:\n\t"
		"-b  The backend - \"file\", \"media\", \"x\", \"db\" or "
			"\"prop\"\n\t"
		"-v  Validate entries against loaded policy.\n\t"
		"-r  Use \"raw\" function.\n\t"
		"-k  Lookup key - Depends on backend.\n\t"
		"-t  Lookup type - Optional as depends on backend.\n\t"
		"-f  Optional file containing the specs (defaults to\n\t"
		"    those used by loaded policy).\n\n"
		"Examples:\n\t"
		"%s -v -b file -k /run -t 0\n\t"
		"   lookup with validation against the loaded policy, the\n\t"
		"   \"file\" backend for path \"/run\" with mode = 0\n\t"
		"%s -r -b x -t 4 -k X11:ButtonPress\n\t"
		"   lookup_raw the \"X\" backend for type SELABEL_X_EVENT\n\t"
		"   using key \"X11:ButtonPress\"\n\n",
		progname, progname, progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int raw = 0, type = 0, rc, opt;
	unsigned int backend = SELABEL_CTX_FILE;
	char *validate = NULL, *key = NULL, *context = NULL, *file = NULL;

	struct selabel_handle *hnd;
	struct selinux_opt selabel_option[] = {
		{ SELABEL_OPT_PATH, file },
		{ SELABEL_OPT_VALIDATE, validate }
	};

	if (argc < 3)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "b:f:vrk:t:")) > 0) {
		switch (opt) {
		case 'b':
			if (!strcasecmp(optarg, "file")) {
				backend = SELABEL_CTX_FILE;
			} else if (!strcmp(optarg, "media")) {
				backend = SELABEL_CTX_MEDIA;
			} else if (!strcmp(optarg, "x")) {
				backend = SELABEL_CTX_X;
			} else if (!strcmp(optarg, "db")) {
				backend = SELABEL_CTX_DB;
			} else if (!strcmp(optarg, "prop")) {
				backend = SELABEL_CTX_ANDROID_PROP;
			} else if (!strcmp(optarg, "service")) {
				backend = SELABEL_CTX_ANDROID_SERVICE;
			} else {
				fprintf(stderr, "Unknown backend: %s\n",
								    optarg);
				usage(argv[0]);
			}
			break;
		case 'f':
			file = optarg;
			break;
		case 'v':
			validate = (char *)1;
			break;
		case 'r':
			raw = 1;
			break;
		case 'k':
			key = optarg;
			break;
		case 't':
			type = atoi(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}

	selabel_option[0].value = file;
	selabel_option[1].value = validate;

	hnd = selabel_open(backend, selabel_option, 2);
	if (!hnd) {
		fprintf(stderr, "ERROR: selabel_open - Could not obtain "
							     "handle:  %s\n",
							     strerror(errno));
		return -1;
	}

	switch (raw) {
	case 1:
		rc = selabel_lookup_raw(hnd, &context, key, type);
		break;
	default:
		rc = selabel_lookup(hnd, &context, key, type);
	}
	selabel_close(hnd);

	if (rc) {
		switch (errno) {
		case ENOENT:
			fprintf(stderr, "ERROR: selabel_lookup failed to "
					    "find a valid context.\n");
			break;
		case EINVAL:
			fprintf(stderr, "ERROR: selabel_lookup failed to "
				    "validate context, or key / type are "
				    "invalid.\n");
			break;
		default:
			fprintf(stderr, "selabel_lookup ERROR: %s\n",
						    strerror(errno));
		}
	} else {
		printf("Default context: %s\n", context);
		freecon(context);
	}

	return rc;
}
