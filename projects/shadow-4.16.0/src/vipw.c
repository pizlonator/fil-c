/*
  vipw, vigr  edit the password or group file
  with -s will edit shadow or gshadow file
 *
 * SPDX-FileCopyrightText: 1997       , Guy Maor <maor@ece.utexas.edu>
 * SPDX-FileCopyrightText: 1999 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2002 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2013, Nicolas François
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#ident "$Id$"

#include <errno.h>
#include <getopt.h>
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif				/* WITH_SELINUX */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "alloc.h"
#include "defines.h"
#include "getdef.h"
#include "groupio.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#include "pwio.h"
#include "sgroupio.h"
#include "shadowio.h"
/*@-exitarg@*/
#include "exitcodes.h"
#ifdef WITH_TCB
#include <tcb.h>
#include "tcbfuncs.h"
#endif				/* WITH_TCB */
#include "shadowlog.h"
#include "string/sprintf.h"


#define MSG_WARN_EDIT_OTHER_FILE _( \
	"You have modified %s.\n"\
	"You may need to modify %s for consistency.\n"\
	"Please use the command '%s' to do so.\n")

/*
 * Global variables
 */
static const char *Prog;

static const char *filename, *fileeditname;
static bool filelocked = false;
static bool createedit = false;
static int (*unlock) (void);
static bool quiet = false;
#ifdef WITH_TCB
static const char *user = NULL;
static bool tcb_mode = false;
#define SHADOWTCB_SCRATCHDIR ":tmp"
#endif				/* WITH_TCB */

/* local function prototypes */
static void usage (int status);
static int create_backup_file (FILE *, const char *, struct stat *);
static void vipwexit (const char *msg, int syserr, int ret);
static void vipwedit (const char *, int (*)(void), int (*)(void));

/*
 * usage - display usage message and exit
 */
static void usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (stderr,
	                _("Usage: %s [options]\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -g, --group                   edit group database\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -p, --passwd                  edit passwd database\n"), usageout);
	(void) fputs (_("  -q, --quiet                   quiet mode\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -s, --shadow                  edit shadow or gshadow database\n"), usageout);
#ifdef WITH_TCB
	(void) fputs (_("  -u, --user                    which user's tcb shadow file to edit\n"), usageout);
#endif				/* WITH_TCB */
	(void) fputs (_("\n"), usageout);
	exit (status);
}

/*
 *
 */
static int create_backup_file (FILE * fp, const char *backup, struct stat *sb)
{
	struct utimbuf ub;
	FILE *bkfp;
	int c;
	mode_t mask;

	mask = umask (077);
	bkfp = fopen (backup, "w");
	(void) umask (mask);
	if (NULL == bkfp) {
		return -1;
	}

	c = 0;
	if (fseeko (fp, 0, SEEK_SET) == 0)
		while ((c = getc (fp)) != EOF) {
			if (putc (c, bkfp) == EOF) {
				break;
			}
		}
	if ((EOF != c) || (ferror (fp) != 0) || (fflush (bkfp) != 0)) {
		fclose (bkfp);
		unlink (backup);
		return -1;
	}
	if (fsync (fileno (bkfp)) != 0) {
		(void) fclose (bkfp);
		unlink (backup);
		return -1;
	}
	if (fclose (bkfp) != 0) {
		unlink (backup);
		return -1;
	}

	ub.actime = sb->st_atime;
	ub.modtime = sb->st_mtime;
	if (   (utime (backup, &ub) != 0)
	    || (chmod (backup, sb->st_mode) != 0)
	    || (chown (backup, sb->st_uid, sb->st_gid) != 0)) {
		unlink (backup);
		return -1;
	}
	return 0;
}

/*
 *
 */
static void vipwexit (const char *msg, int syserr, int ret)
{
	int err = errno;

	if (createedit) {
		if (unlink (fileeditname) != 0) {
			fprintf (stderr, _("%s: failed to remove %s\n"), Prog, fileeditname);
			/* continue */
		}
	}
	if (filelocked) {
		if ((*unlock) () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, fileeditname);
			SYSLOG ((LOG_ERR, "failed to unlock %s", fileeditname));
			/* continue */
		}
	}
	if (NULL != msg) {
		fprintf (stderr, "%s: %s", Prog, msg);
	}
	if (0 != syserr) {
		fprintf (stderr, ": %s", strerror (err));
	}
	if (   (NULL != msg)
	    || (0 != syserr)) {
		(void) fputs ("\n", stderr);
	}
	if (!quiet) {
		fprintf (stdout, _("%s: %s is unchanged\n"), Prog,
			 filename);
	}
	exit (ret);
}

#ifndef DEFAULT_EDITOR
#define DEFAULT_EDITOR "vi"
#endif

/*
 *
 */
static void
vipwedit (const char *file, int (*file_lock) (void), int (*file_unlock) (void))
{
	int          status;
	char         *to_rename;
	FILE         *f;
	pid_t        pid, orig_pgrp, editor_pgrp = -1;
	sigset_t     mask, omask;
	const char   *editor;
	struct stat  st1, st2;
	/* FIXME: the following should have variable sizes */
	char         filebackup[1024], fileedit[1024];

	SNPRINTF(filebackup, "%s-", file);
#ifdef WITH_TCB
	if (tcb_mode) {
		if (   (mkdir (TCB_DIR "/" SHADOWTCB_SCRATCHDIR, 0700) != 0)
		    && (errno != EEXIST)) {
			vipwexit (_("failed to create scratch directory"), errno, 1);
		}
		if (shadowtcb_drop_priv () == SHADOWTCB_FAILURE) {
			vipwexit (_("failed to drop privileges"), errno, 1);
		}
		SNPRINTF(fileedit,
		         TCB_DIR "/" SHADOWTCB_SCRATCHDIR "/.vipw.shadow.%s",
		         user);
	} else {
#endif				/* WITH_TCB */
		SNPRINTF(fileedit, "%s.edit", file);
#ifdef WITH_TCB
	}
#endif				/* WITH_TCB */
	unlock = file_unlock;
	filename = file;
	fileeditname = fileedit;

	if (access (file, F_OK) != 0) {
		vipwexit (file, 1, 1);
	}
#ifdef WITH_SELINUX
	/* if SE Linux is enabled then set the context of all new files
	   to be the context of the file we are editing */
	if (is_selinux_enabled () != 0) {
		char *passwd_context_raw = NULL;
		int ret = 0;
		if (getfilecon_raw (file, &passwd_context_raw) < 0) {
			vipwexit (_("Couldn't get file context"), errno, 1);
		}
		ret = setfscreatecon_raw (passwd_context_raw);
		freecon (passwd_context_raw);
		if (0 != ret) {
			vipwexit (_("setfscreatecon () failed"), errno, 1);
		}
	}
#endif				/* WITH_SELINUX */
#ifdef WITH_TCB
	if (tcb_mode && (shadowtcb_gain_priv () == SHADOWTCB_FAILURE)) {
		vipwexit (_("failed to gain privileges"), errno, 1);
	}
#endif				/* WITH_TCB */
	if (file_lock () == 0) {
		vipwexit (_("Couldn't lock file"), errno, 5);
	}
	filelocked = true;
#ifdef WITH_TCB
	if (tcb_mode && (shadowtcb_drop_priv () == SHADOWTCB_FAILURE)) {
		vipwexit (_("failed to drop privileges"), errno, 1);
	}
#endif				/* WITH_TCB */

	/* edited copy has same owners, perm */
	if (stat (file, &st1) != 0) {
		vipwexit (file, 1, 1);
	}
	f = fopen (file, "r");
	if (NULL == f) {
		vipwexit (file, 1, 1);
	}
#ifdef WITH_TCB
	if (tcb_mode && (shadowtcb_gain_priv () == SHADOWTCB_FAILURE))
		vipwexit (_("failed to gain privileges"), errno, 1);
#endif				/* WITH_TCB */
	if (create_backup_file (f, fileedit, &st1) != 0) {
		vipwexit (_("Couldn't make backup"), errno, 1);
	}
	(void) fclose (f);
	createedit = true;

	editor = getenv ("VISUAL");
	if (NULL == editor) {
		editor = getenv ("EDITOR");
	}
	if (NULL == editor) {
		editor = DEFAULT_EDITOR;
	}

	orig_pgrp = tcgetpgrp(STDIN_FILENO);

	pid = fork ();
	if (-1 == pid) {
		vipwexit ("fork", 1, 1);
	} else if (0 == pid) {
		/* use the system() call to invoke the editor so that it accepts
		   command line args in the EDITOR and VISUAL environment vars */
		char  *buf;

		/* Wait for parent to make us the foreground pgrp. */
		if (orig_pgrp != -1) {
			pid = getpid();
			setpgid(0, 0);
			while (tcgetpgrp(STDIN_FILENO) != pid)
				continue;
		}

		xasprintf(&buf, "%s %s", editor, fileedit);

		status = system (buf);
		if (-1 == status) {
			fprintf (stderr, _("%s: %s: %s\n"), Prog, editor,
			         strerror (errno));
			exit (1);
		} else if (   WIFEXITED (status)
		           && (WEXITSTATUS (status) != 0)) {
			fprintf (stderr, _("%s: %s returned with status %d\n"),
			         Prog, editor, WEXITSTATUS (status));
			exit (WEXITSTATUS (status));
		} else if (WIFSIGNALED (status)) {
			fprintf (stderr, _("%s: %s killed by signal %d\n"),
			         Prog, editor, WTERMSIG (status));
			exit (1);
		} else {
			exit (0);
		}
	}

	/* Run child in a new pgrp and make it the foreground pgrp. */
	if (orig_pgrp != -1) {
		setpgid(pid, pid);
		tcsetpgrp(STDIN_FILENO, pid);

		/* Avoid SIGTTOU when changing foreground pgrp below. */
		sigemptyset(&mask);
		sigaddset(&mask, SIGTTOU);
		sigprocmask(SIG_BLOCK, &mask, &omask);
	}

	/* set SIGCHLD to default for waitpid */
	signal(SIGCHLD, SIG_DFL);

	for (;;) {
		pid = waitpid (pid, &status, WUNTRACED);
		if ((pid != -1) && (WIFSTOPPED (status) != 0)) {
			/* The child (editor) was suspended.
			 * Restore terminal pgrp and suspend vipw. */
			if (orig_pgrp != -1) {
				editor_pgrp = tcgetpgrp(STDIN_FILENO);
				if (editor_pgrp == -1) {
					fprintf (stderr, "%s: %s: %s", Prog,
						 "tcgetpgrp", strerror (errno));
				}
				if (tcsetpgrp(STDIN_FILENO, orig_pgrp) == -1) {
					fprintf (stderr, "%s: %s: %s", Prog,
						 "tcsetpgrp", strerror (errno));
				}
			}
			kill (getpid (), SIGSTOP);
			/* wake child when resumed */
			if (editor_pgrp != -1) {
				if (tcsetpgrp(STDIN_FILENO, editor_pgrp) == -1) {
					fprintf (stderr, "%s: %s: %s", Prog,
						 "tcsetpgrp", strerror (errno));
				}
			}
			killpg (pid, SIGCONT);
		} else {
			break;
		}
	}

	if (orig_pgrp != -1)
		sigprocmask(SIG_SETMASK, &omask, NULL);

	if (-1 == pid) {
		vipwexit (editor, 1, 1);
	} else if (   WIFEXITED (status)
	           && (WEXITSTATUS (status) != 0)) {
		vipwexit (NULL, 0, WEXITSTATUS (status));
	} else if (WIFSIGNALED (status)) {
		fprintf (stderr, _("%s: %s killed by signal %d\n"),
		         Prog, editor, WTERMSIG(status));
		vipwexit (NULL, 0, 1);
	}

	if (stat (fileedit, &st2) != 0) {
		vipwexit (fileedit, 1, 1);
	}
	if (st1.st_mtime == st2.st_mtime) {
		vipwexit (0, 0, 0);
	}
#ifdef WITH_SELINUX
	/* unset the fscreatecon */
	if (is_selinux_enabled () != 0) {
		if (setfscreatecon_raw (NULL) != 0) {
			vipwexit (_("setfscreatecon () failed"), errno, 1);
		}
	}
#endif				/* WITH_SELINUX */

	/*
	 * XXX - here we should check fileedit for errors; if there are any,
	 * ask the user what to do (edit again, save changes anyway, or quit
	 * without saving). Use pwck or grpck to do the check.  --marekm
	 */
	createedit = false;
#ifdef WITH_TCB
	if (tcb_mode) {
		f = fopen (fileedit, "r");
		if (NULL == f) {
			vipwexit (_("failed to open scratch file"), errno, 1);
		}
		if (unlink (fileedit) != 0) {
			vipwexit (_("failed to unlink scratch file"), errno, 1);
		}
		if (shadowtcb_drop_priv () == SHADOWTCB_FAILURE) {
			vipwexit (_("failed to drop privileges"), errno, 1);
		}
		if (stat (file, &st1) != 0) {
			vipwexit (_("failed to stat edited file"), errno, 1);
		}
		if (asprintf(&to_rename, "%s+", file) == -1)
			vipwexit (_("asprintf(3) failed"), errno, 1);

		if (create_backup_file (f, to_rename, &st1) != 0) {
			free(to_rename);
			vipwexit (_("failed to create backup file"), errno, 1);
		}
		(void) fclose (f);
	} else {
#endif				/* WITH_TCB */
		to_rename = fileedit;
#ifdef WITH_TCB
	}
#endif				/* WITH_TCB */
	unlink (filebackup);
	link (file, filebackup);
	if (rename (to_rename, file) == -1) {
		fprintf (stderr,
		         _("%s: can't restore %s: %s (your changes are in %s)\n"),
		         Prog, file, strerror (errno), to_rename);
#ifdef WITH_TCB
		if (tcb_mode) {
			free(to_rename);
		}
#endif				/* WITH_TCB */
		vipwexit (0, 0, 1);
	}

#ifdef WITH_TCB
	if (tcb_mode) {
		free(to_rename);
		if (shadowtcb_gain_priv () == SHADOWTCB_FAILURE) {
			vipwexit (_("failed to gain privileges"), errno, 1);
		}
	}
#endif				/* WITH_TCB */

	if ((*file_unlock) () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, fileeditname);
		SYSLOG ((LOG_ERR, "failed to unlock %s", fileeditname));
		/* continue */
	}
	SYSLOG ((LOG_INFO, "file %s edited", fileeditname));
}

int main (int argc, char **argv)
{
	bool  editshadow = false;
	bool  do_vigr;

	do_vigr = (strcmp(Basename(argv[0]), "vigr") == 0);

	Prog = do_vigr ? "vigr" : "vipw";
	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

	OPENLOG(Prog);

	{
		/*
		 * Parse the command line options.
		 */
		int c;
		static struct option long_options[] = {
			{"group",  no_argument,       NULL, 'g'},
			{"help",   no_argument,       NULL, 'h'},
			{"passwd", no_argument,       NULL, 'p'},
			{"quiet",  no_argument,       NULL, 'q'},
			{"root",   required_argument, NULL, 'R'},
			{"shadow", no_argument,       NULL, 's'},
#ifdef WITH_TCB
			{"user",   required_argument, NULL, 'u'},
#endif				/* WITH_TCB */
			{NULL, 0, NULL, '\0'}
		};
		while ((c = getopt_long (argc, argv,
#ifdef WITH_TCB
		                         "ghpqR:su:",
#else				/* !WITH_TCB */
		                         "ghpqR:s",
#endif				/* !WITH_TCB */
		                         long_options, NULL)) != -1) {
			switch (c) {
			case 'g':
				do_vigr = true;
				break;
			case 'h':
				usage (E_SUCCESS);
				break;
			case 'p':
				do_vigr = false;
				break;
			case 'q':
				quiet = true;
				break;
			case 'R': /* no-op, handled in process_root_flag () */
				break;
			case 's':
				editshadow = true;
				break;
#ifdef WITH_TCB
			case 'u':
				user = optarg;
				break;
#endif				/* WITH_TCB */
			default:
				usage (E_USAGE);
			}
		}

		if (optind != argc) {
			usage (E_USAGE);
		}
	}

	if (do_vigr) {
#ifdef SHADOWGRP
		if (editshadow) {
			vipwedit (sgr_dbname (), sgr_lock, sgr_unlock);
			printf (MSG_WARN_EDIT_OTHER_FILE,
			        sgr_dbname (),
			        gr_dbname (),
			        "vigr");
		} else {
#endif				/* SHADOWGRP */
			vipwedit (gr_dbname (), gr_lock, gr_unlock);
#ifdef SHADOWGRP
			if (sgr_file_present ()) {
				printf (MSG_WARN_EDIT_OTHER_FILE,
				        gr_dbname (),
				        sgr_dbname (),
				        "vigr -s");
			}
		}
#endif				/* SHADOWGRP */
	} else {
		if (editshadow) {
#ifdef WITH_TCB
			if (getdef_bool ("USE_TCB") && (NULL != user)) {
				if (shadowtcb_set_user (user) == SHADOWTCB_FAILURE) {
					fprintf (stderr,
					         _("%s: failed to find tcb directory for %s\n"),
					         Prog, user);
					return E_SHADOW_NOTFOUND;
				}
				tcb_mode = true;
			}
#endif				/* WITH_TCB */
			vipwedit (spw_dbname (), spw_lock, spw_unlock);
			printf (MSG_WARN_EDIT_OTHER_FILE,
			        spw_dbname (),
			        pw_dbname (),
			        "vipw");
		} else {
			vipwedit (pw_dbname (), pw_lock, pw_unlock);
			if (spw_file_present ()) {
				printf (MSG_WARN_EDIT_OTHER_FILE,
				        pw_dbname (),
				        spw_dbname (),
				        "vipw -s");
			}
		}
	}

	nscd_flush_cache ("passwd");
	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_PASSWD | SSSD_DB_GROUP);

	return E_SUCCESS;
}

