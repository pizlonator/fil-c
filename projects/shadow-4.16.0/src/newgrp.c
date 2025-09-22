/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2008, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <assert.h>

#include "agetpass.h"
#include "alloc.h"
#include "defines.h"
#include "getdef.h"
#include "prototypes.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"
#include "string/sprintf.h"


/*
 * Global variables
 */
static const char *Prog;

extern char **newenvp;

#ifdef HAVE_SETGROUPS
static int ngroups;
static /*@null@*/ /*@only@*/GETGROUPS_T *grouplist;
#endif

static bool is_newgrp;

#ifdef WITH_AUDIT
static char audit_buf[80];
#endif

/* local function prototypes */
static void usage (void);
static void check_perms (const struct group *grp,
                         struct passwd *pwd,
                         const char *groupname);
static void syslog_sg (const char *name, const char *group);

/*
 * usage - print command usage message
 */
static void usage (void)
{
	if (is_newgrp) {
		(void) fputs (_("Usage: newgrp [-] [group]\n"), stderr);
	} else {
		(void) fputs (_("Usage: sg group [[-c] command]\n"), stderr);
	}
}

static bool ingroup(const char *name, struct group *gr)
{
	char **look;
	bool notfound = true;

	look = gr->gr_mem;
	while (*look && notfound)
		notfound = strcmp (*look++, name);

	return !notfound;
}

/*
 * find_matching_group - search all groups of a gr's group id for
 *                       membership of a given username
 *                       but check gr itself first
 */
static /*@null@*/struct group *find_matching_group (const char *name, struct group *gr)
{
	gid_t gid = gr->gr_gid;

	if (ingroup(name, gr))
		return gr;

	setgrent ();
	while ((gr = getgrent ()) != NULL) {
		if (gr->gr_gid != gid) {
			continue;
		}

		/*
		 * A group with matching GID was found.
		 * Test for membership of 'name'.
		 */
		if (ingroup(name, gr))
			break;
	}
	endgrent ();
	return gr;
}

/*
 * check_perms - check if the user is allowed to switch to this group
 *
 *	If needed, the user will be authenticated.
 *
 *	It will not return if the user could not be authenticated.
 */
static void check_perms (const struct group *grp,
                         struct passwd *pwd,
                         const char *groupname)
{
	bool needspasswd = false;
	struct spwd *spwd;
	char *cp;
	const char *cpasswd;

	/*
	 * see if she is a member of this group (i.e. in the list of
	 * members of the group, or if the group is her primary group).
	 *
	 * If she isn't a member, she needs to provide the group password.
	 * If there is no group password, she will be denied access
	 * anyway.
	 *
	 */
	if (   (grp->gr_gid != pwd->pw_gid)
	    && !is_on_list (grp->gr_mem, pwd->pw_name)) {
		needspasswd = true;
	}

	/*
	 * If she does not have either a shadowed password, or a regular
	 * password, and the group has a password, she needs to give the
	 * group password.
	 */
	spwd = xgetspnam (pwd->pw_name);
	if (NULL != spwd) {
		pwd->pw_passwd = xstrdup (spwd->sp_pwdp);
		spw_free (spwd);
	}

	if ((pwd->pw_passwd[0] == '\0') && (grp->gr_passwd[0] != '\0')) {
		needspasswd = true;
	}

	/*
	 * Now I see about letting her into the group she requested. If she
	 * is the root user, I'll let her in without having to prompt for
	 * the password. Otherwise I ask for a password if she flunked one
	 * of the tests above.
	 */
	if ((getuid () != 0) && needspasswd) {
		/*
		 * get the password from her, and set the salt for
		 * the decryption from the group file.
		 */
		cp = agetpass (_("Password: "));
		if (NULL == cp) {
			goto failure;
		}

		/*
		 * encrypt the key she gave us using the salt from the
		 * password in the group file. The result of this encryption
		 * must match the previously encrypted value in the file.
		 */
		cpasswd = pw_encrypt (cp, grp->gr_passwd);
		erase_pass (cp);

		if (NULL == cpasswd) {
			fprintf (stderr,
			         _("%s: failed to crypt password with previous salt: %s\n"),
			         Prog, strerror (errno));
			SYSLOG ((LOG_INFO,
			         "Failed to crypt password with previous salt of group '%s'",
			         groupname));
			goto failure;
		}

		if (grp->gr_passwd[0] == '\0' ||
		    strcmp (cpasswd, grp->gr_passwd) != 0) {
#ifdef WITH_AUDIT
			SNPRINTF(audit_buf, "authentication new-gid=%lu",
			         (unsigned long) grp->gr_gid);
			audit_logger (AUDIT_GRP_AUTH, Prog,
			              audit_buf, NULL, getuid (), 0);
#endif
			SYSLOG ((LOG_INFO,
				 "Invalid password for group '%s' from '%s'",
				 groupname, pwd->pw_name));
			(void) sleep (1);
			(void) fputs (_("Invalid password.\n"), stderr);
			goto failure;
		}
#ifdef WITH_AUDIT
		SNPRINTF(audit_buf, "authentication new-gid=%lu",
		         (unsigned long) grp->gr_gid);
		audit_logger (AUDIT_GRP_AUTH, Prog,
		              audit_buf, NULL, getuid (), 1);
#endif
	}

	return;

failure:
	/* The closelog is probably unnecessary, but it does no
	 * harm.  -- JWP
	 */
	closelog ();
#ifdef WITH_AUDIT
	if (groupname) {
		SNPRINTF(audit_buf, "changing new-group=%s", groupname);
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              audit_buf, NULL, getuid (), 0);
	} else {
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              "changing", NULL, getuid (), 0);
	}
#endif
	exit (EXIT_FAILURE);
}

/*
 * syslog_sg - log the change of group to syslog
 *
 *	The logout will also be logged when the user will quit the
 *	sg/newgrp session.
 */
static void syslog_sg (const char *name, const char *group)
{
	const char *loginname = getlogin ();
	const char *tty = ttyname (0);
	char *free_login = NULL, *free_tty = NULL;

	if (loginname != NULL) {
		free_login = xstrdup (loginname);
		loginname = free_login;
	}
	if (tty != NULL) {
		free_tty = xstrdup (tty);
		tty = free_tty;
	}

	if (loginname == NULL) {
		loginname = "???";
	}
	if (tty == NULL) {
		tty = "???";
	} else if (strncmp (tty, "/dev/", 5) == 0) {
		tty += 5;
	}
	SYSLOG ((LOG_INFO,
		 "user '%s' (login '%s' on %s) switched to group '%s'",
		 name, loginname, tty, group));
#ifdef USE_PAM
	/*
	 * We want to fork and exec the new shell in the child, leaving the
	 * parent waiting to log the session close.
	 *
	 * The parent must ignore signals generated from the console
	 * (SIGINT, SIGQUIT, SIGHUP) which might make the parent terminate
	 * before its child. When bash is exec'ed as the subshell, it
	 * generates a new process group id for itself, and consequently
	 * only SIGHUP, which is sent to all process groups in the session,
	 * can reach the parent. However, since arbitrary programs can be
	 * specified as login shells, there is no such guarantee in general.
	 * For the same reason, we must also ignore stop signals generated
	 * from the console (SIGTSTP, SIGTTIN, and SIGTTOU) in order to
	 * avoid any possibility of the parent being stopped when it
	 * receives SIGCHLD from the terminating subshell.  -- JWP
	 */
	{
		pid_t child;

		/* Ignore these signals. The signal handlers will later be
		 * restored to the default handlers. */
		(void) signal (SIGINT, SIG_IGN);
		(void) signal (SIGQUIT, SIG_IGN);
		(void) signal (SIGHUP, SIG_IGN);
		(void) signal (SIGTSTP, SIG_IGN);
		(void) signal (SIGTTIN, SIG_IGN);
		(void) signal (SIGTTOU, SIG_IGN);
		/* set SIGCHLD to default for waitpid */
		(void) signal(SIGCHLD, SIG_DFL);

		child = fork ();
		if ((pid_t)-1 == child) {
			/* error in fork() */
			fprintf (stderr, _("%s: failure forking: %s\n"),
				 is_newgrp ? "newgrp" : "sg", strerror (errno));
#ifdef WITH_AUDIT
			if (group) {
				SNPRINTF(audit_buf,
				         "changing new-group=%s", group);
				audit_logger (AUDIT_CHGRP_ID, Prog,
				              audit_buf, NULL, getuid (), 0);
			} else {
				audit_logger (AUDIT_CHGRP_ID, Prog,
				              "changing", NULL, getuid (), 0);
			}
#endif
			exit (EXIT_FAILURE);
		} else if (child != 0) {
			/* parent - wait for child to finish, then log session close */
			int cst = 0;
			gid_t gid = getgid();
			struct group *grp = getgrgid (gid);
			pid_t pid;

			do {
				errno = 0;
				pid = waitpid (child, &cst, WUNTRACED);
				if ((pid == child) && (WIFSTOPPED (cst) != 0)) {
					/* The child (shell) was suspended.
					 * Suspend sg/newgrp. */
					kill (getpid (), SIGSTOP);
					/* wake child when resumed */
					kill (child, SIGCONT);
				}
			} while (   ((pid == child) && (WIFSTOPPED (cst) != 0))
			         || ((pid != child) && (errno == EINTR)));
			/* local, no need for xgetgrgid */
			if (NULL != grp) {
				SYSLOG ((LOG_INFO,
				         "user '%s' (login '%s' on %s) returned to group '%s'",
				         name, loginname, tty, grp->gr_name));
			} else {
				SYSLOG ((LOG_INFO,
				         "user '%s' (login '%s' on %s) returned to group '%lu'",
				         name, loginname, tty,
				         (unsigned long) gid));
				/* Either the user's passwd entry has a
				 * GID that does not match with any group,
				 * or the group was deleted while the user
				 * was in a newgrp session.*/
				SYSLOG ((LOG_WARN,
				         "unknown GID '%lu' used by user '%s'",
				         (unsigned long) gid, name));
			}
			closelog ();
			exit ((0 != WIFEXITED (cst)) ? WEXITSTATUS (cst)
			                             : WTERMSIG (cst) + 128);
		}

		/* child - restore signals to their default state */
		(void) signal (SIGINT, SIG_DFL);
		(void) signal (SIGQUIT, SIG_DFL);
		(void) signal (SIGHUP, SIG_DFL);
		(void) signal (SIGTSTP, SIG_DFL);
		(void) signal (SIGTTIN, SIG_DFL);
		(void) signal (SIGTTOU, SIG_DFL);
	}
#endif				/* USE_PAM */
	free(free_login);
	free(free_tty);
}

/*
 * newgrp - change the invokers current real and effective group id
 */
int main (int argc, char **argv)
{
	bool initflag = false;
	int i;
	bool is_member = false;
	bool cflag = false;
	int err = 0;
	gid_t gid;
	char *cp;
	const char *progbase;
	const char *name, *prog;
	char *group = NULL;
	char *command = NULL;
	char **envp = environ;
	struct passwd *pwd;
	/*@null@*/struct group *grp;

#ifdef SHADOWGRP
	struct sgrp *sgrp;
#endif

#ifdef WITH_AUDIT
	audit_help_open ();
#endif

	check_fds ();

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	/*
	 * Save my name for error messages and save my real gid in case of
	 * errors. If there is an error i have to exec a new login shell for
	 * the user since her old shell won't have fork'd to create the
	 * process. Skip over the program name to the next command line
	 * argument.
	 *
	 * This historical comment, and the code itself, suggest that the
	 * behavior of the system/shell on which it was written differed
	 * significantly from the one I am using. If this process was
	 * started from a shell (including the login shell), it was fork'ed
	 * and exec'ed as a child by that shell. In order to get the user
	 * back to that shell, it is only necessary to exit from this
	 * process which terminates the child of the fork. The parent shell,
	 * which is blocked waiting for a signal, will then receive a
	 * SIGCHLD and will continue; any changes made to the process
	 * persona or the environment after the fork never occurred in the
	 * parent process.
	 *
	 * Bottom line: we want to save the name and real gid for messages,
	 * but we do not need to restore the previous process persona and we
	 * don't need to re-exec anything.  -- JWP
	 */

	/*
	 * Ensure that "Prog" is always either "newgrp" or "sg" to avoid
	 * injecting arbitrary strings into our stderr/stdout, as this can
	 * be an exploit vector.
	 */
	is_newgrp = (strcmp (Basename (argv[0]), "newgrp") == 0);
	Prog = is_newgrp ? "newgrp" : "sg";

	log_set_progname(Prog);
	log_set_logfd(stderr);
	OPENLOG (Prog);
	argc--;
	argv++;

	initenv ();

	pwd = get_my_pwent ();
	if (NULL == pwd) {
		fprintf (stderr, _("%s: Cannot determine your user name.\n"),
		         Prog);
#ifdef WITH_AUDIT
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              "changing", NULL, getuid (), 0);
#endif
		SYSLOG ((LOG_WARN, "Cannot determine the user name of the caller (UID %lu)",
		         (unsigned long) getuid ()));
		closelog ();
		exit (EXIT_FAILURE);
	}
	name = pwd->pw_name;

	/*
	 * Parse the command line. There are two accepted flags. The first
	 * is "-", which for newgrp means to re-create the entire
	 * environment as though a login had been performed, and "-c", which
	 * for sg causes a command string to be executed.
	 *
	 * The next argument, if present, must be the new group name. Any
	 * remaining remaining arguments will be used to execute a command
	 * as the named group. If the group name isn't present, I just use
	 * the login group ID of the current user.
	 *
	 * The valid syntax are
	 *      newgrp [-] [groupid]
	 *      newgrp [-l] [groupid]
	 *      sg [-]
	 *      sg [-] groupid [[-c command]
	 */
	if (   (argc > 0)
	    && (   (strcmp (argv[0], "-")  == 0)
	        || (strcmp (argv[0], "-l") == 0))) {
		argc--;
		argv++;
		initflag = true;
	}
	if (!is_newgrp) {
		/*
		 * Do the command line for everything that is
		 * not "newgrp".
		 */
		if ((argc > 0) && (argv[0][0] != '-')) {
			group = argv[0];
			argc--;
			argv++;
		} else {
			usage ();
			closelog ();
			exit (EXIT_FAILURE);
		}
		if (argc > 0) {

			/*
			 * skip -c if specified so both forms work:
			 * "sg group -c command" (as in the man page) or
			 * "sg group command" (as in the usage message).
			 */
			if ((argc > 1) && (strcmp (argv[0], "-c") == 0)) {
				command = argv[1];
			} else {
				command = argv[0];
			}
			cflag = true;
		}
	} else {
		/*
		 * Do the command line for "newgrp". It's just making sure
		 * there aren't any flags and getting the new group name.
		 */
		if ((argc > 0) && (argv[0][0] == '-')) {
			usage ();
			goto failure;
		} else if (argv[0] != NULL) {
			group = argv[0];
		} else {
			/*
			 * get the group file entry for her login group id.
			 * the entry must exist, simply to be annoying.
			 *
			 * Perhaps in the past, but the default behavior now depends on the
			 * group entry, so it had better exist.  -- JWP
			 */
			grp = xgetgrgid (pwd->pw_gid);
			if (NULL == grp) {
				fprintf (stderr,
				         _("%s: GID '%lu' does not exist\n"),
				         Prog, (unsigned long) pwd->pw_gid);
				SYSLOG ((LOG_CRIT, "GID '%lu' does not exist",
				        (unsigned long) pwd->pw_gid));
				goto failure;
			} else {
				group = grp->gr_name;
			}
		}
	}

#ifdef HAVE_SETGROUPS
	/*
	 * get the current users groupset. The new group will be added to
	 * the concurrent groupset if there is room, otherwise you get a
	 * nasty message but at least your real and effective group id's are
	 * set.
	 */
	/* don't use getgroups(0, 0) - it doesn't work on some systems */
	i = 16;
	for (;;) {
		grouplist = XMALLOC(i, GETGROUPS_T);
		ngroups = getgroups (i, grouplist);
		if (i > ngroups && !(ngroups == -1 && errno == EINVAL)) {
			break;
		}
		/* not enough room, so try allocating a larger buffer */
		free (grouplist);
		i *= 2;
	}
	if (ngroups < 0) {
		perror ("getgroups");
#ifdef WITH_AUDIT
		if (group) {
			SNPRINTF(audit_buf, "changing new-group=%s", group);
			audit_logger (AUDIT_CHGRP_ID, Prog,
			              audit_buf, NULL, getuid (), 0);
		} else {
			audit_logger (AUDIT_CHGRP_ID, Prog,
			              "changing", NULL, getuid (), 0);
		}
#endif
		exit (EXIT_FAILURE);
	}
#endif				/* HAVE_SETGROUPS */

	/*
	 * now we put her in the new group. The password file entry for her
	 * current user id has been gotten. If there was no optional group
	 * argument she will have her real and effective group id set to the
	 * set to the value from her password file entry.
	 *
	 * If run as newgrp, or as sg with no command, this process exec's
	 * an interactive subshell with the effective GID of the new group.
	 * If run as sg with a command, that command is exec'ed in this
	 * subshell. When this process terminates, either because the user
	 * exits, or the command completes, the parent of this process
	 * resumes with the current GID.
	 *
	 * If a group is explicitly specified on the command line, the
	 * interactive shell or command is run with that effective GID.
	 * Access will be denied if no entry for that group can be found in
	 * /etc/group. If the current user name appears in the members list
	 * for that group, access will be granted immediately; if not, the
	 * user will be challenged for that group's password. If the
	 * password response is incorrect, if the specified group does not
	 * have a password, or if that group has been locked by gpasswd -R,
	 * access will be denied. This is true even if the group specified
	 * has the user's login GID (as shown in /etc/passwd). If no group
	 * is explicitly specified on the command line, the effect is
	 * exactly the same as if a group name matching the user's login GID
	 * had been explicitly specified. Root, however, is never
	 * challenged for passwords, and is always allowed access.
	 *
	 * The previous behavior was to allow access to the login group if
	 * no explicit group was specified, irrespective of the group
	 * control file(s). This behavior is usually not desirable. A user
	 * wishing to return to the login group has only to exit back to the
	 * login shell. Generating yet more shell levels in order to
	 * provide a convenient "return" to the default group has the
	 * undesirable side effects of confusing the user, scrambling the
	 * history file, and consuming system resources. The default now is
	 * to lock out such behavior. A sys admin can allow it by explicitly
	 * including the user's name in the member list of the user's login
	 * group.  -- JWP
	 */
	grp = getgrnam (group); /* local, no need for xgetgrnam */
	if (NULL == grp) {
		fprintf (stderr, _("%s: group '%s' does not exist\n"), Prog, group);
		goto failure;
	}

#ifdef HAVE_SETGROUPS
	/* when using pam_group, she will not be listed in the groups
	 * database. However getgroups() will return the group. So
	 * if she is listed there already it is ok to grant membership.
	 */
	for (i = 0; i < ngroups; i++) {
		if (grp->gr_gid == grouplist[i]) {
			is_member = true;
			break;
		}
	}
#endif                          /* HAVE_SETGROUPS */
	/*
	 * For split groups (due to limitations of NIS), check all
	 * groups of the same GID like the requested group for
	 * membership of the current user.
	 */
	if (!is_member) {
		grp = find_matching_group (name, grp);
		if (NULL == grp) {
			/*
			 * No matching group found. As we already know that
			 * the group exists, this happens only in the case
			 * of a requested group where the user is not member.
			 *
			 * Re-read the group entry for further processing.
			 */
			grp = xgetgrnam (group);
			assert (NULL != grp);
		}
	}
#ifdef SHADOWGRP
	sgrp = getsgnam (group);
	if (NULL != sgrp) {
		grp->gr_passwd = sgrp->sg_passwd;
		grp->gr_mem = sgrp->sg_mem;
	}
#endif

	/*
	 * Check if the user is allowed to access this group.
	 */
	if (!is_member) {
		check_perms (grp, pwd, group);
	}

	/*
	 * all successful validations pass through this point. The group id
	 * will be set, and the group added to the concurrent groupset.
	 */
	if (getdef_bool ("SYSLOG_SG_ENAB")) {
		syslog_sg (name, group);
	}

	gid = grp->gr_gid;

#ifdef HAVE_SETGROUPS
	/*
	 * I am going to try to add her new group id to her concurrent group
	 * set. If the group id is already present i'll just skip this part.
	 * If the group doesn't fit, i'll complain loudly and skip this
	 * part.
	 */
	for (i = 0; i < ngroups; i++) {
		if (gid == grouplist[i]) {
			break;
		}
	}
	if (i == ngroups) {
		if (ngroups >= sysconf (_SC_NGROUPS_MAX)) {
			(void) fputs (_("too many groups\n"), stderr);
		} else {
			grouplist[ngroups++] = gid;
			if (setgroups (ngroups, grouplist) != 0) {
				perror ("setgroups");
			}
		}
	}
#endif

	/*
	 * Close all files before changing the user/group IDs.
	 *
	 * The needed structure should have been copied before, or
	 * permission to read the database will be required.
	 */
	endspent ();
#ifdef	SHADOWGRP
	endsgent ();
#endif
	endpwent ();
	endgrent ();

	/*
	 * Set the effective GID to the new group id and the effective UID
	 * to the real UID. For root, this also sets the real GID to the
	 * new group id.
	 */
	if (setgid (gid) != 0) {
		perror ("setgid");
#ifdef WITH_AUDIT
		SNPRINTF(audit_buf, "changing new-gid=%lu", (unsigned long) gid);
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              audit_buf, NULL, getuid (), 0);
#endif
		exit (EXIT_FAILURE);
	}

	if (setuid (getuid ()) != 0) {
		perror ("setuid");
#ifdef WITH_AUDIT
		SNPRINTF(audit_buf, "changing new-gid=%lu", (unsigned long) gid);
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              audit_buf, NULL, getuid (), 0);
#endif
		exit (EXIT_FAILURE);
	}

	/*
	 * See if the "-c" flag was used. If it was, i just create a shell
	 * command for her using the argument that followed the "-c" flag.
	 */
	if (cflag) {
		closelog ();
		execl (SHELL, "sh", "-c", command, (char *) NULL);
#ifdef WITH_AUDIT
		SNPRINTF(audit_buf, "changing new-gid=%lu", (unsigned long) gid);
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              audit_buf, NULL, getuid (), 0);
#endif
		perror (SHELL);
		exit ((errno == ENOENT) ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
	}

	/*
	 * I have to get the pathname of her login shell. As a favor, i'll
	 * try her environment for a $SHELL value first, and then try the
	 * password file entry. Obviously this shouldn't be in the
	 * restricted command directory since it could be used to leave the
	 * restricted environment.
	 *
	 * Note that the following assumes this user's entry in /etc/passwd
	 * does not have a chroot * prefix. If it does, the * will be copied
	 * verbatim into the exec path. This is probably not an issue
	 * because if this user is operating in a chroot jail, her entry in
	 * the version of /etc/passwd that is accessible here should
	 * probably never have a chroot shell entry (but entries for other
	 * users might). If I have missed something, and this causes you a
	 * problem, try using $SHELL as a workaround; also please notify me
	 * at jparmele@wildbear.com -- JWP
	 */
	cp = getenv ("SHELL");
	if (!initflag && (NULL != cp)) {
		prog = cp;
	} else if ((NULL != pwd->pw_shell) && ('\0' != pwd->pw_shell[0])) {
		prog = pwd->pw_shell;
	} else {
		prog = SHELL;
	}

	/*
	 * Now I try to find the basename of the login shell. This will
	 * become argv[0] of the spawned command.
	 */
	progbase = Basename (prog);

	/*
	 * Switch back to her home directory if i am doing login
	 * initialization.
	 */
	if (initflag) {
		if (chdir (pwd->pw_dir) != 0) {
			perror ("chdir");
		}

		while (NULL != *envp) {
			if (strncmp (*envp, "PATH=", 5) == 0 ||
			    strncmp (*envp, "HOME=", 5) == 0 ||
			    strncmp (*envp, "SHELL=", 6) == 0 ||
			    strncmp (*envp, "TERM=", 5) == 0)
				addenv (*envp, NULL);

			envp++;
		}
	} else {
		while (NULL != *envp) {
			addenv (*envp, NULL);
			envp++;
		}
	}

#ifdef WITH_AUDIT
	SNPRINTF(audit_buf, "changing new-gid=%lu", (unsigned long) gid);
	audit_logger (AUDIT_CHGRP_ID, Prog,
	              audit_buf, NULL, getuid (), 1);
#endif
	/*
	 * Exec the login shell and go away. We are trying to get back to
	 * the previous environment which should be the user's login shell.
	 */
	err = shell (prog, initflag ? NULL : progbase, newenvp);
	exit ((err == ENOENT) ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
	/*@notreached@*/
      failure:

	/*
	 * The previous code, when run as newgrp, re-exec'ed the shell in
	 * the current process with the original gid on error conditions.
	 * See the comment above. This historical behavior now has the
	 * effect of creating unlogged extraneous shell layers when the
	 * command line has an error or there is an authentication failure.
	 * We now just want to exit with error status back to the parent
	 * process. The closelog is probably unnecessary, but it does no
	 * harm.  -- JWP
	 */
	closelog ();
#ifdef WITH_AUDIT
	if (NULL != group) {
		SNPRINTF(audit_buf, "changing new-group=%s", group);
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              audit_buf, NULL, getuid (), 0);
	} else {
		audit_logger (AUDIT_CHGRP_ID, Prog,
		              "changing", NULL, getuid (), 0);
	}
#endif
	exit (EXIT_FAILURE);
}

