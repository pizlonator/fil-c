/*	$OpenBSD: ttydef.h,v 1.12 2015/03/24 22:28:10 bcallah Exp $	*/
/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

/* This file is in the public domain. */

#ifndef TTYDEF_H
#define TTYDEF_H

#include "config.h"
#include <termios.h>

#define	putpad(str, num)	tputs(str, num, ttputc)

/*
 * This flag is used on *BSD when calling tcsetattr() to prevent it
 * from changing speed, duplex, parity.  GNU says we should use the
 * CIGNORE flag to c_cflag, but that doesn't exist so ... we rely on
 * our initial tcgetattr() and prey that nothing changes on the TTY
 * before we exit and restore with tcsetattr()
 */
#ifndef TCSASOFT
#define TCSASOFT 0
#endif

#ifndef WITHOUT_CURSES
#ifndef HAVE_NCURSES_CURSES_H
#include <term.h>
#else
#include <ncurses/curses.h>
#include <ncurses/term.h>
#endif
#define STANDOUT_GLITCH		/* possible standout glitch	 */
#else
#include "ansi.h"
#undef  STANDOUT_GLITCH		/* never, messes up modeline	 */
#endif

#endif /* TTYDEF_H */
