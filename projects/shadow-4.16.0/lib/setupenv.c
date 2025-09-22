/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Separated from setup.c.  --marekm
 */

#include <config.h>

#ident "$Id$"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>

#include "alloc.h"
#include "prototypes.h"
#include "defines.h"
#include <pwd.h>
#include "getdef.h"
#include "shadowlog.h"
#include "string/sprintf.h"


#ifndef USE_PAM
static void
addenv_path(const char *varname, const char *dirname, const char *filename)
{
	char  *buf;

	xasprintf(&buf, "%s/%s", dirname, filename);
	addenv(varname, buf);
	free(buf);
}

static void read_env_file (const char *filename)
{
	FILE *fp;
	char buf[1024];
	char *cp, *name, *val;

	fp = fopen (filename, "r");
	if (NULL == fp) {
		return;
	}
	while (fgets (buf, (int)(sizeof buf), fp) == buf) {
		cp = strrchr (buf, '\n');
		if (NULL == cp) {
			break;
		}
		*cp = '\0';

		cp = buf;
		/* ignore whitespace and comments */
		while (isspace (*cp)) {
			cp++;
		}
		if (('\0' == *cp) || ('#' == *cp)) {
			continue;
		}
		/*
		 * ignore lines which don't follow the name=value format
		 * (for example, the "export NAME" shell commands)
		 */
		name = cp;
		while (('\0' != *cp) && !isspace (*cp) && ('=' != *cp)) {
			cp++;
		}
		if ('=' != *cp) {
			continue;
		}
		/* NUL-terminate the name */
		*cp = '\0';
		cp++;
		val = cp;
#if 0				/* XXX untested, and needs rewrite with fewer goto's :-) */
/*
 (state, char_type) -> (state, action)

 state: unquoted, single_quoted, double_quoted, escaped, double_quoted_escaped
 char_type: normal, white, backslash, single, double
 action: remove_curr, remove_curr_skip_next, remove_prev, finish XXX
*/
	      no_quote:
		if (*cp == '\\') {
			/* remove the backslash */
			remove_char (cp);
			/* skip over the next character */
			if (*cp)
				cp++;
			goto no_quote;
		} else if (*cp == '\'') {
			/* remove the quote */
			remove_char (cp);
			/* now within single quotes */
			goto s_quote;
		} else if (*cp == '"') {
			/* remove the quote */
			remove_char (cp);
			/* now within double quotes */
			goto d_quote;
		} else if (*cp == '\0') {
			/* end of string */
			goto finished;
		} else if (isspace (*cp)) {
			/* unescaped whitespace - end of string */
			*cp = '\0';
			goto finished;
		} else {
			cp++;
			goto no_quote;
		}
	      s_quote:
		if (*cp == '\'') {
			/* remove the quote */
			remove_char (cp);
			/* unquoted again */
			goto no_quote;
		} else if (*cp == '\0') {
			/* end of string */
			goto finished;
		} else {
			/* preserve everything within single quotes */
			cp++;
			goto s_quote;
		}
	      d_quote:
		if (*cp == '\"') {
			/* remove the quote */
			remove_char (cp);
			/* unquoted again */
			goto no_quote;
		} else if (*cp == '\\') {
			cp++;
			/* if backslash followed by double quote, remove backslash
			   else skip over the backslash and following char */
			if (*cp == '"')
				remove_char (cp - 1);
			else if (*cp)
				cp++;
			goto d_quote;
		}
		else if (*cp == '\0') {
			/* end of string */
			goto finished;
		} else {
			/* preserve everything within double quotes */
			goto d_quote;
		}
	      finished:
#endif				/* 0 */
		/*
		 * XXX - should handle quotes, backslash escapes, etc.
		 * like the shell does.
		 */
		addenv (name, val);
	}
	(void) fclose (fp);
}
#endif				/* USE_PAM */


/*
 *	change to the user's home directory
 *	set the HOME, SHELL, MAIL, PATH, and LOGNAME or USER environmental
 *	variables.
 */

void setup_env (struct passwd *info)
{
#ifndef USE_PAM
	const char *envf;
#endif
	const char *cp;

	/*
	 * Change the current working directory to be the home directory
	 * of the user.  It is a fatal error for this process to be unable
	 * to change to that directory.  There is no "default" home
	 * directory.
	 *
	 * We no longer do it as root - should work better on NFS-mounted
	 * home directories.  Some systems default to HOME=/, so we make
	 * this a configurable option.  --marekm
	 */

	if (chdir (info->pw_dir) == -1) {
		if (!getdef_bool ("DEFAULT_HOME") || chdir ("/") == -1) {
			fprintf (log_get_logfd(), _("Unable to cd to '%s'\n"),
				 info->pw_dir);
			SYSLOG ((LOG_WARN,
				 "unable to cd to `%s' for user `%s'\n",
				 info->pw_dir, info->pw_name));
			closelog ();
			exit (EXIT_FAILURE);
		}
		(void) puts (_("No directory, logging in with HOME=/"));
		free (info->pw_dir);
		info->pw_dir = xstrdup ("/");
	}

	/*
	 * Create the HOME environmental variable and export it.
	 */

	addenv ("HOME", info->pw_dir);

	/*
	 * Create the SHELL environmental variable and export it.
	 */

	if ((NULL == info->pw_shell) || ('\0' == *info->pw_shell)) {
		free (info->pw_shell);
		info->pw_shell = xstrdup (SHELL);
	}

	addenv ("SHELL", info->pw_shell);

	/*
	 * Export the user name.  For BSD derived systems, it's "USER", for
	 * all others it's "LOGNAME".  We set both of them.
	 */

	addenv ("USER", info->pw_name);
	addenv ("LOGNAME", info->pw_name);

	/*
	 * Create the PATH environmental variable and export it.
	 */

	cp = getdef_str ((info->pw_uid == 0) ? "ENV_SUPATH" : "ENV_PATH");

	if (NULL == cp) {
		/* not specified, use a minimal default */
		addenv ((info->pw_uid == 0) ? "PATH=/sbin:/bin:/usr/sbin:/usr/bin" : "PATH=/bin:/usr/bin", NULL);
	} else if (strchr (cp, '=')) {
		/* specified as name=value (PATH=...) */
		addenv (cp, NULL);
	} else {
		/* only value specified without "PATH=" */
		addenv ("PATH", cp);
	}

#ifndef USE_PAM
	/*
	 * Create the MAIL environmental variable and export it.  login.defs
	 * knows the prefix.
	 */

	if (getdef_bool ("MAIL_CHECK_ENAB")) {
		cp = getdef_str ("MAIL_DIR");
		if (NULL != cp) {
			addenv_path ("MAIL", cp, info->pw_name);
		} else {
			cp = getdef_str ("MAIL_FILE");
			if (NULL != cp) {
				addenv_path ("MAIL", info->pw_dir, cp);
			} else {
#if defined(MAIL_SPOOL_FILE)
				addenv_path ("MAIL", info->pw_dir, MAIL_SPOOL_FILE);
#elif defined(MAIL_SPOOL_DIR)
				addenv_path ("MAIL", MAIL_SPOOL_DIR, info->pw_name);
#endif
			}
		}
	}

	/*
	 * Read environment from optional config file.  --marekm
	 */
	envf = getdef_str ("ENVIRON_FILE");
	if (NULL != envf) {
		read_env_file (envf);
	}
#endif				/* !USE_PAM */
}

