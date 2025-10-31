#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <selinux/selinux.h>

int main(int argc, char **argv)
{
	pid_t pid;
	char *buf;
	int rc;

	if (argc != 2) {
		fprintf(stderr, "usage:  %s pid\n", argv[0]);
		exit(1);
	}

	if (sscanf(argv[1], "%d", &pid) != 1) {
		fprintf(stderr, "%s:  invalid pid %s\n", argv[0], argv[1]);
		exit(2);
	}

	rc = getpidcon(pid, &buf);
	if (rc < 0) {
		fprintf(stderr, "%s:  getpidcon() failed:  %s\n", argv[0], strerror(errno));
		exit(3);
	}

	printf("%s\n", buf);
	freecon(buf);
	exit(EXIT_SUCCESS);
}
