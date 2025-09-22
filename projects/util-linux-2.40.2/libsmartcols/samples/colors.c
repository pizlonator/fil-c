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


enum { COL_NAME, COL_FOO, COL_BAR };

/* add columns to the @tb */
static void setup_columns(struct libscols_table *tb)
{
	if (!scols_table_new_column(tb, "NAME", 0, 0))
		goto fail;
	if (!scols_table_new_column(tb, "BAR", 0, 0))
		goto fail;
	if (!scols_table_new_column(tb, "FOO", 0, 0))
		goto fail;
	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output columns");
}

static struct libscols_line *add_line(struct libscols_table *tb, const char *name, const char *data)
{
	struct libscols_line *ln = scols_table_new_line(tb, NULL);
	if (!ln)
		err(EXIT_FAILURE, "failed to create output line");

	if (scols_line_set_data(ln, COL_NAME, name))
		goto fail;
	if (scols_line_set_data(ln, COL_FOO, data))
		goto fail;
	if (scols_line_set_data(ln, COL_BAR, data))
		goto fail;
	return ln;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output line");
}

int main(int argc, char *argv[])
{
	struct libscols_table *tb;
	struct libscols_column *cl;
	struct libscols_line *ln;
	struct libscols_cell *ce;
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
	add_line(tb, "AAA", "bla bla bla");
	add_line(tb, "BB", "b");
	add_line(tb, "CCCC", "fooo");
	add_line(tb, "D",   "baaar");
	add_line(tb, "EE", "eee");

	cl = scols_table_get_column(tb, 1);
	scols_column_set_color(cl, "red");		/* red column */

	cl = scols_table_get_column(tb, 2);
	scols_column_set_color(cl, "reverse");		/* reverse column */

	ln = scols_table_get_line(tb, 0);
	scols_line_set_color(ln, "\033[37;41m");	/* line with red bg */
	ce = scols_line_get_cell(ln, 0);
	scols_cell_set_color(ce, "\033[37;45m");	/* cell with purple bg */

	ln = scols_table_get_line(tb, 3);
	scols_line_set_color(ln, "\033[37;41m");	/* line with red bg */
	ce = scols_line_get_cell(ln, 2);
	scols_cell_set_color(ce, "\033[37;44m");	/* cell with blue bg */

	scols_print_table(tb);
	scols_unref_table(tb);
	return EXIT_SUCCESS;
}
