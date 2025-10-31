#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <selinux/selinux.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr, "usage:  %s -a or %s boolean...\n", progname, progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int i, get_all = 0, rc = 0, active, pending, len = 0, opt;
	char **names = NULL;

	while ((opt = getopt(argc, argv, "a")) > 0) {
		switch (opt) {
		case 'a':
			if (argc > 2)
				usage(argv[0]);
			if (is_selinux_enabled() <= 0) {
				fprintf(stderr, "%s:  SELinux is disabled\n",
					argv[0]);
				return 1;
			}
			errno = 0;
			rc = security_get_boolean_names(&names, &len);
			if (rc) {
				fprintf(stderr,
					"%s:  Unable to get boolean names:  %s\n",
					argv[0], strerror(errno));
				return 1;
			}
			if (!len) {
				printf("No booleans\n");
				return 0;
			}
			get_all = 1;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (is_selinux_enabled() <= 0) {
		fprintf(stderr, "%s:  SELinux is disabled\n", argv[0]);
		return 1;
	}

	if (!len) {
		if (argc < 2)
			usage(argv[0]);
		len = argc - 1;
		names = calloc(len, sizeof(char *));
		if (!names) {
			fprintf(stderr, "%s:  out of memory\n", argv[0]);
			return 2;
		}
		for (i = 0; i < len; i++) {
			names[i] = strdup(argv[i + 1]);
			if (!names[i]) {
				fprintf(stderr, "%s:  out of memory\n",
					argv[0]);
				rc = 2;
				goto out;
			}
		}
	}

	for (i = 0; i < len; i++) {
		active = security_get_boolean_active(names[i]);
		if (active < 0) {
			if (get_all && errno == EACCES) 
				continue;
			fprintf(stderr, "Error getting active value for %s\n",
				names[i]);
			rc = -1;
			goto out;
		}
		pending = security_get_boolean_pending(names[i]);
		if (pending < 0) {
			fprintf(stderr, "Error getting pending value for %s\n",
				names[i]);
			rc = -1;
			goto out;
		}
		char *alt_name = selinux_boolean_sub(names[i]);
		if (! alt_name) {
			perror("Out of memory\n");
			rc = -1;
			goto out;
		}

		if (pending != active) {
			printf("%s --> %s pending: %s\n", alt_name,
			       (active ? "on" : "off"),
			       (pending ? "on" : "off"));
		} else {
			printf("%s --> %s\n", alt_name,
			       (active ? "on" : "off"));
		}
		free(alt_name);
	}

      out:
	for (i = 0; i < len; i++)
		free(names[i]);
	free(names);
	return rc;
}
