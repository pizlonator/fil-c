/*
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2002 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2011       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * grpconv - create or update /etc/gshadow with information from
 * /etc/group.
 *
 */

#include <config.h>
#ident "$Id$"

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#include "attr.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
/*@-exitarg@*/
#include "exitcodes.h"
#ifdef SHADOWGRP
#include "groupio.h"
#include "sgroupio.h"
#include "shadowlog.h"
/*
 * Global variables
 */
static const char Prog[] = "grpconv";

static bool gr_locked  = false;
static bool sgr_locked = false;

/* local function prototypes */
static void fail_exit (int status);
static void usage (int status);
static void process_flags (int argc, char **argv);

static void fail_exit (int status)
{
	if (gr_locked) {
		if (gr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, gr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
			/* continue */
		}
	}

	if (sgr_locked) {
		if (sgr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
			/* continue */
		}
	}

	exit (status);
}

static void usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options]\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * process_flags - parse the command line options
 *
 *	It will not return if an error is encountered.
 */
static void process_flags (int argc, char **argv)
{
	/*
	 * Parse the command line options.
	 */
	int c;
	static struct option long_options[] = {
		{"help", no_argument,       NULL, 'h'},
		{"root", required_argument, NULL, 'R'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "hR:",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		default:
			usage (E_USAGE);
		}
	}

	if (optind != argc) {
		usage (E_USAGE);
	}
}

int main (int argc, char **argv)
{
	const struct group *gr;
	struct group grent;
	const struct sgrp *sg;
	struct sgrp sgent;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

	OPENLOG (Prog);

	process_flags (argc, argv);

	if (gr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, gr_dbname ());
		fail_exit (5);
	}
	gr_locked = true;
	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, gr_dbname ());
		fail_exit (1);
	}

	if (sgr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, sgr_dbname ());
		fail_exit (5);
	}
	sgr_locked = true;
	if (sgr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, sgr_dbname ());
		fail_exit (1);
	}

	/*
	 * Remove /etc/gshadow entries for groups not in /etc/group.
	 */
	(void) sgr_rewind ();
	while ((sg = sgr_next ()) != NULL) {
		if (gr_locate (sg->sg_name) != NULL) {
			continue;
		}

		if (sgr_remove (sg->sg_name) == 0) {
			/*
			 * This shouldn't happen (the entry exists) but...
			 */
			fprintf (stderr,
			         _("%s: cannot remove entry '%s' from %s\n"),
			         Prog, sg->sg_name, sgr_dbname ());
			fail_exit (3);
		}
		(void) sgr_rewind ();
	}

	/*
	 * Update shadow group passwords if non-shadow password is not "x".
	 * Add any missing shadow group entries.
	 */
	(void) gr_rewind ();
	while ((gr = gr_next ()) != NULL) {
		sg = sgr_locate (gr->gr_name);
		if (NULL != sg) {
			/* update existing shadow group entry */
			sgent = *sg;
			if (strcmp (gr->gr_passwd, SHADOW_PASSWD_STRING) != 0)
				sgent.sg_passwd = gr->gr_passwd;
		} else {
			static char *empty = 0;

			/* add new shadow group entry */
			bzero(&sgent, sizeof sgent);
			sgent.sg_name = gr->gr_name;
			sgent.sg_passwd = gr->gr_passwd;
			sgent.sg_adm = &empty;
		}
		/*
		 * XXX - sg_mem is redundant, it is currently always a copy
		 * of gr_mem. Very few programs actually use sg_mem, and all
		 * of them are in the shadow suite. Maybe this field could
		 * be used for something else? Any suggestions?
		 */
		sgent.sg_mem = gr->gr_mem;

		if (sgr_update (&sgent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, sgr_dbname (), sgent.sg_name);
			fail_exit (3);
		}
		/* remove password from /etc/group */
		grent = *gr;
		grent.gr_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
		if (gr_update (&grent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, gr_dbname (), grent.gr_name);
			fail_exit (3);
		}
	}

	if (sgr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, sgr_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", sgr_dbname ()));
		fail_exit (3);
	}
	if (gr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", gr_dbname ()));
		fail_exit (3);
	}
	if (sgr_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sgr_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
		/* continue */
	}
	if (gr_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
		/* continue */
	}

	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_GROUP);

	return 0;
}
#else				/* !SHADOWGRP */
int main (MAYBE_UNUSED int argc, char **argv)
{
	fprintf (stderr,
		 "%s: not configured for shadow group support.\n", argv[0]);
	exit (1);
}
#endif				/* !SHADOWGRP */

