#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <selinux/selinux.h>

static __attribute__ ((__noreturn__)) void usage(const char *progname)
{
	fprintf(stderr, "usage:  %s [-a auditdata] scon tcon class perm\n"
		"\nWhere:\n\t"
		"-a  Optional information added to audit message.\n",
		progname);
	exit(1);
}

static int cb_auditinfo(void *auditdata,
			__attribute__((unused))security_class_t class,
			char *msgbuf, size_t msgbufsize)
{
	return snprintf(msgbuf, msgbufsize, "%s", (char *)auditdata);
}

int main(int argc, char **argv)
{
	int opt, rc;
	char *audit_msg = NULL;

	while ((opt = getopt(argc, argv, "a:")) != -1) {
		switch (opt) {
		case 'a':
			audit_msg = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 4)
		usage(argv[0]);

	if (audit_msg)
		selinux_set_callback(SELINUX_CB_AUDIT,
				     (union selinux_callback) { .func_audit = cb_auditinfo });

	rc = selinux_check_access(argv[optind], argv[optind + 1],
				  argv[optind + 2], argv[optind + 3],
				  audit_msg);
	if (rc < 0)
		perror("selinux_check_access");

	return rc;
}
