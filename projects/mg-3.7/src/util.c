/*	$OpenBSD: util.c,v 1.49 2023/04/21 14:14:13 op Exp $	*/

/* This file is in the public domain. */

/*
 *		Assorted commands.
 * This file contains the command processors for a large assortment of
 * unrelated commands.  The only thing they have in common is that they
 * are all command processors.
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>

#include "def.h"

/*
 * Compute next tab stop, with `col' being the a column number and
 * `tabw' the tab width.
 */
int
ntabstop(int col, int tabw)
{
	return (((col + tabw) / tabw) * tabw);
}

/*
 * Display a bunch of useful information about the current location of dot.
 * The character under the cursor (in octal), the current line, row, and
 * column, and approximate position of the cursor in the file (as a
 * percentage) is displayed.
 * Also included at the moment are some values in parenthesis for debugging
 * explicit newline inclusion into the buffer.
 * The column position assumes an infinite
 * position display; it does not truncate just because the screen does.
 * This is normally bound to "C-x =".
 */
int
showcpos(int f, int n)
{
	struct line	*clp;
	char		*msg;
	long	 nchar, cchar;
	int	 nline, row;
	int	 cline, cbyte;		/* Current line/char/byte */
	int	 ratio;

	/* collect the data */
	clp = bfirstlp(curbp);
	msg = "Char:";
	cchar = 0;
	cline = 0;
	cbyte = 0;
	nchar = 0;
	nline = 0;
	for (;;) {
		/* count lines and display total as (raw) 'lines' and
		   compare with b_lines */
		++nline;
		if (clp == curwp->w_dotp) {
			/* obtain (raw) dot line # and compare with w_dotline */
			cline = nline;
			cchar = nchar + curwp->w_doto;
			if (curwp->w_doto == llength(clp))
				/* fake a \n at end of line */
				cbyte = *curbp->b_nlchr;
			else
				cbyte = lgetc(clp, curwp->w_doto);
		}
		/* include # of chars in this line for point-thru-buff ratio */
		nchar += llength(clp);
		clp = lforw(clp);
		if (clp == curbp->b_headp) {
			if (cbyte == *curbp->b_nlchr &&
			    cline == curbp->b_lines) {
				/* swap faked \n for EOB msg */
				cbyte = EOF;
				msg = "(EOB)";
			}
			break;
		}
		/* count the implied newline */
		nchar++;
	}
	/* determine row # within current window */
	row = curwp->w_toprow + 1;
	clp = curwp->w_linep;
	while (clp != curbp->b_headp && clp != curwp->w_dotp) {
		++row;
		clp = lforw(clp);
	}
	ratio = nchar ? (100L * cchar) / nchar : 100;
	ewprintf("%s %c (0%o)  point=%ld(%d%%)  line=%d  row=%d  col=%d" \
            "  (blines=%d rlines=%d l_size=%d)", msg,
	    cbyte, cbyte, cchar, ratio, cline, row, getcolpos(curwp),
	    curbp->b_lines, nline, clp->l_size);
	return (TRUE);
}

int
getcolpos(struct mgwin *wp)
{
	int	col, i, c;
	char tmp[5];

	/* determine column */
	col = 0;

	for (i = 0; i < wp->w_doto; ++i) {
		c = lgetc(wp->w_dotp, i);
		if (c == '\t') {
			col = ntabstop(col, wp->w_bufp->b_tabw);
		} else if (ISCTRL(c) != FALSE)
			col += 2;
		else if (isprint(c)) {
			col++;
		} else {
			col += snprintf(tmp, sizeof(tmp), "\\%o", c);
		}

	}
	return (col);
}

/*
 * Twiddle the two characters in front of and under dot, then move forward
 * one character.  Treat new-line characters the same as any other.
 * Normally bound to "C-t".  This always works within a line, so "WFEDIT"
 * is good enough.
 */
int
twiddle(int f, int n)
{
	struct line	*dotp;
	int	 doto, cr;

	if (n == 0)
		return (TRUE);

	dotp = curwp->w_dotp;
	doto = curwp->w_doto;

	/* Don't twiddle if the dot is on the first char of buffer */
	if (doto == 0 && lback(dotp) == curbp->b_headp) {
		dobeep();
		ewprintf("Beginning of buffer");
		return(FALSE);
	}
	/* Don't twiddle if the dot is on the last char of buffer */
	if (doto == llength(dotp) && lforw(dotp) == curbp->b_headp) {
		dobeep();
		return(FALSE);
	}
	undo_boundary_enable(FFRAND, 0);
	if (doto == 0 && doto == llength(dotp)) { /* only '\n' on this line */
		(void)forwline(FFRAND, 1);
		curwp->w_doto = 0;
	} else {
		if (doto == 0) { /* 1st twiddle is on 1st character of a line */
			cr = lgetc(dotp, doto);
			(void)backdel(FFRAND, 1);
			(void)forwchar(FFRAND, 1);
			lnewline();
			linsert(1, cr);
			(void)backdel(FFRAND, 1);
		} else {	/* twiddle is elsewhere in line */
			cr = lgetc(dotp, doto - 1);
			(void)backdel(FFRAND, 1);
			(void)forwchar(FFRAND, 1);
			linsert(1, cr);
		}
	}
	undo_boundary_enable(FFRAND, 1);
	lchange(WFEDIT);
	return (TRUE);
}

/*
 * Open up some blank space.  The basic plan is to insert a bunch of
 * newlines, and then back up over them.  Everything is done by the
 * subcommand processors.  They even handle the looping.  Normally this
 * is bound to "C-o".
 */
int
openline(int f, int n)
{
	int	i, s;

	if (n < 0)
		return (FALSE);
	if (n == 0)
		return (TRUE);

	/* insert newlines */
	undo_boundary_enable(FFRAND, 0);
	i = n;
	do {
		s = lnewline();
	} while (s == TRUE && --i);

	/* then go back up overtop of them all */
	if (s == TRUE)
		s = backchar(f | FFRAND, n);
	undo_boundary_enable(FFRAND, 1);
	return (s);
}

/*
 * Insert a newline.
 */
int
enewline(int f, int n)
{
	int	 s;

	if (n < 0)
		return (FALSE);

	while (n--) {
		if ((s = lnewline()) != TRUE)
			return (s);
	}
	return (TRUE);
}

/*
 * Delete blank lines around dot. What this command does depends if dot is
 * sitting on a blank line. If dot is sitting on a blank line, this command
 * deletes all the blank lines above and below the current line. If it is
 * sitting on a non blank line then it deletes all of the blank lines after
 * the line. Normally this command is bound to "C-x C-o". Any argument is
 * ignored.
 */
int
deblank(int f, int n)
{
	struct line	*lp1, *lp2;
	RSIZE	 nld;

	lp1 = curwp->w_dotp;
	while (llength(lp1) == 0 && (lp2 = lback(lp1)) != curbp->b_headp)
		lp1 = lp2;
	lp2 = lp1;
	nld = (RSIZE)0;
	while ((lp2 = lforw(lp2)) != curbp->b_headp && llength(lp2) == 0)
		++nld;
	if (nld == 0)
		return (TRUE);
	curwp->w_dotp = lforw(lp1);
	curwp->w_doto = 0;
	return (ldelete((RSIZE)nld, KNONE));
}

/*
 * Delete any whitespace around dot, then insert a space.
 */
int
justone(int f, int n)
{
	undo_boundary_enable(FFRAND, 0);
	(void)delwhite(f, n);
	linsert(1, ' ');
	undo_boundary_enable(FFRAND, 1);
	return (TRUE);
}

/*
 * Delete any whitespace around dot.
 */
int
delwhite(int f, int n)
{
	int	col, s;

	col = curwp->w_doto;

	while (col < llength(curwp->w_dotp) &&
	    (isspace(lgetc(curwp->w_dotp, col))))
		++col;
	do {
		if (curwp->w_doto == 0) {
			s = FALSE;
			break;
		}
		if ((s = backchar(FFRAND, 1)) != TRUE)
			break;
	} while (isspace(lgetc(curwp->w_dotp, curwp->w_doto)));

	if (s == TRUE)
		(void)forwchar(FFRAND, 1);
	(void)ldelete((RSIZE)(col - curwp->w_doto), KNONE);
	return (TRUE);
}

/*
 * Delete any leading whitespace on the current line
 */
int
delleadwhite(int f, int n)
{
	int soff, ls;
	struct line *slp;

	/* Save current position */
	slp = curwp->w_dotp;
	soff = curwp->w_doto;

	for (ls = 0; ls < llength(slp); ls++)
                 if (!isspace(lgetc(slp, ls)))
                        break;
	gotobol(FFRAND, 1);
	forwdel(FFRAND, ls);
	soff -= ls;
	if (soff < 0)
		soff = 0;
	(void)forwchar(FFRAND, soff);

	return (TRUE);
}

/*
 * Delete any trailing whitespace on the current line
 */
int
deltrailwhite(int f, int n)
{
	int soff;

	/* Save current position */
	soff = curwp->w_doto;

	gotoeol(FFRAND, 1);
	delwhite(FFRAND, 1);

	/* restore original position, if possible */
	if (soff < curwp->w_doto)
		curwp->w_doto = soff;

	return (TRUE);
}

/*
 * Raw indent routine.  Use spaces and tabs to fill the given number of
 * cols, but respect no-tab-mode.
 */
int
doindent(int cols)
{
	int n;

	if (curbp->b_flag & BFNOTAB)
		return (linsert(cols, ' '));
	if ((n = cols / 8) != 0 && linsert(n, '\t') == FALSE)
		return (FALSE);
	if ((n = cols % 8) != 0 && linsert(n, ' ') == FALSE)
		return (FALSE);
	return (TRUE);
}

/*
 * Insert a newline, then enough tabs and spaces to duplicate the indentation
 * of the previous line, respecting no-tab-mode and the buffer tab width.
 * Figure out the indentation of the current line.  Insert a newline by
 * calling the standard routine.  Insert the indentation by inserting the
 * right number of tabs and spaces.  Return TRUE if all ok.  Return FALSE if
 * one of the subcommands failed. Normally bound to "C-m".
 */
int
lfindent(int f, int n)
{
	int	c, i, nicol;
	int	s = TRUE;

	if (n < 0)
		return (FALSE);

	undo_boundary_enable(FFRAND, 0);
	while (n--) {
		nicol = 0;
		for (i = 0; i < llength(curwp->w_dotp); ++i) {
			c = lgetc(curwp->w_dotp, i);
			if (c != ' ' && c != '\t')
				break;
			if (c == '\t')
				nicol = ntabstop(nicol, curwp->w_bufp->b_tabw);
			else
				++nicol;
		}
		delwhite(FFRAND, 1);
		if (lnewline() == FALSE || doindent(nicol) == FALSE) {
			s = FALSE;
			break;
		}
	}
	undo_boundary_enable(FFRAND, 1);
	return (s);
}

/*
 * Indent the current line. Delete existing leading whitespace,
 * and use tabs/spaces to achieve correct indentation. Try
 * to leave dot where it started.
 */
int
indent(int f, int n)
{
	int soff;

	if (n < 0)
		return (FALSE);

	delleadwhite(FFRAND, 1);

	/* If not invoked with a numerical argument, done */
	if (!(f & FFARG))
		return (TRUE);

	/* insert appropriate whitespace */
	soff = curwp->w_doto;
	(void)gotobol(FFRAND, 1);
	if ( doindent(n) == FALSE)
		return (FALSE);

	(void)forwchar(FFRAND, soff);

	return (TRUE);
}


/*
 * Delete forward.  This is real easy, because the basic delete routine does
 * all of the work.  Watches for negative arguments, and does the right thing.
 * If any argument is present, it kills rather than deletes, to prevent loss
 * of text if typed with a big argument.  Normally bound to "C-d".
 */
int
forwdel(int f, int n)
{
	if (n < 0)
		return (backdel(f | FFRAND, -n));

	/* really a kill */
	if (f & FFARG) {
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}

	return (ldelete((RSIZE) n, (f & FFARG) ? KFORW : KNONE));
}

/*
 * Delete backwards.  This is quite easy too, because it's all done with
 * other functions.  Just move the cursor back, and delete forwards.  Like
 * delete forward, this actually does a kill if presented with an argument.
 */
int
backdel(int f, int n)
{
	int	s;

	if (n < 0)
		return (forwdel(f | FFRAND, -n));

	/* really a kill */
	if (f & FFARG) {
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}
	if ((s = backchar(f | FFRAND, n)) == TRUE)
		s = ldelete((RSIZE)n, (f & FFARG) ? KFORW : KNONE);

	return (s);
}

int
space_to_tabstop(int f, int n)
{
	int	col, target;

	(void)f;

	if (n < 0)
		return (FALSE);
	if (n == 0)
		return (TRUE);

	col = target = getcolpos(curwp);
	while (n-- > 0)
		target = ntabstop(target, curbp->b_tabw);
	return (linsert(target - col, ' '));
}

/*
 * Move the dot to the first non-whitespace character of the current line.
 */
int
backtoindent(int f, int n)
{
	gotobol(FFRAND, 1);
	while (curwp->w_doto < llength(curwp->w_dotp) &&
	    (isspace(lgetc(curwp->w_dotp, curwp->w_doto))))
		++curwp->w_doto;
	return (TRUE);
}

/*
 * Join the current line to the previous, or with arg, the next line
 * to the current one.  If the former line is not empty, leave exactly
 * one space at the joint.  Otherwise, leave no whitespace.
 */
int
joinline(int f, int n)
{
	int doto;

	(void)n;
	undo_boundary_enable(FFRAND, 0);
	if (f & FFARG) {
		gotoeol(FFRAND, 1);
		forwdel(FFRAND, 1);
	} else {
		gotobol(FFRAND, 1);
		backdel(FFRAND, 1);
	}

	delwhite(FFRAND, 1);

	if ((doto = curwp->w_doto) > 0) {
		linsert(1, ' ');
		curwp->w_doto = doto;
	}
	undo_boundary_enable(FFRAND, 1);

	return (TRUE);
}
