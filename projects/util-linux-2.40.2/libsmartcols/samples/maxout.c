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
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "libsmartcols.h"

enum { COL_LEFT, COL_FOO, COL_RIGHT };

int main(void)
{
	struct libscols_table *tb;
	int rc = -1, nlines = 3;

	setlocale(LC_ALL, "");	/* just to have enable UTF8 chars */

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, "failed to create output table");

	scols_table_enable_maxout(tb, TRUE);
	if (!scols_table_new_column(tb, "LEFT", 0, 0))
		goto done;
	if (!scols_table_new_column(tb, "FOO", 0, 0))
		goto done;
	if (!scols_table_new_column(tb, "RIGHT", 0, SCOLS_FL_RIGHT))
		goto done;

	while (nlines--) {
		struct libscols_line *ln = scols_table_new_line(tb, NULL);

		rc = scols_line_set_data(ln, COL_LEFT, "A");
		if (!rc)
			rc = scols_line_set_data(ln, COL_FOO, "B");
		if (!rc)
			rc = scols_line_set_data(ln, COL_RIGHT, "C");
		if (rc)
			err(EXIT_FAILURE, "failed to set line data");
	}

	scols_print_table(tb);
	rc = 0;
done:
	scols_unref_table(tb);
	return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
