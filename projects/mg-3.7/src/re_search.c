/*	$OpenBSD: re_search.c,v 1.36 2021/04/22 19:50:55 lum Exp $	*/

/* This file is in the public domain. */

/*
 *	regular expression search commands for Mg
 *
 * This file contains functions to implement several of gnuemacs's regular
 * expression functions for Mg.  Several of the routines below are just minor
 * re-arrangements of Mg's non-regular expression search functions.  Some of
 * them are similar in structure to the original MicroEMACS, others are
 * modifications of Rich Ellison's code.  Peter Newton re-wrote about half of
 * them from scratch.
 */

#include <sys/types.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "macro.h"

#define SRCH_BEGIN	(0)		/* search sub-codes		    */
#define SRCH_FORW	(-1)
#define SRCH_BACK	(-2)
#define SRCH_NOPR	(-3)
#define SRCH_ACCM	(-4)
#define SRCH_MARK	(-5)

#define RE_NMATCH	10		/* max number of matches	    */
#define REPLEN		256		/* max length of replacement string */

#ifndef REG_STARTEND
#define REG_STARTEND 4
#endif

char	re_pat[NPAT];			/* regex pattern		    */
int	re_srch_lastdir = SRCH_NOPR;	/* last search flags		    */
int	casefoldsearch = TRUE;		/* does search ignore case?	    */

static int	 re_doreplace(RSIZE, char *);
static int	 re_forwsrch(void);
static int	 re_backsrch(void);
static int	 re_readpattern(char *);
static int	 killmatches(int);
static int	 countmatches(int);

/*
 * Search forward.
 * Get a search string from the user and search for it starting at ".".  If
 * found, move "." to just after the matched characters.  display does all
 * the hard stuff.  If not found, it just prints a message.
 */
int
re_forwsearch(int f, int n)
{
	int	s;

	if ((s = re_readpattern("RE Search")) != TRUE)
		return (s);
	if (re_forwsrch() == FALSE) {
		dobeep();
		ewprintf("Search failed: \"%s\"", re_pat);
		return (FALSE);
	}
	re_srch_lastdir = SRCH_FORW;
	return (TRUE);
}

/*
 * Reverse search.
 * Get a search string from the user, and search, starting at "."
 * and proceeding toward the front of the buffer. If found "." is left
 * pointing at the first character of the pattern [the last character that
 * was matched].
 */
int
re_backsearch(int f, int n)
{
	int	s;

	if ((s = re_readpattern("RE Search backward")) != TRUE)
		return (s);
	if (re_backsrch() == FALSE) {
		dobeep();
		ewprintf("Search failed: \"%s\"", re_pat);
		return (FALSE);
	}
	re_srch_lastdir = SRCH_BACK;
	return (TRUE);
}

/*
 * Search again, using the same search string and direction as the last search
 * command.  The direction has been saved in "srch_lastdir", so you know which
 * way to go.
 *
 * XXX: This code has problems -- some incompatibility(?) with extend.c causes
 * match to fail when it should not.
 */
int
re_searchagain(int f, int n)
{
	if (re_srch_lastdir == SRCH_NOPR) {
		dobeep();
		ewprintf("No last search");
		return (FALSE);
	}
	if (re_srch_lastdir == SRCH_FORW) {
		if (re_forwsrch() == FALSE) {
			dobeep();
			ewprintf("Search failed: \"%s\"", re_pat);
			return (FALSE);
		}
		return (TRUE);
	}
	if (re_srch_lastdir == SRCH_BACK)
		if (re_backsrch() == FALSE) {
			dobeep();
			ewprintf("Search failed: \"%s\"", re_pat);
			return (FALSE);
		}

	return (TRUE);
}

/* Compiled regex goes here-- changed only when new pattern read */
static regex_t		regex_buff;
static regmatch_t	regex_match[RE_NMATCH];

/*
 * Re-Query Replace.
 *	Replace strings selectively.  Does a search and replace operation.
 */
int
re_queryrepl(int f, int n)
{
	int	rcnt = 0;		/* replacements made so far	*/
	int	plen, s;		/* length of found string	*/
	char	news[NPAT];		/* replacement string		*/

	if ((s = re_readpattern("RE Query replace")) != TRUE)
		return (s);
	if (eread("Query replace %s with: ", news, NPAT,
	    EFNUL | EFNEW | EFCR, re_pat) == NULL)
		return (ABORT);
	ewprintf("Query replacing %s with %s:", re_pat, news);

	/*
	 * Search forward repeatedly, checking each time whether to insert
	 * or not.  The "!" case makes the check always true, so it gets put
	 * into a tighter loop for efficiency.
	 */
	while (re_forwsrch() == TRUE) {
retry:
		update(CMODE);
		switch (getkey(FALSE)) {
		case ' ':
			plen = regex_match[0].rm_eo - regex_match[0].rm_so;
			if (re_doreplace((RSIZE)plen, news) == FALSE)
				return (FALSE);
			rcnt++;
			break;

		case '.':
			plen = regex_match[0].rm_eo - regex_match[0].rm_so;
			if (re_doreplace((RSIZE)plen, news) == FALSE)
				return (FALSE);
			rcnt++;
			goto stopsearch;

		case CCHR('G'):				/* ^G */
			(void)ctrlg(FFRAND, 0);
			goto stopsearch;
		case CCHR('['):				/* ESC */
		case '`':
			goto stopsearch;
		case '!':
			do {
				plen = regex_match[0].rm_eo - regex_match[0].rm_so;
				if (re_doreplace((RSIZE)plen, news) == FALSE)
					return (FALSE);
				rcnt++;
			} while (re_forwsrch() == TRUE);
			goto stopsearch;

		case CCHR('?'):				/* To not replace */
			break;

		default:
			ewprintf("<SP> replace, [.] rep-end, <DEL> don't, [!] repl rest <ESC> quit");
			goto retry;
		}
	}

stopsearch:
	curwp->w_rflag |= WFFULL;
	update(CMODE);
	if (!inmacro) {
		if (rcnt == 0)
			ewprintf("(No replacements done)");
		else if (rcnt == 1)
			ewprintf("(1 replacement done)");
		else
			ewprintf("(%d replacements done)", rcnt);
	}
	return (TRUE);
}

int
re_repl(int f, int n)
{
	int     rcnt = 0;		/* replacements made so far     */
	int     plen, s;		/* length of found string       */
	char    news[NPAT];		/* replacement string           */

	if ((s = re_readpattern("RE Replace")) != TRUE)
		return (s);
	if (eread("Replace %s with: ", news, NPAT,
	    EFNUL | EFNEW | EFCR, re_pat) == NULL)
                return (ABORT);

	while (re_forwsrch() == TRUE) {
		plen = regex_match[0].rm_eo - regex_match[0].rm_so;
		if (re_doreplace((RSIZE)plen, news) == FALSE)
			return (FALSE);
		rcnt++;
	}

	curwp->w_rflag |= WFFULL;
	update(CMODE);
	if (!inmacro)
		ewprintf("(%d replacement(s) done)", rcnt);
	
	return(TRUE);
}

/*
 * Routine re_doreplace calls lreplace to make replacements needed by
 * re_query replace.  Its reason for existence is to deal with \1, \2. etc.
 *  plen: length to remove
 *  st:   replacement string
 */
static int
re_doreplace(RSIZE plen, char *st)
{
	int	 j, k, s, more, num, state;
	struct line	*clp;
	char	 repstr[REPLEN];

	clp = curwp->w_dotp;
	more = TRUE;
	j = 0;
	state = 0;
	num = 0;

	/* The following FSA parses the replacement string */
	while (more) {
		switch (state) {
		case 0:
			if (*st == '\\') {
				st++;
				state = 1;
			} else if (*st == '\0')
				more = FALSE;
			else {
				repstr[j] = *st;
				j++;
				if (j >= REPLEN)
					return (FALSE);
				st++;
			}
			break;
		case 1:
			if (*st >= '0' && *st <= '9') {
				num = *st - '0';
				st++;
				state = 2;
			} else if (*st == '\0')
				more = FALSE;
			else {
				repstr[j] = *st;
				j++;
				if (j >= REPLEN)
					return (FALSE);
				st++;
				state = 0;
			}
			break;
		case 2:
			if (*st >= '0' && *st <= '9') {
				num = 10 * num + *st - '0';
				st++;
			} else {
				if (num >= RE_NMATCH)
					return (FALSE);
				k = regex_match[num].rm_eo - regex_match[num].rm_so;
				if (j + k >= REPLEN)
					return (FALSE);
				bcopy(&(clp->l_text[regex_match[num].rm_so]),
				    &repstr[j], k);
				j += k;
				if (*st == '\0')
					more = FALSE;
				if (*st == '\\') {
					st++;
					state = 1;
				} else {
					repstr[j] = *st;
					j++;
					if (j >= REPLEN)
						return (FALSE);
					st++;
					state = 0;
				}
			}
			break;
		}		/* switch (state) */
	}			/* while (more)   */

	repstr[j] = '\0';
	s = lreplace(plen, repstr);
	return (s);
}

/*
 * This routine does the real work of a forward search.  The pattern is
 * sitting in the external variable "pat".  If found, dot is updated, the
 * window system is notified of the change, and TRUE is returned.  If the
 * string isn't found, FALSE is returned.
 */
static int
re_forwsrch(void)
{
	int	 	 re_flags, tbo, tdotline, error;
	struct line	*clp;

	clp = curwp->w_dotp;
	tbo = curwp->w_doto;
	tdotline = curwp->w_dotline;

	if (tbo == clp->l_used)
		/*
		 * Don't start matching past end of line -- must move to
		 * beginning of next line, unless line is empty or at
		 * end of file.
		 */
		if (clp != curbp->b_headp && llength(clp) != 0) {
			clp = lforw(clp);
			tdotline++;
			tbo = 0;
		}
	/*
	 * Note this loop does not process the last line, but this editor
	 * always makes the last line empty so this is good.
	 */
	while (clp != (curbp->b_headp)) {
		re_flags = REG_STARTEND;
		if (tbo != 0)
			re_flags |= REG_NOTBOL;
		regex_match[0].rm_so = tbo;
		regex_match[0].rm_eo = llength(clp);
		error = regexec(&regex_buff, ltext(clp) ? ltext(clp) : "",
		    RE_NMATCH, regex_match, re_flags);
		if (error != 0) {
			clp = lforw(clp);
			tdotline++;
			tbo = 0;
		} else {
			curwp->w_doto = regex_match[0].rm_eo;
			curwp->w_dotp = clp;
			curwp->w_dotline = tdotline;
			curwp->w_rflag |= WFMOVE;
			return (TRUE);
		}
	}
	return (FALSE);
}

/*
 * This routine does the real work of a backward search.  The pattern is sitting
 * in the external variable "re_pat".  If found, dot is updated, the window
 * system is notified of the change, and TRUE is returned.  If the string isn't
 * found, FALSE is returned.
 */
static int
re_backsrch(void)
{
	struct line		*clp;
	int		 tbo, tdotline;
	regmatch_t	 lastmatch;

	clp = curwp->w_dotp;
	tbo = curwp->w_doto;
	tdotline = curwp->w_dotline;

	/* Start search one position to the left of dot */
	tbo = tbo - 1;
	if (tbo < 0) {
		/* must move up one line */
		clp = lback(clp);
		tdotline--;
		tbo = llength(clp);
	}

	/*
	 * Note this loop does not process the last line, but this editor
	 * always makes the last line empty so this is good.
	 */
	while (clp != (curbp->b_headp)) {
		regex_match[0].rm_so = 0;
		regex_match[0].rm_eo = llength(clp);
		lastmatch.rm_so = -1;
		/*
		 * Keep searching until we don't match any longer.  Assumes a
		 * non-match does not modify the regex_match array.  We have to
		 * do this character-by-character after the first match since
		 * POSIX regexps don't give you a way to do reverse matches.
		 */
		while (!regexec(&regex_buff, ltext(clp) ? ltext(clp) : "",
		    RE_NMATCH, regex_match, REG_STARTEND) &&
		    regex_match[0].rm_so <= tbo) {
			memcpy(&lastmatch, &regex_match[0], sizeof(regmatch_t));
			regex_match[0].rm_so++;
			regex_match[0].rm_eo = llength(clp);
		}
		if (lastmatch.rm_so == -1) {
			clp = lback(clp);
			tdotline--;
			tbo = llength(clp);
		} else {
			memcpy(&regex_match[0], &lastmatch, sizeof(regmatch_t));
			curwp->w_doto = regex_match[0].rm_so;
			curwp->w_dotp = clp;
			curwp->w_dotline = tdotline;
			curwp->w_rflag |= WFMOVE;
			return (TRUE);
		}
	}
	return (FALSE);
}

/*
 * Read a pattern.
 * Stash it in the external variable "re_pat". The "pat" is
 * not updated if the user types in an empty line. If the user typed
 * an empty line, and there is no old pattern, it is an error.
 * Display the old pattern, in the style of Jeff Lomicka. There is
 * some do-it-yourself control expansion.
 */
static int
re_readpattern(char *re_prompt)
{
	static int	dofree = 0;
	int		flags, error, s;
	char		tpat[NPAT], *rep;

	if (re_pat[0] == '\0')
		rep = eread("%s: ", tpat, NPAT, EFNEW | EFCR, re_prompt);
	else
		rep = eread("%s (default %s): ", tpat, NPAT,
		    EFNUL | EFNEW | EFCR, re_prompt, re_pat);
	if (rep == NULL)
		return (ABORT);
	if (rep[0] != '\0') {
		/* New pattern given */
		(void)strlcpy(re_pat, tpat, sizeof(re_pat));
		if (casefoldsearch)
			flags = REG_EXTENDED | REG_ICASE;
		else
			flags = REG_EXTENDED;
		if (dofree)
			regfree(&regex_buff);
		error = regcomp(&regex_buff, re_pat, flags);
		if (error != 0) {
			char	message[256];
			regerror(error, &regex_buff, message, sizeof(message));
			dobeep();
			ewprintf("Regex Error: %s", message);
			re_pat[0] = '\0';
			return (FALSE);
		}
		dofree = 1;
		s = TRUE;
	} else if (rep[0] == '\0' && re_pat[0] != '\0')
		/* Just using old pattern */
		s = TRUE;
	else
		s = FALSE;
	return (s);
}

/*
 * Cause case to not matter in searches.  This is the default.	If called
 * with argument cause case to matter.
 */
/* ARGSUSED*/
int
setcasefold(int f, int n)
{
	if (f & FFARG) {
		casefoldsearch = FALSE;
		ewprintf("Case-fold-search unset");
	} else {
		casefoldsearch = TRUE;
		ewprintf("Case-fold-search set");
	}

	/*
	 * Invalidate the regular expression pattern since I'm too lazy to
	 * recompile it.
	 */
	re_pat[0] = '\0';
	return (TRUE);
}

/*
 * Delete all lines after dot that contain a string matching regex.
 */
int
delmatchlines(int f, int n)
{
	int	s;

	if ((s = re_readpattern("Flush lines (containing match for regexp)"))
	    != TRUE)
		return (s);

	s = killmatches(TRUE);
	return (s);
}

/*
 * Delete all lines after dot that don't contain a string matching regex.
 */
int
delnonmatchlines(int f, int n)
{
	int	s;

	if ((s = re_readpattern("Keep lines (containing match for regexp)"))
	    != TRUE)
		return (s);

	s = killmatches(FALSE);
	return (s);
}

/*
 * This function does the work of deleting matching lines.
 */
static int
killmatches(int cond)
{
	int	 s, error;
	int	 count = 0;
	struct line	*clp;

	clp = curwp->w_dotp;
	if (curwp->w_doto == llength(clp))
		/* Consider dot on next line */
		clp = lforw(clp);

	while (clp != (curbp->b_headp)) {
		/* see if line matches */
		regex_match[0].rm_so = 0;
		regex_match[0].rm_eo = llength(clp);
		error = regexec(&regex_buff, ltext(clp) ? ltext(clp) : "",
		    RE_NMATCH, regex_match, REG_STARTEND);

		/* Delete line when appropriate */
		if ((cond == FALSE && error) || (cond == TRUE && !error)) {
			curwp->w_doto = 0;
			curwp->w_dotp = clp;
			count++;
			s = ldelete(llength(clp) + 1, KNONE);
			clp = curwp->w_dotp;
			curwp->w_rflag |= WFMOVE;
			if (s == FALSE)
				return (FALSE);
		} else
			clp = lforw(clp);
	}

	ewprintf("%d line(s) deleted", count);
	if (count > 0)
		curwp->w_rflag |= WFMOVE;

	return (TRUE);
}

/*
 * Count lines matching regex.
 */
int
cntmatchlines(int f, int n)
{
	int	s;

	if ((s = re_readpattern("Count lines (matching regexp)")) != TRUE)
		return (s);
	s = countmatches(TRUE);

	return (s);
}

/*
 * Count lines that fail to match regex.
 */
int
cntnonmatchlines(int f, int n)
{
	int	s;

	if ((s = re_readpattern("Count lines (not matching regexp)")) != TRUE)
		return (s);
	s = countmatches(FALSE);

	return (s);
}

/*
 * This function does the work of counting matching lines.
 */
int
countmatches(int cond)
{
	int	 error;
	int	 count = 0;
	struct line	*clp;

	clp = curwp->w_dotp;
	if (curwp->w_doto == llength(clp))
		/* Consider dot on next line */
		clp = lforw(clp);

	while (clp != (curbp->b_headp)) {
		/* see if line matches */
		regex_match[0].rm_so = 0;
		regex_match[0].rm_eo = llength(clp);
		error = regexec(&regex_buff, ltext(clp) ? ltext(clp) : "",
		    RE_NMATCH, regex_match, REG_STARTEND);

		/* Count line when appropriate */
		if ((cond == FALSE && error) || (cond == TRUE && !error))
			count++;
		clp = lforw(clp);
	}

	if (cond)
		ewprintf("Number of lines matching: %d", count);
	else
		ewprintf("Number of lines not matching: %d", count);

	return (TRUE);
}
