/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1997       , Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include "defines.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "prototypes.h"

/*
 * pwd_init - ignore signals, and set resource limits to safe
 * values.  Call this before modifying password files, so that
 * it is less likely to fail in the middle of operation.
 */
void pwd_init (void)
{
	struct rlimit rlim;

	rlim.rlim_cur = rlim.rlim_max = 0;
	setrlimit (RLIMIT_CORE, &rlim);

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit (RLIMIT_AS, &rlim);

	setrlimit (RLIMIT_CPU, &rlim);
	setrlimit (RLIMIT_DATA, &rlim);
	setrlimit (RLIMIT_FSIZE, &rlim);
	setrlimit (RLIMIT_NOFILE, &rlim);
#ifdef RLIMIT_RSS
	setrlimit (RLIMIT_RSS, &rlim);
#endif
	setrlimit (RLIMIT_STACK, &rlim);

	signal (SIGALRM, SIG_IGN);
	signal (SIGHUP, SIG_IGN);
	signal (SIGINT, SIG_IGN);
	signal (SIGPIPE, SIG_IGN);
	signal (SIGQUIT, SIG_IGN);
	signal (SIGTERM, SIG_IGN);
	signal (SIGTSTP, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);

	umask (077);
}
