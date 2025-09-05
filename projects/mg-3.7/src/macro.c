/*	$OpenBSD: macro.c,v 1.17 2022/12/26 19:16:02 jmc Exp $	*/

/* This file is in the public domain. */

/*
 *	Keyboard macros.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "def.h"
#include "key.h"
#include "macro.h"

int inmacro = FALSE;	/* Macro playback in progress */
int macrodef = FALSE;	/* Macro recording in progress */
int macrocount = 0;

struct line *maclhead = NULL;
struct line *maclcur;

union macrodef macro[MAXMACRO];

int
definemacro(int f, int n)
{
	struct line	*lp1, *lp2;

	macrocount = 0;

	if (macrodef) {
		ewprintf("already defining macro");
		return (macrodef = FALSE);
	}

	/* free lines allocated for string arguments */
	if (maclhead != NULL) {
		for (lp1 = maclhead->l_fp; lp1 != maclhead; lp1 = lp2) {
			lp2 = lp1->l_fp;
			free(lp1);
		}
		free(lp1);
	}

	if ((maclhead = lp1 = lalloc(0)) == NULL)
		return (FALSE);

	ewprintf("Defining Keyboard Macro...");
	maclcur = lp1->l_fp = lp1->l_bp = lp1;
	return (macrodef = TRUE);
}

int
finishmacro(int f, int n)
{
	if (macrodef == TRUE) {
		macrodef = FALSE;
		ewprintf("End Keyboard Macro Definition");
		return (TRUE);
	}
	return (FALSE);
}

int
executemacro(int f, int n)
{
	int	 i, j, flag, num;
	PF	 funct;

	if (macrodef ||
	    (macrocount >= MAXMACRO && macro[MAXMACRO - 1].m_funct
	    != finishmacro)) {
		dobeep();
		ewprintf("Macro too long. Aborting.");
		return (FALSE);
	}

	if (macrocount == 0)
		return (TRUE);

	inmacro = TRUE;

	for (i = n; i > 0; i--) {
		maclcur = maclhead->l_fp;
		flag = 0;
		num = 1;
		for (j = 0; j < macrocount - 1; j++) {
			funct = macro[j].m_funct;
			if (funct == universal_argument) {
				flag = FFARG;
				num = macro[++j].m_count;
				continue;
			}
			if ((*funct)(flag, num) != TRUE) {
				inmacro = FALSE;
				return (FALSE);
			}
			lastflag = thisflag;
			thisflag = 0;
			flag = 0;
			num = 1;
		}
	}
	inmacro = FALSE;
	return (TRUE);
}

int
endorexecmacro(int f, int n)
{
	if (macrodef)
		return finishmacro(f, n);

	return executemacro(f, n);
}

int
applymacro(int f, int n)
{
	struct line	*odotp, *omarkp, *tmarkp;
	int	odoto, odotline, omarko, omarkline, tmarko, tmarkline;

	if (curwp->w_markp == NULL) {
		dobeep();
		ewprintf("No mark set in this window");
		return (FALSE);
	}

	/* (save-mark-and-excursion) save the state of the "." and mark */
	odotp = curwp->w_dotp;
	omarkp = curwp->w_markp;
	odoto = curwp->w_doto;
	odotline = curwp->w_dotline;
	omarko = curwp->w_marko;
	omarkline = curwp->w_markline;

	if (curwp->w_dotline > curwp->w_markline) {
		swapmark(FFRAND, 0);
	}

	tmarkp = curwp->w_markp;
	tmarko = curwp->w_marko;
	tmarkline = curwp->w_markline;

	while (curwp->w_dotline < curwp->w_markline) {
		gotobol(FFRAND, 1);
		executemacro(FFRAND, 1);
		forwline(FFRAND, 1);

		/* restore mark that may have been altered by the executed macro */
		curwp->w_markp = tmarkp;
		curwp->w_marko = tmarko;
		curwp->w_markline = tmarkline;
	}

	/* (save-mark-and-excursion) restore the state of the "." and mark */
	curwp->w_dotp = odotp;
	curwp->w_doto = odoto;
	curwp->w_dotline = odotline;
	curwp->w_markp = omarkp;
	curwp->w_marko = omarko;
	curwp->w_markline = omarkline;

	return (TRUE);
}
