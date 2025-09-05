/*	$OpenBSD: basic.c,v 1.54 2023/04/21 13:39:36 op Exp $	*/

/* This file is in the public domain */

/*
 *		Basic cursor motion commands.
 *
 * The routines in this file are the basic
 * command functions for moving the cursor around on
 * the screen, setting mark, and swapping dot with
 * mark. Only moves between lines, which might make the
 * current buffer framing bad, are hard.
 */

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "def.h"

#define percint(n1, n2)		((n1 * (int) n2) * 0.1)

/*
 * Go to beginning of line.
 */
int
gotobol(int f, int n)
{
	if (n == 0)
		return (TRUE);

	curwp->w_doto = 0;
	return (TRUE);
}

/*
 * Move cursor backwards. Do the
 * right thing if the count is less than
 * 0. Error if you try to move back from
 * the beginning of the buffer.
 */
int
backchar(int f, int n)
{
	struct line   *lp;

	if (n < 0)
		return (forwchar(f, -n));
	while (n--) {
		if (curwp->w_doto == 0) {
			if ((lp = lback(curwp->w_dotp)) == curbp->b_headp) {
				if (!(f & FFRAND))
					(void)dobeep_msg("Beginning "
					    "of buffer");
				return (FALSE);
			}
			curwp->w_dotp = lp;
			curwp->w_doto = llength(lp);
			curwp->w_rflag |= WFMOVE;
			curwp->w_dotline--;
		} else
			curwp->w_doto--;
	}
	return (TRUE);
}

/*
 * Go to end of line.
 */
int
gotoeol(int f, int n)
{
	if (n == 0)
		return (TRUE);

	curwp->w_doto = llength(curwp->w_dotp);
	return (TRUE);
}

/*
 * Move cursor forwards. Do the
 * right thing if the count is less than
 * 0. Error if you try to move forward
 * from the end of the buffer.
 */
int
forwchar(int f, int n)
{
	if (n < 0)
		return (backchar(f, -n));
	while (n--) {
		if (curwp->w_doto == llength(curwp->w_dotp)) {
			curwp->w_dotp = lforw(curwp->w_dotp);
			if (curwp->w_dotp == curbp->b_headp) {
				curwp->w_dotp = lback(curwp->w_dotp);
				if (!(f & FFRAND))
					(void)dobeep_msg("End of buffer");
				return (FALSE);
			}
			curwp->w_doto = 0;
			curwp->w_dotline++;
			curwp->w_rflag |= WFMOVE;
		} else
			curwp->w_doto++;
	}
	return (TRUE);
}

/*
 * Go to the beginning of the buffer. Setting WFFULL is conservative,
 * but almost always the case. A universal argument of higher than 9
 * puts the cursor back to the end of buffer.
 */
int
gotobob(int f, int n)
{
	if (!curwp->w_markp)
		(void) setmark(f, n);
	curwp->w_dotp = bfirstlp(curbp);
	curwp->w_doto = 0;
	curwp->w_rflag |= WFFULL;
	curwp->w_dotline = 1;
	if (f & FFOTHARG && n > 0) {
		if (n > 9)
			gotoeob(0, 0);
		else
			forwline(f, percint(curwp->w_bufp->b_lines, n) - 1);
	}
	return (TRUE);
}

/*
 * Go to the end of the buffer. Leave dot 3 lines from the bottom of the
 * window if buffer length is longer than window length; same as emacs.
 * Setting WFFULL is conservative, but almost always the case. A universal
 * argument of higher than 9 puts the cursor back to the start of buffer.
 */
int
gotoeob(int f, int n)
{
	int		 ln;
	struct line	*lp;

	if (!curwp->w_markp)
		(void) setmark(f, n);
	curwp->w_dotp = blastlp(curbp);
	curwp->w_doto = llength(curwp->w_dotp);
	curwp->w_dotline = curwp->w_bufp->b_lines;

	lp = curwp->w_dotp;
	ln = curwp->w_ntrows - 3;

	if (ln < curwp->w_bufp->b_lines && ln >= 3) {
		while (ln--)
			curwp->w_dotp = lback(curwp->w_dotp);

		curwp->w_linep = curwp->w_dotp;
		curwp->w_dotp = lp;
	}
	if (f & FFOTHARG && n > 0) {
		if (n > 9)
			gotobob(0, 0);
		else
			backline(f, percint(curwp->w_bufp->b_lines, n));
	}

	curwp->w_rflag |= WFFULL;
	return (TRUE);
}

/*
 * Move forward by full lines.
 * If the number of lines to move is less
 * than zero, call the backward line function to
 * actually do it. The last command controls how
 * the goal column is set.
 */
int
forwline(int f, int n)
{
	struct line  *dlp;

	if (n < 0)
		return (backline(f | FFRAND, -n));
	if ((dlp = curwp->w_dotp) == curbp->b_headp) {
		if (!(f & FFRAND))
			(void)dobeep_msg("End of buffer");
		return(TRUE);
	}
	if ((lastflag & CFCPCN) == 0)	/* Fix goal. */
		setgoal();
	thisflag |= CFCPCN;
	if (n == 0)
		return (TRUE);
	while (n--) {
		dlp = lforw(dlp);
		if (dlp == curbp->b_headp) {
			curwp->w_dotp = lback(dlp);
			curwp->w_doto = llength(curwp->w_dotp);
			curwp->w_rflag |= WFMOVE;
			if (!(f & FFRAND))
				(void)dobeep_msg("End of buffer");
			return (TRUE);
		}
		curwp->w_dotline++;
	}
	curwp->w_rflag |= WFMOVE;
	curwp->w_dotp = dlp;
	curwp->w_doto = getgoal(dlp);

	return (TRUE);
}

/*
 * This function is like "forwline", but
 * goes backwards. The scheme is exactly the same.
 * Check for arguments that are less than zero and
 * call your alternate. Figure out the new line and
 * call "movedot" to perform the motion.
 */
int
backline(int f, int n)
{
	struct line   *dlp;

	if (n < 0)
		return (forwline(f | FFRAND, -n));
	if ((lastflag & CFCPCN) == 0)	/* Fix goal. */
		setgoal();
	thisflag |= CFCPCN;
	dlp = curwp->w_dotp;
	if (lback(dlp) == curbp->b_headp)  {
		if (!(f & FFRAND))
			(void)dobeep_msg("Beginning of buffer");
		return(TRUE);
	}
	while (n-- && lback(dlp) != curbp->b_headp) {
		dlp = lback(dlp);
		curwp->w_dotline--;
	}
	if (n > 0 && !(f & FFRAND))
		(void)dobeep_msg("Beginning of buffer");
	curwp->w_dotp = dlp;
	curwp->w_doto = getgoal(dlp);
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}

/*
 * Set the current goal column, which is saved in the external variable
 * "curgoal", to the current cursor column. The column is never off
 * the edge of the screen; it's more like display then show position.
 */
void
setgoal(void)
{
	curgoal = getcolpos(curwp);	/* Get the position. */
	/* we can now display past end of display, don't chop! */
}

/*
 * This routine looks at a line (pointed
 * to by the LINE pointer "dlp") and the current
 * vertical motion goal column (set by the "setgoal"
 * routine above) and returns the best offset to use
 * when a vertical motion is made into the line.
 */
int
getgoal(struct line *dlp)
{
	int c, i, col = 0;
	char tmp[5];

	for (i = 0; i < llength(dlp); i++) {
		c = lgetc(dlp, i);
		if (c == '\t') {
			col = ntabstop(col, curbp->b_tabw);
		} else if (ISCTRL(c) != FALSE) {
			col += 2;
		} else if (isprint(c))
			col++;
		else {
			col += snprintf(tmp, sizeof(tmp), "\\%o", c);
		}
		if (col > curgoal)
			break;
	}
	return (i);
}

/*
 * Scroll forward by a specified number
 * of lines, or by a full page if no argument.
 * The "2" is the window overlap (this is the default
 * value from ITS EMACS). Because the top line in
 * the window is zapped, we have to do a hard
 * update and get it back.
 */
int
forwpage(int f, int n)
{
	struct line  *lp;

	if (!(f & FFARG)) {
		n = curwp->w_ntrows - 2;	/* Default scroll.	 */
		if (n <= 0)			/* Forget the overlap	 */
			n = 1;			/* if tiny window.	 */
	} else if (n < 0)
		return (backpage(f | FFRAND, -n));

	lp = curwp->w_linep;
	while (n--)
		if ((lp = lforw(lp)) == curbp->b_headp) {
			(void)dobeep_msg("End of buffer");
			return(TRUE);
		}
	curwp->w_linep = lp;
	curwp->w_rflag |= WFFULL;

	/* if in current window, don't move dot */
	for (n = curwp->w_ntrows; n-- && lp != curbp->b_headp; lp = lforw(lp))
		if (lp == curwp->w_dotp)
			return (TRUE);

	/* Advance the dot the slow way, for line nos */
	while (curwp->w_dotp != curwp->w_linep) {
		curwp->w_dotp = lforw(curwp->w_dotp);
		curwp->w_dotline++;
	}
	curwp->w_doto = 0;
	return (TRUE);
}

/*
 * This command is like "forwpage",
 * but it goes backwards. The "2", like above,
 * is the overlap between the two windows. The
 * value is from the ITS EMACS manual. The
 * hard update is done because the top line in
 * the window is zapped.
 */
int
backpage(int f, int n)
{
	struct line  *lp, *lp2;

	if (!(f & FFARG)) {
		n = curwp->w_ntrows - 2;	/* Default scroll.	 */
		if (n <= 0)			/* Don't blow up if the  */
			return (backline(f, 1));/* window is tiny.	 */
	} else if (n < 0)
		return (forwpage(f | FFRAND, -n));

	lp = lp2 = curwp->w_linep;

	while (n-- && lback(lp) != curbp->b_headp) {
		lp = lback(lp);
	}
	if (lp == curwp->w_linep)
		(void)dobeep_msg("Beginning of buffer");

	curwp->w_linep = lp;
	curwp->w_rflag |= WFFULL;

	/* if in current window, don't move dot */
	for (n = curwp->w_ntrows; n-- && lp != curbp->b_headp; lp = lforw(lp))
		if (lp == curwp->w_dotp)
			return (TRUE);

        lp2 = lforw(lp2);

	/* Move the dot the slow way, for line nos */
	while (curwp->w_dotp != lp2) {
                if (curwp->w_dotline <= curwp->w_ntrows)
			goto out;
		curwp->w_dotp = lback(curwp->w_dotp);
		curwp->w_dotline--;
	}
out:
	curwp->w_doto = 0;
	return (TRUE);
}

/*
 * These functions are provided for compatibility with Gosling's Emacs. They
 * are used to scroll the display up (or down) one line at a time.
 */
int
forw1page(int f, int n)
{
	if (!(f & FFARG)) {
		n = 1;
		f = FFUNIV;
	}
	forwpage(f | FFRAND, n);
	return (TRUE);
}

int
back1page(int f, int n)
{
	if (!(f & FFARG)) {
		n = 1;
		f = FFUNIV;
	}
	backpage(f | FFRAND, n);
	return (TRUE);
}

/*
 * Page the other window. Check to make sure it exists, then
 * nextwind, forwpage and restore window pointers.
 */
int
pagenext(int f, int n)
{
	struct mgwin *wp;

	if (wheadp->w_wndp == NULL)
		return(dobeep_msg("No other window"));

	wp = curwp;
	(void) nextwind(f, n);
	(void) forwpage(f, n);
	curwp = wp;
	curbp = wp->w_bufp;
	return (TRUE);
}

/*
 * Internal set mark routine, used by other functions (daveb).
 */
void
isetmark(void)
{
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
	curwp->w_markline = curwp->w_dotline;
}

/*
 * Set the mark in the current window
 * to the value of dot. A message is written to
 * the echo line.  (ewprintf knows about macros)
 */
int
setmark(int f, int n)
{
	isetmark();
	ewprintf("Mark set");
	return (TRUE);
}

/* Clear the mark, if set. */
int
clearmark(int f, int n)
{
	if (!curwp->w_markp)
		return (FALSE);

	curwp->w_markp = NULL;
	curwp->w_marko = 0;
	curwp->w_markline = 0;

	return (TRUE);
}

/*
 * Swap the values of "dot" and "mark" in
 * the current window. This is pretty easy, because
 * all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible
 * error is "no mark".
 */
int
swapmark(int f, int n)
{
	struct line  *odotp;
	int odoto, odotline;

	if (curwp->w_markp == NULL)
		return(dobeep_msg("No mark in this window"));

	odotp = curwp->w_dotp;
	odoto = curwp->w_doto;
	odotline = curwp->w_dotline;
	curwp->w_dotp = curwp->w_markp;
	curwp->w_doto = curwp->w_marko;
	curwp->w_dotline = curwp->w_markline;
	curwp->w_markp = odotp;
	curwp->w_marko = odoto;
	curwp->w_markline = odotline;
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}

/*
 * Go to a specific line, mostly for
 * looking up errors in C programs, which give the
 * error a line number. If an argument is present, then
 * it is the line number, else prompt for a line number
 * to use.
 */
int
gotoline(int f, int n)
{
	char   buf[32], *bufp;
	const char *err;

	if (!(f & FFARG)) {
		if ((bufp = eread("Goto line: ", buf, sizeof(buf),
		    EFNUL | EFNEW | EFCR)) == NULL)
			return (ABORT);
		if (bufp[0] == '\0')
			return (ABORT);
		n = (int)strtonum(buf, INT_MIN, INT_MAX, &err);
		if (err)
			return(dobeep_msgs("Line number", err));
	}
	return(setlineno(n));
}

/*
 * Set the line number and switch to it.
 */
int
setlineno(int n)
{
	struct line  *clp;

	if (n >= 0) {
		if (n == 0)
			n++;
		curwp->w_dotline = n;
		clp = lforw(curbp->b_headp);	/* "clp" is first line */
		while (--n > 0) {
			if (lforw(clp) == curbp->b_headp) {
				curwp->w_dotline = curwp->w_bufp->b_lines;
				break;
			}
			clp = lforw(clp);
		}
	} else {
		curwp->w_dotline = curwp->w_bufp->b_lines + n;
		clp = lback(curbp->b_headp);	/* "clp" is last line */
		while (n < 0) {
			if (lback(clp) == curbp->b_headp) {
				curwp->w_dotline = 1;
				break;
			}
			clp = lback(clp);
			n++;
		}
	}
	curwp->w_dotp = clp;
	curwp->w_doto = 0;
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}
