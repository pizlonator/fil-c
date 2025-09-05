/*	$OpenBSD: match.c,v 1.25 2023/04/21 13:39:37 op Exp $	*/

/* This file is in the public domain. */

/*
 *	Limited parenthesis matching routines
 *
 * The hacks in this file implement automatic matching * of (), [], {}, and
 * other characters.  It would be better to have a full-blown syntax table,
 * but there's enough overhead in the editor as it is.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "key.h"

static int	balance(void);
static void	displaymatch(struct line *, int);

/*
 * Balance table. When balance() encounters a character that is to be
 * matched, it first searches this table for a balancing left-side character.
 * If the character is not in the table, the character is balanced by itself.
 */
static struct balance {
	char	left, right;
} bal[] = {
	{ '(', ')' },
	{ '[', ']' },
	{ '{', '}' },
	{ '<', '>' },
	{ '\0', '\0' }
};

/*
 * Hack to show matching paren.  Self-insert character, then show matching
 * character, if any.  Bound to "blink-and-insert".  Used in the mg startup
 * file to amend the default cursor behaviour of a key press, e.g:
 *   global-set-key "%" blink-and-insert
 */
int
showmatch(int f, int n)
{
	int	i, s;

	for (i = 0; i < n; i++) {
		if ((s = selfinsert(FFRAND, 1)) != TRUE)
			return (s);
		/* unbalanced -- warn user */
		if (balance() != TRUE)
			dobeep();
	}
	return (TRUE);
}

/*
 * Search for and display a matching character.
 *
 * This routine does the real work of searching backward
 * for a balancing character.  If such a balancing character
 * is found, it uses displaymatch() to display the match.
 */
static int
balance(void)
{
	struct line	*clp;
	int	 cbo;
	int	 c, i, depth;
	int	 rbal, lbal;

	rbal = key.k_chars[key.k_count - 1];

	/* See if there is a matching character -- default to the same */
	lbal = rbal;
	for (i = 0; bal[i].right != '\0'; i++)
		if (bal[i].right == rbal) {
			lbal = bal[i].left;
			break;
		}

	/*
	 * Move behind the inserted character.	We are always guaranteed
	 * that there is at least one character on the line.
	 */
	clp = curwp->w_dotp;
	cbo = curwp->w_doto - 1;

	/* init nesting depth */
	depth = 0;

	for (;;) {
		if (cbo == 0) {
			clp = lback(clp);	/* beginning of line	*/
			if (clp == curbp->b_headp)
				return (FALSE);
			cbo = llength(clp) + 1;
		}
		if (--cbo == llength(clp))
			c = *curbp->b_nlchr;	/* end of line		*/
		else
			c = lgetc(clp, cbo);	/* somewhere in middle	*/

		/*
		 * Check for a matching character.  If still in a nested
		 * level, pop out of it and continue search.  This check
		 * is done before the nesting check so single-character
		 * matches will work too.
		 */
		if (c == lbal) {
			if (depth == 0) {
				displaymatch(clp, cbo);
				return (TRUE);
			} else
				depth--;
		}
		/* Check for another level of nesting.	 */
		if (c == rbal)
			depth++;
	}
	/* NOTREACHED */
}

/*
 * Display matching character.  Matching characters that are not in the
 * current window are displayed in the echo line. If in the current window,
 * move dot to the matching character, sit there a while, then move back.
 */
static void
displaymatch(struct line *clp, int cbo)
{
	struct line	*tlp;
	int	 tbo;
	int	 cp;
	int	 buflen;
	int	 bufo;
	int	 c;
	int	 col;
	int	 inwindow;
	char	 buf[NLINE];

	/*
	 * Figure out if matching char is in current window by
	 * searching from the top of the window to dot.
	 */
	inwindow = FALSE;
	for (tlp = curwp->w_linep; tlp != lforw(curwp->w_dotp);
	     tlp = lforw(tlp)) {
		if (tlp == clp)
			inwindow = TRUE;
	}

	if (inwindow == TRUE) {
		tlp = curwp->w_dotp;	/* save current position */
		tbo = curwp->w_doto;

		curwp->w_dotp = clp;	/* move to new position */
		curwp->w_doto = cbo;
		curwp->w_rflag |= WFMOVE;

		update(CMODE);		/* show match */
		ttwait(1000);		/* wait for key or 1 second */

		curwp->w_dotp = tlp;	/* return to old position */
		curwp->w_doto = tbo;
		curwp->w_rflag |= WFMOVE;
		update(CMODE);
	} else {
		/* match is not in this window, so display line in echo area */
		buflen = (int)sizeof(buf) - 1;
		bufo = 0;
		for (cp = 0; cp < llength(clp); cp++) {
			if (bufo >= buflen)
				break;

			c = lgetc(clp, cp);
			if (c != '\t') {
				if (ISCTRL(c)) {
					buf[bufo++] = '^';
					buf[bufo++] = CCHR(c);
				} else
					buf[bufo++] = c;
			} else {
				col = ntabstop(bufo, curbp->b_tabw);
				while (bufo < col && bufo < buflen)
					buf[bufo++] = ' ';
			}
		}
		buf[bufo++] = 0;
		ewprintf("Matches %s", buf);
	}
}
