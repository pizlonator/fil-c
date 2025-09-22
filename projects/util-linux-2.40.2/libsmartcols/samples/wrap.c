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


enum { COL_NAME, COL_DESC, COL_FOO, COL_LIKE, COL_TEXT };

/* add columns to the @tb */
static void setup_columns(struct libscols_table *tb)
{
	if (!scols_table_new_column(tb, "NAME", 0, SCOLS_FL_TREE))
		goto fail;
	if (!scols_table_new_column(tb, "DESC", 0, 0))
		goto fail;
	if (!scols_table_new_column(tb, "FOO", 0, SCOLS_FL_WRAP))
		goto fail;
	if (!scols_table_new_column(tb, "LIKE", 0, SCOLS_FL_RIGHT))
		goto fail;
	if (!scols_table_new_column(tb, "TEXT", 0, SCOLS_FL_WRAP))
		goto fail;
	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output columns");
}

static char *gen_text(const char *prefix, const char *sub_prefix, char *buf, size_t sz)
{
	int x = snprintf(buf, sz,  "%s-%s-", prefix, sub_prefix);

	for ( ; (size_t)x < sz - 1; x++)
		buf[x] = *prefix;

	buf[x++] = 'x';
	buf[x] = '\0';
	return buf;
}

static struct libscols_line * add_line(	struct libscols_table *tb,
					struct libscols_line *parent,
					const char *prefix)
{
	char buf[BUFSIZ];
	struct libscols_line *ln = scols_table_new_line(tb, parent);
	if (!ln)
		err(EXIT_FAILURE, "failed to create output line");

	if (scols_line_set_data(ln, COL_NAME, gen_text(prefix, "N", buf, 15)))
		goto fail;
	if (scols_line_set_data(ln, COL_DESC, gen_text(prefix, "D", buf, 10)))
		goto fail;
	if (scols_line_set_data(ln, COL_FOO, gen_text(prefix, "U", buf, 55)))
		goto fail;
	if (scols_line_set_data(ln, COL_LIKE, "1"))
		goto fail;
	if (scols_line_set_data(ln, COL_TEXT, gen_text(prefix, "T", buf, 50)))
		goto fail;
	return ln;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output line");
}

int main(int argc, char *argv[])
{
	struct libscols_table *tb;
	struct libscols_line *ln, *xln;

	setlocale(LC_ALL, "");	/* just to have enable UTF8 chars */

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, "failed to create output table");

	if (argc > 1 && strcmp(argv[1], "--export") == 0)
		scols_table_enable_export(tb, 1);
	else if (argc > 1 && strcmp(argv[1], "--raw") == 0)
		scols_table_enable_raw(tb, 1);
	else if (argc > 1 && strcmp(argv[1], "--json") == 0)
		scols_table_enable_json(tb, 1);
	else
		scols_table_enable_colors(tb, isatty(STDOUT_FILENO));

	setup_columns(tb);

	ln = add_line(tb, NULL, "A");
	add_line(tb, ln, "aa");
	add_line(tb, ln, "ab");

	ln = add_line(tb, NULL, "B");
	xln = add_line(tb, ln, "ba");
	add_line(tb, xln, "baa");
	add_line(tb, xln, "bab");
	add_line(tb, ln, "bb");

	scols_print_table(tb);
	scols_unref_table(tb);
	return EXIT_SUCCESS;
}
