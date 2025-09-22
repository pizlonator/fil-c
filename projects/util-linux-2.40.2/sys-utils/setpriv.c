/*
 * setpriv(1) - set various kernel privilege bits and run something
 *
 * Copyright (C) 2012 Andy Lutomirski <luto@amacapital.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cap-ng.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <linux/securebits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "caputils.h"
#include "closestream.h"
#include "nls.h"
#include "optutils.h"
#include "strutils.h"
#include "xalloc.h"
#include "pathnames.h"
#include "signames.h"
#include "env.h"
#include "setpriv-landlock.h"

#ifndef PR_SET_NO_NEW_PRIVS
# define PR_SET_NO_NEW_PRIVS 38
#endif
#ifndef PR_GET_NO_NEW_PRIVS
# define PR_GET_NO_NEW_PRIVS 39
#endif

#define SETPRIV_EXIT_PRIVERR 127	/* how we exit when we fail to set privs */

/* The shell to set SHELL env.variable if none is given in the user's passwd entry.  */
#define DEFAULT_SHELL "/bin/sh"

static gid_t get_group(const char *s, const char *err);

enum cap_type {
	CAP_TYPE_EFFECTIVE   = CAPNG_EFFECTIVE,
	CAP_TYPE_PERMITTED   = CAPNG_PERMITTED,
	CAP_TYPE_INHERITABLE = CAPNG_INHERITABLE,
	CAP_TYPE_BOUNDING    = CAPNG_BOUNDING_SET,
	CAP_TYPE_AMBIENT     = (1 << 4)
};

/*
 * Note: We are subject to https://bugzilla.redhat.com/show_bug.cgi?id=895105
 * and we will therefore have problems if new capabilities are added.  Once
 * that bug is fixed, I'll (Andy Lutomirski) submit a corresponding fix to
 * setpriv.  In the mean time, the code here tries to work reasonably well.
 */

struct privctx {
	unsigned int
		nnp:1,			/* no_new_privs */
		have_ruid:1,		/* real uid */
		have_euid:1,		/* effective uid */
		have_rgid:1,		/* real gid */
		have_egid:1,		/* effective gid */
		have_passwd:1,		/* passwd entry */
		have_groups:1,		/* add groups */
		keep_groups:1,		/* keep groups */
		clear_groups:1,		/* remove groups */
		init_groups:1,		/* initialize groups */
		reset_env:1,		/* reset environment */
		have_securebits:1;	/* remove groups */

	/* uids and gids */
	uid_t ruid, euid;
	gid_t rgid, egid;

	/* real user passwd entry */
	struct passwd passwd;

	/* supplementary groups */
	size_t num_groups;
	gid_t *groups;

	/* caps */
	const char *caps_to_inherit;
	const char *ambient_caps;
	const char *bounding_set;

	/* securebits */
	int securebits;
	/* parent death signal (<0 clear, 0 nothing, >0 signal) */
	int pdeathsig;

	/* LSMs */
	const char *selinux_label;
	const char *apparmor_profile;
	struct setpriv_landlock_opts landlock;
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] <program> [<argument>...]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Run a program with different privilege settings.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -d, --dump                  show current state (and do not exec)\n"), out);
	fputs(_(" --nnp, --no-new-privs       disallow granting new privileges\n"), out);
	fputs(_(" --ambient-caps <caps,...>   set ambient capabilities\n"), out);
	fputs(_(" --inh-caps <caps,...>       set inheritable capabilities\n"), out);
	fputs(_(" --bounding-set <caps>       set capability bounding set\n"), out);
	fputs(_(" --ruid <uid|user>           set real uid\n"), out);
	fputs(_(" --euid <uid|user>           set effective uid\n"), out);
	fputs(_(" --rgid <gid|group>          set real gid\n"), out);
	fputs(_(" --egid <gid|group>          set effective gid\n"), out);
	fputs(_(" --reuid <uid|user>          set real and effective uid\n"), out);
	fputs(_(" --regid <gid|group>         set real and effective gid\n"), out);
	fputs(_(" --clear-groups              clear supplementary groups\n"), out);
	fputs(_(" --keep-groups               keep supplementary groups\n"), out);
	fputs(_(" --init-groups               initialize supplementary groups\n"), out);
	fputs(_(" --groups <group,...>        set supplementary groups by UID or name\n"), out);
	fputs(_(" --securebits <bits>         set securebits\n"), out);
	fputs(_(" --pdeathsig keep|clear|<signame>\n"
	        "                             set or clear parent death signal\n"), out);
	fputs(_(" --selinux-label <label>     set SELinux label\n"), out);
	fputs(_(" --apparmor-profile <pr>     set AppArmor profile\n"), out);
	fputs(_(" --landlock-access <access>  add Landlock access\n"), out);
	fputs(_(" --landlock-rule <rule>      add Landlock rule\n"), out);
	fputs(_(" --reset-env                 clear all environment and initialize\n"
		"                               HOME, SHELL, USER, LOGNAME and PATH\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(29));
	fputs(USAGE_SEPARATOR, out);
	fputs(_(" This tool can be dangerous.  Read the manpage, and be careful.\n"), out);
	fprintf(out, USAGE_MAN_TAIL("setpriv(1)"));

	usage_setpriv(out);

	exit(EXIT_SUCCESS);
}

static int has_cap(enum cap_type which, unsigned int i)
{
	switch (which) {
	case CAP_TYPE_EFFECTIVE:
	case CAP_TYPE_BOUNDING:
	case CAP_TYPE_INHERITABLE:
	case CAP_TYPE_PERMITTED:
		return capng_have_capability((capng_type_t)which, i);
	case CAP_TYPE_AMBIENT:
		return prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET,
				(unsigned long) i, 0UL, 0UL);
	default:
		warnx(_("invalid capability type"));
		return -1;
	}
}

/* Returns the number of capabilities printed. */
static int print_caps(FILE *f, enum cap_type which)
{
	int i, n = 0, max = cap_last_cap();

	for (i = 0; i <= max; i++) {
		int ret = has_cap(which, i);

		if (i == 0 && ret < 0)
			return -1;

		if (ret == 1) {
			const char *name = capng_capability_to_name(i);
			if (n)
				fputc(',', f);
			if (name)
				fputs(name, f);
			else
				warnx(_("cap %d: libcap-ng is broken"), i);
			n++;
		}
	}

	return n;
}

static void dump_one_secbit(int *first, int *bits, int bit, const char *name)
{
	if (*bits & bit) {
		if (*first)
			*first = 0;
		else
			printf(",");
		fputs(name, stdout);
		*bits &= ~bit;
	}
}

static void dump_securebits(void)
{
	int first = 1;
	int bits = prctl(PR_GET_SECUREBITS, 0, 0, 0, 0);

	if (bits < 0) {
		warnx(_("getting process secure bits failed"));
		return;
	}

	printf(_("Securebits: "));

	dump_one_secbit(&first, &bits, SECBIT_NOROOT, "noroot");
	dump_one_secbit(&first, &bits, SECBIT_NOROOT_LOCKED, "noroot_locked");
	dump_one_secbit(&first, &bits, SECBIT_NO_SETUID_FIXUP,
			"no_setuid_fixup");
	dump_one_secbit(&first, &bits, SECBIT_NO_SETUID_FIXUP_LOCKED,
			"no_setuid_fixup_locked");
	bits &= ~SECBIT_KEEP_CAPS;
	dump_one_secbit(&first, &bits, SECBIT_KEEP_CAPS_LOCKED,
			"keep_caps_locked");
	if (bits) {
		if (first)
			first = 0;
		else
			printf(",");
		printf("0x%x", (unsigned)bits);
	}

	if (first)
		printf(_("[none]\n"));
	else
		printf("\n");
}

static void dump_label(const char *name)
{
	char buf[4097];
	ssize_t len;
	int fd, e;

	fd = open(_PATH_PROC_ATTR_CURRENT, O_RDONLY);
	if (fd == -1) {
		warn(_("cannot open %s"), _PATH_PROC_ATTR_CURRENT);
		return;
	}

	len = read(fd, buf, sizeof(buf));
	e = errno;
	close(fd);
	if (len < 0) {
		errno = e;
		warn(_("cannot read %s"), name);
		return;
	}
	if (sizeof(buf) - 1 <= (size_t)len) {
		warnx(_("%s: too long"), name);
		return;
	}

	buf[len] = 0;
	if (0 < len && buf[len - 1] == '\n')
		buf[len - 1] = 0;
	printf("%s: %s\n", name, buf);
}

static void dump_groups(void)
{
	int n = getgroups(0, NULL);
	gid_t *groups;

	if (n < 0) {
		warn("getgroups failed");
		return;
	}

	groups = xmalloc(n * sizeof(gid_t));
	n = getgroups(n, groups);
	if (n < 0) {
		free(groups);
		warn("getgroups failed");
		return;
	}

	printf(_("Supplementary groups: "));
	if (n == 0)
		printf(_("[none]"));
	else {
		int i;
		for (i = 0; i < n; i++) {
			if (0 < i)
				printf(",");
			printf("%ld", (long)groups[i]);
		}
	}
	printf("\n");
	free(groups);
}

static void dump_pdeathsig(void)
{
	int pdeathsig;

	if (prctl(PR_GET_PDEATHSIG, &pdeathsig) != 0) {
		warn(_("get pdeathsig failed"));
		return;
	}

	printf(_("Parent death signal: "));
	if (pdeathsig && signum_to_signame(pdeathsig) != NULL)
		printf("%s\n", signum_to_signame(pdeathsig));
	else if (pdeathsig)
		printf("%d\n", pdeathsig);
	else
		printf(_("[none]\n"));
}

static void dump(int dumplevel)
{
	int x;
	uid_t ru, eu, su;
	gid_t rg, eg, sg;

	if (getresuid(&ru, &eu, &su) == 0) {
		printf(_("uid: %u\n"), ru);
		printf(_("euid: %u\n"), eu);
		/* Saved and fs uids always equal euid. */
		if (3 <= dumplevel)
			printf(_("suid: %u\n"), su);
	} else
		warn(_("getresuid failed"));

	if (getresgid(&rg, &eg, &sg) == 0) {
		printf("gid: %ld\n", (long)rg);
		printf("egid: %ld\n", (long)eg);
		/* Saved and fs gids always equal egid. */
		if (dumplevel >= 3)
			printf("sgid: %ld\n", (long)sg);
	} else
		warn(_("getresgid failed"));

	dump_groups();

	x = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
	if (0 <= x)
		printf("no_new_privs: %d\n", x);
	else
		warn("setting no_new_privs failed");

	if (2 <= dumplevel) {
		printf(_("Effective capabilities: "));
		if (print_caps(stdout, CAP_TYPE_EFFECTIVE) == 0)
			printf(_("[none]"));
		printf("\n");

		printf(_("Permitted capabilities: "));
		if (print_caps(stdout, CAP_TYPE_PERMITTED) == 0)
			printf(_("[none]"));
		printf("\n");
	}

	printf(_("Inheritable capabilities: "));
	if (print_caps(stdout, CAP_TYPE_INHERITABLE) == 0)
		printf(_("[none]"));
	printf("\n");

	printf(_("Ambient capabilities: "));
	x = print_caps(stdout, CAP_TYPE_AMBIENT);
	if (x == 0)
		printf(_("[none]"));
	if (x < 0)
		printf(_("[unsupported]"));
	printf("\n");

	printf(_("Capability bounding set: "));
	if (print_caps(stdout, CAP_TYPE_BOUNDING) == 0)
		printf(_("[none]"));
	printf("\n");

	dump_securebits();
	dump_pdeathsig();

	if (access(_PATH_SYS_SELINUX, F_OK) == 0)
		dump_label(_("SELinux label"));

	if (access(_PATH_SYS_APPARMOR, F_OK) == 0) {
		dump_label(_("AppArmor profile"));
	}
}

static void list_known_caps(void)
{
	int i, max = cap_last_cap();

	for (i = 0; i <= max; i++) {
		const char *name = capng_capability_to_name(i);
		if (name)
			printf("%s\n", name);
		else
			warnx(_("cap %d: libcap-ng is broken"), i);
	}
}

static void parse_groups(struct privctx *opts, const char *str)
{
	char *groups = xstrdup(str);
	char *buf = groups;	/* We'll reuse it */
	char *c;
	size_t i = 0;

	opts->have_groups = 1;
	opts->num_groups = 0;
	while ((c = strsep(&groups, ",")))
		opts->num_groups++;

	/* Start again */
	strcpy(buf, str);	/* It's exactly the right length */
	groups = buf;

	opts->groups = xcalloc(opts->num_groups, sizeof(gid_t));
	while ((c = strsep(&groups, ",")))
		opts->groups[i++] = get_group(c, _("Invalid supplementary group id"));

	free(groups);
}

static void parse_pdeathsig(struct privctx *opts, const char *str)
{
	if (!strcmp(str, "keep")) {
		if (prctl(PR_GET_PDEATHSIG, &opts->pdeathsig) != 0)
			errx(SETPRIV_EXIT_PRIVERR,
				 _("failed to get parent death signal"));
	} else if (!strcmp(str, "clear")) {
		opts->pdeathsig = -1;
	} else if ((opts->pdeathsig = signame_to_signum(str)) < 0) {
		errx(EXIT_FAILURE, _("unknown signal: %s"), str);
	}
}

static void do_setresuid(const struct privctx *opts)
{
	uid_t ruid, euid, suid;
	if (getresuid(&ruid, &euid, &suid) != 0)
		err(SETPRIV_EXIT_PRIVERR, _("getresuid failed"));
	if (opts->have_ruid)
		ruid = opts->ruid;
	if (opts->have_euid)
		euid = opts->euid;

	/* Also copy effective to saved (for paranoia). */
	if (setresuid(ruid, euid, euid) != 0)
		err(SETPRIV_EXIT_PRIVERR, _("setresuid failed"));
}

static void do_setresgid(const struct privctx *opts)
{
	gid_t rgid, egid, sgid;
	if (getresgid(&rgid, &egid, &sgid) != 0)
		err(SETPRIV_EXIT_PRIVERR, _("getresgid failed"));
	if (opts->have_rgid)
		rgid = opts->rgid;
	if (opts->have_egid)
		egid = opts->egid;

	/* Also copy effective to saved (for paranoia). */
	if (setresgid(rgid, egid, egid) != 0)
		err(SETPRIV_EXIT_PRIVERR, _("setresgid failed"));
}

static void bump_cap(unsigned int cap)
{
	if (capng_have_capability(CAPNG_PERMITTED, cap))
		capng_update(CAPNG_ADD, CAPNG_EFFECTIVE, cap);
}

static int cap_update(capng_act_t action,
		enum cap_type type, unsigned int cap)
{
	switch (type) {
		case CAP_TYPE_EFFECTIVE:
		case CAP_TYPE_BOUNDING:
		case CAP_TYPE_INHERITABLE:
		case CAP_TYPE_PERMITTED:
			return capng_update(action, (capng_type_t) type, cap);
		case CAP_TYPE_AMBIENT:
		{
			int ret;

			if (action == CAPNG_ADD)
				ret = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE,
						(unsigned long) cap, 0UL, 0UL);
			else
				ret = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_LOWER,
						(unsigned long) cap, 0UL, 0UL);

			return ret;
		}
		default:
			errx(EXIT_FAILURE, _("unsupported capability type"));
			return -1;
	}
}

static void do_caps(enum cap_type type, const char *caps)
{
	char *my_caps = xstrdup(caps);
	char *c;

	while ((c = strsep(&my_caps, ","))) {
		capng_act_t action;
		if (*c == '+')
			action = CAPNG_ADD;
		else if (*c == '-')
			action = CAPNG_DROP;
		else
			errx(EXIT_FAILURE, _("bad capability string"));

		if (!strcmp(c + 1, "all")) {
			int i;
			/* We can trust the return value from cap_last_cap(),
			 * so use that directly. */
			for (i = 0; i <= cap_last_cap(); i++)
				cap_update(action, type, i);
		} else {
			int cap = capng_name_to_capability(c + 1);
			if (0 <= cap)
				cap_update(action, type, cap);
			else if (sscanf(c + 1, "cap_%d", &cap) == 1
			    && 0 <= cap && cap <= cap_last_cap())
				cap_update(action, type, cap);
			else
				errx(EXIT_FAILURE,
				     _("unknown capability \"%s\""), c + 1);
		}
	}

	free(my_caps);
}

static void parse_securebits(struct privctx *opts, const char *arg)
{
	char *buf = xstrdup(arg);
	char *c;

	opts->have_securebits = 1;
	opts->securebits = prctl(PR_GET_SECUREBITS, 0, 0, 0, 0);
	if (opts->securebits < 0)
		err(SETPRIV_EXIT_PRIVERR, _("getting process secure bits failed"));

	if (opts->securebits & ~(int)(SECBIT_NOROOT |
				      SECBIT_NOROOT_LOCKED |
				      SECBIT_NO_SETUID_FIXUP |
				      SECBIT_NO_SETUID_FIXUP_LOCKED |
				      SECBIT_KEEP_CAPS |
				      SECBIT_KEEP_CAPS_LOCKED))
		errx(SETPRIV_EXIT_PRIVERR,
		     _("unrecognized securebit set -- refusing to adjust"));

	while ((c = strsep(&buf, ","))) {
		if (*c != '+' && *c != '-')
			errx(EXIT_FAILURE, _("bad securebits string"));

		if (!strcmp(c + 1, "all")) {
			if (*c == '-')
				opts->securebits = 0;
			else
				errx(EXIT_FAILURE,
				     _("+all securebits is not allowed"));
		} else {
			int bit;
			if (!strcmp(c + 1, "noroot"))
				bit = SECBIT_NOROOT;
			else if (!strcmp(c + 1, "noroot_locked"))
				bit = SECBIT_NOROOT_LOCKED;
			else if (!strcmp(c + 1, "no_setuid_fixup"))
				bit = SECBIT_NO_SETUID_FIXUP;
			else if (!strcmp(c + 1, "no_setuid_fixup_locked"))
				bit = SECBIT_NO_SETUID_FIXUP_LOCKED;
			else if (!strcmp(c + 1, "keep_caps"))
				errx(EXIT_FAILURE,
				     _("adjusting keep_caps does not make sense"));
			else if (!strcmp(c + 1, "keep_caps_locked"))
				bit = SECBIT_KEEP_CAPS_LOCKED;	/* sigh */
			else
				errx(EXIT_FAILURE, _("unrecognized securebit"));

			if (*c == '+')
				opts->securebits |= bit;
			else
				opts->securebits &= ~bit;
		}
	}

	opts->securebits |= SECBIT_KEEP_CAPS;	/* We need it, and it's reset on exec */

	free(buf);
}

static void do_selinux_label(const char *label)
{
	int fd;
	size_t len;

	if (access(_PATH_SYS_SELINUX, F_OK) != 0)
		errx(SETPRIV_EXIT_PRIVERR, _("SELinux is not running"));

	fd = open(_PATH_PROC_ATTR_EXEC, O_RDWR);
	if (fd == -1)
		err(SETPRIV_EXIT_PRIVERR,
		    _("cannot open %s"), _PATH_PROC_ATTR_EXEC);

	len = strlen(label);
	errno = 0;
	if (write(fd, label, len) != (ssize_t) len)
		err(SETPRIV_EXIT_PRIVERR,
		    _("write failed: %s"), _PATH_PROC_ATTR_EXEC);

	if (close(fd) != 0)
		err(SETPRIV_EXIT_PRIVERR,
		    _("close failed: %s"), _PATH_PROC_ATTR_EXEC);
}

static void do_apparmor_profile(const char *label)
{
	FILE *f;

	if (access(_PATH_SYS_APPARMOR, F_OK) != 0)
		errx(SETPRIV_EXIT_PRIVERR, _("AppArmor is not running"));

	f = fopen(_PATH_PROC_ATTR_EXEC, "r+");
	if (!f)
		err(SETPRIV_EXIT_PRIVERR,
		    _("cannot open %s"), _PATH_PROC_ATTR_EXEC);

	fprintf(f, "exec %s", label);

	if (close_stream(f) != 0)
		err(SETPRIV_EXIT_PRIVERR,
		    _("write failed: %s"), _PATH_PROC_ATTR_EXEC);
}

static void do_reset_environ(struct passwd *pw)
{
	char *term = getenv("TERM");

	if (term)
		term = xstrdup(term);
#ifdef HAVE_CLEARENV
	clearenv();
#else
	environ = NULL;
#endif
	if (term) {
		xsetenv("TERM", term, 1);
		free(term);
	}

	if (pw->pw_shell && *pw->pw_shell)
		xsetenv("SHELL", pw->pw_shell, 1);
	else
		xsetenv("SHELL", DEFAULT_SHELL, 1);

	xsetenv("HOME", pw->pw_dir, 1);
	xsetenv("USER", pw->pw_name, 1);
	xsetenv("LOGNAME", pw->pw_name, 1);

	if (pw->pw_uid)
		xsetenv("PATH", _PATH_DEFPATH, 1);
	else
		xsetenv("PATH", _PATH_DEFPATH_ROOT, 1);
}

static uid_t get_user(const char *s, const char *err)
{
	struct passwd *pw;
	long tmp;
	pw = getpwnam(s);
	if (pw)
		return pw->pw_uid;
	tmp = strtol_or_err(s, err);
	return tmp;
}

static gid_t get_group(const char *s, const char *err)
{
	struct group *gr;
	long tmp;
	gr = getgrnam(s);
	if (gr)
		return gr->gr_gid;
	tmp = strtol_or_err(s, err);
	return tmp;
}

static struct passwd *get_passwd(const char *s, uid_t *uid, const char *err)
{
	struct passwd *pw;
	long tmp;
	pw = getpwnam(s);
	if (pw) {
		*uid = pw->pw_uid;
	} else {
		tmp = strtol_or_err(s, err);
		*uid = tmp;
		pw = getpwuid(*uid);
	}
	return pw;
}

static struct passwd *passwd_copy(struct passwd *dst, const struct passwd *src)
{
	struct passwd *rv;
	rv = memcpy(dst, src, sizeof(*dst));
	rv->pw_name = xstrdup(rv->pw_name);
	rv->pw_passwd = xstrdup(rv->pw_passwd);
	rv->pw_gecos = xstrdup(rv->pw_gecos);
	rv->pw_dir = xstrdup(rv->pw_dir);
	rv->pw_shell = xstrdup(rv->pw_shell);
	return rv;
}

int main(int argc, char **argv)
{
	enum {
		NNP = CHAR_MAX + 1,
		RUID,
		EUID,
		RGID,
		EGID,
		REUID,
		REGID,
		CLEAR_GROUPS,
		KEEP_GROUPS,
		INIT_GROUPS,
		GROUPS,
		INHCAPS,
		AMBCAPS,
		LISTCAPS,
		CAPBSET,
		SECUREBITS,
		PDEATHSIG,
		SELINUX_LABEL,
		APPARMOR_PROFILE,
		LANDLOCK_ACCESS,
		LANDLOCK_RULE,
		RESET_ENV
	};

	static const struct option longopts[] = {
		{ "dump",             no_argument,       NULL, 'd'              },
		{ "nnp",              no_argument,       NULL, NNP              },
		{ "no-new-privs",     no_argument,       NULL, NNP              },
		{ "inh-caps",         required_argument, NULL, INHCAPS          },
		{ "ambient-caps",     required_argument, NULL, AMBCAPS          },
		{ "list-caps",        no_argument,       NULL, LISTCAPS         },
		{ "ruid",             required_argument, NULL, RUID             },
		{ "euid",             required_argument, NULL, EUID             },
		{ "rgid",             required_argument, NULL, RGID             },
		{ "egid",             required_argument, NULL, EGID             },
		{ "reuid",            required_argument, NULL, REUID            },
		{ "regid",            required_argument, NULL, REGID            },
		{ "clear-groups",     no_argument,       NULL, CLEAR_GROUPS     },
		{ "keep-groups",      no_argument,       NULL, KEEP_GROUPS      },
		{ "init-groups",      no_argument,       NULL, INIT_GROUPS      },
		{ "groups",           required_argument, NULL, GROUPS           },
		{ "bounding-set",     required_argument, NULL, CAPBSET          },
		{ "securebits",       required_argument, NULL, SECUREBITS       },
		{ "pdeathsig",        required_argument, NULL, PDEATHSIG,       },
		{ "selinux-label",    required_argument, NULL, SELINUX_LABEL    },
		{ "apparmor-profile", required_argument, NULL, APPARMOR_PROFILE },
		{ "landlock-access",  required_argument, NULL, LANDLOCK_ACCESS  },
		{ "landlock-rule",    required_argument, NULL, LANDLOCK_RULE    },
		{ "help",             no_argument,       NULL, 'h'              },
		{ "reset-env",        no_argument,       NULL, RESET_ENV,       },
		{ "version",          no_argument,       NULL, 'V'              },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {
		/* keep in same order with enum definitions */
		{CLEAR_GROUPS, KEEP_GROUPS, INIT_GROUPS, GROUPS},
		{0}
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	int c;
	struct privctx opts;
	struct passwd *pw = NULL;
	int dumplevel = 0;
	int total_opts = 0;
	int list_caps = 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	memset(&opts, 0, sizeof(opts));
	init_landlock_opts(&opts.landlock);

	while ((c = getopt_long(argc, argv, "+dhV", longopts, NULL)) != -1) {
		err_exclusive_options(c, longopts, excl, excl_st);
		total_opts++;
		switch (c) {
		case 'd':
			dumplevel++;
			break;
		case NNP:
			if (opts.nnp)
				errx(EXIT_FAILURE,
				     _("duplicate --no-new-privs option"));
			opts.nnp = 1;
			break;
		case RUID:
			if (opts.have_ruid)
				errx(EXIT_FAILURE, _("duplicate ruid"));
			opts.have_ruid = 1;
			pw = get_passwd(optarg, &opts.ruid, _("failed to parse ruid"));
			if (pw) {
				passwd_copy(&opts.passwd, pw);
				opts.have_passwd = 1;
			}
			break;
		case EUID:
			if (opts.have_euid)
				errx(EXIT_FAILURE, _("duplicate euid"));
			opts.have_euid = 1;
			opts.euid = get_user(optarg, _("failed to parse euid"));
			break;
		case REUID:
			if (opts.have_ruid || opts.have_euid)
				errx(EXIT_FAILURE, _("duplicate ruid or euid"));
			opts.have_ruid = opts.have_euid = 1;
			pw = get_passwd(optarg, &opts.ruid, _("failed to parse reuid"));
			opts.euid = opts.ruid;
			if (pw) {
				passwd_copy(&opts.passwd, pw);
				opts.have_passwd = 1;
			}
			break;
		case RGID:
			if (opts.have_rgid)
				errx(EXIT_FAILURE, _("duplicate rgid"));
			opts.have_rgid = 1;
			opts.rgid = get_group(optarg, _("failed to parse rgid"));
			break;
		case EGID:
			if (opts.have_egid)
				errx(EXIT_FAILURE, _("duplicate egid"));
			opts.have_egid = 1;
			opts.egid = get_group(optarg, _("failed to parse egid"));
			break;
		case REGID:
			if (opts.have_rgid || opts.have_egid)
				errx(EXIT_FAILURE, _("duplicate rgid or egid"));
			opts.have_rgid = opts.have_egid = 1;
			opts.rgid = opts.egid = get_group(optarg, _("failed to parse regid"));
			break;
		case CLEAR_GROUPS:
			if (opts.clear_groups)
				errx(EXIT_FAILURE,
				     _("duplicate --clear-groups option"));
			opts.clear_groups = 1;
			break;
		case KEEP_GROUPS:
			if (opts.keep_groups)
				errx(EXIT_FAILURE,
				     _("duplicate --keep-groups option"));
			opts.keep_groups = 1;
			break;
		case INIT_GROUPS:
			if (opts.init_groups)
				errx(EXIT_FAILURE,
				     _("duplicate --init-groups option"));
			opts.init_groups = 1;
			break;
		case GROUPS:
			if (opts.have_groups)
				errx(EXIT_FAILURE,
				     _("duplicate --groups option"));
			parse_groups(&opts, optarg);
			break;
		case PDEATHSIG:
			if (opts.pdeathsig)
				errx(EXIT_FAILURE,
				     _("duplicate --keep-pdeathsig option"));
			parse_pdeathsig(&opts, optarg);
			break;
		case LISTCAPS:
			list_caps = 1;
			break;
		case INHCAPS:
			if (opts.caps_to_inherit)
				errx(EXIT_FAILURE,
				     _("duplicate --inh-caps option"));
			opts.caps_to_inherit = optarg;
			break;
		case AMBCAPS:
			if (opts.ambient_caps)
				errx(EXIT_FAILURE,
				     _("duplicate --ambient-caps option"));
			opts.ambient_caps = optarg;
			break;
		case CAPBSET:
			if (opts.bounding_set)
				errx(EXIT_FAILURE,
				     _("duplicate --bounding-set option"));
			opts.bounding_set = optarg;
			break;
		case SECUREBITS:
			if (opts.have_securebits)
				errx(EXIT_FAILURE,
				     _("duplicate --securebits option"));
			parse_securebits(&opts, optarg);
			break;
		case SELINUX_LABEL:
			if (opts.selinux_label)
				errx(EXIT_FAILURE,
				     _("duplicate --selinux-label option"));
			opts.selinux_label = optarg;
			break;
		case APPARMOR_PROFILE:
			if (opts.apparmor_profile)
				errx(EXIT_FAILURE,
				     _("duplicate --apparmor-profile option"));
			opts.apparmor_profile = optarg;
			break;
		case LANDLOCK_ACCESS:
			parse_landlock_access(&opts.landlock, optarg);
			break;
		case LANDLOCK_RULE:
			parse_landlock_rule(&opts.landlock, optarg);
			break;
		case RESET_ENV:
			opts.reset_env = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (dumplevel) {
		if (total_opts != dumplevel || optind < argc)
			errx(EXIT_FAILURE,
			     _("--dump is incompatible with all other options"));
		dump(dumplevel);
		return EXIT_SUCCESS;
	}

	if (list_caps) {
		if (total_opts != 1 || optind < argc)
			errx(EXIT_FAILURE,
			     _("--list-caps must be specified alone"));
		list_known_caps();
		return EXIT_SUCCESS;
	}

	if (argc <= optind)
		errx(EXIT_FAILURE, _("No program specified"));

	if ((opts.have_rgid || opts.have_egid)
	    && !opts.keep_groups && !opts.clear_groups && !opts.init_groups
	    && !opts.have_groups)
		errx(EXIT_FAILURE,
		     _("--[re]gid requires --keep-groups, --clear-groups, --init-groups, or --groups"));

	if (opts.init_groups && !opts.have_ruid)
		errx(EXIT_FAILURE,
		     _("--init-groups requires --ruid or --reuid"));

	if (opts.init_groups && !opts.have_passwd)
		errx(EXIT_FAILURE,
		     _("uid %ld not found, --init-groups requires an user that "
		       "can be found on the system"),
		     (long) opts.ruid);

	if (opts.reset_env) {
		if (opts.have_passwd)
			/* pwd according to --ruid or --reuid */
			pw = &opts.passwd;
		else
			/* pwd for the current user */
			pw = getpwuid(getuid());
		do_reset_environ(pw);
	}

	if (opts.nnp && prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1)
		err(EXIT_FAILURE, _("disallow granting new privileges failed"));

	if (opts.selinux_label)
		do_selinux_label(opts.selinux_label);
	if (opts.apparmor_profile)
		do_apparmor_profile(opts.apparmor_profile);

	if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) == -1)
		err(EXIT_FAILURE, _("keep process capabilities failed"));

	/* We're going to want CAP_SETPCAP, CAP_SETUID, and CAP_SETGID if
	 * possible.  */
	bump_cap(CAP_SETPCAP);
	bump_cap(CAP_SETUID);
	bump_cap(CAP_SETGID);
	if (capng_apply(CAPNG_SELECT_CAPS) != 0)
		err(SETPRIV_EXIT_PRIVERR, _("activate capabilities"));

	if (opts.have_ruid || opts.have_euid) {
		do_setresuid(&opts);
		/* KEEPCAPS doesn't work for the effective mask. */
		if (capng_apply(CAPNG_SELECT_CAPS) != 0)
			err(SETPRIV_EXIT_PRIVERR, _("reactivate capabilities"));
	}

	if (opts.have_rgid || opts.have_egid)
		do_setresgid(&opts);

	if (opts.have_groups) {
		if (setgroups(opts.num_groups, opts.groups) != 0)
			err(SETPRIV_EXIT_PRIVERR, _("setgroups failed"));
	} else if (opts.init_groups) {
		if (initgroups(opts.passwd.pw_name, opts.passwd.pw_gid) != 0)
			err(SETPRIV_EXIT_PRIVERR, _("initgroups failed"));
	} else if (opts.clear_groups) {
		gid_t x = 0;
		if (setgroups(0, &x) != 0)
			err(SETPRIV_EXIT_PRIVERR, _("setgroups failed"));
	}

	if (opts.have_securebits && prctl(PR_SET_SECUREBITS, opts.securebits, 0, 0, 0) != 0)
		err(SETPRIV_EXIT_PRIVERR, _("set process securebits failed"));

	if (opts.bounding_set) {
		do_caps(CAP_TYPE_BOUNDING, opts.bounding_set);
		errno = EPERM;	/* capng doesn't set errno if we're missing CAP_SETPCAP */
		if (capng_apply(CAPNG_SELECT_BOUNDS) != 0)
			err(SETPRIV_EXIT_PRIVERR, _("apply bounding set"));
	}

	if (opts.caps_to_inherit) {
		do_caps(CAP_TYPE_INHERITABLE, opts.caps_to_inherit);
		if (capng_apply(CAPNG_SELECT_CAPS) != 0)
			err(SETPRIV_EXIT_PRIVERR, _("apply capabilities"));
	}

	if (opts.ambient_caps) {
		do_caps(CAP_TYPE_AMBIENT, opts.ambient_caps);
	}

	/* Clear or set parent death signal */
	if (opts.pdeathsig && prctl(PR_SET_PDEATHSIG, opts.pdeathsig < 0 ? 0 : opts.pdeathsig) != 0)
		err(SETPRIV_EXIT_PRIVERR, _("set parent death signal failed"));

	do_landlock(&opts.landlock);

	execvp(argv[optind], argv + optind);
	errexec(argv[optind]);
}
