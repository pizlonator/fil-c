/*	$OpenBSD: echo.c,v 1.69 2022/10/15 17:01:14 op Exp $	*/

/* This file is in the public domain. */

/*
 *	Echo line reading and writing.
 *
 * Common routines for reading and writing characters in the echo line area
 * of the display screen. Used by the entire known universe.
 */

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ttydef.h"
#include "def.h"
#include "funmap.h"
#include "key.h"
#include "macro.h"

static char	*veread(const char *, char *, size_t, int, va_list)
			__attribute__((__format__ (printf, 1, 0)));
static int	 complt(int, int, char *, size_t, int, int *);
static int	 complt_list(int, char *, int);
static void	 eformat(const char *, va_list)
			__attribute__((__format__ (printf, 1, 0)));
static void	 eputi(int, int);
static void	 eputl(long, int);
static void	 eputs(const char *);
static void	 eputc(char);
static struct list	*copy_list(struct list *);

int		epresf = FALSE;		/* stuff in echo line flag */
int		helpsh = TRUE;		/* help-text in echo buffer */

/*
 * Toggle permanent display of short help text in echo buffer
 */
int
helptoggle(int f, int n)
{
	if (f & FFARG)
		helpsh = n > 0;
	else
		helpsh = !helpsh;

	sgarbf = TRUE;

	return (TRUE);
}

/*
 * Erase the echo line.
 */
void
eerase(void)
{
	ttcolor(CTEXT);
	ttmove(nrow - 1, 0);
	tteeol();
	ttflush();
	epresf = FALSE;
	if (helpsh)
		ewprintf(" %s", hlp);
}

/*
 * Ask a "yes" or "no" question.  Return ABORT if the user answers the
 * question with the abort ("^G") character.  Return FALSE for "no" and
 * TRUE for "yes".  No formatting services are available.  No newline
 * required.
 */
int
eyorn(const char *sp)
{
	int	 s;

	if (inmacro)
		return (TRUE);

	ewprintf("%s? (y or n) ", sp);
	for (;;) {
		s = getkey(FALSE);
		if (s == 'y' || s == 'Y' || s == ' ') {
			eerase();
			return (TRUE);
		}
		if (s == 'n' || s == 'N' || s == CCHR('M')) {
			eerase();
			return (FALSE);
		}
		if (s == CCHR('G')) {
			eerase();
			return (ctrlg(FFRAND, 1));
		}
		ewprintf("Please answer y or n.  %s? (y or n) ", sp);
	}
	/* NOTREACHED */
}

/*
 * Ask a "yes", "no" or "revert" question.  Return ABORT if the user answers
 * the question with the abort ("^G") character.  Return FALSE for "no",
 * TRUE for "yes" and REVERT for "revert". No formatting services are
 * available.  No newline required.
 */
int
eynorr(const char *sp)
{
	int	 s;

	if (inmacro)
		return (TRUE);

	ewprintf("%s? (y, n or r) ", sp);
	for (;;) {
		s = getkey(FALSE);
		if (s == 'y' || s == 'Y' || s == ' ') {
			eerase();
			return (TRUE);
		}
		if (s == 'n' || s == 'N' || s == CCHR('M')) {
			eerase();
			return (FALSE);
		}
		if (s == 'r' || s == 'R') {
			eerase();
			return (REVERT);
		}
		if (s == CCHR('G')) {
			eerase();
			return (ctrlg(FFRAND, 1));
		}
		ewprintf("Please answer y, n or r.");
	}
	/* NOTREACHED */
}

/*
 * Like eyorn, but for more important questions.  User must type all of
 * "yes" or "no" and the trailing newline.
 */
int
eyesno(const char *sp)
{
	char	 buf[64], *rep;

	if (inmacro)
		return (TRUE);

	rep = eread("%s? (yes or no) ", buf, sizeof(buf),
	    EFNUL | EFNEW | EFCR, sp);
	for (;;) {
		if (rep == NULL) {
			eerase();
			return (ABORT);
		}
		if (rep[0] != '\0') {
			if (macrodef) {
				struct line	*lp = maclcur;

				maclcur = lp->l_bp;
				maclcur->l_fp = lp->l_fp;
				free(lp);
			}
			if (strcasecmp(rep, "yes") == 0) {
				eerase();
				return (TRUE);
			}
			if (strcasecmp(rep, "no") == 0) {
				eerase();
				return (FALSE);
			}
		}
		rep = eread("Please answer yes or no.  %s? (yes or no) ",
		    buf, sizeof(buf), EFNUL | EFNEW | EFCR, sp);
	}
	/* NOTREACHED */
}

/*
 * This is the general "read input from the echo line" routine.  The basic
 * idea is that the prompt string "prompt" is written to the echo line, and
 * a one line reply is read back into the supplied "buf" (with maximum
 * length "len").
 * XXX: When checking for an empty return value, always check rep, *not* buf
 * as buf may be freed in pathological cases.
 */
char *
eread(const char *fmt, char *buf, size_t nbuf, int flag, ...)
{
	va_list	 ap;
	char	*rep;

	va_start(ap, flag);
	rep = veread(fmt, buf, nbuf, flag, ap);
	va_end(ap);
	return (rep);
}

static char *
veread(const char *fp, char *buf, size_t nbuf, int flag, va_list ap)
{
	int	 dynbuf = (buf == NULL);
	int	 cpos, epos;		/* cursor, end position in buf */
	int	 c, i, y;
	int	 cplflag;		/* display completion list */
	int	 cwin = FALSE;		/* completion list created */
	int	 mr, ml;		/* match left/right arrows */
	int	 esc;			/* position in esc pattern */
	struct buffer	*bp;			/* completion list buffer */
	struct mgwin	*wp;			/* window for compl list */
	int	 match;			/* esc match found */
	int	 cc, rr;		/* saved ttcol, ttrow */
	char	*ret;			/* return value */

	static char emptyval[] = "";	/* XXX hackish way to return err msg*/

	if (inmacro) {
		if (dynbuf) {
			if ((buf = malloc(maclcur->l_used + 1)) == NULL)
				return (NULL);
		} else if ((size_t)maclcur->l_used >= nbuf)
			return (NULL);
		bcopy(maclcur->l_text, buf, maclcur->l_used);
		buf[maclcur->l_used] = '\0';
		maclcur = maclcur->l_fp;
		return (buf);
	}
	epos = cpos = 0;
	ml = mr = esc = 0;
	cplflag = FALSE;

	if ((flag & EFNEW) != 0 || ttrow != nrow - 1) {
		ttcolor(CTEXT);
		ttmove(nrow - 1, 0);
		epresf = TRUE;
	} else
		eputc(' ');
	eformat(fp, ap);
	if ((flag & EFDEF) != 0) {
		if (buf == NULL)
			return (NULL);
		eputs(buf);
		epos = cpos += strlen(buf);
	}
	tteeol();
	ttflush();
	for (;;) {
		c = getkey(FALSE);
		if ((flag & EFAUTO) != 0 && c == CCHR('I')) {
			if (buf == NULL)
				goto memfail;

			if (cplflag == TRUE) {
				complt_list(flag, buf, cpos);
				cwin = TRUE;
			} else if (complt(flag, c, buf, nbuf, epos, &i) == TRUE) {
				cplflag = TRUE;
				epos += i;
				cpos = epos;
			}
			continue;
		}
		cplflag = FALSE;

		if (esc > 0) { /* ESC sequence started */
			match = 0;
			if (ml == esc && key_left[ml] && c == key_left[ml]) {
				match++;
				if (key_left[++ml] == '\0') {
					c = CCHR('B');
					esc = 0;
				}
			}
			if (mr == esc && key_right[mr] && c == key_right[mr]) {
				match++;
				if (key_right[++mr] == '\0') {
					c = CCHR('F');
					esc = 0;
				}
			}
			if (match == 0) {
				esc = 0;
				continue;
				/* hack. how do we know esc pattern is done? */
			}
			if (esc > 0) {
				esc++;
				continue;
			}
		}

		switch (c) {
		case CCHR('A'): /* start of line */
			while (cpos > 0) {
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					--ttcol;
				}
				ttputc('\b');
				--ttcol;
			}
			ttflush();
			break;

		case CCHR('D'):
			if (cpos != epos) {
				tteeol();
				epos--;
				rr = ttrow;
				cc = ttcol;
				for (i = cpos; i < epos; i++) {
					buf[i] = buf[i + 1];
					eputc(buf[i]);
				}
				ttmove(rr, cc);
				ttflush();
			}
			break;

		case CCHR('E'): /* end of line */
			while (cpos < epos) {
				eputc(buf[cpos++]);
			}
			ttflush();
			break;

		case CCHR('B'): /* back */
			if (cpos > 0) {
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					--ttcol;
				}
				ttputc('\b');
				--ttcol;
				ttflush();
			}
			break;

		case CCHR('F'): /* forw */
			if (cpos < epos) {
				eputc(buf[cpos++]);
				ttflush();
			}
			break;

		case CCHR('Y'): /* yank from kill buffer */
			i = 0;
			while ((y = kremove(i++)) >= 0 && y != *curbp->b_nlchr) {
				int t;

				if (dynbuf && (size_t)(epos + 1) >= nbuf) {
					void *newp;
					size_t newsize = epos + epos + 16;
					if ((newp = realloc(buf, newsize))
					    == NULL)
						goto memfail;
					buf = newp;
					nbuf = newsize;
				}
				if (!dynbuf && (size_t)(epos + 1) >= nbuf) {
					dobeep();
					ewprintf("Line too long. Press Control-g to escape.");
					goto skipkey;
				}
				if (buf == NULL)
					goto memfail;
				for (t = epos; t > cpos; t--)
					buf[t] = buf[t - 1];
				buf[cpos++] = (char)y;
				epos++;
				eputc((char)y);
				cc = ttcol;
				rr = ttrow;
				for (t = cpos; t < epos; t++)
					eputc(buf[t]);
				ttmove(rr, cc);
			}
			ttflush();
			break;

		case CCHR('K'): /* copy here-EOL to kill buffer */
			kdelete();
			for (i = cpos; i < epos; i++)
				kinsert(buf[i], KFORW);
			tteeol();
			epos = cpos;
			ttflush();
			break;

		case CCHR('['):
			ml = mr = esc = 1;
			break;

		case CCHR('J'):
			c = CCHR('M');
			/* fallthrough */

		case CCHR('M'):			/* return, done */
			/* if there's nothing in the minibuffer, abort */
			if (epos == 0 && !(flag & EFNUL)) {
				(void)ctrlg(FFRAND, 0);
				ttflush();
				if (dynbuf && buf)
					free(buf);
				return (NULL);
			}
			if ((flag & EFFUNC) != 0) {
				if (buf == NULL)
					goto memfail;
				if (complt(flag, c, buf, nbuf, epos, &i)
				    == FALSE)
					continue;
				if (i > 0)
					epos += i;
			}
			if (buf != NULL)
				buf[epos] = '\0';
			if ((flag & EFCR) != 0) {
				ttputc(CCHR('M'));
				ttflush();
			}
			if (macrodef) {
				struct line	*lp;

				if ((lp = lalloc(cpos)) == NULL)
					goto memfail;
				lp->l_fp = maclcur->l_fp;
				maclcur->l_fp = lp;
				lp->l_bp = maclcur;
				maclcur = lp;
				bcopy(buf, lp->l_text, cpos);
			}
			ret = buf;
			goto done;

		case CCHR('G'):			/* bell, abort */
			eputc(CCHR('G'));
			(void)ctrlg(FFRAND, 0);
			ttflush();
			ret = NULL;
			goto done;

		case CCHR('H'):			/* rubout, erase */
			/* fallthrough */
		case CCHR('?'):
			if (cpos != 0) {
				if (buf == NULL)
					goto memfail;
				y = buf[--cpos];
				epos--;
				ttputc('\b');
				ttcol--;
				if (ISCTRL(y) != FALSE) {
					ttputc('\b');
					ttcol--;
				}
				rr = ttrow;
				cc = ttcol;
				for (i = cpos; i < epos; i++) {
					buf[i] = buf[i + 1];
					eputc(buf[i]);
				}
				ttputc(' ');
				if (ISCTRL(y) != FALSE) {
					ttputc(' ');
					ttputc('\b');
				}
				ttputc('\b');
				ttmove(rr, cc);
				ttflush();
			}
			break;

		case CCHR('X'):			/* kill line */
			/* fallthrough */
		case CCHR('U'):
			while (cpos != 0) {
				if (buf == NULL)
					goto memfail;
				ttputc('\b');
				ttputc(' ');
				ttputc('\b');
				--ttcol;
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					ttputc(' ');
					ttputc('\b');
					--ttcol;
				}
				epos--;
			}
			ttflush();
			break;

		case CCHR('W'):			/* kill to beginning of word */
			if (buf == NULL)
				goto memfail;
			while ((cpos > 0) && !ISWORD(buf[cpos - 1])) {
				ttputc('\b');
				ttputc(' ');
				ttputc('\b');
				--ttcol;
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					ttputc(' ');
					ttputc('\b');
					--ttcol;
				}
				epos--;
			}
			while ((cpos > 0) && ISWORD(buf[cpos - 1])) {
				if (buf == NULL)
					goto memfail;
				ttputc('\b');
				ttputc(' ');
				ttputc('\b');
				--ttcol;
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					ttputc(' ');
					ttputc('\b');
					--ttcol;
				}
				epos--;
			}
			ttflush();
			break;

		case CCHR('\\'):
			/* fallthrough */
		case CCHR('Q'):			/* quote next */
			c = getkey(FALSE);
			/* fallthrough */
		default:
			if (dynbuf && (size_t)(epos + 1) >= nbuf) {
				void *newp;
				size_t newsize = epos + epos + 16;
				if ((newp = realloc(buf, newsize)) == NULL)
					goto memfail;
				buf = newp;
				nbuf = newsize;
			}
			if (!dynbuf && (size_t)(epos + 1) >= nbuf) {
				dobeep();
				ewprintf("Line too long. Press Control-g to escape.");
				goto skipkey;
			}
			for (i = epos; i > cpos; i--)
				buf[i] = buf[i - 1];
			buf[cpos++] = (char)c;
			epos++;
			eputc((char)c);
			cc = ttcol;
			rr = ttrow;
			for (i = cpos; i < epos; i++)
				eputc(buf[i]);
			ttmove(rr, cc);
			ttflush();
		}

skipkey:	/* ignore key press */
;
	}
done:
	if (cwin == TRUE) {
		/* blow away cpltion window */
		bp = bfind("*Completions*", TRUE);
		if ((wp = popbuf(bp, WEPHEM)) != NULL) {
			if (wp->w_flag & WEPHEM) {
				curwp = wp;
				delwind(FFRAND, 1);
			} else {
				killbuffer(bp);
			}
		}
	}
	return (ret);
memfail:
	if (dynbuf && buf)
		free(buf);
	dobeep();
	ewprintf("Out of memory");
	return (emptyval);
}

/*
 * Do completion on a list of objects.
 * c is SPACE, TAB, or CR
 * return TRUE if matched (or partially matched)
 * FALSE is result is ambiguous,
 * ABORT on error.
 */
static int
complt(int flags, int c, char *buf, size_t nbuf, int cpos, int *nx)
{
	struct list	*lh, *lh2;
	struct list	*wholelist = NULL;
	int	 i, nxtra, nhits, bxtra, msglen, nshown;
	int	 wflag = FALSE;
	char	*msg;

	lh = lh2 = NULL;

	if ((flags & EFFUNC) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_function_list(buf);
	} else if ((flags & EFBUF) != 0) {
		lh = &(bheadp->b_list);
	} else if ((flags & EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf);
	} else
		panic("broken complt call: flags");

	if (c == ' ')
		wflag = TRUE;
	else if (c != '\t' && c != CCHR('M'))
		panic("broken complt call: c");

	nhits = 0;
	nxtra = HUGE;

	for (; lh != NULL; lh = lh->l_next) {
		if (memcmp(buf, lh->l_name, cpos) != 0)
			continue;
		if (nhits == 0)
			lh2 = lh;
		++nhits;
		if (lh->l_name[cpos] == '\0')
			nxtra = -1; /* exact match */
		else {
			bxtra = getxtra(lh, lh2, cpos, wflag);
			if (bxtra < nxtra)
				nxtra = bxtra;
			lh2 = lh;
		}
	}
	if (nhits == 0)
		msg = " [No match]";
	else if (nhits > 1 && nxtra == 0)
		msg = " [Ambiguous. Ctrl-G to cancel]";
	else {
		/*
		 * Being lazy - ought to check length, but all things
		 * autocompleted have known types/lengths.
		 */
		if (nxtra < 0 && nhits > 1 && c == ' ')
			nxtra = 1; /* ??? */
		for (i = 0; i < nxtra && (size_t)cpos < nbuf; ++i) {
			buf[cpos] = lh2->l_name[cpos];
			eputc(buf[cpos++]);
		}
		/* XXX should grow nbuf */
		ttflush();
		free_file_list(wholelist);
		*nx = nxtra;
		if (nxtra < 0 && c != CCHR('M')) /* exact */
			*nx = 0;
		return (TRUE);
	}

	/*
	 * wholelist is NULL if we are doing buffers.  Want to free lists
	 * that were created for us, but not the buffer list!
	 */
	free_file_list(wholelist);

	/* Set up backspaces, etc., being mindful of echo line limit. */
	msglen = strlen(msg);
	nshown = (ttcol + msglen + 2 > ncol) ?
		ncol - ttcol - 2 : msglen;
	eputs(msg);
	ttcol -= (i = nshown);	/* update ttcol!		 */
	while (i--)		/* move back before msg		 */
		ttputc('\b');
	ttflush();		/* display to user		 */
	i = nshown;
	while (i--)		/* blank out on next flush	 */
		eputc(' ');
	ttcol -= (i = nshown);	/* update ttcol on BS's		 */
	while (i--)
		ttputc('\b');	/* update ttcol again!		 */
	*nx = nxtra;
	return ((nhits > 0) ? TRUE : FALSE);
}

/*
 * Do completion on a list of objects, listing instead of completing.
 */
static int
complt_list(int flags, char *buf, int cpos)
{
	struct list	*lh, *lh2, *lh3;
	struct list	*wholelist = NULL;
	struct buffer	*bp;
	int	 i, maxwidth, width;
	int	 preflen = 0;
	int	 oldrow = ttrow;
	int	 oldcol = ttcol;
	int	 oldhue = tthue;
	char	 *linebuf;
	size_t	 linesize, len;
	char *cp;

	lh = NULL;

	ttflush();

	/* The results are put into a completion buffer. */
	bp = bfind("*Completions*", TRUE);
	if (bclear(bp) == FALSE)
		return (FALSE);
	bp->b_flag |= BFREADONLY;

	/*
	 * First get the list of objects.  This list may contain only
	 * the ones that complete what has been typed, or may be the
	 * whole list of all objects of this type.  They are filtered
	 * later in any case.  Set wholelist if the list has been
	 * cons'ed up just for us, so we can free it later.  We have
	 * to copy the buffer list for this function even though we
	 * didn't for complt.  The sorting code does destructive
	 * changes to the list, which we don't want to happen to the
	 * main buffer list!
	 */
	if ((flags & EFBUF) != 0)
		wholelist = lh = copy_list(&(bheadp->b_list));
	else if ((flags & EFFUNC) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_function_list(buf);
	} else if ((flags & EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf);
		/*
		 * We don't want to display stuff up to the / for file
		 * names preflen is the list of a prefix of what the
		 * user typed that should not be displayed.
		 */
		cp = strrchr(buf, '/');
		if (cp)
			preflen = cp - buf + 1;
	} else
		panic("broken complt call: flags");

	/*
	 * Sort the list, since users expect to see it in alphabetic
	 * order.
	 */
	lh2 = lh;
	while (lh2 != NULL) {
		lh3 = lh2->l_next;
		while (lh3 != NULL) {
			if (strcmp(lh2->l_name, lh3->l_name) > 0) {
				cp = lh2->l_name;
				lh2->l_name = lh3->l_name;
				lh3->l_name = cp;
			}
			lh3 = lh3->l_next;
		}
		lh2 = lh2->l_next;
	}

	/*
	 * First find max width of object to be displayed, so we can
	 * put several on a line.
	 */
	maxwidth = 0;
	lh2 = lh;
	while (lh2 != NULL) {
		for (i = 0; i < cpos; ++i) {
			if (buf[i] != lh2->l_name[i])
				break;
		}
		if (i == cpos) {
			width = strlen(lh2->l_name);
			if (width > maxwidth)
				maxwidth = width;
		}
		lh2 = lh2->l_next;
	}
	maxwidth += 1 - preflen;

	/*
	 * Now do the display.  Objects are written into linebuf until
	 * it fills, and then put into the help buffer.
	 */
	linesize = (ncol > maxwidth ? ncol : maxwidth) + 1;
	if ((linebuf = malloc(linesize)) == NULL) {
		free_file_list(wholelist);
		return (FALSE);
	}
	width = 0;

	/*
	 * We're going to strlcat() into the buffer, so it has to be
	 * NUL terminated.
	 */
	linebuf[0] = '\0';
	for (lh2 = lh; lh2 != NULL; lh2 = lh2->l_next) {
		for (i = 0; i < cpos; ++i) {
			if (buf[i] != lh2->l_name[i])
				break;
		}
		/* if we have a match */
		if (i == cpos) {
			/* if it wraps */
			if ((width + maxwidth) > ncol) {
				addline(bp, linebuf);
				linebuf[0] = '\0';
				width = 0;
			}
			len = strlcat(linebuf, lh2->l_name + preflen,
			    linesize);
			width += maxwidth;
			if (len < (size_t)width && (size_t)width < linesize) {
				/* pad so the objects nicely line up */
				memset(linebuf + len, ' ',
				    maxwidth - strlen(lh2->l_name + preflen));
				linebuf[width] = '\0';
			}
		}
	}
	if (width > 0)
		addline(bp, linebuf);
	free(linebuf);

	/*
	 * Note that we free lists only if they are put in wholelist lists
	 * that were built just for us should be freed.  However when we use
	 * the buffer list, obviously we don't want it freed.
	 */
	free_file_list(wholelist);
	popbuftop(bp, WEPHEM);	/* split the screen and put up the help
				 * buffer */
	update(CMODE);		/* needed to make the new stuff actually
				 * appear */
	ttmove(oldrow, oldcol);	/* update leaves cursor in arbitrary place */
	ttcolor(oldhue);	/* with arbitrary color */
	ttflush();
	return (0);
}

/*
 * The "lp1" and "lp2" point to list structures.  The "cpos" is a horizontal
 * position in the name.  Return the longest block of characters that can be
 * autocompleted at this point.  Sometimes the two symbols are the same, but
 * this is normal.
 */
int
getxtra(struct list *lp1, struct list *lp2, int cpos, int wflag)
{
	int	i;

	i = cpos;
	for (;;) {
		if (lp1->l_name[i] != lp2->l_name[i])
			break;
		if (lp1->l_name[i] == '\0')
			break;
		++i;
		if (wflag && !ISWORD(lp1->l_name[i - 1]))
			break;
	}
	return (i - cpos);
}

/*
 * Special "printf" for the echo line.  Each call to "ewprintf" starts a
 * new line in the echo area, and ends with an erase to end of the echo
 * line.  The formatting is done by a call to the standard formatting
 * routine.
 */
void
ewprintf(const char *fmt, ...)
{
	va_list	 ap;

	if (inmacro)
		return;

	va_start(ap, fmt);
	ttcolor(CTEXT);
	ttmove(nrow - 1, 0);
	eformat(fmt, ap);
	va_end(ap);
	tteeol();
	ttflush();
	epresf = TRUE;
}

/*
 * Printf style formatting. This is called by "ewprintf" to provide
 * formatting services to its clients.  The move to the start of the
 * echo line, and the erase to the end of the echo line, is done by
 * the caller. 
 * %c prints the "name" of the supplied character.
 * %k prints the name of the current key (and takes no arguments).
 * %d prints a decimal integer
 * %o prints an octal integer
 * %p prints a pointer
 * %s prints a string
 * %ld prints a long word
 * Anything else is echoed verbatim
 */
static void
eformat(const char *fp, va_list ap)
{
	char	kname[NKNAME], tmp[100], *cp;
	int	c;

	while ((c = *fp++) != '\0') {
		if (c != '%')
			eputc(c);
		else {
			c = *fp++;
			switch (c) {
			case 'c':
				getkeyname(kname, sizeof(kname),
				    va_arg(ap, int));
				eputs(kname);
				break;

			case 'k':
				for (cp = kname, c = 0; c < key.k_count; c++) {
					if (c)
						*cp++ = ' ';
					cp = getkeyname(cp, sizeof(kname) -
					    (cp - kname) - 1, key.k_chars[c]);
				}
				eputs(kname);
				break;

			case 'd':
				eputi(va_arg(ap, int), 10);
				break;

			case 'o':
				eputi(va_arg(ap, int), 8);
				break;

			case 'p':
				snprintf(tmp, sizeof(tmp), "%p",
				    va_arg(ap, void *));
				eputs(tmp);
				break;

			case 's':
				eputs(va_arg(ap, char *));
				break;

			case 'l':
				/* explicit longword */
				c = *fp++;
				switch (c) {
				case 'd':
					eputl(va_arg(ap, long), 10);
					break;
				default:
					eputc(c);
					break;
				}
				break;

			default:
				eputc(c);
			}
		}
	}
}

/*
 * Put integer, in radix "r".
 */
static void
eputi(int i, int r)
{
	int	 q;

	if (i < 0) {
		eputc('-');
		i = -i;
	}
	if ((q = i / r) != 0)
		eputi(q, r);
	eputc(i % r + '0');
}

/*
 * Put long, in radix "r".
 */
static void
eputl(long l, int r)
{
	long	 q;

	if (l < 0) {
		eputc('-');
		l = -l;
	}
	if ((q = l / r) != 0)
		eputl(q, r);
	eputc((int)(l % r) + '0');
}

/*
 * Put string.
 */
static void
eputs(const char *s)
{
	int	 c;

	while ((c = *s++) != '\0')
		eputc(c);
}

/*
 * Put character.  Watch for control characters, and for the line getting
 * too long.
 */
static void
eputc(char c)
{
	if (ttcol + 2 < ncol) {
		if (ISCTRL(c)) {
			eputc('^');
			c = CCHR(c);
		}
		ttputc(c);
		++ttcol;
	}
}

void
free_file_list(struct list *lp)
{
	struct list	*next;

	while (lp) {
		next = lp->l_next;
		free(lp->l_name);
		free(lp);
		lp = next;
	}
}

static struct list *
copy_list(struct list *lp)
{
	struct list	*current, *last, *nxt;

	last = NULL;
	while (lp) {
		current = malloc(sizeof(struct list));
		if (current == NULL) {
			/* Free what we have allocated so far */
			for (current = last; current; current = nxt) {
				nxt = current->l_next;
				free(current->l_name);
				free(current);
			}
			return (NULL);
		}
		current->l_next = last;
		current->l_name = strdup(lp->l_name);
		last = current;
		lp = lp->l_next;
	}
	return (last);
}
