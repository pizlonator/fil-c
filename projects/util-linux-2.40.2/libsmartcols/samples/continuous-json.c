/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright (C) 2016 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "c.h"
#include "xalloc.h"

#include "libsmartcols.h"

/* add columns to the @tb */
static void setup_columns(struct libscols_table *tb)
{
	scols_table_enable_maxout(tb, 1);
	if (!scols_table_new_column(tb, "COUNT", 0.1, SCOLS_FL_RIGHT))
		goto fail;
	if (!scols_table_new_column(tb, "TEXT", 0.9, 0))
		goto fail;
	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output columns");
}

static struct libscols_line *add_line(struct libscols_table *tb, int i)
{
	char *p;
	struct libscols_line *ln = scols_table_new_line(tb, NULL);

	if (!ln)
		err(EXIT_FAILURE, "failed to create output line");

	xasprintf(&p, "%d", i);
	if (scols_line_refer_data(ln, 0, p))
		goto fail;

	xasprintf(&p, "text%d", i);
	if (scols_line_refer_data(ln, 1, p))
		goto fail;

	return ln;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output line");
}

int main(void)
{
	struct libscols_table *tb;
	size_t i;

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, "failed to create output table");
	scols_table_enable_json(tb, 1);

	setup_columns(tb);

	for (i = 0; i < 10; i++) {
		struct libscols_line *line;

		line = add_line(tb, i);

		/* print the line */
		scols_table_print_range(tb, line, NULL);

		fflush(scols_table_get_stream(tb));
	}

	scols_unref_table(tb);
	return EXIT_SUCCESS;
}
