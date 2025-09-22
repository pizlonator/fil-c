/*
 * Copyright (C) 2016 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"

#include "libsmartcols.h"

#define TIME_PERIOD	3.0	/* seconds */

enum { COL_NUM, COL_DATA, COL_TIME };

static double time_diff(struct timeval *a, struct timeval *b)
{
	return (a->tv_sec - b->tv_sec) + (a->tv_usec - b->tv_usec) / 1E6;
}

/* add columns to the @tb */
static void setup_columns(struct libscols_table *tb)
{
	scols_table_enable_maxout(tb, 1);
	if (!scols_table_new_column(tb, "#NUM", 0.1, SCOLS_FL_RIGHT))
		goto fail;
	if (!scols_table_new_column(tb, "DATA", 0.7, 0))
		goto fail;
	if (!scols_table_new_column(tb, "TIME", 0.2, 0))
		goto fail;
	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output columns");
}

static struct libscols_line *add_line(struct libscols_table *tb, size_t i)
{
	char *p;
	struct libscols_line *ln = scols_table_new_line(tb, NULL);

	if (!ln)
		err(EXIT_FAILURE, "failed to create output line");

	xasprintf(&p, "%zu", i);
	if (scols_line_refer_data(ln, COL_NUM, p))
		goto fail;

	xasprintf(&p, "data-%02zu-%02zu-%02zu-end", i + 1, i + 2, i + 3);
	if (scols_line_refer_data(ln, COL_DATA, p))
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
	const size_t timecellsz = 500;
	struct timeval last;

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, "failed to create output table");

	setup_columns(tb);
	gettimeofday(&last, NULL);

	for (i = 0; i < 10; i++) {
		struct libscols_line *line;
		struct timeval now;
		int done = 0;
		char *timecell = xcalloc(1, timecellsz );

		line = add_line(tb, i);

		/* Make a reference from cell data to the buffer, then we can
		 * update cell data without any interaction with libsmartcols
		 */
		if (scols_line_refer_data(line, COL_TIME, timecell) != 0)
			err(EXIT_FAILURE, "failed to add data to table");

		do {
			double diff;

			gettimeofday(&now, NULL);
			diff = time_diff(&now, &last);

			if (now.tv_sec == last.tv_sec + (long) TIME_PERIOD)
				done = 1;
			else
				xusleep(100000);

			/* update "TIME" cell data */
			snprintf(timecell, timecellsz, "%f [%3d%%]", diff,
				done ? 100 : (int)(diff / (TIME_PERIOD / 100.0)));

			/* Note that libsmartcols don't print \n for last line
			 * in the table, but if you print a line somewhere in
			 * the midle of the table you need
			 *
			 *    scols_table_enable_nolinesep(tb, !done);
			 *
			 * to disable line breaks. In this example it's
			 * unnecessary as we print the latest line only.
			 */

			/* print the line */
			scols_table_print_range(tb, line, NULL);

			if (!done) {
				/* terminal is waiting for \n, fflush() to force output */
				fflush(scols_table_get_stream(tb));
				/* move to the begin of the line */
				fputc('\r', scols_table_get_stream(tb));
			} else
				fputc('\n', scols_table_get_stream(tb));
		} while (!done);

		last = now;
	}

	scols_unref_table(tb);
	return EXIT_SUCCESS;
}
