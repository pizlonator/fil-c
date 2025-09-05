/*	$OpenBSD: file.c,v 1.102 2019/06/22 15:03:43 lum Exp $	*/

/* This file is in the public domain. */

/*
 *	File commands.
 */

#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"

size_t xdirname(char *, const char *, size_t);

/*
 * Insert a file into the current buffer.  Real easy - just call the
 * insertfile routine with the file name.
 */
int
fileinsert(int f, int n)
{
	char	 fname[NFILEN], *bufp, *adjf;

	if (getbufcwd(fname, sizeof(fname)) != TRUE)
		fname[0] = '\0';
	bufp = eread("Insert file: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE | EFDEF);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	adjf = adjustname(bufp, TRUE);
	if (adjf == NULL)
		return (FALSE);
	return (insertfile(adjf, NULL, FALSE));
}

/*
 * Select a file for editing.  If the file is a directory, invoke dired.
 * Otherwise, look around to see if you can find the file in another buffer;
 * if you can find it, just switch to the buffer.  If you cannot find the
 * file, create a new buffer, read in the text, and switch to the new buffer.
 */
int
filevisit(int f, int n)
{
	struct buffer	*bp;
	char	 fname[NFILEN], *bufp, *adjf;
	int	 status;

	if (getbufcwd(fname, sizeof(fname)) != TRUE)
		fname[0] = '\0';
	bufp = eread("Find file: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE | EFDEF);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	adjf = adjustname(fname, TRUE);
	if (adjf == NULL)
		return (FALSE);
	if (fisdir(adjf) == TRUE)
		return (do_dired(adjf));
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] == '\0') {
		if ((status = readin(adjf)) != TRUE)
			killbuffer(bp);
		return (status);
	}
	return (TRUE);
}

/*
 * Replace the current file with an alternate one. Semantics for finding
 * the replacement file are the same as 'filevisit', except the current
 * buffer is killed before the switch. If the kill fails, or is aborted,
 * revert to the original file.
 */
int
filevisitalt(int f, int n)
{
	char	 fname[NFILEN], *bufp;

	if (getbufcwd(fname, sizeof(fname)) != TRUE)
		fname[0] = '\0';
	bufp = eread("Find alternate file: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE | EFDEF);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	return (do_filevisitalt(fname));
}

int
do_filevisitalt(char *fn)
{
	struct buffer	*bp;
	int		 status;
	char		*adjf;

	status = killbuffer(curbp);
	if (status == ABORT || status == FALSE)
		return (ABORT);

	adjf = adjustname(fn, TRUE);
	if (adjf == NULL)
		return (FALSE);
	if (fisdir(adjf) == TRUE)
		return (do_dired(adjf));
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] == '\0') {
		if ((status = readin(adjf)) != TRUE)
			killbuffer(bp);
		return (status);
	}
	return (TRUE);
}

int
filevisitro(int f, int n)
{
	int error;

	error = filevisit(f, n);
	if (error != TRUE)
		return (error);
	curbp->b_flag |= BFREADONLY;
	return (TRUE);
}

/*
 * Pop to a file in the other window.  Same as the last function, but uses
 * popbuf instead of showbuffer.
 */
int
poptofile(int f, int n)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	char	 fname[NFILEN], *adjf, *bufp;
	int	 status;

	if (getbufcwd(fname, sizeof(fname)) != TRUE)
		fname[0] = '\0';
	if ((bufp = eread("Find file in other window: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE | EFDEF)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	adjf = adjustname(fname, TRUE);
	if (adjf == NULL)
		return (FALSE);
	if (fisdir(adjf) == TRUE)
		return (do_dired(adjf));
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	if (bp == curbp)
		return (splitwind(f, n));
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == '\0') {
		if ((status = readin(adjf)) != TRUE)
			killbuffer(bp);
		return (status);
	}
	return (TRUE);
}

/*
 * Read the file "fname" into the current buffer.  Make all of the text
 * in the buffer go away, after checking for unsaved changes.  This is
 * called by the "read" command, the "visit" command, and the mainline
 * (for "mg file").
 */
int
readin(char *fname)
{
	struct mgwin	*wp;
	struct stat	 statbuf;
	int	 status, ro = FALSE;
#ifdef ENABLE_AUTOEXEC
	PF	*ael;
#endif
	char	 dp[NFILEN];

	/* might be old */
	if (bclear(curbp) != TRUE)
		return (TRUE);
	/* Clear readonly. May be set by autoexec path */
	curbp->b_flag &= ~BFREADONLY;
	if ((status = insertfile(fname, fname, TRUE)) != TRUE) {
		dobeep();
		ewprintf("File is not readable: %s", fname);
		return (FALSE);
	}

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp == curbp) {
			wp->w_dotp = wp->w_linep = bfirstlp(curbp);
			wp->w_doto = 0;
			wp->w_markp = NULL;
			wp->w_marko = 0;
		}
	}

#ifdef ENABLE_AUTOEXEC
	/*
	 * Call auto-executing function if we need to.
	 */
	if ((ael = find_autoexec(fname)) != NULL) {
		int i;

		for (i = 0; ael[i] != NULL; i++)
			(*ael[i])(0, 1);
		free(ael);
	}
#endif

	/* no change */
	curbp->b_flag &= ~BFCHG;

	/*
	 * Set the buffer READONLY flag if any of following are true:
	 *   1. file is a directory.
	 *   2. file is read-only.
	 *   3. file doesn't exist and directory is read-only.
	 */
	if (fisdir(fname) == TRUE) {
		ro = TRUE;
	} else if ((access(fname, W_OK) == -1)) {
		if (errno != ENOENT) {
			ro = TRUE;
		} else {
			(void)xdirname(dp, fname, sizeof(dp));
			(void)strlcat(dp, "/", sizeof(dp));

			/* Missing directory; keep buffer rw, like emacs */
			if (stat(dp, &statbuf) == -1 && errno == ENOENT) {
				if (eyorn("Missing directory, create") == TRUE)
					(void)do_makedir(dp);
			} else if (access(dp, W_OK) == -1 && errno == EACCES) {
				ewprintf("File not found and directory"
				    " write-protected");
				ro = TRUE;
			}
		}
	}
	if (ro == TRUE)
		curbp->b_flag |= BFREADONLY;

	if (startrow) {
		gotoline(FFARG, startrow);
		startrow = 0;
	}

	undo_add_modified();
	return (status);
}

/*
 * NB, getting file attributes is done here under control of a flag
 * rather than in readin, which would be cleaner.  I was concerned
 * that some operating system might require the file to be open
 * in order to get the information.  Similarly for writing.
 */

/*
 * Insert a file in the current buffer, after dot. If file is a directory,
 * and 'replacebuf' is TRUE, invoke dired mode, else die with an error.
 * If file is a regular file, set mark at the end of the text inserted;
 * point at the beginning.  Return a standard status. Print a summary
 * (lines read, error message) out as well. This routine also does the
 * read end of backup processing.  The BFBAK flag, if set in a buffer,
 * says that a backup should be taken.  It is set when a file is read in,
 * but not on a new file. You don't need to make a backup copy of nothing.
 */

static char	*line = NULL;
static int	linesize = 0;

int
insertfile(char *fname, char *newname, int replacebuf)
{
	struct buffer	*bp;
	struct line	*lp1, *lp2;
	struct line	*olp;			/* line we started at */
	struct mgwin	*wp;
	int	 nbytes, s, nline = 0, siz, x, x2;
	int	 pipe = 0;		/* File read from pipe */
	int	 opos;			/* offset we started at */
	int	 oline;			/* original line number */
        FILE    *ffp;

	if (replacebuf == TRUE)
		x = undo_enable(FFRAND, 0);
	else
		x = undo_enabled();

	lp1 = NULL;
	if (line == NULL) {
		line = malloc(NLINE);
		if (line == NULL)
			panic("out of memory");
		linesize = NLINE;
	}

	/* cheap */
	bp = curbp;
	if (newname != NULL) {
		(void)strlcpy(bp->b_fname, newname, sizeof(bp->b_fname));
		(void)xdirname(bp->b_cwd, newname, sizeof(bp->b_cwd));
		(void)strlcat(bp->b_cwd, "/", sizeof(bp->b_cwd));
	}

	/* hard file open */
	s = ffropen(&ffp, fname, (replacebuf == TRUE) ? bp : NULL);
	if (s == FIOERR)
		goto out;
	if (s == FIOFNF) {
		/* file not found */
		if (newname != NULL)
			ewprintf("(New file)");
		else
			ewprintf("(File not found)");
		goto out;
	} else if (s == FIODIR) {
		/* file was a directory */
		(void)ffclose(ffp, NULL);
#ifdef ENABLE_DIRED
		if (replacebuf == FALSE) {
			dobeep();
			ewprintf("Cannot insert: file is a directory, %s", fname);
			goto cleanup;
		}
		killbuffer(bp);
		bp = dired_(fname);
		undo_enable(FFRAND, x);
		if (bp == NULL)
			return (FALSE);
		curbp = bp;
		return (showbuffer(bp, curwp, WFFULL | WFMODE));
#else
		dobeep();
		ewprintf("Cannot insert: file is a directory, %s", fname);
		goto cleanup;
#endif /* ENABLE_DIRED */
	} else if (s == FIOGZIP) {
		pipe = 1;
	} else {
		(void)xdirname(bp->b_cwd, fname, sizeof(bp->b_cwd));
		(void)strlcat(bp->b_cwd, "/", sizeof(bp->b_cwd));
	}
	opos = curwp->w_doto;
	oline = curwp->w_dotline;
	/*
	 * Open a new line at dot and start inserting after it.
	 * We will delete this newline after insertion.
	 * Disable undo, as we create the undo record manually.
	 */
	x2 = undo_enable(FFRAND, 0);
	(void)lnewline();
	olp = lback(curwp->w_dotp);
	undo_enable(FFRAND, x2);

	nline = 0;
	siz = 0;
	while ((s = ffgetline(ffp, line, linesize, &nbytes)) != FIOERR) {
retry:
		siz += nbytes + 1;
		switch (s) {
		case FIOSUC:
			/* FALLTHRU */
		case FIOEOF:
			++nline;
			if ((lp1 = lalloc(nbytes)) == NULL) {
				/* keep message on the display */
				s = FIOERR;
				undo_add_insert(olp, opos,
				    siz - nbytes - 1 - 1);
				goto endoffile;
			}
			bcopy(line, &ltext(lp1)[0], nbytes);
			lp2 = lback(curwp->w_dotp);
			lp2->l_fp = lp1;
			lp1->l_fp = curwp->w_dotp;
			lp1->l_bp = lp2;
			curwp->w_dotp->l_bp = lp1;
			if (s == FIOEOF) {
				undo_add_insert(olp, opos, siz - 1);
				goto endoffile;
			}
			break;
		case FIOLONG: {
				/* a line too long to fit in our buffer */
				char	*cp;
				int	newsize;

				newsize = linesize * 2;
				if (newsize < 0 ||
				    (cp = malloc(newsize)) == NULL) {
					dobeep();
					ewprintf("Could not allocate %d bytes",
					    newsize);
						s = FIOERR;
						goto endoffile;
				}
				bcopy(line, cp, linesize);
				free(line);
				line = cp;
				s = ffgetline(ffp, line + linesize, linesize,
				    &nbytes);
				nbytes += linesize;
				linesize = newsize;
				if (s == FIOERR)
					goto endoffile;
				goto retry;
			}
		default:
			dobeep();
			ewprintf("Unknown code %d reading file", s);
			s = FIOERR;
			break;
		}
	}
endoffile:
	/* ignore errors */
	if (pipe)
		(void)pclose(ffp);
	else
		(void)ffclose(ffp, NULL);

	/* don't zap an error */
	if (s == FIOEOF) {
		if (nline == 1)
			ewprintf("(Read 1 line)");
		else
			ewprintf("(Read %d lines)", nline);
	}
	/* set mark at the end of the text */
	curwp->w_dotp = curwp->w_markp = lback(curwp->w_dotp);
	curwp->w_marko = llength(curwp->w_markp);
	curwp->w_markline = oline + nline + 1;
	/*
	 * if we are at the end of the file, ldelnewline is a no-op,
	 * but we still need to decrement the line and markline counts
	 * as we've accounted for this fencepost in our arithmetic
	 */
	if (lforw(curwp->w_dotp) == curwp->w_bufp->b_headp) {
		curwp->w_bufp->b_lines--;
		curwp->w_markline--;
	} else
		(void)ldelnewline();
	curwp->w_dotp = olp;
	curwp->w_doto = opos;
	curwp->w_dotline = oline;
	if (olp == curbp->b_headp)
		curwp->w_dotp = lforw(olp);
	if (newname != NULL)
		bp->b_flag |= BFCHG | BFBAK;	/* Need a backup.	 */
	else
		bp->b_flag |= BFCHG;
	/*
	 * If the insert was at the end of buffer, set lp1 to the end of
	 * buffer line, and lp2 to the beginning of the newly inserted text.
	 * (Otherwise lp2 is set to NULL.)  This is used below to set
	 * pointers in other windows correctly if they are also at the end of
	 * buffer.
	 */
	lp1 = bp->b_headp;
	if (curwp->w_markp == lp1) {
		lp2 = curwp->w_dotp;
	} else {
		/* delete extraneous newline */
		(void)ldelnewline();
out:		lp2 = NULL;
	}
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp == curbp) {
			wp->w_rflag |= WFMODE | WFEDIT;
			if (wp != curwp && lp2 != NULL) {
				if (wp->w_dotp == lp1)
					wp->w_dotp = lp2;
				if (wp->w_markp == lp1)
					wp->w_markp = lp2;
				if (wp->w_linep == lp1)
					wp->w_linep = lp2;
			}
		}
	}
	bp->b_lines += nline;
cleanup:
	undo_enable(FFRAND, x);
	if (pipe)
		bp->b_flag |= BFREADONLY;

	/* return FALSE if error */
	return (s != FIOERR);
}

/*
 * Ask for a file name and write the contents of the current buffer to that
 * file.  Update the remembered file name and clear the buffer changed flag.
 * This handling of file names is different from the earlier versions and
 * is more compatible with Gosling EMACS than with ITS EMACS.
 */
int
filewrite(int f, int n)
{
	struct stat     statbuf;
	int	 s;
	char	 fname[NFILEN], bn[NBUFN], tmp[NFILEN + 25];
	char	*adjfname, *bufp;
        FILE    *ffp;

	if (getbufcwd(fname, sizeof(fname)) != TRUE)
		fname[0] = '\0';
	if ((bufp = eread("Write file: ", fname, NFILEN,
	    EFDEF | EFNEW | EFCR | EFFILE)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	adjfname = adjustname(fname, TRUE);
	if (adjfname == NULL)
		return (FALSE);

        /* Check if file exists; write checks done later */
        if (stat(adjfname, &statbuf) == 0) {
		if (S_ISDIR(statbuf.st_mode)) {
			dobeep();
			ewprintf("%s is a directory", adjfname);
			return (FALSE);
		}
		snprintf(tmp, sizeof(tmp), "File `%s' exists; overwrite",
		    adjfname);
		if ((s = eyorn(tmp)) != TRUE)
                        return (s);
        }

	/* old attributes are no longer current */
	bzero(&curbp->b_fi, sizeof(curbp->b_fi));
	if ((s = writeout(&ffp, curbp, adjfname)) == TRUE) {
		(void)strlcpy(curbp->b_fname, adjfname, sizeof(curbp->b_fname));
		if (getbufcwd(curbp->b_cwd, sizeof(curbp->b_cwd)) != TRUE)
			(void)strlcpy(curbp->b_cwd, "/", sizeof(curbp->b_cwd));
		if (augbname(bn, curbp->b_fname, sizeof(bn))
		    == FALSE)
			return (FALSE);
		free(curbp->b_bname);
		if ((curbp->b_bname = strdup(bn)) == NULL)
			return (FALSE);
		(void)fupdstat(curbp);
		curbp->b_flag &= ~(BFBAK | BFCHG);
		upmodes(curbp);
		undo_add_boundary(FFRAND, 1);
		undo_add_modified();
	}
	return (s);
}

/*
 * Save the contents of the current buffer back into its associated file.
 */
static int	makebackup = TRUE;

int
filesave(int f, int n)
{
	if (curbp->b_fname[0] == '\0')
		return (filewrite(f, n));
	else
		return (buffsave(curbp));
}

/*
 * Save the contents of the buffer argument into its associated file.  Do
 * nothing if there have been no changes (is this a bug, or a feature?).
 * Error if there is no remembered file name. If this is the first write
 * since the read or visit, then a backup copy of the file is made.
 * Allow user to select whether or not to make backup files by looking at
 * the value of makebackup.
 */
int
buffsave(struct buffer *bp)
{
	int	 s;
        FILE    *ffp;

	/* return, no changes */
	if ((bp->b_flag & BFCHG) == 0) {
		ewprintf("(No changes need to be saved)");
		return (TRUE);
	}

	/* must have a name */
	if (bp->b_fname[0] == '\0') {
		dobeep();
		ewprintf("No file name");
		return (FALSE);
	}

	/* Ensure file has not been modified elsewhere */
	/* We don't use the ignore flag here */
	if (fchecktime(bp) != TRUE) {
		if ((s = eyesno("File has changed on disk since last save. "
		    "Save anyway")) != TRUE)
			return (s);
	}
	
	if (makebackup && (bp->b_flag & BFBAK)) {
		s = fbackupfile(bp->b_fname);
		/* hard error */
		if (s == ABORT)
			return (FALSE);
		/* softer error */
		if (s == FALSE &&
		    (s = eyesno("Backup error, save anyway")) != TRUE)
			return (s);
	}
	if ((s = writeout(&ffp, bp, bp->b_fname)) == TRUE) {
		(void)fupdstat(bp);
		bp->b_flag &= ~(BFCHG | BFBAK);
		upmodes(bp);
		undo_add_boundary(FFRAND, 1);
		undo_add_modified();
	}
	return (s);
}

/*
 * Since we don't have variables (we probably should) this is a command
 * processor for changing the value of the make backup flag.  If no argument
 * is given, sets makebackup to true, so backups are made.  If an argument is
 * given, no backup files are made when saving a new version of a file.
 */
int
makebkfile(int f, int n)
{
	if (f & FFARG)
		makebackup = n > 0;
	else
		makebackup = !makebackup;
	ewprintf("Backup files %sabled", makebackup ? "en" : "dis");
	return (TRUE);
}

/*
 * NB: bp is passed to both ffwopen and ffclose because some
 * attribute information may need to be updated at open time
 * and others after the close.  This is OS-dependent.  Note
 * that the ff routines are assumed to be able to tell whether
 * the attribute information has been set up in this buffer
 * or not.
 */

/*
 * This function performs the details of file writing; writing the file
 * in buffer bp to file fn. Uses the file management routines in the
 * "fileio.c" package. Most of the grief is checking of some sort.
 * You may want to call fupdstat() after using this function.
 */
int
writeout(FILE ** ffp, struct buffer *bp, char *fn)
{
	struct stat	 statbuf;
	struct line	*lpend;
	int		 s, eobnl;
	char		 dp[NFILEN];

	if (stat(fn, &statbuf) == -1 && errno == ENOENT) {
		errno = 0;
		(void)xdirname(dp, fn, sizeof(dp));
		(void)strlcat(dp, "/", sizeof(dp));
		if (access(dp, W_OK) && errno == EACCES) {
			dobeep();
			ewprintf("Directory %s write-protected", dp);
			return (FIOERR);
		} else if (errno == ENOENT) {
   			dobeep();
			ewprintf("%s: no such directory", dp);
			return (FIOERR);
		}
        }
	lpend = bp->b_headp;
	eobnl = 0;
	if (llength(lback(lpend)) != 0) {
		eobnl = eyorn("No newline at end of file, add one");
		if (eobnl != TRUE && eobnl != FALSE)
			return (eobnl); /* abort */
	}
	/* open writes message */
	if ((s = ffwopen(ffp, fn, bp)) != FIOSUC)
		return (FALSE);
	s = ffputbuf(*ffp, bp, eobnl);
	if (s == FIOSUC) {
		/* no write error */
		s = ffclose(*ffp, bp);
		if (s == FIOSUC)
			ewprintf("Wrote %s", fn);
	} else {
		/* print a message indicating write error */
		(void)ffclose(*ffp, bp);
		dobeep();
		ewprintf("Unable to write %s", fn);
	}
	return (s == FIOSUC);
}

/*
 * Tag all windows for bp (all windows if bp == NULL) as needing their
 * mode line updated.
 */
void
upmodes(struct buffer *bp)
{
	struct mgwin	*wp;

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (bp == NULL || curwp->w_bufp == bp)
			wp->w_rflag |= WFMODE;
}

/*
 * dirname using strlcpy semantic.
 * Like dirname() except an empty string is returned in
 * place of "/". This means we can always add a trailing
 * slash and be correct.
 * Address portability issues by copying argument
 * before using. Some implementations modify the input string.
 */
size_t
xdirname(char *dp, const char *path, size_t dplen)
{
	char ts[NFILEN];
	size_t len;

	(void)strlcpy(ts, path, NFILEN);
	len = strlcpy(dp, dirname(ts), dplen);
	if (dplen > 0 && dp[0] == '/' && dp[1] == '\0') {
		dp[0] = '\0';
		len = 0;
	}
	return (len);
}

/*
 * basename using strlcpy/strlcat semantic.
 * Address portability issue by copying argument
 * before using: some implementations modify the input string.
 */
size_t
xbasename(char *bp, const char *path, size_t bplen)
{
	char ts[NFILEN];

	(void)strlcpy(ts, path, NFILEN);
	return (strlcpy(bp, basename(ts), bplen));
}

/*
 * The adjusted file name refers to a directory, so open dired mode.
 */
int
do_dired(char *adjf)
{
#ifdef ENABLE_DIRED
	struct buffer	*bp;

	if ((bp = dired_(adjf)) == FALSE)
		return (FALSE);
	curbp = bp;
	return (showbuffer(bp, curwp, WFFULL | WFMODE));
#else
	return (FALSE);
#endif /* ENABLE_DIRED */
}
