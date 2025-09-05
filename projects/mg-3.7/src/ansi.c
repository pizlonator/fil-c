/*
 * ansi display driver backend
 *
 * This file emulates the termcap/terminfo/ncurses backend with simple
 * ANSI escape sequences.  Since terminfo is the de facto default in 
 * all other Mg clones, this file hooks into that and emulates that
 * interfaces to the best of its abilities.
 */

/* This file is in the public domain. */

#include <poll.h>
#include <stdio.h>		/* FILE */
#include <stdlib.h>		/* getenv() */
#include <unistd.h>		/* write() */

#include "ttydef.h"
#include "def.h"

TERMINAL *cur_term;

int setupterm(const char *term, int filedes, int *errret)
{
	/* Save pos+attr, disable margins, set cursor far away, query pos */
	const char query[] = "\e7" "\e[r" "\e[999;999H" "\e[6n";
	struct pollfd fd = { filedes, POLLIN, 0 };
	static TERMINAL t = {
		.t_nrow = 24,
		.t_ncol = 80,
		.t_str  = {
			"\e[%dZ",       /* 0: Backtab */
			"\a",	        /* 1: BEL */
			"\r",	        /* 2: CR */
			NULL,	        /* 3: STBM \e[L1;L2r */
			"\e[3g",        /* 4: Clear all tabs */
			"\e[2J",        /* 5: Clear screen */
			"\e[K",	        /* 6: Clear EOL */
			"\e[J",	        /* 7: Clear EOS */
			NULL,	        /* 8: Column addr */
			NULL,	        /* 9: Command char */
			"\e[%i%d;%dH",  /* 10: Cursor addr */
			"\e[B",		/* 11: Cursor Down  */
			"\e[F",		/* 12: Cursor End   */
			"\e[H",		/* 13: Cursor Home  */
			"\e[?25l",	/* 14: Cursor invisible */
			"\e[D",		/* 15: Cursor Left  */
			"\e[?25h",	/* 16: Cursor normal */
			"\e[C",		/* 17: Cursor Right */
			NULL,		/* 18: Cursor to LL */
			"\e[A",		/* 19: Cursor Up    */
			"\e[?25h",	/* 20: Cursor visible */
			"\b",		/* 21: Delete char */
			"\e[2K",	/* 22: Delete line */
			"\e[1L",	/* 23: Insert line*/
			NULL,		/* 24: */
			NULL,		/* 25: */
			"\e[5m",        /* 26: Enter blink mode */
			"\e[1m",        /* 27: Enter bold mode */
			NULL,		/* 28: Enter ca mode */
			NULL,		/* 29: Enter delete mode */
			"\e[2m",	/* 30: Enter dim mode */
			NULL,		/* 31: */
			NULL,		/* 32: */
			NULL,		/* 33: */
			"\e[7m",	/* 34: Enter reverse mode */
			"\e[7m",	/* 35: Enter standout mode */
			"\e[4m",	/* 36: Enter underline mode */
			NULL,		/* 37: */
			NULL,		/* 38: */
			"\e[0m",	/* 39: Disable attributes */
			NULL,		/* 40: */
			NULL,		/* 41: */
			NULL,		/* 42: */
			"\e[0m",	/* 43: Exit standout */
			"\e[0m",	/* 44: Exit underline */
			NULL,		/* 45: */
			"\e[%dM",	/* 46: Parm delete N lines*/
			"\e[%dL",	/* 47: Parm insert N lines */
			"\e[%de",	/* 48: Parm cusror down N lines*/
			"\e[1~",	/* 49: Home  */
			"\e[2~",	/* 50: Ins   */
			"\e[3~",	/* 51: Del   */
			"\e[4~",	/* 52: End   */
			"\e[5~",	/* 53: PgUp  */
			"\e[6~",	/* 54: PgDn  */
			"\e[7~",	/* 55: Home  */
			"\e[8~",	/* 56: End   */
			"\e[10~",	/* 57: F0    */
			"\eOP",		/* 58: F1    */
			"\eOQ",		/* 59: F2    */
			"\eOR",		/* 60: F3    */
			"\eOS",		/* 61: F4    */
			"\e[15~",	/* 62: F5    */
			"\e[17~",	/* 63: F6    */
			"\e[18~",	/* 64: F7    */
			"\e[19~",	/* 65: F8    */
			"\e[20~",	/* 66: F9    */
			"\e[21~",	/* 67: F10   */
			"\e[23~",	/* 68: F11   */
			"\e[24~",	/* 69: F12   */
		}
	};
	struct	termios	 ostate;	/* saved tty state */
	struct	termios	 nstate;	/* values for editor mode */

	if (!term) {
		term = getenv("TERM");
		if (!term)
			term = "vt100";
	}

	cur_term = &t;
	t.t_fd = filedes;

	/* Adjust output channel */
	tcgetattr(filedes, &ostate);		/* save old state */
	nstate = ostate;			/* get base of new state */
#ifndef cfmakeraw
	nstate.c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	nstate.c_oflag &= ~OPOST;
	nstate.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	nstate.c_cflag &= ~(CSIZE|PARENB);
	nstate.c_cflag |= CS8;
#else
	cfmakeraw(&nstate);
#endif
	tcsetattr(filedes, TCSADRAIN, &nstate);	/* set mode */

	/* Query size of terminal by first trying to position cursor */
	if (write(filedes, query, sizeof(query)) != -1 && poll(&fd, 1, 300) > 0) {
		int row = 0, col = 0;

		/* Terminal responds with \e[row;posR */
		if (scanf("\e[%d;%dR", &row, &col) == 2) {
			t.t_nrow = row;
			t.t_ncol = col;
		}
	}

	return 0;
}

char *
tgoto(const char *cap, int col, int row)
{
        static char buf[42];
	char *p, *q, *fmt;
        int *val, tmp;

        val = &row;
        for (p = (char *)cap, q = buf; *p; p++) {
                if (*p != '%') {
                        *q++ = *p;
                        continue;
                }

                switch (*++p) {
                case 'd':
                        fmt = "%dX";
                        goto num;
                case '2':
                        fmt = "%02dX";
                        goto num;
                case '3':
                        fmt = "%03dX";
                num:
                        sprintf(q, fmt, *val);
                        while (*q != 'X')
                                q++;
                        val = &col;
                        break;
                case '.':
                        *q++ = *val;
                        val = &col;
                        break;
                case '+':
                        *q++ = *val + *++p;
                        val = &col;
                        break;
                case '>':
                        p++;
                        if (*val > *p++)
                                *val += *p;
                        break;
                case 'r':
                        tmp = row;
                        row = col;
                        col = tmp;
                        break;
                case 'i':
                        row++;
                        col++;
                        break;
                case 'n':
                        row ^= 0140;
                        col ^= 0140;
                        break;
                case 'B':
                        *val += 6 * (*val / 10);
                        break;
                case 'D':
                        *val -= 2 * (*val % 16);
                        break;
                case '%':
                        *q++ = '%';
                        break;
                default:
                        return "OOPS";
                }
        }
        *q = 0;
        return buf;
}

int
tputs(const char *str, int affcnt, int (*outc)(int))
{
        while (*str == '.' || *str == '*' || (*str >= '0' && *str <= '9'))
                str++;
        while (*str)
                (*outc)(*str++);

	return 0;
}
