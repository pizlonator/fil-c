#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <selinux/label.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr,
		"usage:  %s [-V] [-N] [-n] [-m type] [-f file_contexts_file] [-p prefix] [-P policy_root_path] filepath...\n",
		progname);
	exit(1);
}

static int printmatchpathcon(struct selabel_handle *hnd, const char *path, int header, int mode, int notrans)
{
	char *buf = NULL;
	int rc;

	if (notrans) {
		rc = selabel_lookup_raw(hnd, &buf, path, mode);
	} else {
		rc = selabel_lookup(hnd, &buf, path, mode);
	}
	if (rc < 0) {
		if (errno == ENOENT) {
			buf = strdup("<<none>>");
		} else {
			fprintf(stderr, "selabel_lookup(%s) failed: %s\n", path,
				strerror(errno));
			return 1;
		}
	}
	if (header)
		printf("%s\t%s\n", path, buf);
	else
		printf("%s\n", buf);

	freecon(buf);
	return 0;
}

static mode_t string_to_mode(char *s)
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
	default:
		return -1;
	}
	return -1;
}

int main(int argc, char **argv)
{
	int i, force_mode = 0;
	int header = 1, opt;
	int verify = 0;
	int notrans = 0;
	int error = 0;
	int quiet = 0;
	struct selabel_handle *hnd;
	struct selinux_opt options[SELABEL_NOPT] = {};

	if (argc < 2)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "m:Nnf:P:p:Vq")) > 0) {
		switch (opt) {
		case 'n':
			header = 0;
			break;
		case 'm':
			force_mode = string_to_mode(optarg);
			if (force_mode < 0) {
				fprintf(stderr, "%s: mode %s is invalid\n", argv[0], optarg);
				exit(1);
			}
			break;
		case 'V':
			verify = 1;
			break;
		case 'N':
			notrans = 1;
			break;
		case 'f':
			options[SELABEL_OPT_PATH].type = SELABEL_OPT_PATH;
			options[SELABEL_OPT_PATH].value = optarg;
			break;
		case 'P':
			if (selinux_set_policy_root(optarg) < 0 ) {
				fprintf(stderr,
					"Error setting policy root  %s:  %s\n",
					optarg,
					errno ? strerror(errno) : "invalid");
				exit(1);
			}
			break;
		case 'p':
			// This option has been deprecated since libselinux 2.5 (2016):
			// https://github.com/SELinuxProject/selinux/commit/26e05da0fc2d0a4bd274320968a88f8acbb3b6a6
			fprintf(stderr, "Warning: using %s -p is deprecated\n", argv[0]);
			options[SELABEL_OPT_SUBSET].type = SELABEL_OPT_SUBSET;
			options[SELABEL_OPT_SUBSET].value = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage(argv[0]);
		}
	}
	hnd = selabel_open(SELABEL_CTX_FILE, options, SELABEL_NOPT);
	if (!hnd) {
		fprintf(stderr,
			"Error while opening file contexts database: %s\n",
			strerror(errno));
		return -1;
	}
	for (i = optind; i < argc; i++) {
		int rc, mode = 0;
		struct stat buf;
		char *path = argv[i];
		int len = strlen(path);
		if (len > 1  && path[len - 1 ] == '/')
			path[len - 1 ] = '\0';

		if (lstat(path, &buf) == 0)
			mode = buf.st_mode;
		if (force_mode)
			mode = force_mode;

		if (verify) {
			rc = selinux_file_context_verify(path, mode);

			if (quiet) {
				if (rc == 1)
					continue;
				else
					exit(1);
			}

			if (rc == -1) {
				printf("%s error: %s\n", path, strerror(errno));
				exit(1);
			} else if (rc == 1) {
				printf("%s verified.\n", path);
			} else {
				char * con;
				error = 1;
				if (notrans)
					rc = lgetfilecon_raw(path, &con);
				else
					rc = lgetfilecon(path, &con);

				if (rc >= 0) {
					printf("%s has context %s, should be ",
					       path, con);
					printmatchpathcon(hnd, path, 0, mode, notrans);
					freecon(con);
				} else {
					printf
					    ("actual context unknown: %s, should be ",
					     strerror(errno));
					printmatchpathcon(hnd, path, 0, mode, notrans);
				}
			}
		} else {
			error |= printmatchpathcon(hnd, path, header, mode, notrans);
		}
	}
	selabel_close(hnd);
	return error;
}
