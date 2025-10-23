#include "pam_modutil_private.h"
#include <security/pam_ext.h>

#include <stdio.h>
#include <string.h>
#include <syslog.h>

int
pam_modutil_check_user_in_passwd(pam_handle_t *pamh,
				 const char *user_name,
				 const char *file_name)
{
	int rc, c = EOF;
	FILE *fp;

	/* Validate the user name.  */
	if (user_name[0] == '\0') {
		pam_syslog(pamh, LOG_NOTICE, "user name is not valid");
		return PAM_SERVICE_ERR;
	}

	if (strchr(user_name, ':') != NULL) {
		/*
		 * "root:x" is not a local user name even if the passwd file
		 * contains a line starting with "root:x:".
		 */
		return PAM_PERM_DENIED;
	}

	/* Open the passwd file.  */
	if (file_name == NULL) {
		file_name = "/etc/passwd";
	}
	if ((fp = fopen(file_name, "r")) == NULL) {
		pam_syslog(pamh, LOG_ERR, "error opening %s: %m", file_name);
		return PAM_SERVICE_ERR;
	}

	/*
	 * Scan the file using fgetc() instead of fgetpwent_r() because
	 * the latter is not flexible enough in handling long lines
	 * in passwd files.
	 */
	rc = PAM_PERM_DENIED;
	do {
		const char *p;

		/*
		 * Does this line start with the user name
		 * followed by a colon?
		 */
		for (p = user_name; *p != '\0'; p++) {
			c = fgetc(fp);
			if (c == EOF || c == '\n' || (char)c != *p)
				break;
		}

		if (c != EOF && c != '\n')
			c = fgetc(fp);

		if (*p == '\0' && c == ':') {
			rc = PAM_SUCCESS;
			/*
			 * Continue reading the file to avoid timing attacks.
			 */
		}

		/* Read till the end of this line.  */
		while (c != EOF && c != '\n')
			c = fgetc(fp);

		/* Continue with the next line.  */
	} while (c != EOF);

	fclose(fp);
	return rc;
}
