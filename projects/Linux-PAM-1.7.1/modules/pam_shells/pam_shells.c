/*
 * pam_shells module
 *
 * by Erik Troan <ewt@redhat.com>, Red Hat Software.
 * August 5, 1996.
 * This code shamelessly ripped from the pam_securetty module.
 */

#include "config.h"

#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#if defined (USE_ECONF)	&& defined (VENDORDIR)
#include "pam_econf.h"
#endif

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>

#define SHELL_FILE "/etc/shells"
#define SHELLS "shells"
#define ETCDIR "/etc"
#define DEFAULT_SHELL "/bin/sh"

static bool check_file(const char *filename, const void *pamh)
{
    struct stat sb;

    if (stat(filename, &sb)) {
	pam_syslog(pamh, LOG_ERR, "Cannot stat %s: %m", filename);
	return false;		/* must have /etc/shells */
    }

    if ((sb.st_mode & S_IWOTH) || !S_ISREG(sb.st_mode)) {
	pam_syslog(pamh, LOG_ERR,
		   "%s is either world writable or not a normal file",
		   filename);
	return false;
    }
    return true;
}

static int perform_check(pam_handle_t *pamh)
{
    int retval = PAM_AUTH_ERR;
    const char *userName;
    const char *userShell;
    struct passwd * pw;

    retval = pam_get_user(pamh, &userName, NULL);
    if (retval != PAM_SUCCESS) {
	return PAM_SERVICE_ERR;
    }

    pw = pam_modutil_getpwnam(pamh, userName);
    if (pw == NULL) {
	return PAM_USER_UNKNOWN;
    }
    if (pw->pw_shell == NULL) {
	/* TODO: when does this happen? I would join it with
	 * the case userShell[0] == '\0' below.
	 *
	 * For now, keep the existing stricter behaviour
	 */
	return PAM_AUTH_ERR;
    }
    userShell = pw->pw_shell;
    if (userShell[0] == '\0')
	userShell = DEFAULT_SHELL;

#if defined (USE_ECONF)	&& defined (VENDORDIR)
    size_t size = 0;
    econf_err error;
    char **keys;
    econf_file *key_file = NULL;

    error = pam_econf_readconfig(&key_file,
				 VENDORDIR,
				 ETCDIR,
				 SHELLS,
				 NULL,
				 "", /* key only */
				 "#", /* comment */
				 check_file, pamh);
    if (error != ECONF_SUCCESS) {
	pam_syslog(pamh, LOG_ERR,
		   "Cannot parse shell files: %s",
		   econf_errString(error));
	return PAM_AUTH_ERR;
    }

    error = econf_getKeys(key_file, NULL, &size, &keys);
    if (error) {
	pam_syslog(pamh, LOG_ERR,
		   "Cannot evaluate entries in shell files: %s",
		   econf_errString(error));
	econf_free (key_file);
	return PAM_AUTH_ERR;
    }

    retval = 1;
    for (size_t i = 0; i < size; i++) {
	retval = strcmp(keys[i], userShell);
        if (!retval)
	   break;
    }
    econf_free (keys);
    econf_free (key_file);
#else
    FILE *shellFile;
    char *p = NULL;
    size_t n = 0;

    if (!check_file(SHELL_FILE, pamh))
        return PAM_AUTH_ERR;

    shellFile = fopen(SHELL_FILE,"r");
    if (shellFile == NULL) {       /* Check that we opened it successfully */
	pam_syslog(pamh, LOG_ERR, "Error opening %s: %m", SHELL_FILE);
	return PAM_SERVICE_ERR;
    }

    retval = 1;

    while (retval && getline(&p, &n, shellFile) != -1) {
	p[strcspn(p, "\n")] = '\0';

	if (p[0] != '/') {
		continue;
	}
	retval = strcmp(p, userShell);
    }

    free(p);
    fclose(shellFile);
#endif

    if (retval) {
	pam_syslog(pamh, LOG_NOTICE, "User has an invalid shell '%s'", userShell);
	return PAM_AUTH_ERR;
    } else {
	return PAM_SUCCESS;
    }
}

/* --- authentication management functions (only) --- */

int pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		        int argc UNUSED, const char **argv UNUSED)
{
    return perform_check(pamh);
}

int pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
		   int argc UNUSED, const char **argv UNUSED)
{
     return PAM_SUCCESS;
}

/* --- account management functions (only) --- */

int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		     int argc UNUSED, const char **argv UNUSED)
{
    return perform_check(pamh);
}

/* end of module definition */
