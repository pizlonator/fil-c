/*	$OpenBSD: word.c,v 1.20 2022/12/26 19:16:02 jmc Exp $	*/

/* This file is in the public domain. */

/*
 *		Word mode commands.
 * The routines in this file implement commands that work word at a time.
 * There are all sorts of word mode commands.
 */

#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"

RSIZE	countfword(void);
int	grabword(char **);

/*
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "backchar" and "forwchar" routines.
 */
int
backword(int f, int n)
{
	if (n < 0)
		return (forwword(f | FFRAND, -n));
	if (backchar(FFRAND, 1) == FALSE)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		while (inword() != FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (forwchar(FFRAND, 1));
}

/*
 * Move the cursor forward by the specified number of words.  All of the
 * motion is done by "forwchar".
 */
int
forwword(int f, int n)
{
	if (n < 0)
		return (backword(f | FFRAND, -n));
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		while (inword() != FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (TRUE);
}

/*
 * Transpose 2 words. 
 * The function below is artificially restricted to only a maximum of 1 iteration
 * at the moment because the 'undo' functionality within mg needs amended for
 * multiple movements of point, backwards and forwards.
 */
int
transposeword(int f, int n)
{
	struct line	*tmp1_w_dotp = NULL;
	struct line	*tmp2_w_dotp = NULL;
	int		 tmp2_w_doto = 0;
	int		 tmp1_w_dotline = 0;
	int		 tmp2_w_dotline = 0;
	int		 tmp1_w_doto;
	int		 i;		/* start-of-line space counter */
	int		 ret, s;
	int		 newline;
	int		 leave = 0;
	int		 tmp_len;
	char		*word1 = NULL;
	char		*word2 = NULL;
	char		*chr;

	if (n == 0)
		return (TRUE);

	n = 1; /* remove this line to allow muliple-iterations */

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		dobeep();
		ewprintf("Buffer is read-only");
		return (FALSE);
	}
	undo_boundary_enable(FFRAND, 0);

	/* go backwards to find the start of a word to transpose. */
	(void)backword(FFRAND, 1);
	ret = grabword(&word1);
	if (ret == ABORT) {
		ewprintf("No word to the left to tranpose.");
		return (FALSE);
	}
	if (ret < 0) {
		dobeep();
		ewprintf("Error copying word: %s", strerror(ret));
		free(word1);
		return (FALSE);
	}

	while (n-- > 0) {
		i = 0;
		newline = 0;

		tmp1_w_doto = curwp->w_doto;
		tmp1_w_dotline = curwp->w_dotline;
		tmp1_w_dotp = curwp->w_dotp;

		/* go forward and find next word. */
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE) {
				leave = 1;
				if (tmp1_w_dotline < curwp->w_dotline)
					curwp->w_dotline--;
				ewprintf("Don't have two things to transpose");
				break;
			}
			if (curwp->w_doto == 0) {
				newline = 1;
				i = 0;
			} else if (newline)
				i++;
		}
		if (leave) {
			tmp2_w_doto = tmp1_w_doto;
			tmp2_w_dotline = tmp1_w_dotline;
			tmp2_w_dotp = tmp1_w_dotp;
			break;
		}
		tmp2_w_doto = curwp->w_doto;
		tmp2_w_dotline = curwp->w_dotline;
		tmp2_w_dotp = curwp->w_dotp;

		ret = grabword(&word2);
		if (ret < 0 || ret == ABORT) {
			dobeep();
			ewprintf("Error copying word: %s", strerror(ret));
			free(word1);
			return (FALSE);
		}
		tmp_len = strlen(word2);
		tmp2_w_doto += tmp_len;

		curwp->w_doto = tmp1_w_doto;
		curwp->w_dotline = tmp1_w_dotline;
		curwp->w_dotp = tmp1_w_dotp;

		/* insert shuffled along word */
		for (chr = word2; *chr != '\0'; ++chr)
			linsert(1, *chr);

		if (newline)
			tmp2_w_doto = i;

		curwp->w_doto = tmp2_w_doto;
		curwp->w_dotline = tmp2_w_dotline;
		curwp->w_dotp = tmp2_w_dotp;

		word2 = NULL;
	}
	curwp->w_doto = tmp2_w_doto;
	curwp->w_dotline = tmp2_w_dotline;
	curwp->w_dotp = tmp2_w_dotp;

	/* insert very first word in its new position */
	for (chr = word1; *chr != '\0'; ++chr)
		linsert(1, *chr);

	if (leave)
		(void)backword(FFRAND, 1);

	free(word1);
	free(word2);

	undo_boundary_enable(FFRAND, 1);

	return (TRUE);
}

/*
 * copy and delete word.
*/
int
grabword(char **word)
{
	int c;

	while (inword() == TRUE) {
		c = lgetc(curwp->w_dotp, curwp->w_doto);
		if (*word == NULL) {
			if (asprintf(word, "%c", c) == -1)
				return (errno);
		} else {
			if (asprintf(word, "%s%c", *word, c) == -1)
				return (errno);
		}
		(void)forwdel(FFRAND, 1);
	}
	if (*word == NULL)
		return (ABORT);
	return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words.  As you move,
 * convert any characters to upper case.
 */
int
upperword(int f, int n)
{
	int	c, s;
	RSIZE	size;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		dobeep();
		ewprintf("Buffer is read-only");
		return (FALSE);
	}

	if (n < 0)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		size = countfword();
		undo_add_change(curwp->w_dotp, curwp->w_doto, size);

		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISLOWER(c) != FALSE) {
				c = TOUPPER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFFULL);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words.  As you move
 * convert characters to lower case.
 */
int
lowerword(int f, int n)
{
	int	c, s;
	RSIZE	size;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		dobeep();
		ewprintf("Buffer is read-only");
		return (FALSE);
	}
	if (n < 0)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		size = countfword();
		undo_add_change(curwp->w_dotp, curwp->w_doto, size);

		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISUPPER(c) != FALSE) {
				c = TOLOWER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFFULL);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words.  As you move
 * convert the first character of the word to upper case, and subsequent
 * characters to lower case.  Error if you try to move past the end of the
 * buffer.
 */
int
capword(int f, int n)
{
	int	c, s;
	RSIZE	size;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		dobeep();
		ewprintf("Buffer is read-only");
		return (FALSE);
	}

	if (n < 0)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		size = countfword();
		undo_add_change(curwp->w_dotp, curwp->w_doto, size);

		if (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISLOWER(c) != FALSE) {
				c = TOUPPER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFFULL);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
			while (inword() != FALSE) {
				c = lgetc(curwp->w_dotp, curwp->w_doto);
				if (ISUPPER(c) != FALSE) {
					c = TOLOWER(c);
					lputc(curwp->w_dotp, curwp->w_doto, c);
					lchange(WFFULL);
				}
				if (forwchar(FFRAND, 1) == FALSE)
					return (TRUE);
			}
		}
	}
	return (TRUE);
}

/*
 * Count characters in word, from current position
 */
RSIZE
countfword()
{
	RSIZE		 size;
	struct line	*dotp;
	int		 doto;

	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	size = 0;

	while (inword() != FALSE) {
		if (forwchar(FFRAND, 1) == FALSE)
			/* hit the end of the buffer */
			goto out;
		++size;
	}
out:
	curwp->w_dotp = dotp;
	curwp->w_doto = doto;
	return (size);
}


/*
 * Kill forward by "n" words.
 */
int
delfword(int f, int n)
{
	RSIZE		 size;
	struct line	*dotp;
	int		 doto;
	int s;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		dobeep();
		ewprintf("Buffer is read-only");
		return (FALSE);
	}
	if (n < 0)
		return (FALSE);

	/* purge kill buffer */
	if ((lastflag & CFKILL) == 0)
		kdelete();

	thisflag |= CFKILL;
	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	size = 0;

	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				/* hit the end of the buffer */
				goto out;
			++size;
		}
		while (inword() != FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				/* hit the end of the buffer */
				goto out;
			++size;
		}
	}
out:
	curwp->w_dotp = dotp;
	curwp->w_doto = doto;
	return (ldelete(size, KFORW));
}

/*
 * Kill backwards by "n" words.  The rules for success and failure are now
 * different, to prevent strange behavior at the start of the buffer.  The
 * command only fails if something goes wrong with the actual delete of the
 * characters.  It is successful even if no characters are deleted, or if you
 * say delete 5 words, and there are only 4 words left.  I considered making
 * the first call to "backchar" special, but decided that that would just be
 * weird. Normally this is bound to "M-Rubout" and to "M-Backspace".
 */
int
delbword(int f, int n)
{
	RSIZE	size;
	int s;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		dobeep();
		ewprintf("Buffer is read-only");
		return (FALSE);
	}

	if (n < 0)
		return (FALSE);

	/* purge kill buffer */
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	if (backchar(FFRAND, 1) == FALSE)
		/* hit buffer start */
		return (TRUE);

	/* one deleted */
	size = 1;
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				/* hit buffer start */
				goto out;
			++size;
		}
		while (inword() != FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				/* hit buffer start */
				goto out;
			++size;
		}
	}
	if (forwchar(FFRAND, 1) == FALSE)
		return (FALSE);

	/* undo assumed delete */
	--size;
out:
	return (ldelete(size, KBACK));
}

/*
 * Return TRUE if the character at dot is a character that is considered to be
 * part of a word. The word character list is hard coded. Should be settable.
 */
int
inword(void)
{
	/* can't use lgetc in ISWORD due to bug in OSK cpp */
	return (curwp->w_doto != llength(curwp->w_dotp) &&
	    ISWORD(curwp->w_dotp->l_text[curwp->w_doto]));
}
