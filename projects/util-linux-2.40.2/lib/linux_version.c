/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#include <stdio.h>
#include <sys/utsname.h>

#include "c.h"
#include "linux_version.h"

int get_linux_version (void)
{
	static int kver = -1;
	struct utsname uts;
	int x = 0, y = 0, z = 0;
	int n;

	if (kver != -1)
		return kver;
	if (uname(&uts))
		return kver = 0;

	n = sscanf(uts.release, "%d.%d.%d", &x, &y, &z);
	if (n < 1 || n > 3)
		return kver = 0;

	return kver = KERNEL_VERSION(x, y, z);
}

#ifdef TEST_PROGRAM_LINUXVERSION
# include <stdlib.h>
int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;

	if (argc == 1) {
		printf("Linux version: %d\n", get_linux_version());
		rc = EXIT_SUCCESS;

	} else if (argc == 5) {
		const char *oper = argv[1];

		int x = atoi(argv[2]),
		    y = atoi(argv[3]),
		    z = atoi(argv[4]);
		int kver = get_linux_version();
		int uver = KERNEL_VERSION(x, y, z);

		if (strcmp(oper, "==") == 0)
			rc = kver == uver;
		else if (strcmp(oper, "<=") == 0)
			rc = kver <= uver;
		else if (strcmp(oper, ">=") == 0)
			rc = kver >= uver;
		else
			errx(EXIT_FAILURE, "unsupported operator");

		if (rc)
			printf("match\n");
		else
			printf("not-match [%d %s %d, x.y.z: %d.%d.%d]\n",
					kver, oper, uver, x, y, z);

		rc = rc ? EXIT_SUCCESS : EXIT_FAILURE;

	} else
		 fprintf(stderr, "Usage:\n"
				 "   %s [<oper> <x> <y> <z>]\n"
				 "supported operators:\n"
				 "   ==, <=, >=\n",
				 program_invocation_short_name);

	return rc;
}
#endif
