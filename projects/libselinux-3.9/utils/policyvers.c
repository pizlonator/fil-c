#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <selinux/selinux.h>

int main(int argc __attribute__ ((unused)), char **argv)
{
	int rc;

	rc = security_policyvers();
	if (rc < 0) {
		fprintf(stderr, "%s:  security_policyvers() failed:  %s\n", argv[0], strerror(errno));
		exit(2);
	}

	printf("%d\n", rc);
	exit(EXIT_SUCCESS);
}
