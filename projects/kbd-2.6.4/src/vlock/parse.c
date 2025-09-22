
/*

  Parse command line options for vlock, the VT locking program for linux.

  Copyright (C) 1994-1998 Michael K. Johnson <johnsonm@redhat.com>
  Copyright (C) 2002, 2005 Dmitry V. Levin <ldv@altlinux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sysexits.h>

#include "vlock.h"

#include "libcommon.h"

/*
 * This determines whether the default behavior is to lock only the
 * current VT or all of them.
 * 0 means current, 1 means all.
 */
int o_lock_all;

const char *
locked_name(void)
{
	return o_lock_all ? "console" : (is_vt ? "VC" : "tty");
}

static void __attribute__((__noreturn__))
show_usage(void)
{
	fprintf(stderr,
	        _("Try `%s --help' for more information.\n"),
	        program_invocation_short_name);
	exit(EX_USAGE);
}

static void __attribute__((__noreturn__))
show_help(void)
{
	const char *progname = get_progname();
	printf(_("%s: locks virtual consoles, saving your current session.\n"
	         "Usage: %s [options]\n"
	         "       Where [options] are any of:\n"
	         "-c or --current: lock only this virtual console, allowing user to\n"
	         "       switch to other virtual consoles.\n"
	         "-a or --all: lock all virtual consoles by preventing other users\n"
	         "       from switching virtual consoles.\n"
	         "-v or --version: Print the version number of vlock and exit.\n"
	         "-h or --help: Print this help message and exit.\n"),
	       progname, progname);
	exit(0);
}

void parse(int ac, char *const av[])
{
	static struct option long_options[] = {
		{ "current", 0, 0, 'c' },
		{ "all", 0, 0, 'a' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ 0, 0, 0, 0 },
	};
	int c;

	while ((c = getopt_long(ac, av, "acvh", long_options, 0)) != -1) {
		switch (c) {
			case 'c':
				o_lock_all = 0;
				break;
			case 'a':
				o_lock_all = 1;
				break;
			case 'v':
				fprintf(stderr, "%s\n", VERSION);
				exit(EXIT_SUCCESS);
				break;
			case 'h':
				show_help();
				break;
			default:
				show_usage();
				break;
		}
	}
}
