/*	$OpenBSD: tty.c,v 1.39 2021/03/20 09:00:49 lum Exp $	*/

/* This file is in the public domain. */

/*
 * Terminfo display driver
 *
 * Terminfo is a terminal information database and routines to describe
 * terminals on most modern UNIX systems.  Many other systems have adopted
 * this as a reasonable way to allow for widely varying and ever changing
 * varieties of terminal types.	 This should be used where practical.
 */
/*
 * Known problems: If you have a terminal with no clear to end of screen and
 * memory of lines below the ones visible on the screen, display will be
 * wrong in some cases.  I doubt that any such terminal was ever made, but I
 * thought everyone with delete line would have clear to end of screen too...
 *
 * Code for terminals without clear to end of screen and/or clear to end of line
 * has not been extensively tested.
 *
 * Cost calculations are very rough.  Costs of insert/delete line may be far
 * from the truth.  This is accentuated by display.c not knowing about
 * multi-line insert/delete.
 *
 * Using scrolling region vs insert/delete line should probably be based on cost
 * rather than the assumption that scrolling region operations look better.
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "ttydef.h"
#include "def.h"

static int	 charcost(const char *);

static int	 cci;
static int	 insdel;	/* Do we have both insert & delete line? */
static char	*scroll_fwd;	/* How to scroll forward. */

static void	 winchhandler(int);

volatile sig_atomic_t	 winch_flag;
int			 tceeol;
int			 tcinsl;
int			 tcdell;

static void
winchhandler(int sig)
{
	winch_flag = 1;
}

/*
 * Initialize the terminal when the editor
 * gets started up.
 */
void
ttinit(void)
{
	struct sigaction sa = { .sa_handler = winchhandler };
	int errret;
	char *tty;

	if (batch == 1)
		tty = "pty";
	else
		tty = NULL;

	if (setupterm(tty, STDOUT_FILENO, &errret))
		panic("Terminal setup failed");

	sigaction(SIGWINCH, &sa, NULL);
	sigaction(SIGCONT, &sa, NULL);

	scroll_fwd = scroll_forward;
	if (scroll_fwd == NULL || *scroll_fwd == '\0') {
		/* this is what GNU Emacs does */
		scroll_fwd = parm_down_cursor;
		if (scroll_fwd == NULL || *scroll_fwd == '\0')
			scroll_fwd = curbp->b_nlchr;
	}

	if (cursor_address == NULL || cursor_up == NULL)
		panic("This terminal is too stupid to run mg");

	/* set nrow & ncol */
	ttresize();

	if (!clr_eol)
		tceeol = ncol;
	else
		tceeol = charcost(clr_eol);

	/* Estimate cost of inserting a line */
	if (change_scroll_region && scroll_reverse)
		tcinsl = charcost(change_scroll_region) * 2 +
		    charcost(scroll_reverse);
	else if (parm_insert_line)
		tcinsl = charcost(parm_insert_line);
	else if (insert_line)
		tcinsl = charcost(insert_line);
	else
		/* make this cost high enough */
		tcinsl = nrow * ncol;

	/* Estimate cost of deleting a line */
	if (change_scroll_region)
		tcdell = charcost(change_scroll_region) * 2 +
		    charcost(scroll_fwd);
	else if (parm_delete_line)
		tcdell = charcost(parm_delete_line);
	else if (delete_line)
		tcdell = charcost(delete_line);
	else
		/* make this cost high enough */
		tcdell = nrow * ncol;

	/* Flag to indicate that we can both insert and delete lines */
	insdel = (insert_line || parm_insert_line) &&
	    (delete_line || parm_delete_line);

	if (enter_ca_mode)
		/* enter application mode */
		putpad(enter_ca_mode, 1);

	ttresize();
}

/*
 * Re-initialize the terminal when the editor is resumed.
 * The keypad_xmit doesn't really belong here but...
 */
void
ttreinit(void)
{
	/* check if file was modified while we were gone */
	if (fchecktime(curbp) != TRUE) {
		curbp->b_flag |= BFDIRTY;
	}

	if (enter_ca_mode)
		/* enter application mode */
		putpad(enter_ca_mode, 1);

	if (keypad_xmit)
		/* turn on keypad */
		putpad(keypad_xmit, 1);

	ttresize();
}

/*
 * Clean up the terminal, in anticipation of a return to the command
 * interpreter. This is a no-op on the ANSI display. On the SCALD display,
 * it sets the window back to half screen scrolling. Perhaps it should
 * query the display for the increment, and put it back to what it was.
 */
void
tttidy(void)
{
	ttykeymaptidy();

	/* set the term back to normal mode */
	if (exit_ca_mode)
		putpad(exit_ca_mode, 1);
}

/*
 * Move the cursor to the specified origin 0 row and column position. Try to
 * optimize out extra moves; redisplay may have left the cursor in the right
 * location last time!
 */
void
ttmove(int row, int col)
{
	if (ttrow != row || ttcol != col) {
		putpad(tgoto(cursor_address, col, row), 1);
		ttrow = row;
		ttcol = col;
	}
}

/*
 * Erase to end of line.
 */
void
tteeol(void)
{
	int	i;

	if (clr_eol)
		putpad(clr_eol, 1);
	else {
		i = ncol - ttcol;
		while (i--)
			ttputc(' ');
		ttrow = ttcol = HUGE;
	}
}

/*
 * Erase to end of page.
 */
void
tteeop(void)
{
	int	line;

	if (clr_eos)
		putpad(clr_eos, nrow - ttrow);
	else {
		putpad(clr_eol, 1);
		if (insdel)
			ttdell(ttrow + 1, lines, lines - ttrow - 1);
		else {
			/* do it by hand */
			for (line = ttrow + 1; line <= lines; ++line) {
				ttmove(line, 0);
				tteeol();
			}
		}
		ttrow = ttcol = HUGE;
	}
}

/*
 * Make a noise.
 */
void
ttbeep(void)
{
	putpad(bell, 1);
	ttflush();
}

/*
 * Insert nchunk blank line(s) onto the screen, scrolling the last line on
 * the screen off the bottom.  Use the scrolling region if possible for a
 * smoother display.  If there is no scrolling region, use a set of insert
 * and delete line sequences.
 */
void
ttinsl(int row, int bot, int nchunk)
{
	int	i, nl;

	/* One line special cases */
	if (row == bot) {
		ttmove(row, 0);
		tteeol();
		return;
	}
	/* Use scroll region and back index */
	if (change_scroll_region && scroll_reverse) {
		nl = bot - row;
		ttwindow(row, bot);
		ttmove(row, 0);
		while (nchunk--)
			putpad(scroll_reverse, nl);
		ttnowindow();
		return;
	/* else use insert/delete line */
	} else if (insdel) {
		ttmove(1 + bot - nchunk, 0);
		nl = nrow - ttrow;
		if (parm_delete_line)
			putpad(tgoto(parm_delete_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk */
			for (i = 0; i < nchunk; i++)
				putpad(delete_line, nl);
		ttmove(row, 0);

		/* ttmove() changes ttrow */
		nl = nrow - ttrow;

		if (parm_insert_line)
			putpad(tgoto(parm_insert_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk */
			for (i = 0; i < nchunk; i++)
				putpad(insert_line, nl);
		ttrow = HUGE;
		ttcol = HUGE;
	} else
		panic("ttinsl: Can't insert/delete line");
}

/*
 * Delete nchunk line(s) from "row", replacing the bottom line on the
 * screen with a blank line.  Unless we're using the scrolling region,
 * this is done with crafty sequences of insert and delete lines.  The
 * presence of the echo area makes a boundary condition go away.
 */
void
ttdell(int row, int bot, int nchunk)
{
	int	i, nl;

	/* One line special cases */
	if (row == bot) {
		ttmove(row, 0);
		tteeol();
		return;
	}
	/* scrolling region */
	if (change_scroll_region) {
		nl = bot - row;
		ttwindow(row, bot);
		ttmove(bot, 0);
		while (nchunk--)
			putpad(scroll_fwd, nl);
		ttnowindow();
	/* else use insert/delete line */
	} else if (insdel) {
		ttmove(row, 0);
		nl = nrow - ttrow;
		if (parm_delete_line)
			putpad(tgoto(parm_delete_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk */
			for (i = 0; i < nchunk; i++)
				putpad(delete_line, nl);
		ttmove(1 + bot - nchunk, 0);

		/* ttmove() changes ttrow */
		nl = nrow - ttrow;

		if (parm_insert_line)
			putpad(tgoto(parm_insert_line, 0, nchunk), nl);
		else
			/* For all lines in the chunk */
			for (i = 0; i < nchunk; i++)
				putpad(insert_line, nl);
		ttrow = HUGE;
		ttcol = HUGE;
	} else
		panic("ttdell: Can't insert/delete line");
}

/*
 * This routine sets the scrolling window on the display to go from line
 * "top" to line "bot" (origin 0, inclusive).  The caller checks for the
 * pathological 1-line scroll window which doesn't work right and avoids
 * it.  The "ttrow" and "ttcol" variables are set to a crazy value to
 * ensure that the next call to "ttmove" does not turn into a no-op (the
 * window adjustment moves the cursor).
 */
void
ttwindow(int top, int bot)
{
	if (change_scroll_region && (tttop != top || ttbot != bot)) {
		putpad(tgoto(change_scroll_region, bot, top), nrow - ttrow);
		ttrow = HUGE;	/* Unknown.		 */
		ttcol = HUGE;
		tttop = top;	/* Remember region.	 */
		ttbot = bot;
	}
}

/*
 * Switch to full screen scroll. This is used by "spawn.c" just before it
 * suspends the editor and by "display.c" when it is getting ready to
 * exit.  This function does a full screen scroll by telling the terminal
 * to set a scrolling region that is lines or nrow rows high, whichever is
 * larger.  This behavior seems to work right on systems where you can set
 * your terminal size.
 */
void
ttnowindow(void)
{
	if (change_scroll_region) {
		putpad(tgoto(change_scroll_region,
		    (nrow > lines ? nrow : lines) - 1, 0), nrow - ttrow);
		ttrow = HUGE;	/* Unknown.		 */
		ttcol = HUGE;
		tttop = HUGE;	/* No scroll region.	 */
		ttbot = HUGE;
	}
}

/*
 * Set the current writing color to the specified color. Watch for color
 * changes that are not going to do anything (the color is already right)
 * and don't send anything to the display.  The rainbow version does this
 * in putline.s on a line by line basis, so don't bother sending out the
 * color shift.
 */
void
ttcolor(int color)
{
	if (color != tthue) {
		if (color == CTEXT)
			/* normal video */
			putpad(exit_standout_mode, 1);
		else if (color == CMODE)
			/* reverse video */
			putpad(enter_standout_mode, 1);
		/* save the color */
		tthue = color;
	}
}

/*
 * This routine is called by the "refresh the screen" command to try
 * to resize the display. Look in "window.c" to see how
 * the caller deals with a change.
 *
 * We use `newrow' and `newcol' so vtresize() know the difference between the
 * new and old settings.
 */
void
ttresize(void)
{
	int newrow = 0, newcol = 0;

	struct	winsize winsize;

	if (ioctl(0, TIOCGWINSZ, &winsize) == 0) {
		newrow = winsize.ws_row;
		newcol = winsize.ws_col;
	}
	if ((newrow <= 0 || newcol <= 0) &&
	    ((newrow = lines) <= 0 || (newcol = columns) <= 0)) {
		newrow = 24;
		newcol = 80;
	}
	if (vtresize(1, newrow, newcol) != TRUE)
		panic("vtresize failed");
}

/*
 * fake char output for charcost()
 */
static int
fakec(int c)
{
	cci++;
	return (0);
}

/* calculate the cost of doing string s */
static int
charcost(const char *s)
{
	cci = 0;

	tputs(s, nrow, fakec);
	return (cci);
}
