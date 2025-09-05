/* $OpenBSD: buffer.c,v 1.114 2023/04/21 13:39:36 op Exp $ */

/* This file is in the public domain. */

/*
 *		Buffer handling.
 */

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "kbd.h"		/* needed for modes */

#define DIFFTOOL "/usr/bin/diff"

static struct buffer  *makelist(void);
static struct buffer *bnew(const char *);

static int usebufname(const char *);

/* Default tab width */
int	 defb_tabw = 8;

/*
 * Set the tab width for the current buffer, or the default for new
 * buffers if called with a prefix argument.
 */
int
settabw(int f, int n)
{
	char	buf[8], *bufp;
	const char *errstr;

	if (f & FFARG) {
		if (n <= 0 || n > 16)
			return (FALSE);
		defb_tabw = n;
		return (TRUE);
	}

	if ((bufp = eread("Tab Width: ", buf, sizeof(buf),
	    EFNUL | EFNEW | EFCR)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (ABORT);
	n = strtonum(buf, 1, 16, &errstr);
	if (errstr)
		return (dobeep_msgs("Tab width", errstr));
	curbp->b_tabw = n;
	curwp->w_rflag |= WFFRAME;
	return (TRUE);
}

int
togglereadonlyall(int f, int n)
{
	struct buffer *bp = NULL;
	int len = 0;

	allbro = !allbro;
	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		len = strlen(bp->b_bname);
		if (bp->b_bname[0] != '*' && bp->b_bname[len - 1] != '*') {
			if (allbro)
				bp->b_flag |= BFREADONLY;
			else
				bp->b_flag &= ~BFREADONLY;
		}
	}
	curwp->w_rflag |= WFMODE;

	return (TRUE);
}

int
togglereadonly(int f, int n)
{
	int s;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (!(curbp->b_flag & BFREADONLY))
		curbp->b_flag |= BFREADONLY;
	else {
		curbp->b_flag &= ~BFREADONLY;
		if (curbp->b_flag & BFCHG)
			ewprintf("Warning: Buffer was modified");
	}
	curwp->w_rflag |= WFMODE;

	return (TRUE);
}

/* Switch to the named buffer.
 * If no name supplied, switch to the default (alternate) buffer.
 */
int
usebufname(const char *bufp)
{
	struct buffer *bp = NULL;
	int rc;

	if (bufp == NULL) {
		if ((bp = bfind("*scratch*", TRUE)) == NULL)
			return(FALSE);
	} else if (bufp[0] == '\0' && curbp->b_altb != NULL)
			bp = curbp->b_altb;
	else if ((bp = bfind(bufp, TRUE)) == NULL)
		return (FALSE);

	/* and put it in current window */
	curbp = bp;
	rc = showbuffer(bp, curwp, WFFRAME | WFFULL);
	eerase();

	return rc;
}

/*
 * Attach a buffer to a window. The values of dot and mark come
 * from the buffer if the use count is 0. Otherwise, they come
 * from some other window.  *scratch* is the default alternate
 * buffer.
 */
int
usebuffer(int f, int n)
{
	char    bufn[NBUFN], *bufp;

	/* Get buffer to use from user */
	if (curbp->b_altb == NULL)
		bufp = eread("Switch to buffer: ", bufn, NBUFN, EFNEW | EFBUF);
	else
		bufp = eread("Switch to buffer (default %s): ", bufn, NBUFN,
		    EFNUL | EFNEW | EFBUF, curbp->b_altb->b_bname);

	if (bufp == NULL)
		return (ABORT);

	return (usebufname(bufp));
}

/*
 * pop to buffer asked for by the user.
 */
int
poptobuffer(int f, int n)
{
	struct buffer *bp;
	struct mgwin  *wp;
	char    bufn[NBUFN], *bufp;

	/* Get buffer to use from user */
	if ((curbp->b_altb == NULL) &&
	    ((curbp->b_altb = bfind("*scratch*", TRUE)) == NULL))
		bufp = eread("Switch to buffer in other window: ", bufn, NBUFN,
		    EFNEW | EFBUF);
	else
		bufp = eread("Switch to buffer in other window (default %s): ",
		    bufn, NBUFN, EFNUL | EFNEW | EFBUF, curbp->b_altb->b_bname);
	if (bufp == NULL)
		return (ABORT);
	if (bufp[0] == '\0' && curbp->b_altb != NULL)
		bp = curbp->b_altb;
	else if ((bp = bfind(bufp, TRUE)) == NULL)
		return (FALSE);
	if (bp == curbp)
		return (splitwind(f, n));
	/* and put it in a new, non-ephemeral window */
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	return (TRUE);
}

/*
 * Dispose of a buffer, by name.
 * Ask for the name (unless called by dired mode). Look it up (don't
 * get too upset if it isn't there at all!). Clear the buffer (ask
 * if the buffer has been changed). Then free the header
 * line and the buffer header. Bound to "C-x k".
 */
int
killbuffer_cmd(int f, int n)
{
	char    bufn[NBUFN], *bufp;
	struct buffer *bp;
	int rc;

	if (f & FFRAND) /* dired mode 'q' */
		bp = curbp;
	else if ((bufp = eread("Kill buffer (default %s): ", bufn, NBUFN,
	    EFNUL | EFNEW | EFBUF, curbp->b_bname)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		bp = curbp;
	else if ((bp = bfind(bufp, FALSE)) == NULL)
		return (FALSE);
	rc = killbuffer(bp);
	eerase();

	return rc;
}

int
killbuffer(struct buffer *bp)
{
	struct buffer *bp1;
	struct buffer *bp2;
	struct mgwin  *wp;
	int s;
	struct undo_rec *rec;

	/*
	 * Find some other buffer to display. Try the alternate buffer,
	 * then the first different buffer in the buffer list.  If there's
	 * only one buffer, create buffer *scratch* and make it the alternate
	 * buffer.  Return if *scratch* is only buffer...
	 */
	if ((bp1 = bp->b_altb) == NULL) {
		/* only one buffer. see if it's *scratch* */
		if (bp == bfind("*scratch*", FALSE)) {
			/*
			 * Actually clear *scratch* buffer
			 * Note: emacs does not do this
			 */
			if ((s = bclear(bp)) != TRUE)
				return (s);

			/*
			 * Not really kosher, breaks the abstraction model
			 * to call redraw() from the buffer handling, but
			 * if we don't the old buffer contents will remain
			 * until the next update.
			 */
			redraw(0,0);

			return (TRUE);
		}
		/* create *scratch* for alternate buffer */
		if ((bp1 = bfind("*scratch*", TRUE)) == NULL)
			return (FALSE);
	}
	if ((s = bclear(bp)) != TRUE)
		return (s);
	for (wp = wheadp; bp->b_nwnd > 0; wp = wp->w_wndp) {
		if (wp->w_bufp == bp) {
			bp2 = bp1->b_altb;	/* save alternate buffer */
			if (showbuffer(bp1, wp, WFMODE | WFFRAME | WFFULL))
				bp1->b_altb = bp2;
			else
				bp1 = bp2;
		}
	}
	if (bp == curbp)
		curbp = bp1;
	free(bp->b_headp);			/* Release header line.  */
	bp2 = NULL;				/* Find the header.	 */
	bp1 = bheadp;
	while (bp1 != bp) {
		if (bp1->b_altb == bp)
			bp1->b_altb = (bp->b_altb == bp1) ? NULL : bp->b_altb;
		bp2 = bp1;
		bp1 = bp1->b_bufp;
	}
	bp1 = bp1->b_bufp;			/* Next one in chain.	 */
	if (bp2 == NULL)			/* Unlink it.		 */
		bheadp = bp1;
	else
		bp2->b_bufp = bp1;
	while (bp1 != NULL) {			/* Finish with altb's	 */
		if (bp1->b_altb == bp)
			bp1->b_altb = (bp->b_altb == bp1) ? NULL : bp->b_altb;
		bp1 = bp1->b_bufp;
	}

	while ((rec = TAILQ_FIRST(&bp->b_undo))) {
		TAILQ_REMOVE(&bp->b_undo, rec, next);
		free_undo_record(rec);
	}

	free(bp->b_bname);			/* Release name block	 */
	free(bp);				/* Release buffer block */

	return (TRUE);
}

/*
 * Save some buffers - just call anycb with the arg flag.
 */
int
savebuffers(int f, int n)
{
	if (anycb(f) == ABORT)
		return (ABORT);
	return (TRUE);
}

/*
 * Listing buffers.
 */
static int listbuf_ncol;

static int	listbuf_goto_buffer(int f, int n);
static int	listbuf_goto_buffer_one(int f, int n);
static int	listbuf_goto_buffer_helper(int f, int n, int only);

static PF listbuf_pf[] = {
	listbuf_goto_buffer
};
static PF listbuf_one[] = {
	listbuf_goto_buffer_one
};


static struct KEYMAPE (2) listbufmap = {
	2,
	2,
	rescan,
	{
		{
			CCHR('M'), CCHR('M'), listbuf_pf, NULL
		},
		{
			'1', '1', listbuf_one, NULL
		}
	}
};

/*
 * Display the buffer list. This is done
 * in two parts. The "makelist" routine figures out
 * the text, and puts it in a buffer. "popbuf"
 * then pops the data onto the screen. Bound to
 * "C-x C-b".
 */
int
listbuffers(int f, int n)
{
	static int		 initialized = 0;
	struct buffer		*bp;
	struct mgwin		*wp;

	if (!initialized) {
		maps_add((KEYMAP *)&listbufmap, "listbufmap");
		initialized = 1;
	}

	if ((bp = makelist()) == NULL || (wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	wp->w_dotp = bp->b_dotp; /* fix up if window already on screen */
	wp->w_doto = bp->b_doto;
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("listbufmap");
	bp->b_nmodes = 1;

	return (TRUE);
}

/*
 * This routine rebuilds the text for the
 * list buffers command. Return pointer
 * to new list if everything works.
 * Return NULL if there is an error (if
 * there is no memory).
 */
static struct buffer *
makelist(void)
{
	int		w = ncol / 2;
	struct buffer	*bp, *blp;
	struct line	*lp;

	if ((blp = bfind("*Buffer List*", TRUE)) == NULL)
		return (NULL);
	if (bclear(blp) != TRUE)
		return (NULL);
	blp->b_flag &= ~BFCHG;		/* Blow away old.	 */
	blp->b_flag |= BFREADONLY;

	listbuf_ncol = ncol;		/* cache ncol for listbuf_goto_buffer */

	if (addlinef(blp, "%-*s%s", w, " MR Buffer", "Size   File") == FALSE ||
	    addlinef(blp, "%-*s%s", w, " -- ------", "----   ----") == FALSE)
		return (NULL);

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		RSIZE nbytes;

		nbytes = 0;			/* Count bytes in buf.	 */
		if (bp != blp) {
			lp = bfirstlp(bp);
			while (lp != bp->b_headp) {
				nbytes += llength(lp) + 1;
				lp = lforw(lp);
			}
			if (nbytes)
				nbytes--;	/* no bonus newline	 */
		}

		if (addlinef(blp, "%c%c%c %-*.*s%c%-6d %-*s",
		    (bp == curbp) ? '>' : ' ',	/* current buffer ? */
		    ((bp->b_flag & BFCHG) != 0) ? '*' : ' ',	/* changed ? */
		    ((bp->b_flag & BFREADONLY) != 0) ? '*' : ' ',
		    w - 5,		/* four chars already written */
		    w - 5,		/* four chars already written */
		    bp->b_bname,	/* buffer name */
		    (int)strlen(bp->b_bname) < w - 5 ? ' ' : '$', /* truncated? */
		    nbytes,		/* buffer size */
		    w - 7,		/* seven chars already written */
		    bp->b_fname) == FALSE)
			return (NULL);
	}
	blp->b_dotp = bfirstlp(blp);		/* put dot at beginning of
						 * buffer */
	blp->b_doto = 0;
	return (blp);				/* All done		 */
}

static int
listbuf_goto_buffer(int f, int n)
{
	return (listbuf_goto_buffer_helper(f, n, 0));
}

static int
listbuf_goto_buffer_one(int f, int n)
{
	return (listbuf_goto_buffer_helper(f, n, 1));
}

static int
listbuf_goto_buffer_helper(int f, int n, int only)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	char		*line = NULL;
	int		 i, ret = FALSE;

	if (curwp->w_dotp->l_text[listbuf_ncol/2 - 1] == '$')
		return(dobeep_msg("buffer name truncated"));

	if ((line = malloc(listbuf_ncol/2)) == NULL)
		return (FALSE);

	memcpy(line, curwp->w_dotp->l_text + 4, listbuf_ncol/2 - 5);
	for (i = listbuf_ncol/2 - 6; i > 0; i--) {
		if (line[i] != ' ') {
			line[i + 1] = '\0';
			break;
		}
	}
	if (i == 0)
		goto cleanup;

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (strcmp(bp->b_bname, line) == 0)
			break;
	}
	if (bp == NULL)
		goto cleanup;

	if ((wp = popbuf(bp, WNONE)) == NULL)
		goto cleanup;
	curbp = bp;
	curwp = wp;

	if (only)
		ret = (onlywind(FFRAND, 1));
	else
		ret = TRUE;

cleanup:
	free(line);

	return (ret);
}

/*
 * The argument "fmt" points to a format string.  Append this line to the
 * buffer. Handcraft the EOL on the end.  Return TRUE if it worked and
 * FALSE if you ran out of room.
 */
int
addlinef(struct buffer *bp, char *fmt, ...)
{
	va_list		 ap;
	struct line	*lp;

	if ((lp = lalloc(0)) == NULL)
		return (FALSE);
	va_start(ap, fmt);
	if (vasprintf(&lp->l_text, fmt, ap) == -1) {
		lfree(lp);
		va_end(ap);
		return (FALSE);
	}
	lp->l_used = strlen(lp->l_text);
	va_end(ap);

	bp->b_headp->l_bp->l_fp = lp;		/* Hook onto the end	 */
	lp->l_bp = bp->b_headp->l_bp;
	bp->b_headp->l_bp = lp;
	lp->l_fp = bp->b_headp;
	bp->b_lines++;

	return (TRUE);
}

/*
 * Look through the list of buffers, giving the user a chance to save them.
 * Return TRUE if there are any changed buffers afterwards.  Buffers that don't
 * have an associated file don't count.  Return FALSE if there are no changed
 * buffers.  Return ABORT if an error occurs or if the user presses c-g.
 */
int
anycb(int f)
{
	struct buffer	*bp;
	int		 s = FALSE, save = FALSE, save2 = FALSE, ret;
	char		 pbuf[NFILEN + 11];

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (*(bp->b_fname) != '\0' && (bp->b_flag & BFCHG) != 0) {
			ret = snprintf(pbuf, sizeof(pbuf), "Save file %s",
			    bp->b_fname);
			if (ret < 0 || ret >= (int)sizeof(pbuf)) {
				(void)dobeep_msg("Error: filename too long!");
				return (UERROR);
			}
			if ((f == TRUE || (save = eyorn(pbuf)) == TRUE) &&
			    (save2 = buffsave(bp)) == TRUE) {
				bp->b_flag &= ~BFCHG;
				upmodes(bp);
			} else {
				if (save2 == FIOERR)
					return (save2);
				s = TRUE;
			}
			if (save == ABORT)
				return (save);
			save = TRUE;
		}
	}
	if (save == FALSE /* && kbdmop == NULL */ )	/* experimental */
		ewprintf("(No files need saving)");
	return (s);
}

/*
 * Search for a buffer, by name.
 * If not found, and the "cflag" is TRUE,
 * create a new buffer. Return pointer to the found
 * (or new) buffer.
 */
struct buffer *
bfind(const char *bname, int cflag)
{
	struct buffer	*bp;

	bp = bheadp;
	while (bp != NULL) {
		if (strcmp(bname, bp->b_bname) == 0)
			return (bp);
		bp = bp->b_bufp;
	}
	if (cflag != TRUE)
		return (NULL);

	bp = bnew(bname);

	return (bp);
}

/*
 * Create a new buffer and put it in the list of
 * all buffers.
 */
static struct buffer *
bnew(const char *bname)
{
	struct buffer	*bp;
	struct line	*lp;
	int		 i;
	size_t		len;

	bp = calloc(1, sizeof(struct buffer));
	if (bp == NULL) {
		dobeep();
		ewprintf("Can't get %d bytes", sizeof(struct buffer));
		return (NULL);
	}
	if ((lp = lalloc(0)) == NULL) {
		free(bp);
		return (NULL);
	}
	bp->b_altb = bp->b_bufp = NULL;
	bp->b_dotp = lp;
	bp->b_doto = 0;
	bp->b_markp = NULL;
	bp->b_marko = 0;
	bp->b_flag = defb_flag;
	/* if buffer name starts and ends with '*', we ignore changes */
	len = strlen(bname);
	if (len) {
		if (bname[0] == '*' && bname[len - 1] == '*')
			bp->b_flag |= BFIGNDIRTY;
	}
	bp->b_nwnd = 0;
	bp->b_headp = lp;
	bp->b_nmodes = defb_nmodes;
	TAILQ_INIT(&bp->b_undo);
	bp->b_undoptr = NULL;
	i = 0;
	do {
		bp->b_modes[i] = defb_modes[i];
	} while (i++ < defb_nmodes);
	bp->b_fname[0] = '\0';
	bp->b_cwd[0] = '\0';
	bzero(&bp->b_fi, sizeof(bp->b_fi));
	lp->l_fp = lp;
	lp->l_bp = lp;
	bp->b_bufp = bheadp;
	bheadp = bp;
	bp->b_dotline = bp->b_markline = 1;
	bp->b_lines = 1;
	bp->b_nlseq = "\n";		/* use unix default */
	bp->b_nlchr = bp->b_nlseq;
	bp->b_tabw = defb_tabw;
	if ((bp->b_bname = strdup(bname)) == NULL) {
		dobeep();
		ewprintf("Can't get %d bytes", strlen(bname) + 1);
		return (NULL);
	}

	return (bp);
}

/*
 * This routine blows away all of the text
 * in a buffer. If the buffer is marked as changed
 * then we ask if it is ok to blow it away; this is
 * to save the user the grief of losing text. The
 * window chain is nearly always wrong if this gets
 * called; the caller must arrange for the updates
 * that are required. Return TRUE if everything
 * looks good.
 */
int
bclear(struct buffer *bp)
{
	struct line	*lp;
	int		 s;

	/* Has buffer changed, and do we care? */
	if (!(bp->b_flag & BFIGNDIRTY) && (bp->b_flag & BFCHG) != 0 &&
	    (s = eyesno("Buffer modified; kill anyway")) != TRUE)
		return (s);
	bp->b_flag &= ~BFCHG;	/* Not changed		 */
	while ((lp = lforw(bp->b_headp)) != bp->b_headp)
		lfree(lp);
	bp->b_dotp = bp->b_headp;	/* Fix dot */
	bp->b_doto = 0;
	bp->b_markp = NULL;	/* Invalidate "mark"	 */
	bp->b_marko = 0;
	bp->b_dotline = bp->b_markline = 1;
	bp->b_lines = 1;

	return (TRUE);
}

/*
 * Display the given buffer in the given window. Flags indicated
 * action on redisplay. Update modified flag so insert loop can check it.
 */
int
showbuffer(struct buffer *bp, struct mgwin *wp, int flags)
{
	struct buffer	*obp;
	struct mgwin	*owp;

	/* Ensure file has not been modified elsewhere */
	if (fchecktime(bp) != TRUE)
		bp->b_flag |= BFDIRTY;

	if (wp->w_bufp == bp) {	/* Easy case! */
		wp->w_rflag |= flags;
		return (TRUE);
	}
	/* First, detach the old buffer from the window */
	if ((bp->b_altb = obp = wp->w_bufp) != NULL) {
		if (--obp->b_nwnd == 0) {
			obp->b_dotp = wp->w_dotp;
			obp->b_doto = wp->w_doto;
			obp->b_markp = wp->w_markp;
			obp->b_marko = wp->w_marko;
			obp->b_dotline = wp->w_dotline;
			obp->b_markline = wp->w_markline;
		}
	}
	/* Now, attach the new buffer to the window */
	wp->w_bufp = bp;

	if (bp->b_nwnd++ == 0) {	/* First use.		 */
		wp->w_dotp = bp->b_dotp;
		wp->w_doto = bp->b_doto;
		wp->w_markp = bp->b_markp;
		wp->w_marko = bp->b_marko;
		wp->w_dotline = bp->b_dotline;
		wp->w_markline = bp->b_markline;
	} else
		/* already on screen, steal values from other window */
		for (owp = wheadp; owp != NULL; owp = wp->w_wndp)
			if (wp->w_bufp == bp && owp != wp) {
				wp->w_dotp = owp->w_dotp;
				wp->w_doto = owp->w_doto;
				wp->w_markp = owp->w_markp;
				wp->w_marko = owp->w_marko;
				wp->w_dotline = owp->w_dotline;
				wp->w_markline = owp->w_markline;
				break;
			}
	wp->w_rflag |= WFMODE | flags;

	return (TRUE);
}

/*
 * Augment a buffer name with a number, if necessary
 *
 * If more than one file of the same basename() is open,
 * the additional buffers are named "file<2>", "file<3>", and
 * so forth.  This function adjusts a buffer name to
 * include the number, if necessary.
 */
int
augbname(char *bn, const char *fn, size_t bs)
{
	int	 count;
	size_t	 remain, len;

	if ((len = xbasename(bn, fn, bs)) >= bs)
		return (FALSE);

	remain = bs - len;
	for (count = 2; bfind(bn, FALSE) != NULL; count++)
		snprintf(bn + len, remain, "<%d>", count);

	return (TRUE);
}

/*
 * Pop the buffer we got passed onto the screen.
 * Returns a status.
 */
struct mgwin *
popbuf(struct buffer *bp, int flags)
{
	struct mgwin	*wp;

	if (bp->b_nwnd == 0) {	/* Not on screen yet.	 */
		/*
		 * Pick a window for a pop-up.
		 * If only one window, split the screen.
		 * Flag the new window as ephemeral
		 */
		if (wheadp->w_wndp == NULL &&
		    splitwind(FFOTHARG, flags) == FALSE)
 			return (NULL);

		/*
		 * Pick the uppermost window that isn't
		 * the current window. An LRU algorithm
		 * might be better. Return a pointer, or NULL on error.
		 */
		wp = wheadp;

		while (wp != NULL && wp == curwp)
			wp = wp->w_wndp;
	} else {
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
			if (wp->w_bufp == bp) {
				wp->w_rflag |= WFFULL | WFFRAME;
				return (wp);
			}
		}
	}

	if (!wp)
		return (NULL);

	if (showbuffer(bp, wp, WFFULL) != TRUE)
		return (NULL);

	return (wp);
}

/*
 * Insert another buffer at dot.  Very useful.
 */
int
bufferinsert(int f, int n)
{
	struct buffer *bp;
	struct line   *clp;
	int	clo, nline;
	char	bufn[NBUFN], *bufp;

	/* Get buffer to use from user */
	if (curbp->b_altb != NULL)
		bufp = eread("Insert buffer (default %s): ", bufn, NBUFN,
		    EFNUL | EFNEW | EFBUF, curbp->b_altb->b_bname);
	else
		bufp = eread("Insert buffer: ", bufn, NBUFN, EFNEW | EFBUF);
	if (bufp == NULL)
		return (ABORT);
	if (bufp[0] == '\0' && curbp->b_altb != NULL)
		bp = curbp->b_altb;
	else if ((bp = bfind(bufp, FALSE)) == NULL)
		return (FALSE);

	if (bp == curbp)
		return(dobeep_msg("Cannot insert buffer into self"));

	/* insert the buffer */
	nline = 0;
	clp = bfirstlp(bp);
	for (;;) {
		for (clo = 0; clo < llength(clp); clo++)
			if (linsert(1, lgetc(clp, clo)) == FALSE)
				return (FALSE);
		if ((clp = lforw(clp)) == bp->b_headp)
			break;
		if (enewline(FFRAND, 1) == FALSE)	/* fake newline */
			return (FALSE);
		nline++;
	}
	if (nline == 1)
		ewprintf("[Inserted 1 line]");
	else
		ewprintf("[Inserted %d lines]", nline);

	clp = curwp->w_linep;		/* cosmetic adjustment	*/
	if (curwp->w_dotp == clp) {	/* for offscreen insert */
		while (nline-- && lback(clp) != curbp->b_headp)
			clp = lback(clp);
		curwp->w_linep = clp;	/* adjust framing.	*/
		curwp->w_rflag |= WFFULL;
	}
	return (TRUE);
}

/*
 * Turn off the dirty bit on this buffer.
 */
int
notmodified(int f, int n)
{
	struct mgwin *wp;

	curbp->b_flag &= ~BFCHG;
	wp = wheadp;		/* Update mode lines.	 */
	while (wp != NULL) {
		if (wp->w_bufp == curbp)
			wp->w_rflag |= WFMODE;
		wp = wp->w_wndp;
	}
	ewprintf("Modification-flag cleared");
	return (TRUE);
}

/*
 * Popbuf and set all windows to top of buffer.
 */
int
popbuftop(struct buffer *bp, int flags)
{
	struct mgwin *wp;

	bp->b_dotp = bfirstlp(bp);
	bp->b_doto = 0;
	if (bp->b_nwnd != 0) {
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
			if (wp->w_bufp == bp) {
				wp->w_dotp = bp->b_dotp;
				wp->w_doto = 0;
				wp->w_rflag |= WFFULL;
			}
	}
	return (popbuf(bp, flags) != NULL);
}

/*
 * Return the working directory for the current buffer, terminated
 * with a '/'. First, try to extract it from the current buffer's
 * filename. If that fails, use global cwd.
 */
int
getbufcwd(char *path, size_t plen)
{
	char cwd[NFILEN];

	if (plen == 0)
		return (FALSE);

	if (!globalwd() && curbp->b_cwd[0] != '\0') {
		(void)strlcpy(path, curbp->b_cwd, plen);
	} else {
		if (getcwdir(cwd, sizeof(cwd)) == FALSE)
			goto error;
		(void)strlcpy(path, cwd, plen);
	}
	return (TRUE);
error:
	path[0] = '\0';
	return (FALSE);
}

/*
 * Ensures a buffer has not been modified elsewhere; e.g. on disk.
 * Prompt the user if it has.
 * Returns TRUE if it has NOT (i.e. buffer is ok to edit).
 * FALSE or ABORT otherwise
 */
int
checkdirty(struct buffer *bp)
{
	int s;

	if ((bp->b_flag & (BFCHG | BFDIRTY)) == 0)
		if (fchecktime(bp) != TRUE)
			bp->b_flag |= BFDIRTY;

	if ((bp->b_flag & (BFDIRTY | BFIGNDIRTY)) == BFDIRTY) {
		s = eynorr("File changed on disk; really edit the buffer");
		switch (s) {
		case TRUE:
			bp->b_flag &= ~BFDIRTY;
			bp->b_flag |= BFIGNDIRTY;
			return (TRUE);
		case REVERT:
			dorevert();
			return (FALSE);
		default:
			return (s);
		}
	}

	return (TRUE);
}

/*
 * Revert the current buffer to whatever is on disk.
 */
int
revertbuffer(int f, int n)
{
	char fbuf[NFILEN + 32];

	if (curbp->b_fname[0] == 0)
		return(dobeep_msg("Cannot revert buffer not associated "
		    "with any files."));

	snprintf(fbuf, sizeof(fbuf), "Revert buffer from file %s",
	    curbp->b_fname);

	if (eyorn(fbuf) == TRUE)
		return dorevert();

	return (FALSE);
}

int
dorevert(void)
{
	int lineno;
	struct undo_rec *rec;
	char fname[NFILEN];

	if (access(curbp->b_fname, F_OK|R_OK) != 0) {
		dobeep();
		if (errno == ENOENT)
			ewprintf("File %s no longer exists!",
			    curbp->b_fname);
		else
			ewprintf("File %s is no longer readable!",
			    curbp->b_fname);
		return (FALSE);
	}

	/* Save our current line, so we can go back after reloading. */
	lineno = curwp->w_dotline;

	/* Prevent readin from asking if we want to kill the buffer. */
	curbp->b_flag &= ~BFCHG;

	/* Clean up undo memory */
	while ((rec = TAILQ_FIRST(&curbp->b_undo))) {
		TAILQ_REMOVE(&curbp->b_undo, rec, next);
		free_undo_record(rec);
	}

	/* don't use curbp->b_fname as src and dist of filename, causes mem overlap */
	strlcpy(fname, curbp->b_fname, sizeof(fname));

	if (readin(fname))
		return(setlineno(lineno));
	return (FALSE);
}

/*
 * Diff the current buffer to what is on disk.
 */
/*ARGSUSED */
int
diffbuffer(int f, int n)
{
	struct buffer	*bp;
	struct line	*lp, *lpend;
	size_t		 len;
	int		 ret;
	char		*text, *ttext;
	char		* const argv[] =
	    {DIFFTOOL, "-u", "-p", curbp->b_fname, "-", (char *)NULL};

	len = 0;

	/* C-u is not supported */
	if (n > 1)
		return (ABORT);

	if (access(DIFFTOOL, X_OK) != 0) {
		dobeep();
		ewprintf("%s not found or not executable.", DIFFTOOL);
		return (FALSE);
	}

	if (curbp->b_fname[0] == 0)
		return(dobeep_msg("Cannot diff buffer not associated with "
		    "any files."));

	lpend = curbp->b_headp;
	for (lp = lforw(lpend); lp != lpend; lp = lforw(lp)) {
		len+=llength(lp);
		if (lforw(lp) != lpend)		/* no implied \n on last line */
			len++;
	}
	if ((text = calloc(len + 1, sizeof(char))) == NULL)
		return(dobeep_msg("Cannot allocate memory."));

	ttext = text;

	for (lp = lforw(lpend); lp != lpend; lp = lforw(lp)) {
		if (llength(lp) != 0) {
			memcpy(ttext, ltext(lp), llength(lp));
			ttext += llength(lp);
		}
		if (lforw(lp) != lpend)		/* no implied \n on last line */
			*ttext++ = *curbp->b_nlchr;
	}

	bp = bfind("*Diff*", TRUE);
	bp->b_flag |= BFREADONLY;
	if (bclear(bp) != TRUE) {
		free(text);
		return (FALSE);
	}

	ret = pipeio(DIFFTOOL, argv, text, len, bp);

	if (ret == TRUE) {
		eerase();
		if (lforw(bp->b_headp) == bp->b_headp)
			addline(bp, "Diff finished (no differences).");
	}

	free(text);
	return (ret);
}

/*
 * Given a file name, either find the buffer it uses, or create a new
 * empty buffer to put it in.
 */
struct buffer *
findbuffer(char *fn)
{
	struct buffer	*bp;
	char		bname[NBUFN], fname[NBUFN];

	if (strlcpy(fname, fn, sizeof(fname)) >= sizeof(fname)) {
		(void)dobeep_msg("filename too long");
		return (NULL);
	}

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (strcmp(bp->b_fname, fname) == 0)
			return (bp);
	}
	/* Not found. Create a new one, adjusting name first */
	if (augbname(bname, fname, sizeof(bname)) == FALSE)
		return (NULL);

	bp = bfind(bname, TRUE);
	return (bp);
}
