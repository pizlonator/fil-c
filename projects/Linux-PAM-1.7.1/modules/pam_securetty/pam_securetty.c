/*
 * pam_securetty module
 *
 * by Elliot Lee <sopwith@redhat.com>, Red Hat Software.
 * July 25, 1996.
 * This code shamelessly ripped from the pam_rootok module.
 * Slight modifications AGM. 1996/12/3
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"

#define PAM_DEBUG_ARG       0x0001
#define PAM_NOCONSOLE_ARG   0x0002

#define SECURETTY_FILE "/etc/securetty"
#ifdef VENDORDIR
#define SECURETTY2_FILE VENDORDIR"/securetty"
#endif
#define TTY_PREFIX     "/dev/"
#define CMDLINE_FILE   "/proc/cmdline"
#define CONSOLEACTIVE_FILE	"/sys/class/tty/console/active"

static int
_pam_parse (const pam_handle_t *pamh, int argc, const char **argv)
{
    int ctrl=0;

    /* step through arguments */
    for (ctrl=0; argc-- > 0; ++argv) {

	/* generic options */

	if (!strcmp(*argv,"debug"))
	    ctrl |= PAM_DEBUG_ARG;
        else if (!strcmp(*argv, "noconsole"))
            ctrl |= PAM_NOCONSOLE_ARG;
	else {
	    pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	}
    }

    return ctrl;
}

static int
securetty_perform_check (pam_handle_t *pamh, int ctrl,
			 const char *function_name)
{
    int retval = PAM_AUTH_ERR;
    const char *securettyfile;
    const char *username;
    const char *uttyname;
    const char *str;
    const void *void_uttyname;
    char *ttyfileline = NULL;
    size_t ttyfilelinelen = 0;
    char ptname[256];
    struct stat ttyfileinfo;
    struct passwd *user_pwd;
    FILE *ttyfile;

    /* log a trail for debugging */
    if (ctrl & PAM_DEBUG_ARG) {
        pam_syslog(pamh, LOG_DEBUG, "pam_securetty called via %s function",
		   function_name);
    }

    retval = pam_get_user(pamh, &username, NULL);
    if (retval != PAM_SUCCESS) {
	pam_syslog(pamh, LOG_NOTICE, "cannot determine user name: %s",
		   pam_strerror(pamh, retval));
	return (retval == PAM_CONV_AGAIN ? PAM_INCOMPLETE : retval);
    }

    user_pwd = pam_modutil_getpwnam(pamh, username);
    if (user_pwd != NULL && user_pwd->pw_uid != 0) {
	/* If the user is not root, securetty's does not apply to them */
	return PAM_SUCCESS;
    }
    /* The user is now either root or an invalid / mistyped username */

    retval = pam_get_item(pamh, PAM_TTY, &void_uttyname);
    uttyname = void_uttyname;
    if (retval != PAM_SUCCESS || uttyname == NULL) {
        pam_syslog (pamh, LOG_ERR, "cannot determine user's tty");
	return PAM_SERVICE_ERR;
    }

    /* The PAM_TTY item may be prefixed with "/dev/" - skip that */
    if ((str = pam_str_skip_prefix(uttyname, TTY_PREFIX)) != NULL)
	uttyname = str;

    if (stat(SECURETTY_FILE, &ttyfileinfo)) {
#ifdef VENDORDIR
      if (errno == ENOENT) {
	if (stat(SECURETTY2_FILE, &ttyfileinfo)) {
	  if (ctrl & PAM_DEBUG_ARG)
	    pam_syslog(pamh, LOG_DEBUG,
		     "Couldn't open %s: %m", SECURETTY2_FILE);
	  return PAM_SUCCESS; /* for compatibility with old securetty handling,
				 this needs to succeed.  But we still log the
				 error. */
	}
	securettyfile = SECURETTY2_FILE;
      } else {
#endif
	if (ctrl & PAM_DEBUG_ARG)
	  pam_syslog(pamh, LOG_DEBUG, "Couldn't open %s: %m", SECURETTY_FILE);
	return PAM_SUCCESS; /* for compatibility with old securetty handling,
			       this needs to succeed.  But we still log the
			       error. */
#ifdef VENDORDIR
      }
#endif
    } else {
      securettyfile = SECURETTY_FILE;
    }

    if ((ttyfileinfo.st_mode & S_IWOTH) || !S_ISREG(ttyfileinfo.st_mode)) {
	/* If the file is world writable or is not a
	   normal file, return error */
	pam_syslog(pamh, LOG_ERR,
		   "%s is either world writable or not a normal file",
		   securettyfile);
	return PAM_AUTH_ERR;
    }

    ttyfile = fopen(securettyfile,"r");
    if (ttyfile == NULL) { /* Check that we opened it successfully */
	pam_syslog(pamh, LOG_ERR, "Error opening %s: %m", securettyfile);
	return PAM_SERVICE_ERR;
    }

    if (isdigit((unsigned char)uttyname[0])) {
	pam_sprintf(ptname, "pts/%s", uttyname);
    } else {
	ptname[0] = '\0';
    }

    retval = 1;

    while (retval && getline(&ttyfileline, &ttyfilelinelen, ttyfile) != -1) {
	ttyfileline[strcspn(ttyfileline, "\n")] = '\0';

	retval = ( strcmp(ttyfileline, uttyname)
		   && (!ptname[0] || strcmp(ptname, uttyname)) );
    }
    free(ttyfileline);
    fclose(ttyfile);

    if (retval && !(ctrl & PAM_NOCONSOLE_ARG)) {
        FILE *cmdlinefile;

        /* Allow access from the kernel console, if enabled */
        cmdlinefile = fopen(CMDLINE_FILE, "r");

        if (cmdlinefile != NULL) {
            char *p;
            char *line = NULL;
            size_t linelen = 0;

            if (getline(&line, &linelen, cmdlinefile) == -1)
                p = NULL;
            else
                p = line;
            fclose(cmdlinefile);

            for (; p; p = strstr(p+1, "console=")) {
                const char *e;

                /* Test whether this is a beginning of a word? */
                if (p > line && p[-1] != ' ')
                    continue;

                /* Is this our console? */
                if ((e = pam_str_skip_prefix_len(p + 8, uttyname, strlen(uttyname))) == NULL)
                    continue;

                /* Is there any garbage after the TTY name? */
                if (*e == ',' || *e == ' ' || *e == '\n' || *e == 0) {
                    retval = 0;
                    break;
                }
            }

            free(line);
        }
    }
    if (retval && !(ctrl & PAM_NOCONSOLE_ARG)) {
        FILE *consoleactivefile;

        /* Allow access from the active console */
        consoleactivefile = fopen(CONSOLEACTIVE_FILE, "r");

        if (consoleactivefile != NULL) {
            char *p, *n;
            char *line = NULL;
            size_t linelen = 0;

            if (getline(&line, &linelen, consoleactivefile) == -1)
                p = NULL;
            else
                p = line;
            fclose(consoleactivefile);

	    if (p) {
		/* remove the newline character at end */
		line[strcspn(line, "\n")] = '\0';

		for (n = p; n != NULL; p = n+1) {
		    if ((n = strchr(p, ' ')) != NULL)
			*n = '\0';

		    if (strcmp(p, uttyname) == 0) {
			retval = 0;
			break;
		    }
		}
	    }

	    free(line);
	}
    }

    if (retval) {
	    pam_syslog(pamh, LOG_NOTICE, "access denied: tty '%s' is not secure !",
		     uttyname);

	    retval = PAM_AUTH_ERR;
	    if (user_pwd == NULL) {
		retval = PAM_USER_UNKNOWN;
	    }
    } else {
	if (ctrl & PAM_DEBUG_ARG) {
	    pam_syslog(pamh, LOG_DEBUG, "access allowed for '%s' on '%s'",
		     username, uttyname);
	}
	retval = PAM_SUCCESS;

    }

    return retval;
}

/* --- authentication management functions --- */

int pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED, int argc,
			const char **argv)
{
    int ctrl;

    /* parse the arguments */
    ctrl = _pam_parse (pamh, argc, argv);

    return securetty_perform_check(pamh, ctrl, __FUNCTION__);
}

int
pam_sm_setcred (pam_handle_t *pamh UNUSED, int flags UNUSED,
		int argc UNUSED, const char **argv UNUSED)
{
    return PAM_SUCCESS;
}

/* --- account management functions --- */

int
pam_sm_acct_mgmt (pam_handle_t *pamh, int flags UNUSED,
		  int argc, const char **argv)
{
    int ctrl;

    /* parse the arguments */
    ctrl = _pam_parse (pamh, argc, argv);

    /* take the easy route */
    return securetty_perform_check(pamh, ctrl, __FUNCTION__);
}

/* end of module definition */
