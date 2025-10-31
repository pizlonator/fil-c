#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <selinux/selinux.h>
#include <syslog.h>
#include <pwd.h>
#include <string.h>

/* Attempt to rollback the transaction. No need to check error
   codes since this is rolling back something that blew up. */
static __attribute__ ((__noreturn__)) void rollback(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++)
		security_set_boolean(argv[i],
				     security_get_boolean_active(argv[i]));
	exit(1);
}

int main(int argc, char **argv)
{

	int rc, i, commit = 0;

	if (is_selinux_enabled() <= 0) {
		fprintf(stderr, "%s:  SELinux is disabled\n", argv[0]);
		return 1;
	}

	if (argc < 2) {
		printf("Usage:  %s boolname1 [boolname2 ...]\n",
		       basename(argv[0]));
		return 1;
	}

	for (i = 1; i < argc; i++) {
		printf("%s: ", argv[i]);
		rc = security_get_boolean_active(argv[i]);
		switch (rc) {
		case 1:
			if (security_set_boolean(argv[i], 0) >= 0) {
				printf("inactive\n");
				commit++;
			} else {
				printf("%s - rolling back all changes\n",
				       strerror(errno));
				rollback(i, argv);
			}
			break;
		case 0:
			if (security_set_boolean(argv[i], 1) >= 0) {
				printf("active\n");
				commit++;
			} else {
				printf("%s - rolling back all changes\n",
				       strerror(errno));
				rollback(i, argv);
			}
			break;
		default:
			if (errno == ENOENT)
				printf
				    ("Boolean does not exist - rolling back all changes.\n");
			else
				printf("%s - rolling back all changes.\n",
				       strerror(errno));
			rollback(i, argv);
			break;	/* Not reached. */
		}
	}

	if (commit > 0) {
		if (security_commit_booleans() < 0) {
			printf("Commit failed. (%s)  No change to booleans.\n",
			       strerror(errno));
		} else {
			/* syslog all the changes */
			struct passwd *pwd = getpwuid(getuid());
			for (i = 1; i < argc; i++) {
				if (pwd && pwd->pw_name)
					syslog(LOG_NOTICE,
					       "The %s policy boolean was toggled by %s",
					       argv[i], pwd->pw_name);
				else
					syslog(LOG_NOTICE,
					       "The %s policy boolean was toggled by uid:%u",
					       argv[i], getuid());

			}
			return 0;
		}
	}
	return 1;
}
