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
	if (!scols_table_new_column(tb, "NAME", 0, 0))
		goto fail;
	if (!scols_table_new_column(tb, "DATA", 0, 0))
		goto fail;
	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output columns");
}

static void add_line(struct libscols_table *tb, const char *name, const char *data)
{
	struct libscols_line *ln = scols_table_new_line(tb, NULL);
	if (!ln)
		err(EXIT_FAILURE, "failed to create output line");

	if (scols_line_set_data(ln, COL_NAME, name))
		goto fail;
	if (scols_line_set_data(ln, COL_DATA, data))
		goto fail;
	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output line");
}

int main(int argc, char *argv[])
{
	struct libscols_table *tb;
	struct libscols_symbols *sy;
	struct libscols_cell *title;
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
	add_line(tb, "foo", "bla bla bla");
	add_line(tb, "bar", "alb alb alb");

	title = scols_table_get_title(tb);

	/* right */
	scols_cell_set_data(title, "This is right title");
	scols_cell_set_color(title, "red");
	scols_cell_set_flags(title, SCOLS_CELL_FL_RIGHT);
	scols_print_table(tb);

	/* left without padding */
	scols_cell_set_data(title, "This is left title (without padding)");
	scols_cell_set_color(title, "yellow");
	scols_cell_set_flags(title, SCOLS_CELL_FL_LEFT);
	scols_print_table(tb);

	/* center */
	sy = scols_new_symbols();
	if (!sy)
		err_oom();
	scols_table_set_symbols(tb, sy);
	scols_unref_symbols(sy);

	scols_symbols_set_title_padding(sy, "=");
	scols_cell_set_data(title, "This is center title (with padding)");
	scols_cell_set_color(title, "green");
	scols_cell_set_flags(title, SCOLS_CELL_FL_CENTER);
	scols_print_table(tb);

	/* left with padding */
	scols_symbols_set_title_padding(sy, "-");
	scols_cell_set_data(title, "This is left title (with padding)");
	scols_cell_set_color(title, "blue");
	scols_cell_set_flags(title, SCOLS_CELL_FL_LEFT);
	scols_print_table(tb);


	scols_unref_table(tb);
	return EXIT_SUCCESS;
}
