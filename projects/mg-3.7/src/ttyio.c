/*	$OpenBSD: ttyio.c,v 1.40 2021/03/20 09:00:49 lum Exp $	*/

/* This file is in the public domain. */

/*
 * POSIX terminal I/O.
 *
 * The functions in this file negotiate with the operating system for
 * keyboard characters, and write characters to the display in a barely
 * buffered fashion.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ttydef.h"
#include "def.h"

#define NOBUF	512			/* Output buffer size. */

int	ttstarted;
char	obuf[NOBUF];			/* Output buffer. */
size_t	nobuf;				/* Buffer count. */
struct	termios	oldtty;			/* POSIX tty settings. */
struct	termios	newtty;
int	nrow;				/* Terminal size, rows. */
int	ncol;				/* Terminal size, columns. */

/*
 * This function gets called once, to set up the terminal.
 * On systems w/o TCSASOFT we turn off off flow control,
 * which isn't really the right thing to do.
 */
void
ttopen(void)
{
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
		panic("standard input and output must be a terminal");

	if (ttraw() == FALSE)
		panic("aborting due to terminal initialize failure");
}

/*
 * This function sets the terminal to RAW mode, as defined for the current
 * shell.  This is called both by ttopen() above and by spawncli() to
 * get the current terminal settings and then change them to what
 * mg expects.	Thus, tty changes done while spawncli() is in effect
 * will be reflected in mg.
 */
int
ttraw(void)
{
	if (tcgetattr(0, &oldtty) == -1) {
		dobeep();
		ewprintf("ttopen can't get terminal attributes");
		return (FALSE);
	}
	(void)memcpy(&newtty, &oldtty, sizeof(newtty));
	/* Set terminal to 'raw' mode and ignore a 'break' */
	newtty.c_cc[VMIN] = 1;
	newtty.c_cc[VTIME] = 0;
	newtty.c_iflag |= IGNBRK;
	newtty.c_iflag &= ~(BRKINT | PARMRK | INLCR | IGNCR | ICRNL | IXON);
	newtty.c_oflag &= ~OPOST;
	newtty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &newtty) == -1) {
		dobeep();
		ewprintf("ttopen can't tcsetattr");
		return (FALSE);
	}
	ttstarted = 1;

	return (TRUE);
}

/*
 * This function gets called just before we go back home to the shell.
 * Put all of the terminal parameters back.
 * Under UN*X this just calls ttcooked(), but the ttclose() hook is in
 * because vttidy() in display.c expects it for portability reasons.
 */
void
ttclose(void)
{
	if (ttstarted) {
		if (ttcooked() == FALSE)
			panic("");	/* ttcooked() already printf'd */
		ttstarted = 0;
	}
}

/*
 * This function restores all terminal settings to their default values,
 * in anticipation of exiting or suspending the editor.
 */
int
ttcooked(void)
{
	ttflush();
	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &oldtty) == -1) {
		dobeep();
		ewprintf("ttclose can't tcsetattr");
		return (FALSE);
	}
	return (TRUE);
}

/*
 * Write character to the display.  Characters are buffered up,
 * to make things a little bit more efficient.
 */
int
ttputc(int c)
{
	if (nobuf >= NOBUF)
		ttflush();
	obuf[nobuf++] = c;
	return (c);
}

/*
 * Flush output.
 */
void
ttflush(void)
{
	ssize_t	 written;
	char	*buf = obuf;

	if (nobuf == 0 || batch == 1)
		return;

	while ((written = write(fileno(stdout), buf, nobuf)) != (ssize_t)nobuf) {
		if (written == -1) {
			if (errno == EINTR)
				continue;
			panic("ttflush write failed");
		}
		buf += written;
		nobuf -= (size_t)written;
	}
	nobuf = 0;
}

/*
 * Read character from terminal. All 8 bits are returned, so that you
 * can use a multi-national terminal.
 */
int
ttgetc(void)
{
	char	c;
	ssize_t	ret;

	do {
		ret = read(STDIN_FILENO, &c, 1);
		if (ret == -1 && errno == EINTR) {
			if (winch_flag) {
				redraw(0, 0);
				winch_flag = 0;
			}
		} else if (ret == -1 && errno == EIO)
			panic("lost stdin");
		else if (ret == 1)
			break;
	} while (1);
	return ((int) c) & 0xFF;
}

/*
 * Returns TRUE if there are characters waiting to be read.
 */
int
charswaiting(void)
{
	int	x;

	return ((ioctl(0, FIONREAD, &x) == -1) ? 0 : x);
}

/*
 * panic - just exit, as quickly as we can.
 */
void
panic(char *s)
{
	static int panicking = 0;

	if (panicking)
		return;
	else
		panicking = 1;
	ttclose();
	(void) fputs("panic: ", stderr);
	(void) fputs(s, stderr);
	(void) fputc('\n', stderr);	/* Use '\n' as no buffers now. */
	exit(1);
}

/*
 * This function returns FALSE if any characters have showed up on the
 * tty before 'msec' milliseconds.
 */
int
ttwait(int msec)
{
	struct pollfd	pfd[1];

	pfd[0].fd = 0;
	pfd[0].events = POLLIN;

	if ((poll(pfd, 1, msec)) == 0)
		return (TRUE);
	return (FALSE);
}
