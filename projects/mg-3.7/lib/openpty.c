/*
 * Copyright (c) 2004 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2021 Joachim Wiberg <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <fcntl.h>
#ifdef HAVE_PTY_H
# include <pty.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

int
openpty(int *amaster, int *aslave, char *name, struct termios *termp,
   struct winsize *winp)
{
	const char	*ptymajors = "pqrstuvwxyzabcdefghijklmno"
				     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char	*ptyminors = "0123456789abcdef";
	int		 i, m, s, num_minors,  num_ptys;
	char		 ptbuf[64], ttbuf[64], *nm;
	struct termios	 tio;

	num_minors = strlen(ptyminors);
	num_ptys   = strlen(ptymajors) * num_minors;

	for (i = 0; i < num_ptys; i++) {
		snprintf(ptbuf, sizeof(ptbuf), "/dev/pty%c%c", 
			 ptymajors[i / num_minors],
			 ptyminors[i % num_minors]);
		snprintf(ttbuf, sizeof(ttbuf), "/dev/tty%c%c",
			 ptymajors[i / num_minors],
			 ptyminors[i % num_minors]);

		m = open(ptbuf, O_RDWR | O_NOCTTY);
		if (m == -1) {
			snprintf(ptbuf, sizeof(ptbuf), "/dev/ptyp%d", i);
			snprintf(ttbuf, sizeof(ttbuf), "/dev/ttyp%d", i);
			m = open(ptbuf, O_RDWR | O_NOCTTY);
			if (m == -1)
				continue;
		}

		s = open(ttbuf, O_RDWR | O_NOCTTY);
		if (s == -1) {
			close(m);
			return (-1);
		}

		if (tcgetattr(m, &tio) != -1) {
			tio.c_lflag |= (ECHO | ISIG | ICANON);
			tio.c_oflag |= (OPOST | ONLCR);
			tio.c_iflag |= ICRNL;
			tcsetattr(m, TCSANOW, &tio);
		}

		*amaster = m;
		*aslave  = s;

		if (name && (nm = ttyname(s)))
			strcpy(name, nm);
		if (winp)
			(void)ioctl(s, TIOCGWINSZ, winp);

		return (0);
	}

	return (-1);
}
