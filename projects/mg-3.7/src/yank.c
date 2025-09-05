/*	$OpenBSD: yank.c,v 1.15 2021/03/01 10:51:14 lum Exp $	*/

/* This file is in the public domain. */

/*
 *	kill ring functions
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"

#define KBLOCK	 8192		/* Kill grow.                    */

static char	*kbufp = NULL;	/* Kill buffer data.		 */
static RSIZE	 kused = 0;	/* # of bytes used in KB.	 */
static RSIZE	 ksize = 0;	/* # of bytes allocated in KB.	 */
static RSIZE	 kstart = 0;	/* # of first used byte in KB.	 */

static int	 kgrow(int);

/*
 * Delete all of the text saved in the kill buffer.  Called by commands when
 * a new kill context is created. The kill buffer array is released, just in
 * case the buffer has grown to an immense size.  No errors.
 */
void
kdelete(void)
{
	if (kbufp != NULL) {
		free(kbufp);
		kbufp = NULL;
		kstart = kused = ksize = 0;
	}
}

/*
 * Insert a character to the kill buffer, enlarging the buffer if there
 * isn't any room. Always grow the buffer in chunks, on the assumption
 * that if you put something in the kill buffer you are going to put more
 * stuff there too later. Return TRUE if all is well, and FALSE on errors.
 * Print a message on errors.  Dir says whether to put it at back or front.
 * This call is ignored if  KNONE is set.
 */
int
kinsert(int c, int dir)
{
	if (dir == KNONE)
		return (TRUE);
	if (kused == ksize && dir == KFORW && kgrow(dir) == FALSE)
		return (FALSE);
	if (kstart == 0 && dir == KBACK && kgrow(dir) == FALSE)
		return (FALSE);
	if (dir == KFORW)
		kbufp[kused++] = c;
	else if (dir == KBACK)
		kbufp[--kstart] = c;
	else
		panic("broken kinsert call");	/* Oh shit! */
	return (TRUE);
}

/*
 * kgrow - just get more kill buffer for the callee. If dir = KBACK
 * we are trying to get space at the beginning of the kill buffer.
 */
static int
kgrow(int dir)
{
	int	 nstart;
	char	*nbufp;

	if ((unsigned)(ksize + KBLOCK) <= (unsigned)ksize) {
		/* probably 16 bit unsigned */
		dobeep();
		ewprintf("Kill buffer size at maximum");
		return (FALSE);
	}
	if ((nbufp = malloc((unsigned)(ksize + KBLOCK))) == NULL) {
		dobeep();
		ewprintf("Can't get %ld bytes", (long)(ksize + KBLOCK));
		return (FALSE);
	}
	nstart = (dir == KBACK) ? (kstart + KBLOCK) : (KBLOCK / 4);
	bcopy(&(kbufp[kstart]), &(nbufp[nstart]), (int)(kused - kstart));
	free(kbufp);
	kbufp = nbufp;
	ksize += KBLOCK;
	kused = kused - kstart + nstart;
	kstart = nstart;
	return (TRUE);
}

/*
 * This function gets characters from the kill buffer. If the character
 * index "n" is off the end, it returns "-1". This lets the caller just
 * scan along until it gets a "-1" back.
 */
int
kremove(int n)
{
	if (n < 0 || n + kstart >= kused)
		return (-1);
	return (CHARMASK(kbufp[n + kstart]));
}

/*
 * Copy a string into the kill buffer. kflag gives direction.
 * if KNONE, do nothing.
 */
int
kchunk(char *cp1, RSIZE chunk, int kflag)
{
	/*
	 * HACK - doesn't matter, and fixes back-over-nl bug for empty
	 *	kill buffers.
	 */
	if (kused == kstart)
		kflag = KFORW;

	if (kflag & KFORW) {
		while (ksize - kused < chunk)
			if (kgrow(kflag) == FALSE)
				return (FALSE);
		bcopy(cp1, &(kbufp[kused]), (int)chunk);
		kused += chunk;
	} else if (kflag & KBACK) {
		while (kstart < chunk)
			if (kgrow(kflag) == FALSE)
				return (FALSE);
		bcopy(cp1, &(kbufp[kstart - chunk]), (int)chunk);
		kstart -= chunk;
	}

	return (TRUE);
}

/*
 * Kill line.  If called without an argument, it kills from dot to the end
 * of the line, unless it is at the end of the line, when it kills the
 * newline.  If called with an argument of 0, it kills from the start of the
 * line to dot.  If called with a positive argument, it kills from dot
 * forward over that number of newlines.  If called with a negative argument
 * it kills any text before dot on the current line, then it kills back
 * abs(arg) lines.
 */
int
killline(int f, int n)
{
	struct line	*nextp;
	RSIZE	 chunk;
	int	 i, c;

	/* clear kill buffer if last wasn't a kill */
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	if (!(f & FFARG)) {
		for (i = curwp->w_doto; i < llength(curwp->w_dotp); ++i)
			if ((c = lgetc(curwp->w_dotp, i)) != ' ' && c != '\t')
				break;
		if (i == llength(curwp->w_dotp))
			chunk = llength(curwp->w_dotp) - curwp->w_doto + 1;
		else {
			chunk = llength(curwp->w_dotp) - curwp->w_doto;
			if (chunk == 0)
				chunk = 1;
		}
	} else if (n > 0) {
		chunk = llength(curwp->w_dotp) - curwp->w_doto;
		nextp = lforw(curwp->w_dotp);
		if (nextp != curbp->b_headp)
			chunk++;		/* newline */
		if (nextp == curbp->b_headp)
			goto done;		/* EOL */
		i = n;
		while (--i) {
			chunk += llength(nextp);
			nextp = lforw(nextp);
			if (nextp != curbp->b_headp)
				chunk++;	/* newline */
			if (nextp == curbp->b_headp)
				break;		/* EOL */
		}
	} else {
		/* n <= 0 */
		chunk = curwp->w_doto;
		curwp->w_doto = 0;
		i = n;
		while (i++) {
			if (lforw(curwp->w_dotp))
				chunk++;
			curwp->w_dotp = lback(curwp->w_dotp);
			curwp->w_rflag |= WFMOVE;
			chunk += llength(curwp->w_dotp);
		}
	}
	/*
	 * KFORW here is a bug.  Should be KBACK/KFORW, but we need to
	 * rewrite the ldelete code (later)?
	 */
done:
	if (chunk)
		return (ldelete(chunk, KFORW));
	return (TRUE);
}

/*
 * Yank text back from the kill buffer.  This is really easy.  All of the work
 * is done by the standard insert routines.  All you do is run the loop, and
 * check for errors.  The blank lines are inserted with a call to "newline"
 * instead of a call to "lnewline" so that the magic stuff that happens when
 * you type a carriage return also happens when a carriage return is yanked
 * back from the kill buffer.  An attempt has been made to fix the cosmetic
 * bug associated with a yank when dot is on the top line of the window
 * (nothing moves, because all of the new text landed off screen).
 */
int
yank(int f, int n)
{
	struct line	*lp;
	int	 c, i, nline;

	if (n < 0)
		return (FALSE);

	/* newline counting */
	nline = 0;

	undo_boundary_enable(FFRAND, 0);
	while (n--) {
		/* mark around last yank */
		isetmark();
		i = 0;
		while ((c = kremove(i)) >= 0) {
			if (c == *curbp->b_nlchr) {
				if (enewline(FFRAND, 1) == FALSE)
					return (FALSE);
				++nline;
			} else {
				if (linsert(1, c) == FALSE)
					return (FALSE);
			}
			++i;
		}
	}
	/* cosmetic adjustment */
	lp = curwp->w_linep;

	/* if offscreen insert */
	if (curwp->w_dotp == lp) {
		while (nline-- && lback(lp) != curbp->b_headp)
			lp = lback(lp);
		/* adjust framing */
		curwp->w_linep = lp;
		curwp->w_rflag |= WFFULL;
	}
	undo_boundary_enable(FFRAND, 1);
	return (TRUE);
}

