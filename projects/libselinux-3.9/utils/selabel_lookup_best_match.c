#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <selinux/selinux.h>
#include <selinux/label.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr,
		"usage: %s [-v] [-r] -p path [-m mode] [-f file] [link...]\n\n"
		"Where:\n\t"
		"-v     Validate file_contxts entries against loaded policy.\n\t"
		"-r     Use \"raw\" function.\n\t"
		"-p     Path to check for best match using the link(s) provided.\n\t"
		"-m     Optional mode (b, c, d, p, l, s or f) Defaults to 0.\n\t"
		"-f     Optional file containing the specs (defaults to\n\t"
		"       those used by loaded policy).\n\t"
		"link   Zero or more links to check against, the order of\n\t"
		"       precedence for best match is:\n\t\t"
		"   1) An exact match for the real path (if no links), or\n\t\t"
		"   2) An exact match for any of the links (aliases), or\n\t\t"
		"   3) The longest fixed prefix match.\n\n"
		"Example:\n\t"
		"%s -p /dev/initctl /run/systemd/initctl/fifo\n\t"
		"   Find best matching context for the specified path using one link.\n\n",
		progname, progname);
	exit(1);
}

static mode_t string_to_mode(const char *s)
{
	switch (s[0]) {
	case 'b':
		return S_IFBLK;
	case 'c':
		return S_IFCHR;
	case 'd':
		return S_IFDIR;
	case 'p':
		return S_IFIFO;
	case 'l':
		return S_IFLNK;
	case 's':
		return S_IFSOCK;
	case 'f':
		return S_IFREG;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int raw = 0, mode = 0, rc, opt, i, num_links;
	char *validate = NULL, *path = NULL, *context = NULL, *file = NULL;
	char **links = NULL;

	struct selabel_handle *hnd;
	struct selinux_opt options[] = {
		{ SELABEL_OPT_PATH, file },
		{ SELABEL_OPT_VALIDATE, validate }
	};

	if (argc < 3)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "f:vrp:m:")) > 0) {
		switch (opt) {
		case 'f':
			file = optarg;
			break;
		case 'v':
			validate = (char *)1;
			break;
		case 'r':
			raw = 1;
			break;
		case 'p':
			path = optarg;
			break;
		case 'm':
			mode = string_to_mode(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}

	/* Count links */
	for (i = optind, num_links = 0; i < argc; i++, num_links++)
		;

	if (num_links) {
		links = calloc(num_links + 1, sizeof(char *));

		if (!links) {
			fprintf(stderr, "ERROR: calloc failed.\n");
			exit(1);
		}

		for (i = optind, num_links = 0; i < argc; i++, num_links++) {
			links[num_links] = strdup(argv[i]);
			if (!links[num_links]) {
				fprintf(stderr, "ERROR: strdup failed.\n");
				exit(1);
			}
		}
	}

	options[0].value = file;
	options[1].value = validate;

	hnd = selabel_open(SELABEL_CTX_FILE, options, 2);
	if (!hnd) {
		fprintf(stderr, "ERROR: selabel_open - Could not obtain "
							     "handle:  %s\n",
							     strerror(errno));
		rc = -1;
		goto out;
	}

	if (raw)
		rc = selabel_lookup_best_match_raw(hnd, &context, path,
					    (const char **)links, mode);
	else
		rc = selabel_lookup_best_match(hnd, &context, path,
					    (const char **)links, mode);

	selabel_close(hnd);

	if (rc) {
		switch (errno) {
		case ENOENT:
			fprintf(stderr, "ERROR: selabel_lookup_best_match "
				    "failed to find a valid context.\n");
			break;
		case EINVAL:
			fprintf(stderr, "ERROR: selabel_lookup_best_match "
				"failed to validate context, or path / mode "
				"are invalid.\n");
			break;
		default:
			fprintf(stderr, "selabel_lookup_best_match ERROR: "
					    "%s\n", strerror(errno));
		}
	} else {
		printf("Best match context: %s\n", context);
		freecon(context);
	}

out:
	if (links) {
		for (i = 0; links[i]; i++)
			free(links[i]);
		free(links);
	}

	return rc;
}
