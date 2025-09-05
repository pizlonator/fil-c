/*	$OpenBSD: spawn.c,v 1.11 2006/08/01 22:16:03 jason Exp $	*/

/* This file is in the public domain. */

/*
 * Spawn.  Actually just suspends Mg.
 * Assumes POSIX job control.
 */

#include <signal.h>
#include <stdio.h>

#include "ttydef.h"
#include "def.h"

/*
 * This causes mg to send itself a stop signal.  It assumes the parent
 * shell supports POSIX job control.  If the terminal supports an alternate
 * screen, we will switch to it.
 */
int
spawncli(int f, int n)
{
	sigset_t	oset;

	/* Very similar to what vttidy() does. */
	ttcolor(CTEXT);
	ttnowindow();
	ttmove(nrow - 1, 0);
	if (epresf != FALSE) {
		tteeol();
		epresf = FALSE;
	}
	if (ttcooked() == FALSE)
		return (FALSE);

	/* Exit application mode and tidy. */
	tttidy();
	ttflush();
	(void)sigprocmask(SIG_SETMASK, NULL, &oset);
	(void)kill(0, SIGTSTP);
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	ttreinit();

	/* Force repaint. */
	sgarbf = TRUE;
	return (ttraw());
}
