/*
 *   chsh.c -- change your login shell
 *   (c) 1994 by salvatore valente <svalente@athena.mit.edu>
 *   (c) 2012 by Cody Maloney <cmaloney@theoreticalchaos.com>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 *   $Author: aebr $
 *   $Revision: 1.19 $
 *   $Date: 1998/06/11 22:30:14 $
 *
 * Updated Thu Oct 12 09:33:15 1995 by faith@cs.unc.edu with security
 *   patches from Zefram <A.Main@dcs.warwick.ac.uk>
 *
 * Updated Mon Jul  1 18:46:22 1996 by janl@math.uio.no with security
 *   suggestion from Zefram.  Disallowing users with shells not in /etc/shells
 *   from changing their shell.
 *
 *   1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 *   - added Native Language Support
 */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "env.h"
#include "closestream.h"
#include "islocal.h"
#include "nls.h"
#include "pathnames.h"
#include "pwdutils.h"
#include "setpwnam.h"
#include "strutils.h"
#include "xalloc.h"

#include "ch-common.h"
#include "shells.h"

#ifdef HAVE_LIBSELINUX
# include <selinux/selinux.h>
# include "selinux-utils.h"
#endif

#ifdef HAVE_LIBUSER
# include <libuser/user.h>
# include "libuser.h"
#elif CHFN_CHSH_PASSWORD
# include "auth.h"
#endif

struct sinfo {
	char *username;
	char *shell;
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *fp = stdout;
	fputs(USAGE_HEADER, fp);
	fprintf(fp, _(" %s [options] [<username>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, fp);
	fputs(_("Change your login shell.\n"), fp);

	fputs(USAGE_OPTIONS, fp);
	fputs(_(" -s, --shell <shell>  specify login shell\n"), fp);
	fputs(_(" -l, --list-shells    print list of shells and exit\n"), fp);

	fputs(USAGE_SEPARATOR, fp);
	fprintf(fp, USAGE_HELP_OPTIONS(22));

	fprintf(fp, USAGE_MAN_TAIL("chsh(1)"));
	exit(EXIT_SUCCESS);
}

/*
 *  parse_argv () --
 *	parse the command line arguments, and fill in "pinfo" with any
 *	information from the command line.
 */
static void parse_argv(int argc, char **argv, struct sinfo *pinfo)
{
	static const struct option long_options[] = {
		{"shell",       required_argument, NULL, 's'},
		{"list-shells", no_argument,       NULL, 'l'},
		{"help",        no_argument,       NULL, 'h'},
		{"version",     no_argument,       NULL, 'V'},
		{NULL, 0, NULL, 0},
	};
	int c;

	while ((c = getopt_long(argc, argv, "s:lhuvV", long_options, NULL)) != -1) {
		switch (c) {
		case 'v': /* deprecated */
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'u': /* deprecated */
		case 'h':
			usage();
		case 'l':
			print_shells(stdout, "%s\n");
			exit(EXIT_SUCCESS);
		case 's':
			pinfo->shell = optarg;
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}
	/* done parsing arguments.  check for a username. */
	if (optind < argc) {
		if (optind + 1 < argc) {
			errx(EXIT_FAILURE, _("cannot handle multiple usernames"));
		}
		pinfo->username = argv[optind];
	}
}

/*
 *  ask_new_shell () --
 *	ask the user for a shell and return it.
 */
static char *ask_new_shell(char *question, char *oldshell)
{
	int len;
	char *ans = NULL;
	size_t dummy = 0;

	if (!oldshell)
		oldshell = "";
	printf("%s [%s]:", question, oldshell);

	putchar(' ');
	fflush(stdout);

	if (getline(&ans, &dummy, stdin) < 0)
		return NULL;

	/* remove the newline at the end of ans. */
	ltrim_whitespace((unsigned char *) ans);
	len = rtrim_whitespace((unsigned char *) ans);
	if (len == 0)
		return NULL;
	return ans;
}

/*
 *  check_shell () -- if the shell is completely invalid, print
 *	an error and exit.
 */
static void check_shell(const char *shell)
{
	if (*shell != '/')
		errx(EXIT_FAILURE, _("shell must be a full path name"));
	if (access(shell, F_OK) < 0)
		errx(EXIT_FAILURE, _("\"%s\" does not exist"), shell);
	if (access(shell, X_OK) < 0)
		errx(EXIT_FAILURE, _("\"%s\" is not executable"), shell);
	if (illegal_passwd_chars(shell))
		errx(EXIT_FAILURE, _("%s: has illegal characters"), shell);
	if (!is_known_shell(shell)) {
#ifdef ONLY_LISTED_SHELLS
		if (!getuid())
			warnx(_("Warning: \"%s\" is not listed in %s."), shell,
			      _PATH_SHELLS);
		else
			errx(EXIT_FAILURE,
			     _("\"%s\" is not listed in %s.\n"
			       "Use %s -l to see list."), shell, _PATH_SHELLS,
			     program_invocation_short_name);
#else
		warnx(_("\"%s\" is not listed in %s.\n"
			"Use %s -l to see list."), shell, _PATH_SHELLS,
		       program_invocation_short_name);
#endif
	}
}

int main(int argc, char **argv)
{
	char *oldshell, *pwbuf;
	int nullshell = 0;
	const uid_t uid = getuid();
	struct sinfo info = { NULL };
	struct passwd *pw;

	sanitize_env();
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	parse_argv(argc, argv, &info);
	if (!info.username) {
		pw = xgetpwuid(uid, &pwbuf);
		if (!pw)
			errx(EXIT_FAILURE, _("you (user %d) don't exist."),
			     uid);
	} else {
		pw = xgetpwnam(info.username, &pwbuf);
		if (!pw)
			errx(EXIT_FAILURE, _("user \"%s\" does not exist."),
			     info.username);
	}

#ifndef HAVE_LIBUSER
	if (!(is_local(pw->pw_name)))
		errx(EXIT_FAILURE, _("can only change local entries"));
#endif

#ifdef HAVE_LIBSELINUX
	if (is_selinux_enabled() > 0) {
		char *user_cxt = NULL;

		if (uid == 0 && !ul_selinux_has_access("passwd", "chsh", &user_cxt))
			errx(EXIT_FAILURE,
			     _("%s is not authorized to change the shell of %s"),
			     user_cxt ? : _("Unknown user context"),
			     pw->pw_name);

		if (ul_setfscreatecon_from_file(_PATH_PASSWD) != 0)
			errx(EXIT_FAILURE,
			     _("can't set default context for %s"), _PATH_PASSWD);
	}
#endif

	oldshell = pw->pw_shell;
	if (oldshell == NULL || *oldshell == '\0') {
		oldshell = _PATH_BSHELL;	/* default */
		nullshell = 1;
	}

	/* reality check */
#ifdef HAVE_LIBUSER
	/* If we're setuid and not really root, disallow the password change. */
	if (geteuid() != getuid() && uid != pw->pw_uid) {
#else
	if (uid != 0 && uid != pw->pw_uid) {
#endif
		errno = EACCES;
		err(EXIT_FAILURE,
		    _("running UID doesn't match UID of user we're "
		      "altering, shell change denied"));
	}
	if (uid != 0 && !is_known_shell(oldshell)) {
		errno = EACCES;
		err(EXIT_FAILURE, _("your shell is not in %s, "
				    "shell change denied"), _PATH_SHELLS);
	}

	printf(_("Changing shell for %s.\n"), pw->pw_name);

#if !defined(HAVE_LIBUSER) && defined(CHFN_CHSH_PASSWORD)
	if (!auth_pam("chsh", uid, pw->pw_name)) {
		return EXIT_FAILURE;
	}
#endif
	if (!info.shell) {
		info.shell = ask_new_shell(_("New shell"), oldshell);
		if (!info.shell)
			return EXIT_SUCCESS;
	}

	check_shell(info.shell);

	if (!nullshell && strcmp(oldshell, info.shell) == 0)
		errx(EXIT_SUCCESS, _("Shell not changed."));

#ifdef HAVE_LIBUSER
	if (set_value_libuser("chsh", pw->pw_name, uid,
	    LU_LOGINSHELL, info.shell) < 0)
		errx(EXIT_FAILURE, _("Shell *NOT* changed.  Try again later."));
#else
	pw->pw_shell = info.shell;
	if (setpwnam(pw, ".chsh") < 0)
		err(EXIT_FAILURE, _("setpwnam failed\n"
			"Shell *NOT* changed.  Try again later."));
#endif

	printf(_("Shell changed.\n"));
	return EXIT_SUCCESS;
}
