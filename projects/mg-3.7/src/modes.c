/*	$OpenBSD: modes.c,v 1.22 2023/04/17 09:49:04 op Exp $	*/

/* This file is in the public domain. */

/*
 * Commands to toggle modes.   Without an argument, these functions will
 * toggle the given mode.  A negative or zero argument will turn the mode
 * off.  A positive argument will turn the mode on.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "kbd.h"

int	changemode(int, int, char *);

int	 defb_nmodes = 0;
struct maps_s	*defb_modes[PBMODES] = { &fundamental_mode };
int	 defb_flag = 0;

int
changemode(int f, int n, char *newmode)
{
	int	 i;
	struct maps_s	*m;

	if ((m = name_mode(newmode)) == NULL) {
		dobeep();
		ewprintf("Can't find mode %s", newmode);
		return (FALSE);
	}
	if (!(f & FFARG)) {
		for (i = 0; i <= curbp->b_nmodes; i++)
			if (curbp->b_modes[i] == m) {
				/* mode already set */
				n = 0;
				break;
			}
	}
	if (n > 0) {
		for (i = 0; i <= curbp->b_nmodes; i++)
			if (curbp->b_modes[i] == m)
				/* mode already set */
				return (TRUE);
		if (curbp->b_nmodes >= PBMODES - 1) {
			dobeep();
			ewprintf("Too many modes");
			return (FALSE);
		}
		curbp->b_modes[++(curbp->b_nmodes)] = m;
	} else {
		/* fundamental is b_modes[0] and can't be unset */
		for (i = 1; i <= curbp->b_nmodes && m != curbp->b_modes[i];
		    i++);
		if (i > curbp->b_nmodes)
			return (TRUE);	/* mode wasn't set */
		for (; i < curbp->b_nmodes; i++)
			curbp->b_modes[i] = curbp->b_modes[i + 1];
		curbp->b_nmodes--;
	}
	upmodes(curbp);
	return (TRUE);
}

int
indentmode(int f, int n)
{
	return (changemode(f, n, "indent"));
}

int
fillmode(int f, int n)
{
	return (changemode(f, n, "fill"));
}

int
notabmode(int f, int n)
{
	if (changemode(f, n, "notab") == FALSE)
		return (FALSE);
	if (f & FFARG) {
		if (n <= 0)
			curbp->b_flag &= ~BFNOTAB;
		else
			curbp->b_flag |= BFNOTAB;
	} else
		curbp->b_flag ^= BFNOTAB;
	return (TRUE);
}

int
overwrite_mode(int f, int n)
{
	if (changemode(f, n, "overwrite") == FALSE)
		return (FALSE);
	if (f & FFARG) {
		if (n <= 0)
			curbp->b_flag &= ~BFOVERWRITE;
		else
			curbp->b_flag |= BFOVERWRITE;
	} else
		curbp->b_flag ^= BFOVERWRITE;
	return (TRUE);
}

int
set_default_mode(int f, int n)
{
	int	 i;
	struct maps_s	*m;
	char	 modebuf[32], *bufp;

	if ((bufp = eread("Set Default Mode: ", modebuf, sizeof(modebuf),
	    EFNEW)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if ((m = name_mode(modebuf)) == NULL) {
		dobeep();
		ewprintf("can't find mode %s", modebuf);
		return (FALSE);
	}
	if (!(f & FFARG)) {
		for (i = 0; i <= defb_nmodes; i++)
			if (defb_modes[i] == m) {
				/* mode already set */
				n = 0;
				break;
			}
	}
	if (n > 0) {
		for (i = 0; i <= defb_nmodes; i++)
			if (defb_modes[i] == m)
				/* mode already set */
				return (TRUE);
		if (defb_nmodes >= PBMODES - 1) {
			dobeep();
			ewprintf("Too many modes");
			return (FALSE);
		}
		defb_modes[++defb_nmodes] = m;
	} else {
		/* fundamental is defb_modes[0] and can't be unset */
		for (i = 1; i <= defb_nmodes && m != defb_modes[i]; i++);
		if (i > defb_nmodes)
			/* mode was not set */
			return (TRUE);
		for (; i < defb_nmodes; i++)
			defb_modes[i] = defb_modes[i + 1];
		defb_nmodes--;
	}
	if (strcmp(modebuf, "overwrite") == 0) {
		if (n <= 0)
			defb_flag &= ~BFOVERWRITE;
		else
			defb_flag |= BFOVERWRITE;
	}
	if (strcmp(modebuf, "notab") == 0) {
		if (n <= 0)
			defb_flag &= ~BFNOTAB;
		else
			defb_flag |= BFNOTAB;
	}
	return (TRUE);
}
