/*
 *   chfn.c -- change your finger information
 *   (c) 1994 by salvatore valente <svalente@athena.mit.edu>
 *   (c) 2012 by Cody Maloney <cmaloney@theoreticalchaos.com>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 *   $Author: aebr $
 *   $Revision: 1.18 $
 *   $Date: 1998/06/11 22:30:11 $
 *
 * Updated Thu Oct 12 09:19:26 1995 by faith@cs.unc.edu with security
 * patches from Zefram <A.Main@dcs.warwick.ac.uk>
 *
 * Hacked by Peter Breitenlohner, peb@mppmu.mpg.de,
 * to remove trailing empty fields.  Oct 5, 96.
 *
 *  1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 *  - added Native Language Support
 */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdbool.h>
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
#include "setpwnam.h"
#include "strutils.h"
#include "xalloc.h"
#include "logindefs.h"

#include "ch-common.h"

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

struct finfo {
	char *full_name;
	char *office;
	char *office_phone;
	char *home_phone;
	char *other;
};

struct chfn_control {
	struct passwd *pw;
	char *username;
	/*  "oldf"  Contains the users original finger information.
	 *  "newf"  Contains the changed finger information, and contains
	 *          NULL in fields that haven't been changed.
	 *  In the end, "newf" is folded into "oldf".  */
	struct finfo oldf, newf;
	unsigned int
		allow_fullname:1,	/* The login.defs restriction */
		allow_room:1,		   /* see: man login.defs(5) */
		allow_work:1,		   /* and look for CHFN_RESTRICT */
		allow_home:1,		   /* keyword for these four. */
		changed:1,		/* is change requested */
		interactive:1;		/* whether to prompt for fields or not */
};

/* we do not accept gecos field sizes longer than MAX_FIELD_SIZE */
#define MAX_FIELD_SIZE		256

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *fp = stdout;
	fputs(USAGE_HEADER, fp);
	fprintf(fp, _(" %s [options] [<username>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, fp);
	fputs(_("Change your finger information.\n"), fp);

	fputs(USAGE_OPTIONS, fp);
	fputs(_(" -f, --full-name <full-name>  real name\n"), fp);
	fputs(_(" -o, --office <office>        office number\n"), fp);
	fputs(_(" -p, --office-phone <phone>   office phone number\n"), fp);
	fputs(_(" -h, --home-phone <phone>     home phone number\n"), fp);
	fputs(USAGE_SEPARATOR, fp);
	printf( " -u, --help                   %s\n", USAGE_OPTSTR_HELP);
	printf( " -V, --version                %s\n", USAGE_OPTSTR_VERSION);
	printf(USAGE_MAN_TAIL("chfn(1)"));
	exit(EXIT_SUCCESS);
}

/*
 *  check_gecos_string () --
 *	check that the given gecos string is legal.  if it's not legal,
 *	output "msg" followed by a description of the problem, and return (-1).
 */
static int check_gecos_string(const char *msg, char *gecos)
{
	const size_t len = strlen(gecos);

	if (MAX_FIELD_SIZE < len) {
		warnx(_("field %s is too long"), msg);
		return -1;
	}
	if (illegal_passwd_chars(gecos)) {
		warnx(_("%s: has illegal characters"), gecos);
		return -1;
	}
	return 0;
}

/*
 *  parse_argv () --
 *	parse the command line arguments.
 *	returns true if no information beyond the username was given.
 */
static void parse_argv(struct chfn_control *ctl, int argc, char **argv)
{
	int index, c, status = 0;
	static const struct option long_options[] = {
		{ "full-name",    required_argument, NULL, 'f' },
		{ "office",       required_argument, NULL, 'o' },
		{ "office-phone", required_argument, NULL, 'p' },
		{ "home-phone",   required_argument, NULL, 'h' },
		{ "help",         no_argument,       NULL, 'u' },
		{ "version",      no_argument,       NULL, 'V' },
		{ NULL, 0, NULL, 0 },
	};

	while ((c = getopt_long(argc, argv, "f:r:p:h:o:uvV", long_options,
				&index)) != -1) {
		switch (c) {
		case 'f':
			if (!ctl->allow_fullname)
				errx(EXIT_FAILURE, _("login.defs forbids setting %s"), _("Name"));
			ctl->newf.full_name = optarg;
			status += check_gecos_string(_("Name"), optarg);
			break;
		case 'o':
			if (!ctl->allow_room)
				errx(EXIT_FAILURE, _("login.defs forbids setting %s"), _("Office"));
			ctl->newf.office = optarg;
			status += check_gecos_string(_("Office"), optarg);
			break;
		case 'p':
			if (!ctl->allow_work)
				errx(EXIT_FAILURE, _("login.defs forbids setting %s"), _("Office Phone"));
			ctl->newf.office_phone = optarg;
			status += check_gecos_string(_("Office Phone"), optarg);
			break;
		case 'h':
			if (!ctl->allow_home)
				errx(EXIT_FAILURE, _("login.defs forbids setting %s"), _("Home Phone"));
			ctl->newf.home_phone = optarg;
			status += check_gecos_string(_("Home Phone"), optarg);
			break;
		case 'v': /* deprecated */
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'u':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
		ctl->changed = 1;
		ctl->interactive = 0;
	}
	if (status != 0)
		exit(EXIT_FAILURE);
	/* done parsing arguments.  check for a username. */
	if (optind < argc) {
		if (optind + 1 < argc) {
			warnx(_("cannot handle multiple usernames"));
			errtryhelp(EXIT_FAILURE);
		}
		ctl->username = argv[optind];
	}
}

/*
 *  parse_passwd () --
 *	take a struct password and fill in the fields of the struct finfo.
 */
static void parse_passwd(struct chfn_control *ctl)
{
	char *gecos;

	if (!ctl->pw)
		return;
	/* use pw_gecos - we take a copy since PAM destroys the original */
	gecos = xstrdup(ctl->pw->pw_gecos);
	/* extract known fields */
	ctl->oldf.full_name = strsep(&gecos, ",");
	ctl->oldf.office = strsep(&gecos, ",");
	ctl->oldf.office_phone = strsep(&gecos, ",");
	ctl->oldf.home_phone = strsep(&gecos, ",");
	/*  extra fields contain site-specific information, and can
	 *  not be changed by this version of chfn.  */
	ctl->oldf.other = strsep(&gecos, ",");
}

/*
 *  ask_new_field () --
 *	ask the user for a given field and check that the string is legal.
 */
static char *ask_new_field(struct chfn_control *ctl, const char *question,
			   char *def_val)
{
	int len;
	char *buf = NULL; /* leave initialized to NULL or getline segfaults */
	size_t dummy = 0;

	if (!def_val)
		def_val = "";

	while (true) {
		printf("%s [%s]:", question, def_val);
		__fpurge(stdin);

		putchar(' ');
		fflush(stdout);

		if (getline(&buf, &dummy, stdin) < 0)
			errx(EXIT_FAILURE, _("Aborted."));

		/* remove white spaces from string end */
		ltrim_whitespace((unsigned char *) buf);
		len = rtrim_whitespace((unsigned char *) buf);
		if (len == 0) {
			free(buf);
			return xstrdup(def_val);
		}
		if (!strcasecmp(buf, "none")) {
			free(buf);
			ctl->changed = 1;
			return xstrdup("");
		}
		if (check_gecos_string(question, buf) >= 0)
			break;
	}
	ctl->changed = 1;
	return buf;
}

/*
 *  get_login_defs()
 *	find /etc/login.defs CHFN_RESTRICT and save restrictions to run time
 */
static void get_login_defs(struct chfn_control *ctl)
{
	const char *s;
	size_t i;
	int broken = 0;

	/* real root does not have restrictions */
	if (geteuid() == getuid() && getuid() == 0) {
		ctl->allow_fullname = ctl->allow_room = ctl->allow_work = ctl->allow_home = 1;
		return;
	}
	s = getlogindefs_str("CHFN_RESTRICT", "");
	if (!strcmp(s, "yes")) {
		ctl->allow_room = ctl->allow_work = ctl->allow_home = 1;
		return;
	}
	if (!strcmp(s, "no")) {
		ctl->allow_fullname = ctl->allow_room = ctl->allow_work = ctl->allow_home = 1;
		return;
	}
	for (i = 0; s[i]; i++) {
		switch (s[i]) {
		case 'f':
			ctl->allow_fullname = 1;
			break;
		case 'r':
			ctl->allow_room = 1;
			break;
		case 'w':
			ctl->allow_work = 1;
			break;
		case 'h':
			ctl->allow_home = 1;
			break;
		default:
			broken = 1;
		}
	}
	if (broken)
		warnx(_("%s: CHFN_RESTRICT has unexpected value: %s"), _PATH_LOGINDEFS, s);
	if (!ctl->allow_fullname && !ctl->allow_room && !ctl->allow_work && !ctl->allow_home)
		errx(EXIT_FAILURE, _("%s: CHFN_RESTRICT does not allow any changes"), _PATH_LOGINDEFS);
}

/*
 *  ask_info () --
 *	prompt the user for the finger information and store it.
 */
static void ask_info(struct chfn_control *ctl)
{
	if (ctl->allow_fullname)
		ctl->newf.full_name = ask_new_field(ctl, _("Name"), ctl->oldf.full_name);
	if (ctl->allow_room)
		ctl->newf.office = ask_new_field(ctl, _("Office"), ctl->oldf.office);
	if (ctl->allow_work)
		ctl->newf.office_phone = ask_new_field(ctl, _("Office Phone"), ctl->oldf.office_phone);
	if (ctl->allow_home)
		ctl->newf.home_phone = ask_new_field(ctl, _("Home Phone"), ctl->oldf.home_phone);
	putchar('\n');
}

/*
 *  find_field () --
 *	find field value in uninteractive mode; can be new, old, or blank
 */
static char *find_field(char *nf, char *of)
{
	if (nf)
		return nf;
	if (of)
		return of;
	return xstrdup("");
}

/*
 *  add_missing () --
 *	add not supplied field values when in uninteractive mode
 */
static void add_missing(struct chfn_control *ctl)
{
	ctl->newf.full_name = find_field(ctl->newf.full_name, ctl->oldf.full_name);
	ctl->newf.office = find_field(ctl->newf.office, ctl->oldf.office);
	ctl->newf.office_phone = find_field(ctl->newf.office_phone, ctl->oldf.office_phone);
	ctl->newf.home_phone = find_field(ctl->newf.home_phone, ctl->oldf.home_phone);
	ctl->newf.other = find_field(ctl->newf.other, ctl->oldf.other);
	printf("\n");
}

/*
 *  save_new_data () --
 *	save the given finger info in /etc/passwd.
 *	return zero on success.
 */
static int save_new_data(struct chfn_control *ctl)
{
	char *gecos;
	int len;

	/* create the new gecos string */
	len = xasprintf(&gecos, "%s,%s,%s,%s,%s",
			ctl->newf.full_name,
			ctl->newf.office,
			ctl->newf.office_phone,
			ctl->newf.home_phone,
			ctl->newf.other);

	/* remove trailing empty fields (but not subfields of ctl->newf.other) */
	if (!ctl->newf.other || !*ctl->newf.other) {
		while (len > 0 && gecos[len - 1] == ',')
			len--;
		gecos[len] = 0;
	}

#ifdef HAVE_LIBUSER
	if (set_value_libuser("chfn", ctl->username, ctl->pw->pw_uid,
			LU_GECOS, gecos) < 0) {
#else /* HAVE_LIBUSER */
	/* write the new struct passwd to the passwd file. */
	ctl->pw->pw_gecos = gecos;
	if (setpwnam(ctl->pw, ".chfn") < 0) {
		warn("setpwnam failed");
#endif
		printf(_
		       ("Finger information *NOT* changed.  Try again later.\n"));
		return -1;
	}
	free(gecos);
	printf(_("Finger information changed.\n"));
	return 0;
}

int main(int argc, char **argv)
{
	uid_t uid;
	struct chfn_control ctl = {
		.interactive = 1
	};

	sanitize_env();
	setlocale(LC_ALL, "");	/* both for messages and for iscntrl() below */
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	uid = getuid();

	/* check /etc/login.defs CHFN_RESTRICT */
	get_login_defs(&ctl);

	parse_argv(&ctl, argc, argv);
	if (!ctl.username) {
		ctl.pw = getpwuid(uid);
		if (!ctl.pw)
			errx(EXIT_FAILURE, _("you (user %d) don't exist."),
			     uid);
		ctl.username = ctl.pw->pw_name;
	} else {
		ctl.pw = getpwnam(ctl.username);
		if (!ctl.pw)
			errx(EXIT_FAILURE, _("user \"%s\" does not exist."),
			     ctl.username);
	}
	parse_passwd(&ctl);
#ifndef HAVE_LIBUSER
	if (!(is_local(ctl.username)))
		errx(EXIT_FAILURE, _("can only change local entries"));
#endif

#ifdef HAVE_LIBSELINUX
	if (is_selinux_enabled() > 0) {
		char *user_cxt = NULL;

		if (uid == 0 && !ul_selinux_has_access("passwd", "chfn", &user_cxt))
			errx(EXIT_FAILURE,
			     _("%s is not authorized to change "
			       "the finger info of %s"),
			     user_cxt ? : _("Unknown user context"),
			     ctl.username);

		if (ul_setfscreatecon_from_file(_PATH_PASSWD) != 0)
			errx(EXIT_FAILURE,
			     _("can't set default context for %s"), _PATH_PASSWD);
	}
#endif

#ifdef HAVE_LIBUSER
	/* If we're setuid and not really root, disallow the password change. */
	if (geteuid() != getuid() && uid != ctl.pw->pw_uid) {
#else
	if (uid != 0 && uid != ctl.pw->pw_uid) {
#endif
		errno = EACCES;
		err(EXIT_FAILURE, _("running UID doesn't match UID of user we're "
		      "altering, change denied"));
	}

	printf(_("Changing finger information for %s.\n"), ctl.username);

#if !defined(HAVE_LIBUSER) && defined(CHFN_CHSH_PASSWORD)
	if (!auth_pam("chfn", uid, ctl.username)) {
		return EXIT_FAILURE;
	}
#endif

	if (ctl.interactive)
		ask_info(&ctl);

	add_missing(&ctl);

	if (!ctl.changed) {
		printf(_("Finger information not changed.\n"));
		return EXIT_SUCCESS;
	}

	return save_new_data(&ctl) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
