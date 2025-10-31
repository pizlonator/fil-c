#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <selinux/selinux.h>
#include <selinux/get_context_list.h>

int main(int argc, char **argv)
{
	char *seuser = NULL, *level = NULL;
	char **contextlist;
	int rc, n, i;

	if (argc != 3) {
		fprintf(stderr, "usage:  %s linuxuser fromcon\n", argv[0]);
		return 1;
	}

	if (!is_selinux_enabled()) {
		fprintf(stderr, "%s may be used only on a SELinux enabled kernel.\n", argv[0]);
		return 4;
	}

	rc = getseuserbyname(argv[1], &seuser, &level);
	if (rc) {
		fprintf(stderr, "getseuserbyname failed:  %s\n", strerror(errno));
		return 2;
	}
	printf("seuser:  %s, level %s\n", seuser, level);

	rc = security_check_context(argv[2]);
	if (rc) {
		fprintf(stderr, "context '%s' is invalid\n", argv[2]);
		free(seuser);
		free(level);
		return 5;
	}

	n = get_ordered_context_list_with_level(seuser, level, argv[2], &contextlist);
	if (n < 0) {
		fprintf(stderr, "get_ordered_context_list_with_level failed:  %s\n", strerror(errno));
		free(seuser);
		free(level);
		return 3;
	}

	free(seuser);
	free(level);

	if (n == 0)
		printf("no valid context found\n");

	for (i = 0; i < n; i++)
		printf("Context %d\t%s\n", i, contextlist[i]);

	freeconary(contextlist);

	return EXIT_SUCCESS;
}
