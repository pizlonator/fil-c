#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <selinux/selinux.h>

int main(int argc, char **argv)
{
	char *buf;
	security_class_t tclass;
	int ret;

	if (argc != 4) {
		fprintf(stderr, "usage:  %s scontext tcontext tclass\n",
			argv[0]);
		exit(1);
	}

	if (security_check_context(argv[1])) {
		fprintf(stderr, "%s:  invalid source context '%s'\n", argv[0], argv[1]);
		exit(4);
	}

	if (security_check_context(argv[2])) {
		fprintf(stderr, "%s:  invalid target context '%s'\n", argv[0], argv[2]);
		exit(5);
	}

	tclass = string_to_security_class(argv[3]);
	if (!tclass) {
		fprintf(stderr, "%s:  invalid class '%s'\n", argv[0], argv[3]);
		exit(2);
	}

	ret = security_compute_member(argv[1], argv[2], tclass, &buf);
	if (ret < 0) {
		fprintf(stderr, "%s:  security_compute_member failed:  %s\n",
			argv[0], strerror(errno));
		exit(3);
	}

	printf("%s\n", buf);
	freecon(buf);
	exit(EXIT_SUCCESS);
}
