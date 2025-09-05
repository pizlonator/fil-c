/*	$OpenBSD: search.c,v 1.49 2022/12/26 19:16:02 jmc Exp $	*/

/* This file is in the public domain. */

/*
 *		Search commands.
 * The functions in this file implement the search commands (both plain and
 * incremental searches are supported) and the query-replace command.
 *
 * The plain old search code is part of the original MicroEMACS "distribution".
 * The incremental search code and the query-replace code is by Rich Ellison.
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "macro.h"

#define SRCH_BEGIN	(0)	/* Search sub-codes.	 */
#define SRCH_FORW	(-1)
#define SRCH_BACK	(-2)
#define SRCH_NOPR	(-3)
#define SRCH_ACCM	(-4)
#define SRCH_MARK	(-5)

struct srchcom {
	int		 s_code;
	struct line	*s_dotp;
	int		 s_doto;
	int		 s_dotline;
};

static int	isearch(int);
static void	is_cpush(int);
static void	is_lpush(void);
static void	is_pop(void);
static int	is_peek(void);
static void	is_undo(int *, int *);
static int	is_find(int);
static void	is_prompt(int, int, int);
static void	is_dspl(char *, int);
static int	eq(int, int, int);

static struct srchcom	cmds[NSRCH];
static int	cip;

int		srch_lastdir = SRCH_NOPR;	/* Last search flags.	 */

/*
 * Search forward.  Get a search string from the user, and search for it
 * starting at ".".  If found, "." gets moved to just after the matched
 * characters, and display does all the hard stuff.  If not found, it just
 * prints a message.
 */
int
forwsearch(int f, int n)
{
	int	s;

	if ((s = readpattern("Search")) != TRUE)
		return (s);
	if (forwsrch() == FALSE) {
		dobeep();
		ewprintf("Search failed: \"%s\"", pat);
		return (FALSE);
	}
	srch_lastdir = SRCH_FORW;
	return (TRUE);
}

/*
 * Reverse search.  Get a search string from the user, and search, starting
 * at "." and proceeding toward the front of the buffer.  If found "." is
 * left pointing at the first character of the pattern [the last character
 * that was matched].
 */
int
backsearch(int f, int n)
{
	int	s;

	if ((s = readpattern("Search backward")) != TRUE)
		return (s);
	if (backsrch() == FALSE) {
		dobeep();
		ewprintf("Search failed: \"%s\"", pat);
		return (FALSE);
	}
	srch_lastdir = SRCH_BACK;
	return (TRUE);
}

/*
 * Search again, using the same search string and direction as the last
 * search command. The direction has been saved in "srch_lastdir", so you
 * know which way to go.
 */
int
searchagain(int f, int n)
{
	if (srch_lastdir == SRCH_FORW) {
		if (forwsrch() == FALSE) {
			dobeep();
			ewprintf("Search failed: \"%s\"", pat);
			return (FALSE);
		}
		return (TRUE);
	}
	if (srch_lastdir == SRCH_BACK) {
		if (backsrch() == FALSE) {
			dobeep();
			ewprintf("Search failed: \"%s\"", pat);
			return (FALSE);
		}
		return (TRUE);
	}
	dobeep();
	ewprintf("No last search");
	return (FALSE);
}

/*
 * Use incremental searching, initially in the forward direction.
 * isearch ignores any explicit arguments.
 */
int
forwisearch(int f, int n)
{
	if (macrodef || inmacro)
		/* We can't isearch in macro. Use search instead */
		return (forwsearch(f,n));
	else 
		return (isearch(SRCH_FORW));
}

/*
 * Use incremental searching, initially in the reverse direction.
 * isearch ignores any explicit arguments.
 */
int
backisearch(int f, int n)
{
	if (macrodef || inmacro)
		/* We can't isearch in macro. Use search instead */
		return (backsearch(f,n));
	else 
		return (isearch(SRCH_BACK));
}

/*
 * Incremental Search.
 *	dir is used as the initial direction to search.
 *	^M	exit from Isearch, set mark
 *	^S	switch direction to forward
 *	^R	switch direction to reverse
 *	^Q	quote next character (allows searching for ^N etc.)
 *	<ESC>	exit from Isearch, set mark
 *	<DEL>	undoes last character typed. (tricky job to do this correctly).
 *	other ^ exit search, don't set mark
 *	else	accumulate into search string
 */
static int
isearch(int dir)
{
	struct line	*clp;		/* Saved line pointer */
	int		 c;
	int		 cbo;		/* Saved offset */
	int		 success;
	int		 pptr;
	int		 firstc;
	int		 xcase;
	int		 i;
	char		 opat[NPAT];
	int		 cdotline;	/* Saved line number */

	if (macrodef) {
		dobeep();
		ewprintf("Can't isearch in macro");
		return (FALSE);
	}
	for (cip = 0; cip < NSRCH; cip++)
		cmds[cip].s_code = SRCH_NOPR;

	(void)strlcpy(opat, pat, sizeof(opat));
	cip = 0;
	pptr = -1;
	clp = curwp->w_dotp;
	cbo = curwp->w_doto;
	cdotline = curwp->w_dotline;
	is_lpush();
	is_cpush(SRCH_BEGIN);
	success = TRUE;
	is_prompt(dir, TRUE, success);

	for (;;) {
		update(CMODE);

		switch (c = getkey(FALSE)) {
		case CCHR('['):
			/*
			 * If new characters come in the next 300 msec,
			 * we can assume that they belong to a longer
			 * escaped sequence so we should ungetkey the
			 * ESC to avoid writing out garbage.
			 */
			if (ttwait(300) == FALSE)
				ungetkey(c);
			/* FALLTHRU */
		case CCHR('M'):
			srch_lastdir = dir;
			curwp->w_markp = clp;
			curwp->w_marko = cbo;
			curwp->w_markline = cdotline;
			ewprintf("Mark set");
			return (TRUE);
		case CCHR('G'):
			if (success != TRUE) {
				while (is_peek() == SRCH_ACCM)
					is_undo(&pptr, &dir);
				success = TRUE;
				is_prompt(dir, pptr < 0, success);
				break;
			}
			curwp->w_dotp = clp;
			curwp->w_doto = cbo;
			curwp->w_dotline = cdotline;
			curwp->w_rflag |= WFMOVE;
			srch_lastdir = dir;
			(void)ctrlg(FFRAND, 0);
			(void)strlcpy(pat, opat, sizeof(pat));
			return (ABORT);
		case CCHR('S'):
			if (dir == SRCH_BACK) {
				dir = SRCH_FORW;
				is_lpush();
				is_cpush(SRCH_FORW);
				success = TRUE;
			}
			if (success == FALSE && dir == SRCH_FORW) {
				/* wrap the search to beginning */
				curwp->w_dotp = bfirstlp(curbp);
				curwp->w_doto = 0;
				curwp->w_dotline = 1;
				if (is_find(dir) != FALSE) {
					is_cpush(SRCH_MARK);
					success = TRUE;
				}
				ewprintf("Overwrapped I-search: %s", pat);
				break;
			}
			is_lpush();
			pptr = strlen(pat);
			if (forwchar(FFRAND, 1) == FALSE) {
                                dobeep();
                                success = FALSE;
                                ewprintf("Failed I-search: %s", pat);
			} else {
				if (is_find(SRCH_FORW) != FALSE)
					is_cpush(SRCH_MARK);
				else {
					(void)backchar(FFRAND, 1);
					dobeep();
					success = FALSE;
					ewprintf("Failed I-search: %s", pat);
				}
			}
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('R'):
			if (dir == SRCH_FORW) {
				dir = SRCH_BACK;
				is_lpush();
				is_cpush(SRCH_BACK);
				success = TRUE;
			}
			if (success == FALSE && dir == SRCH_BACK) {
				/* wrap the search to end */
				curwp->w_dotp = blastlp(curbp);
				curwp->w_doto = llength(curwp->w_dotp);
				curwp->w_dotline = curwp->w_bufp->b_lines;
				if (is_find(dir) != FALSE) {
					is_cpush(SRCH_MARK);
					success = TRUE;
				}
				ewprintf("Overwrapped I-search: %s", pat);
				break;
			}
			is_lpush();
			pptr = strlen(pat);
                        if (backchar(FFRAND, 1) == FALSE) {
                                dobeep();
                                success = FALSE;
                        } else {
				if (is_find(SRCH_BACK) != FALSE)
					is_cpush(SRCH_MARK);
				else {
					(void)forwchar(FFRAND, 1);
					dobeep();
					success = FALSE;
				}
			}
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('W'):
			/* add the rest of the current word to the pattern */
			clp = curwp->w_dotp;
			cbo = curwp->w_doto;
			firstc = 1;
			if (pptr == -1)
				pptr = 0;
			if (dir == SRCH_BACK) {
				/* when isearching backwards, cbo is the start of the pattern */
				cbo += pptr;
			}

			/* if the search is case insensitive, add to pattern using lowercase */
			xcase = 0;
			for (i = 0; pat[i]; i++)
				if (ISUPPER(CHARMASK(pat[i])))
					xcase = 1;

			while (cbo < llength(clp)) {
				c = lgetc(clp, cbo++);
				if ((!firstc && !isalnum(c)))
					break;

				if (pptr == NPAT - 1) {
					dobeep();
					break;
				}
				firstc = 0;
				if (!xcase && ISUPPER(c))
					c = TOLOWER(c);

				pat[pptr++] = c;
				pat[pptr] = '\0';
				/* cursor only moves when isearching forwards */
				if (dir == SRCH_FORW) {
					curwp->w_doto = cbo;
					curwp->w_rflag |= WFMOVE;
					update(CMODE);
				}
			}
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('H'):
		case CCHR('?'):
			is_undo(&pptr, &dir);
			if (is_peek() != SRCH_ACCM)
				success = TRUE;
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('\\'):
		case CCHR('Q'):
			c = (char)getkey(FALSE);
			goto addchar;
		default:
			if (ISCTRL(c)) {
				ungetkey(c);
				curwp->w_markp = clp;
				curwp->w_marko = cbo;
				curwp->w_markline = cdotline;
				ewprintf("Mark set");
				curwp->w_rflag |= WFMOVE;
				return (TRUE);
			}
			/* FALLTHRU */
		case CCHR('I'):
		case CCHR('J'):
	addchar:
			if (pptr == -1)
				pptr = 0;
			if (pptr == 0)
				success = TRUE;
			if (pptr == NPAT - 1)
				dobeep();
			else {
				pat[pptr++] = c;
				pat[pptr] = '\0';
			}
			is_lpush();
			if (success != FALSE) {
				if (is_find(dir) != FALSE)
					is_cpush(c);
				else {
					success = FALSE;
					dobeep();
					is_cpush(SRCH_ACCM);
				}
			} else
				is_cpush(SRCH_ACCM);
			is_prompt(dir, FALSE, success);
		}
	}
	/* NOTREACHED */
}

static void
is_cpush(int cmd)
{
	if (++cip >= NSRCH)
		cip = 0;
	cmds[cip].s_code = cmd;
}

static void
is_lpush(void)
{
	int	ctp;

	ctp = cip + 1;
	if (ctp >= NSRCH)
		ctp = 0;
	cmds[ctp].s_code = SRCH_NOPR;
	cmds[ctp].s_doto = curwp->w_doto;
	cmds[ctp].s_dotp = curwp->w_dotp;
	cmds[ctp].s_dotline = curwp->w_dotline;
}

static void
is_pop(void)
{
	if (cmds[cip].s_code != SRCH_NOPR) {
		curwp->w_doto = cmds[cip].s_doto;
		curwp->w_dotp = cmds[cip].s_dotp;
		curwp->w_dotline = cmds[cip].s_dotline;
		curwp->w_rflag |= WFMOVE;
		cmds[cip].s_code = SRCH_NOPR;
	}
	if (--cip <= 0)
		cip = NSRCH - 1;
}

static int
is_peek(void)
{
	return (cmds[cip].s_code);
}

/* this used to always return TRUE (the return value was checked) */
static void
is_undo(int *pptr, int *dir)
{
	int	redo = FALSE;

	switch (cmds[cip].s_code) {
	case SRCH_BEGIN:
	case SRCH_NOPR:
		*pptr = -1;
		break;
	case SRCH_MARK:
		break;
	case SRCH_FORW:
		*dir = SRCH_BACK;
		redo = TRUE;
		break;
	case SRCH_BACK:
		*dir = SRCH_FORW;
		redo = TRUE;
		break;
	case SRCH_ACCM:
	default:
		*pptr -= 1;
		if (*pptr < 0)
			*pptr = 0;
		pat[*pptr] = '\0';
		break;
	}
	is_pop();
	if (redo)
		is_undo(pptr, dir);
}

static int
is_find(int dir)
{
	int	 plen, odoto, odotline;
	struct line	*odotp;

	odoto = curwp->w_doto;
	odotp = curwp->w_dotp;
	odotline = curwp->w_dotline;
	plen = strlen(pat);
	if (plen != 0) {
		if (dir == SRCH_FORW) {
			(void)backchar(FFARG | FFRAND, plen);
			if (forwsrch() == FALSE) {
				curwp->w_doto = odoto;
				curwp->w_dotp = odotp;
				curwp->w_dotline = odotline;
				return (FALSE);
			}
			return (TRUE);
		}
		if (dir == SRCH_BACK) {
			(void)forwchar(FFARG | FFRAND, plen);
			if (backsrch() == FALSE) {
				curwp->w_doto = odoto;
				curwp->w_dotp = odotp;
				curwp->w_dotline = odotline;
				return (FALSE);
			}
			return (TRUE);
		}
		dobeep();
		ewprintf("bad call to is_find");
		return (FALSE);
	}
	return (FALSE);
}

/*
 * If called with "dir" not one of SRCH_FORW or SRCH_BACK, this routine used
 * to print an error message.  It also used to return TRUE or FALSE, depending
 * on if it liked the "dir".  However, none of the callers looked at the
 * status, so I just made the checking vanish.
 */
static void
is_prompt(int dir, int flag, int success)
{
	if (dir == SRCH_FORW) {
		if (success != FALSE)
			is_dspl("I-search", flag);
		else
			is_dspl("Failing I-search", flag);
	} else if (dir == SRCH_BACK) {
		if (success != FALSE)
			is_dspl("I-search backward", flag);
		else
			is_dspl("Failing I-search backward", flag);
	} else
		ewprintf("Broken call to is_prompt");
}

/*
 * Prompt writing routine for the incremental search.  The "i_prompt" is just
 * a string. The "flag" determines whether pat should be printed.
 */
static void
is_dspl(char *i_prompt, int flag)
{
	if (flag != FALSE)
		ewprintf("%s: ", i_prompt);
	else
		ewprintf("%s: %s", i_prompt, pat);
}

/*
 * Query Replace.
 *	Replace strings selectively.  Does a search and replace operation.
 */
int
queryrepl(int f, int n)
{
	int	s;
	int	rcnt = 0;		/* replacements made so far	*/
	int	plen;			/* length of found string	*/
	char	news[NPAT], *rep;	/* replacement string		*/

	if (macrodef) {
		dobeep();
		ewprintf("Can't query replace in macro");
		return (FALSE);
	}

	if ((s = readpattern("Query replace")) != TRUE)
		return (s);
	if ((rep = eread("Query replace %s with: ", news, NPAT,
	    EFNUL | EFNEW | EFCR, pat)) == NULL)
		return (ABORT);
	else if (rep[0] == '\0')
		news[0] = '\0';
	ewprintf("Query replacing %s with %s:", pat, news);
	plen = strlen(pat);

	/*
	 * Search forward repeatedly, checking each time whether to insert
	 * or not.  The "!" case makes the check always true, so it gets put
	 * into a tighter loop for efficiency.
	 */
	while (forwsrch() == TRUE) {
retry:
		update(CMODE);
		switch (getkey(FALSE)) {
		case 'y':
		case ' ':
			if (lreplace((RSIZE)plen, news) == FALSE)
				return (FALSE);
			rcnt++;
			break;
		case '.':
			if (lreplace((RSIZE)plen, news) == FALSE)
				return (FALSE);
			rcnt++;
			goto stopsearch;
		/* ^G, CR or ESC */
		case CCHR('G'):
			(void)ctrlg(FFRAND, 0);
			goto stopsearch;
		case CCHR('['):
		case CCHR('M'):
			goto stopsearch;
		case '!':
			do {
				if (lreplace((RSIZE)plen, news) == FALSE)
					return (FALSE);
				rcnt++;
			} while (forwsrch() == TRUE);
			goto stopsearch;
		case 'n':
		case CCHR('H'):
		/* To not replace */
		case CCHR('?'):
			break;
		default:
			ewprintf("y/n or <SP>/<DEL>: replace/don't, [.] repl-end, [!] repl-rest, <CR>/<ESC> quit");
			goto retry;
		}
	}
stopsearch:
	curwp->w_rflag |= WFFULL;
	update(CMODE);
	if (rcnt == 1)
		ewprintf("Replaced 1 occurrence");
	else
		ewprintf("Replaced %d occurrences", rcnt);
	return (TRUE);
}

/*
 * Replace string globally without individual prompting.
 */
int
replstr(int f, int n)
{
	char	news[NPAT];
	int	s, plen, rcnt = 0;
	char	*r;

	if ((s = readpattern("Replace string")) != TRUE)
		return s;

	r = eread("Replace string %s with: ", news, NPAT,
	    EFNUL | EFNEW | EFCR,  pat);
	if (r == NULL)
		 return (ABORT);

	plen = strlen(pat);
	while (forwsrch() == TRUE) {
		update(CMODE);
		if (lreplace((RSIZE)plen, news) == FALSE)
			return (FALSE);

		rcnt++;
	}

	curwp->w_rflag |= WFFULL;
	update(CMODE);

	if (rcnt == 1)
		ewprintf("Replaced 1 occurrence");
	else
		ewprintf("Replaced %d occurrences", rcnt);

	return (TRUE);
}

/*
 * This routine does the real work of a forward search.  The pattern is sitting
 * in the external variable "pat".  If found, dot is updated, the window system
 * is notified of the change, and TRUE is returned.  If the string isn't found,
 * FALSE is returned.
 */
int
forwsrch(void)
{
	struct line	*clp, *tlp;
	int	 cbo, tbo, c, i, xcase = 0;
	char	*pp;
	int	 nline;

	clp = curwp->w_dotp;
	cbo = curwp->w_doto;
	nline = curwp->w_dotline;
	for (i = 0; pat[i]; i++)
		if (ISUPPER(CHARMASK(pat[i])))
			xcase = 1;
	for (;;) {
		if (cbo == llength(clp)) {
			if ((clp = lforw(clp)) == curbp->b_headp)
				break;
			nline++;
			cbo = 0;
			c = CCHR('J');
		} else
			c = lgetc(clp, cbo++);
		if (eq(c, pat[0], xcase) != FALSE) {
			tlp = clp;
			tbo = cbo;
			pp = &pat[1];
			while (*pp != 0) {
				if (tbo == llength(tlp)) {
					tlp = lforw(tlp);
					if (tlp == curbp->b_headp)
						goto fail;
					tbo = 0;
					c = CCHR('J');
					if (eq(c, *pp++, xcase) == FALSE)
						goto fail;
					nline++;
				} else {
					c = lgetc(tlp, tbo++);
					if (eq(c, *pp++, xcase) == FALSE)
						goto fail;
				}
			}
			curwp->w_dotp = tlp;
			curwp->w_doto = tbo;
			curwp->w_dotline = nline;
			curwp->w_rflag |= WFMOVE;
			return (TRUE);
		}
fail:		;
	}
	return (FALSE);
}

/*
 * This routine does the real work of a backward search.  The pattern is
 * sitting in the external variable "pat".  If found, dot is updated, the
 * window system is notified of the change, and TRUE is returned.  If the
 * string isn't found, FALSE is returned.
 */
int
backsrch(void)
{
	struct line	*clp, *tlp;
	int	 cbo, tbo, c, i, xcase = 0;
	char	*epp, *pp;
	int	 nline, pline;

	for (epp = &pat[0]; epp[1] != 0; ++epp);
	clp = curwp->w_dotp;
	cbo = curwp->w_doto;
	nline = curwp->w_dotline;
	for (i = 0; pat[i]; i++)
		if (ISUPPER(CHARMASK(pat[i])))
			xcase = 1;
	for (;;) {
		if (cbo == 0) {
			clp = lback(clp);
			if (clp == curbp->b_headp)
				return (FALSE);
			nline--;
			cbo = llength(clp) + 1;
		}
		if (--cbo == llength(clp))
			c = CCHR('J');
		else
			c = lgetc(clp, cbo);
		if (eq(c, *epp, xcase) != FALSE) {
			tlp = clp;
			tbo = cbo;
			pp = epp;
			pline = nline;
			while (pp != &pat[0]) {
				if (tbo == 0) {
					tlp = lback(tlp);
					if (tlp == curbp->b_headp)
						goto fail;
					nline--;
					tbo = llength(tlp) + 1;
				}
				if (--tbo == llength(tlp))
					c = CCHR('J');
				else
					c = lgetc(tlp, tbo);
				if (eq(c, *--pp, xcase) == FALSE) {
					nline = pline;
					goto fail;
				}
			}
			curwp->w_dotp = tlp;
			curwp->w_doto = tbo;
			curwp->w_dotline = nline;
			curwp->w_rflag |= WFMOVE;
			return (TRUE);
		}
fail:		;
	}
	/* NOTREACHED */
}

/*
 * Compare two characters.  The "bc" comes from the buffer.  It has its case
 * folded out. The "pc" is from the pattern.
 */
static int
eq(int bc, int pc, int xcase)
{
	bc = CHARMASK(bc);
	pc = CHARMASK(pc);
	if (bc == pc)
		return (TRUE);
	if (xcase)
		return (FALSE);
	if (ISUPPER(bc))
		return (TOLOWER(bc) == pc);
	if (ISUPPER(pc))
		return (bc == TOLOWER(pc));
	return (FALSE);
}

/*
 * Read a pattern.  Stash it in the external variable "pat".  The "pat" is not
 * updated if the user types in an empty line.  If the user typed an empty
 * line, and there is no old pattern, it is an error.  Display the old pattern,
 * in the style of Jeff Lomicka.  There is some do-it-yourself control
 * expansion.
 */
int
readpattern(char *r_prompt)
{
	char	tpat[NPAT], *rep;
	int	retval;

	if (pat[0] == '\0')
		rep = eread("%s: ", tpat, NPAT, EFNEW | EFCR, r_prompt);
	else
		rep = eread("%s (default %s): ", tpat, NPAT,
		    EFNUL | EFNEW | EFCR, r_prompt, pat);

	/* specified */
	if (rep == NULL) {
		retval = ABORT;
	} else if (rep[0] != '\0') {
		(void)strlcpy(pat, tpat, sizeof(pat));
		retval = TRUE;
	} else if (pat[0] != '\0') {
		retval = TRUE;
	} else
		retval = FALSE;
	return (retval);
}

/*
 * Prompt for a character and kill until its next occurrence,
 * including it.  Mark is cleared afterwards.
 */
int
zaptochar(int f, int n)
{
       return (zap(TRUE, n));
}

/* Like zaptochar but stops before the character. */
int
zapuptochar(int f, int n)
{
       return (zap(FALSE, n));
}

/*
 * Prompt for a character and deletes from the point up to, optionally
 * including, the first instance of that charcter.  Marks is cleared
 * afterwards.
 */
int
zap(int including, int n)
{
	int	s, backward;

	backward = n < 0;
	if (backward)
		n = -n;

	if (including)
		ewprintf("Zap to char: ");
	else
		ewprintf("Zap up to char: ");

	s = getkey(FALSE);
	eerase();
	if (s == ABORT || s == CCHR('G'))
		return (FALSE);

	if (n == 0)
		return (TRUE);

	pat[0] = (char)s;
	pat[1] = '\0';

	isetmark();
	while (n--) {
		s = backward ? backsrch() : forwsrch();
		if (s != TRUE) {
			dobeep();
			ewprintf("Search failed: \"%s\"", pat);
			swapmark(FFARG, 0);
			clearmark(FFARG, 0);
			return (s);
		}
	}

	if (!including) {
		if (backward)
			forwchar(FFARG, 1);
		else
			backchar(FFARG, 1);
	}

	killregion(FFARG, 0);
	clearmark(FFARG, 0);
	return (TRUE);
}
