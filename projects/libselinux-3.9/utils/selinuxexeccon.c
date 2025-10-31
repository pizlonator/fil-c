#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <selinux/selinux.h>

static __attribute__ ((__noreturn__)) void usage(const char *name, const char *detail, int rc)
{
	fprintf(stderr, "usage:  %s command [ fromcon ]\n", name);
	if (detail)
		fprintf(stderr, "%s:  %s\n", name, detail);
	exit(rc);
}

static char * get_selinux_proc_context(const char *command, const char * execcon) {
	char * fcon = NULL, *newcon = NULL;

	int ret = getfilecon(command, &fcon);
	if (ret < 0) goto err;
	ret = security_compute_create(execcon, fcon, string_to_security_class("process"), &newcon);
	if (ret < 0) goto err;

err:
	freecon(fcon);
	return newcon;
}

int main(int argc, char **argv)
{
	int ret = -1;
	char * proccon = NULL, *con = NULL;
	if (argc < 2 || argc > 3)
		usage(argv[0], "Invalid number of arguments", -1);

	if (argc == 2) {
		if (getcon(&con) < 0) {
			perror(argv[0]);
			return -1;
		}
	} else {
		con = strdup(argv[2]);
		if (security_check_context(con)) {
			fprintf(stderr, "%s:  invalid from context '%s'\n", argv[0], con);
			free(con);
			return -1;
		}
	}

	proccon = get_selinux_proc_context(argv[1], con);
	if (proccon) {
		printf("%s\n", proccon);
		ret = 0;
	} else {
		perror(argv[0]);
	}

	free(proccon);
	free(con);
	return ret;
}
