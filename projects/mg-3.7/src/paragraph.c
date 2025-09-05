/*	$OpenBSD: paragraph.c,v 1.49 2023/04/21 13:39:37 op Exp $	*/

/* This file is in the public domain. */

/*
 * Code for dealing with paragraphs and filling. Adapted from MicroEMACS 3.6
 * and GNU-ified by mwm@ucbvax.	 Several bug fixes by blarson@usc-oberon.
 */

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "def.h"

static int	fillcol = 70;

#define MAXWORD 256

static int	findpara(void);
static int 	do_gotoeop(int, int, int *);

/*
 * Move to start of paragraph.
 * Move backwards by line, checking from the 1st character forwards for the
 * existence a non-space. If a non-space character is found, move to the 
 * preceding line. Keep doing this until a line with only spaces is found or
 * the start of buffer.
 */
int
gotobop(int f, int n)
{
	int col, nospace;

	/* the other way... */
	if (n < 0)
		return (gotoeop(f, -n));

	while (n-- > 0) {
		nospace = 0;
		while (lback(curwp->w_dotp) != curbp->b_headp) {
			curwp->w_doto = 0;
			col = 0;

			while (col < llength(curwp->w_dotp) &&
			    (isspace(lgetc(curwp->w_dotp, col))))
				col++;

			if (col >= llength(curwp->w_dotp)) {
				if (nospace)
					break;
			} else
				nospace = 1;

			curwp->w_dotline--;
			curwp->w_dotp = lback(curwp->w_dotp);
		}
	}
	/* force screen update */
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}

/*
 * Move to end of paragraph.
 * See comments for gotobop(). Same, but moving forwards.
 */
int
gotoeop(int f, int n)
{
	int i;

	return(do_gotoeop(f, n, &i));
}

int
do_gotoeop(int f, int n, int *i)
{
	int col, nospace, j = 0;

	/* the other way... */
	if (n < 0)
		return (gotobop(f, -n));

	/* for each one asked for */
	while (n-- > 0) {
		*i = ++j;
		nospace = 0;
		while (lforw(curwp->w_dotp) != curbp->b_headp) {
			col = 0;
			curwp->w_doto = 0;

			while (col < llength(curwp->w_dotp) &&
			    (isspace(lgetc(curwp->w_dotp, col))))
				col++;

			if (col >= llength(curwp->w_dotp)) {
				if (nospace)
					break;
			} else
				nospace = 1;

			curwp->w_dotp = lforw(curwp->w_dotp);
			curwp->w_dotline++;

			/* do not continue after end of buffer */
			if (lforw(curwp->w_dotp) == curbp->b_headp) {
				gotoeol(FFRAND, 1);
				curwp->w_rflag |= WFMOVE;
				return (FALSE);
			}
		}
	}

	/* force screen update */
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}

/*
 * Justify a paragraph.  Fill the current paragraph according to the current
 * fill column.
 */
int
fillpara(int f, int n)
{
	int	 c;		/* current char during scan		*/
	int	 wordlen;	/* length of current word		*/
	int	 clength;	/* position on line during fill		*/
	int	 i;		/* index during word copy		*/
	int	 eopflag;	/* Are we at the End-Of-Paragraph?	*/
	int	 firstflag;	/* first word? (needs no space)		*/
	int	 newlength;	/* tentative new line length		*/
	int	 eolflag;	/* was at end of line			*/
	int	 retval;	/* return value				*/
	struct line	*eopline;	/* pointer to line just past EOP	*/
	char	 wbuf[MAXWORD];	/* buffer for current word		*/

	if (n == 0)
		return (TRUE);

	undo_boundary_enable(FFRAND, 0);

	/* record the pointer to the line just past the EOP */
	(void)gotoeop(FFRAND, 1);
	if (curwp->w_doto != 0) {
		/* paragraph ends at end of buffer */
		(void)lnewline();
		eopline = lforw(curwp->w_dotp);
	} else
		eopline = curwp->w_dotp;

	/* and back top the beginning of the paragraph */
	(void)gotobop(FFRAND, 1);

	/* initialize various info */
	while (inword() == 0 && forwchar(FFRAND, 1));

	clength = curwp->w_doto;
	wordlen = 0;

	/* scan through lines, filling words */
	firstflag = TRUE;
	eopflag = FALSE;
	while (!eopflag) {

		/* get the next character in the paragraph */
		if ((eolflag = (curwp->w_doto == llength(curwp->w_dotp)))) {
			c = ' ';
			if (lforw(curwp->w_dotp) == eopline)
				eopflag = TRUE;
		} else
			c = lgetc(curwp->w_dotp, curwp->w_doto);

		/* and then delete it */
		if (ldelete((RSIZE) 1, KNONE) == FALSE && !eopflag) {
			retval = FALSE;
			goto cleanup;
		}

		/* if not a separator, just add it in */
		if (c != ' ' && c != '\t') {
			if (wordlen < MAXWORD - 1)
				wbuf[wordlen++] = c;
			else {
				/*
				 * You lose chars beyond MAXWORD if the word
				 * is too long. I'm too lazy to fix it now; it
				 * just silently truncated the word before,
				 * so I get to feel smug.
				 */
				ewprintf("Word too long!");
			}
		} else if (wordlen) {

			/* calculate tentative new length with word added */
			newlength = clength + 1 + wordlen;

			/*
			 * if at end of line or at doublespace and previous
			 * character was one of '.','?','!' doublespace here.
			 * behave the same way if a ')' is preceded by a
			 * [.?!] and followed by a doublespace.
			 */
			if (dblspace && (!eopflag && ((eolflag ||
			    curwp->w_doto == llength(curwp->w_dotp) ||
			    (c = lgetc(curwp->w_dotp, curwp->w_doto)) == ' '
			    || c == '\t') && (ISEOSP(wbuf[wordlen - 1]) ||
			    (wbuf[wordlen - 1] == ')' && wordlen >= 2 &&
			    ISEOSP(wbuf[wordlen - 2])))) &&
			    wordlen < MAXWORD - 1))
				wbuf[wordlen++] = ' ';

			/* at a word break with a word waiting */
			if (newlength <= fillcol) {
				/* add word to current line */
				if (!firstflag) {
					(void)linsert(1, ' ');
					++clength;
				}
				firstflag = FALSE;
			} else {
				if (curwp->w_doto > 0 &&
				    lgetc(curwp->w_dotp, curwp->w_doto - 1) == ' ') {
					curwp->w_doto -= 1;
					(void)ldelete((RSIZE) 1, KNONE);
				}
				/* start a new line */
				(void)lnewline();
				clength = 0;
			}

			/* and add the word in in either case */
			for (i = 0; i < wordlen; i++) {
				(void)linsert(1, wbuf[i]);
				++clength;
			}
			wordlen = 0;
		}
	}
	/* and add a last newline for the end of our new paragraph */
	(void)lnewline();

	/*
	 * We really should wind up where we started, (which is hard to keep
	 * track of) but I think the end of the last line is better than the
	 * beginning of the blank line.
	 */
	(void)backchar(FFRAND, 1);
	retval = TRUE;
cleanup:
	undo_boundary_enable(FFRAND, 1);
	return (retval);
}

/*
 * Delete n paragraphs. Move to the beginning of the current paragraph, or if
 * the cursor is on an empty line, move down the buffer to the first line with
 * non-space characters. Then mark n paragraphs and delete.
 */
int
killpara(int f, int n)
{
	int	lineno, status;

	if (n == 0)
		return (TRUE);

	if (findpara() == FALSE)
		return (TRUE);

	/* go to the beginning of the paragraph */
	(void)gotobop(FFRAND, 1);

	/* take a note of the line number for after deletions and set mark */
	lineno = curwp->w_dotline;
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;

	(void)gotoeop(FFRAND, n);

	if ((status = killregion(FFRAND, 1)) != TRUE)
		return (status);

	curwp->w_dotline = lineno;
	return (TRUE);
}

/*
 * Mark n paragraphs starting with the n'th and working our way backwards.
 * This leaves the cursor at the beginning of the paragraph where markpara()
 * was invoked.
 */
int
markpara(int f, int n)
{
	int i = 0;

	if (n == 0)
		return (TRUE);

	clearmark(FFARG, 0);

	if (findpara() == FALSE)
		return (TRUE);

	(void)do_gotoeop(FFRAND, n, &i);

	/* set the mark here */
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
	curwp->w_markline = curwp->w_dotline;

	(void)gotobop(FFRAND, i);

	return (TRUE);
}

/*
 * Transpose the current paragraph with the following paragraph. If invoked
 * multiple times, transpose to the n'th paragraph. If invoked between 
 * paragraphs, move to the previous paragraph, then continue.
 */
int
transposepara(int f, int n)
{
	int	i = 0, status;
	char	flg;

	if (n == 0)
		return (TRUE);

	undo_boundary_enable(FFRAND, 0);

	/* find a paragraph, set mark, then goto the end */
	gotobop(FFRAND, 1);
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
	(void)gotoeop(FFRAND, 1);

	/* take a note of buffer flags - we may need them */
	flg = curbp->b_flag;	

	/* clean out kill buffer then kill region */
	kdelete();
	if ((status = killregion(FFRAND, 1)) != TRUE)
		return (status);

	/* 
	 * Now step through n paragraphs. If we reach the end of buffer,
	 * stop and paste the killed region back, then display a message.
	 */
	if (do_gotoeop(FFRAND, n, &i) == FALSE) {
		ewprintf("Cannot transpose paragraph, end of buffer reached.");
		(void)gotobop(FFRAND, i);
		(void)yank(FFRAND, 1);
		curbp->b_flag = flg;	
		return (FALSE);
	}
	(void)yank(FFRAND, 1);

	undo_boundary_enable(FFRAND, 1);

	return (TRUE);
}

/*
 * Go down the buffer until we find a line with non-space characters.
 */
int
findpara(void)
{
	int	col, nospace = 0;

	/* we move forward to find a para to mark */
	do {
		curwp->w_doto = 0;
		col = 0;

		/* check if we are on a blank line */
		while (col < llength(curwp->w_dotp)) {
			if (!isspace(lgetc(curwp->w_dotp, col)))
				nospace = 1;
			col++;
		}
		if (nospace)
			break;

		if (lforw(curwp->w_dotp) == curbp->b_headp)
			return (FALSE);

		curwp->w_dotp = lforw(curwp->w_dotp);	
		curwp->w_dotline++;
	} while (1);

	return (TRUE);
}

/*
 * Insert char with work wrap.  Check to see if we're past fillcol, and if so,
 * justify this line.  As a last step, justify the line.
 */
int
fillword(int f, int n)
{
	char	c;
	int	col, i, nce;

	for (i = col = 0; col <= fillcol; ++i, ++col) {
		if (i == curwp->w_doto)
			return selfinsert(f, n);
		c = lgetc(curwp->w_dotp, i);
		if (c == '\t')
			col = ntabstop(col, curwp->w_bufp->b_tabw);
		else if (ISCTRL(c) != FALSE)
			++col;
	}
	if (curwp->w_doto != llength(curwp->w_dotp)) {
		(void)selfinsert(f, n);
		nce = llength(curwp->w_dotp) - curwp->w_doto;
	} else
		nce = 0;
	curwp->w_doto = i;

	if ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ' && c != '\t')
		do {
			(void)backchar(FFRAND, 1);
		} while ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ' &&
		    c != '\t' && curwp->w_doto > 0);

	if (curwp->w_doto == 0)
		do {
			(void)forwchar(FFRAND, 1);
		} while ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ' &&
		    c != '\t' && curwp->w_doto < llength(curwp->w_dotp));

	(void)delwhite(FFRAND, 1);
	(void)lnewline();
	i = llength(curwp->w_dotp) - nce;
	curwp->w_doto = i > 0 ? i : 0;
	curwp->w_rflag |= WFMOVE;
	if (nce == 0 && curwp->w_doto != 0)
		return (fillword(f, n));
	return (TRUE);
}

/*
 * Set fill column to n for justify.
 */
int
setfillcol(int f, int n)
{
	char buf[32], *rep;
	const char *es;
	int nfill;

	if ((f & FFARG) != 0) {
		fillcol = n;
	} else {
		if ((rep = eread("Set fill-column: ", buf, sizeof(buf),
		    EFNEW | EFCR)) == NULL)
			return (ABORT);
		else if (rep[0] == '\0')
			return (FALSE);
		nfill = strtonum(rep, 0, INT_MAX, &es);
		if (es != NULL) {
			dobeep();
			ewprintf("Invalid fill column: %s", rep);
			return (FALSE);
		}
		fillcol = nfill;
		ewprintf("Fill column set to %d", fillcol);
	}
	return (TRUE);
}

int
sentencespace(int f, int n)
{
	if (f & FFARG)
		dblspace = n > 1;
	else
		dblspace = !dblspace;

	return (TRUE);
}
