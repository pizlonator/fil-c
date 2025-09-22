/*
 * Copyright (C) 2010-2014 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"

#include "libsmartcols.h"


enum { COL_NAME, COL_DATA };

/* add columns to the @tb */
static void setup_columns(struct libscols_table *tb)
{
	struct libscols_column *cl;

	if (!scols_table_new_column(tb, "NAME", 0, SCOLS_FL_TREE))
		goto fail;
	cl = scols_table_new_column(tb, "DATA", 0, SCOLS_FL_WRAP);
	if (!cl)
		goto fail;

	scols_column_set_wrapfunc(cl, scols_wrapnl_chunksize,
				      scols_wrapnl_nextchunk,
				      NULL);
	scols_column_set_safechars(cl, "\n");

	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output columns");
}

static struct libscols_line *add_line(struct libscols_table *tb, struct libscols_line *parent, const char *name, const char *data)
{
	struct libscols_line *ln = scols_table_new_line(tb, parent);
	if (!ln)
		err(EXIT_FAILURE, "failed to create output line");

	if (scols_line_set_data(ln, COL_NAME, name))
		goto fail;
	if (scols_line_set_data(ln, COL_DATA, data))
		goto fail;
	return ln;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output line");
}

int main(int argc, char *argv[])
{
	struct libscols_table *tb;
	struct libscols_line *ln;	/* any line */
	struct libscols_line *g1;	/* groups */
	struct libscols_line *p1, *p2;	/* parents */
	int c;

	static const struct option longopts[] = {
		{ "maxout", 0, NULL, 'm' },
		{ "width",  1, NULL, 'w' },
		{ "help",   1, NULL, 'h' },

		{ NULL, 0, NULL, 0 },
	};

	setlocale(LC_ALL, "");	/* just to have enable UTF8 chars */

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, "failed to create output table");

	while((c = getopt_long(argc, argv, "hmw:", longopts, NULL)) != -1) {
		switch(c) {
		case 'h':
			printf("%s [--help | --maxout | --width <num>]\n", program_invocation_short_name);
			break;
		case 'm':
			scols_table_enable_maxout(tb, TRUE);
			break;
		case 'w':
			scols_table_set_termforce(tb, SCOLS_TERMFORCE_ALWAYS);
			scols_table_set_termwidth(tb, strtou32_or_err(optarg, "failed to parse terminal width"));
			break;
		}
	}

	scols_table_enable_colors(tb, isatty(STDOUT_FILENO));
	setup_columns(tb);

	add_line(tb, NULL, "Alone", "bla bla bla");

	p1 = add_line(tb, NULL, "A", "bla bla bla");
	     add_line(tb, p1,   "A:B", "bla bla bla");
	     add_line(tb, p1,   "A:C", "bla bla bla");

	g1 = add_line(tb, NULL, "B", "bla bla bla");
	     add_line(tb, NULL, "C", "bla\nfoo");
	p1 = add_line(tb, NULL, "D", "bla bla\nbar");

	p2 = add_line(tb, p1, "D:A", "bla bla bla");

	ln = add_line(tb, p2, "D:A:A", "bla\nbla\nbla");
	scols_table_group_lines(tb, g1, ln, 0);

	add_line(tb, p1, "D:B", "bla bla bla");
	add_line(tb, p1, "D:C", "bla\nother bla");
	add_line(tb, p1, "D:D", "bla bla bla");

	ln = add_line(tb, NULL, "E", "bla bla bla");
	scols_table_group_lines(tb, g1, ln, 0);

	p1 = ln;
	add_line(tb, p1, "E:A", "bla bla bla");
	add_line(tb, p1, "E:B", "bla bla bla");
	add_line(tb, p1, "E:C", "bla bla bla");

	add_line(tb, NULL, "F", "bla bla bla");

	ln = add_line(tb, NULL, "G1:A", "alb alb alb");
	scols_line_link_group(ln, g1, 0);

	p1 = ln;
	add_line(tb, p1, "G1:A:A", "bla\nbla bla");
	add_line(tb, p1, "G1:A:B", "bla bla bla");
	add_line(tb, p1, "G1:A:C", "bla bla bla");

	add_line(tb, NULL, "G", "bla bla bla");

	ln = add_line(tb, NULL, "G1:B", "alb alb\nalb");
	scols_line_link_group(ln, g1, 0);

	add_line(tb, NULL, "foo", "bla bla bla");
	add_line(tb, NULL, "bar", "bla bla bla");

	scols_print_table(tb);

	scols_unref_table(tb);
	return EXIT_SUCCESS;
}
