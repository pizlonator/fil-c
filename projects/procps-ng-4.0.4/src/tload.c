/*
 * tload.c	- terminal version of xload
 *
 * Copyright © 2009-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 1992      Branko Lankester
 *
 * /proc changes by David Engel (david@ods.com)
 * Made a little more efficient by Michael K. Johnson (johnsonm@sunsite.unc.edu)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"

#include "misc.h"

static char *screen;

static int nrows = 25;
static int ncols = 80;
static int scr_size;
static int fd = STDOUT_FILENO;
static unsigned int dly = 5;
static jmp_buf jb;

static void alrm(int signo __attribute__ ((__unused__)))
{
	signal(SIGALRM, alrm);
	alarm(dly);
}

static void setsize(int i)
{
	struct winsize win;

	signal(SIGWINCH, setsize);
	if (ioctl(fd, TIOCGWINSZ, &win) != -1) {
		if (win.ws_col > 0)
			ncols = win.ws_col;
		if (win.ws_row > 0)
			nrows = win.ws_row;
	}
	if (ncols < 2 || ncols >= INT_MAX)
		xerrx(EXIT_FAILURE, _("screen too small or too large"));
	if (nrows < 2 || nrows >= INT_MAX / ncols)
		xerrx(EXIT_FAILURE, _("screen too small or too large"));
	scr_size = nrows * ncols;
	if (scr_size < 2)
		xerrx(EXIT_FAILURE, _("screen too small"));
	if (screen == NULL)
		screen = (char *)xmalloc(scr_size);
	else
		screen = (char *)xrealloc(screen, scr_size);

	memset(screen, ' ', scr_size - 1);
	*(screen + scr_size - 2) = '\0';
	if (i)
		longjmp(jb, 1);
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] [tty]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -d, --delay <secs>  update delay in seconds\n"), out);
	fputs(_(" -s, --scale <num>   vertical scale\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("tload(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int lines, row, col = 0;
	int i, opt;
	double av[3];
	static double max_scale = 0, scale_fact;
	long tmpdly;

	static const struct option longopts[] = {
		{"scale", required_argument, NULL, 's'},
		{"delay", required_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((opt =
		getopt_long(argc, argv, "s:d:Vh", longopts, NULL)) != -1)
		switch (opt) {
		case 's':
			max_scale = strtod_or_err(optarg, _("failed to parse argument"));
			if (max_scale < 0)
			        xerrx(EXIT_FAILURE, _("scale cannot be negative"));
			break;
		case 'd':
			tmpdly = strtol_or_err(optarg, _("failed to parse argument"));
			if (tmpdly < 1)
				xerrx(EXIT_FAILURE, _("delay must be positive integer"));
			else if (UINT_MAX < tmpdly)
				xerrx(EXIT_FAILURE, _("too large delay value"));
			dly = tmpdly;
			break;
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
			break;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (argc > optind)
		if ((fd = open(argv[optind], O_WRONLY)) == -1)
			xerr(EXIT_FAILURE, _("can not open tty"));

	setsize(0);

	if (max_scale == 0)
		max_scale = nrows;

	scale_fact = max_scale;

	setjmp(jb);
	col = 0;
	alrm(0);

	while (1) {
        int rc;

		if (scale_fact < max_scale)
			scale_fact *= 2.0;	/* help it drift back up. */

		if ((rc = procps_loadavg(&av[0], &av[1], &av[2])) < 0)
        {
            if (rc == -ENOENT)
                xerrx(EXIT_FAILURE,
                      _("Load average file /proc/loadavg does not exist"));
            else
                xerrx(EXIT_FAILURE,
                      _("Unable to get load average"));
        }

		do {
			lines = av[0] * scale_fact;
			row = nrows - 1;

			while (0 <= --lines) {
				*(screen + row * ncols + col) = '*';
				if (--row < 0) {
					scale_fact /= 2.0;
					break;
				}
			}
		} while (0 <= lines);

		while (row >= 0)
			*(screen + row-- * ncols + col) = ' ';

		for (i = 1;; ++i) {
			char *p;
			row = nrows - (i * scale_fact);
			if (row < 0 || row >= nrows)
				break;
			if (*(p = screen + row * ncols + col) == ' ')
				*p = '-';
			else
				*p = '=';
		}

		if (++col == ncols) {
			--col;
			memmove(screen, screen + 1, scr_size - 1);

			for (row = nrows - 2; row >= 0; --row)
				*(screen + row * ncols + col) = ' ';
		}
		i = snprintf(screen, scr_size, " %.2f, %.2f, %.2f", av[0], av[1], av[2]);
		if (i > 0 && i < scr_size)
			screen[i] = ' ';

		if (write(fd, "\033[H", 3) < 0)
			xerr(EXIT_FAILURE, _("writing to tty failed"));
		if (write(fd, screen, scr_size - 1) < 0)
			xerr(EXIT_FAILURE, _("writing to tty failed"));
		pause();
	}
}
