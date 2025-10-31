#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <selinux/selinux.h>
#include <selinux/get_context_list.h>

static __attribute__ ((__noreturn__)) void usage(const char *name, const char *detail, int rc)
{
	fprintf(stderr, "usage:  %s [-l level] user [context]\n", name);
	if (detail)
		fprintf(stderr, "%s:  %s\n", name, detail);
	exit(rc);
}

int main(int argc, char **argv)
{
	char **list;
	const char *cur_context, *user;
	char *cur_con = NULL, *level = NULL;
	int ret, i, opt;

	while ((opt = getopt(argc, argv, "l:")) > 0) {
		switch (opt) {
		case 'l':
			free(level);
			level = strdup(optarg);
			if (!level) {
				fprintf(stderr, "memory allocation failure: %d(%s)\n",
					errno, strerror(errno));
				return 3;
			}
			break;
		default:
			usage(argv[0], "invalid option", 1);
		}
	}

	if (((argc - optind) < 1) || ((argc - optind) > 2))
		usage(argv[0], "invalid number of arguments", 2);

	/* If selinux isn't available, bail out. */
	if (!is_selinux_enabled()) {
		fprintf(stderr,
			"getconlist may be used only on a SELinux kernel.\n");
		free(level);
		return 1;
	}

	user = argv[optind];

	/* If a context wasn't passed, use the current context. */
	if (((argc - optind) < 2)) {
		if (getcon(&cur_con) < 0) {
			fprintf(stderr, "Couldn't get current context:  %s\n", strerror(errno));
			free(level);
			return 2;
		}
		cur_context = cur_con;
	} else {
		cur_context = argv[optind + 1];
		if (security_check_context(cur_context) != 0) {
			fprintf(stderr, "Given context '%s' is invalid.\n", cur_context);
			free(level);
			return 3;
		}
	}

	/* Get the list and print it */
	if (level)
		ret =
		    get_ordered_context_list_with_level(user, level,
							cur_context, &list);
	else
		ret = get_ordered_context_list(user, cur_context, &list);
	if (ret != -1) {
		for (i = 0; list[i]; i++)
			puts(list[i]);
		freeconary(list);
	} else {
		fprintf(stderr, "get_ordered_context_list%s failure: %d(%s)\n",
			level ? "_with_level" : "", errno, strerror(errno));
		free(cur_con);
		free(level);
		return 4;
	}

	free(cur_con);
	free(level);

	return 0;
}
