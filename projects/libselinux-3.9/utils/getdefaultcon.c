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
	fprintf(stderr, "usage:  %s [-r role] [-l level] [-s service] [-v] user [fromcon]\n", name);
	if (detail)
		fprintf(stderr, "%s:  %s\n", name, detail);
	exit(rc);
}

int main(int argc, char **argv)
{
	const char *cur_context, *user;
	char *usercon = NULL, *cur_con = NULL;
	char *level = NULL, *role=NULL, *seuser=NULL, *dlevel=NULL;
	char *service = NULL;
	int ret, opt;
	int verbose = 0;

	while ((opt = getopt(argc, argv, "l:r:s:v")) > 0) {
		switch (opt) {
		case 'l':
			free(level);
			level = strdup(optarg);
			break;
		case 'r':
			free(role);
			role = strdup(optarg);
			break;
		case 's':
			free(service);
			service = strdup(optarg);
			break;
		case 'v':
			verbose = 1;
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
			"%s may be used only on a SELinux kernel.\n", argv[0]);
		free(level);
		free(role);
		free(service);
		return 1;
	}

	user = argv[optind];

	/* If a context wasn't passed, use the current context. */
	if ((argc - optind) < 2) {
		if (getcon(&cur_con) < 0) {
			fprintf(stderr, "%s:  couldn't get current context:  %s\n", argv[0], strerror(errno));
			free(level);
			free(role);
			free(service);
			return 2;
		}
		cur_context = cur_con;
	} else
		cur_context = argv[optind + 1];

	if (security_check_context(cur_context)) {
		fprintf(stderr, "%s:  invalid from context '%s'\n", argv[0], cur_context);
		free(cur_con);
		free(level);
		free(role);
		free(service);
		return 3;
	}

	ret = getseuser(user, service, &seuser, &dlevel);
	if (ret) {
		fprintf(stderr, "%s:  failed to get seuser:  %s\n", argv[0], strerror(errno));
		goto out;
	}

	if (! level) level=dlevel;
	if (role != NULL && role[0])
		ret = get_default_context_with_rolelevel(seuser, role, level, cur_context, &usercon);
	else
		ret = get_default_context_with_level(seuser, level, cur_context, &usercon);
	if (ret) {
		fprintf(stderr, "%s:  failed to get default context:  %s\n", argv[0], strerror(errno));
		goto out;
	}

	if (verbose) {
		printf("%s: %s from %s %s %s %s -> %s\n", argv[0], user, cur_context, seuser, role, level, usercon);
	} else {
		printf("%s\n", usercon);
	}

out:
	free(role);
	free(seuser);
	if (level != dlevel) free(level);
	free(dlevel);
	free(usercon);
	free(cur_con);
	free(service);

	return ret >= 0;
}
