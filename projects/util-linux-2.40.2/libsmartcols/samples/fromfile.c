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
#include "strutils.h"
#include "xalloc.h"
#include "optutils.h"
#include "mangle.h"
#include "path.h"

#include "libsmartcols.h"

static struct libscols_column *parse_column(const char *path)
{
	char buf[BUFSIZ];
	struct libscols_column *cl;

	if (ul_path_read_buffer(NULL, buf, sizeof(buf), path) < 0)
		err(EXIT_FAILURE, "failed to read column: %s", path);

	cl = scols_new_column();
	if (!cl)
		err(EXIT_FAILURE, "failed to allocate column");

	if (scols_column_set_properties(cl, buf) != 0)
		err(EXIT_FAILURE, "failed to set column properties");

	return cl;
}

static int parse_column_data(FILE *f, struct libscols_table *tb, int col)
{
	size_t len = 0, nlines = 0;
	ssize_t i;
	char *str = NULL, *p;

	while ((i = getline(&str, &len, f)) != -1) {
		struct libscols_line *ln;
		int rc = 0;

		ln = scols_table_get_line(tb, nlines++);
		if (!ln)
			break;

		p = strrchr(str, '\n');
		if (p)
			*p = '\0';
		if (!*str)
			continue;

		/* convert \x?? to real bytes */
		if (strstr(str, "\\x")) {
			struct libscols_cell *ce = scols_line_get_cell(ln, col);
			size_t sz = i + 1;
			char *buf = xcalloc(1, sz);

			sz = unhexmangle_to_buffer(str, buf, sz);
			if (sz)
				rc = scols_cell_refer_memory(ce, buf, sz);
			else
				free(buf);
		} else
			rc = scols_line_set_data(ln, col, str);
		if (rc)
			err(EXIT_FAILURE, "failed to add output data");
	}

	free(str);
	return 0;

}

static struct libscols_line *get_line_with_id(struct libscols_table *tb,
						int col_id, const char *id)
{
	struct libscols_line *ln;
	struct libscols_iter *itr = scols_new_iter(SCOLS_ITER_FORWARD);

	while (scols_table_next_line(tb, itr, &ln) == 0) {
		struct libscols_cell *ce = scols_line_get_cell(ln, col_id);
		const char *data = ce ? scols_cell_get_data(ce) : NULL;

		if (data && strcmp(data, id) == 0)
			break;
	}

	scols_free_iter(itr);
	return ln;
}

static void compose_tree(struct libscols_table *tb, int parent_col, int id_col)
{
	struct libscols_line *ln;
	struct libscols_iter *itr = scols_new_iter(SCOLS_ITER_FORWARD);

	while (scols_table_next_line(tb, itr, &ln) == 0) {
		struct libscols_line *parent = NULL;
		struct libscols_cell *ce = scols_line_get_cell(ln, parent_col);
		const char *data = ce ? scols_cell_get_data(ce) : NULL;

		if (data)
			parent = get_line_with_id(tb, id_col, data);
		if (parent)
			scols_line_add_child(parent, ln);
	}

	scols_free_iter(itr);
}

static struct libscols_filter *init_filter(
			struct libscols_table *tb,
			const char *query, int dump)
{
	struct libscols_iter *itr;
	struct libscols_filter *f = scols_new_filter(NULL);
	const char *name = NULL;
	int rc = 0;

	if (!f)
		err(EXIT_FAILURE, "failed to allocate filter");
	if (scols_filter_parse_string(f, query) != 0) {
		warnx("failed to parse filter: %s", scols_filter_get_errmsg(f));
		scols_unref_filter(f);
		return NULL;
	}

	itr = scols_new_iter(SCOLS_ITER_FORWARD);
	if (!itr)
		err(EXIT_FAILURE, "failed to allocate iterator");

	while (scols_filter_next_holder(f, itr, &name, 0) == 0) {
		struct libscols_column *col;

		col = scols_table_get_column_by_name(tb, name);
		if (!col) {
			warnx("unknown column '%s' in filter", name);
			rc++;
			continue;
		}
		scols_filter_assign_column(f, itr, name, col);
	}

	scols_free_iter(itr);
	if (dump && f)
		scols_dump_filter(f, stdout);
	if (rc) {
		scols_unref_filter(f);
		errx(EXIT_FAILURE, "failed to initialize filter");
	}

	return f;
}

/* Note: This is a simple (naive) way to use the filter, employed here for
 * testing functionality.
 *
 * A more effective approach to using the filter is demonstrated in lsblk.c,
 * where data manipulation is divided into two steps. The initial step prepares
 * only the data necessary for evaluating the filter, and the remaining data is
 * gathered later, only if necessary.
 */
static void apply_filter(struct libscols_table *tb, struct libscols_filter *fltr)
{
	struct libscols_iter *itr = scols_new_iter(SCOLS_ITER_FORWARD);
	struct libscols_line *ln;

	if (!itr)
		err(EXIT_FAILURE, "failed to allocate iterator");

	while (scols_table_next_line(tb, itr, &ln) == 0) {
		int status = 0;

		if (scols_line_apply_filter(ln, fltr, &status) != 0)
			err(EXIT_FAILURE, "failed to apply filter");
		if (status == 0) {
			struct libscols_line *x = scols_line_get_parent(ln);

			if (x)
				scols_line_remove_child(x, ln);
			scols_table_remove_line(tb, ln);
			ln = NULL;
		}
	}

	scols_free_iter(itr);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fprintf(out,
		"\n %s [options] <column-data-file> ...\n\n", program_invocation_short_name);

	fputs(" -m, --maxout                   fill all terminal width\n", out);
	fputs(" -M, --minout                   minimize tailing padding\n", out);
	fputs(" -c, --column <file>            column definition\n", out);
	fputs(" -n, --nlines <num>             number of lines\n", out);
	fputs(" -J, --json                     JSON output format\n", out);
	fputs(" -r, --raw                      RAW output format\n", out);
	fputs(" -E, --export                   use key=\"value\" output format\n", out);
	fputs(" -C, --colsep <str>             set columns separator\n", out);
	fputs(" -w, --width <num>              hardcode terminal width\n", out);
	fputs(" -p, --tree-parent-column <n>   parent column\n", out);
	fputs(" -i, --tree-id-column <n>       id column\n", out);
	fputs(" -Q, --filter <expr>            filter\n", out);
	fputs(" -h, --help                     this help\n", out);
	fputs("\n", out);

	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct libscols_table *tb;
	int c, n, nlines = 0, rc;
	int parent_col = -1, id_col = -1;
	int fltr_dump = 0;
	const char *fltr_str = NULL;
	struct libscols_filter *fltr = NULL;

	static const struct option longopts[] = {
		{ "maxout", 0, NULL, 'm' },
		{ "minout", 0, NULL, 'M' },
		{ "column", 1, NULL, 'c' },
		{ "nlines", 1, NULL, 'n' },
		{ "width",  1, NULL, 'w' },
		{ "tree-parent-column", 1, NULL, 'p' },
		{ "tree-id-column",	1, NULL, 'i' },
		{ "json",   0, NULL, 'J' },
		{ "raw",    0, NULL, 'r' },
		{ "export", 0, NULL, 'E' },
		{ "colsep",  1, NULL, 'C' },
		{ "filter", 1, NULL, 'Q' },
		{ "filter-dump", 0, NULL, 'd' },
		{ "help",   0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'E', 'J', 'r' },
		{ 'M', 'm' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");	/* just to have enable UTF8 chars */
	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, "failed to create output table");

	while((c = getopt_long(argc, argv, "hCc:dEi:JMmn:p:Q:rw:", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch(c) {
		case 'c': /* add column from file */
		{
			struct libscols_column *cl = parse_column(optarg);

			if (cl && scols_table_add_column(tb, cl))
				err(EXIT_FAILURE, "%s: failed to add column", optarg);
			scols_unref_column(cl);
			break;
		}
		case 'd':
			fltr_dump = 1;
			break;
		case 'p':
			parent_col = strtou32_or_err(optarg, "failed to parse tree PARENT column");
			break;
		case 'i':
			id_col = strtou32_or_err(optarg, "failed to parse tree ID column");
			break;
		case 'J':
			scols_table_enable_json(tb, 1);
			scols_table_set_name(tb, "testtable");
			break;
		case 'm':
			scols_table_enable_maxout(tb, TRUE);
			break;
		case 'M':
			scols_table_enable_minout(tb, TRUE);
			break;
		case 'r':
			scols_table_enable_raw(tb, TRUE);
			break;
		case 'E':
			scols_table_enable_export(tb, TRUE);
			break;
		case 'C':
			scols_table_set_column_separator(tb, optarg);
			break;
		case 'n':
			nlines = strtou32_or_err(optarg, "failed to parse number of lines");
			break;
		case 'Q':
			fltr_str = optarg;
			break;
		case 'w':
			scols_table_set_termforce(tb, SCOLS_TERMFORCE_ALWAYS);
			scols_table_set_termwidth(tb, strtou32_or_err(optarg, "failed to parse terminal width"));
			break;
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (nlines <= 0)
		errx(EXIT_FAILURE, "--nlines not set");

	for (n = 0; n < nlines; n++) {
		struct libscols_line *ln = scols_new_line();

		if (!ln || scols_table_add_line(tb, ln))
			err(EXIT_FAILURE, "failed to add a new line");

		scols_unref_line(ln);
	}

	if (fltr_str) {
		fltr = init_filter(tb, fltr_str, fltr_dump);
		if (!fltr) {
			rc = EXIT_FAILURE;
			goto done;
		}
	}

	n = 0;

	while (optind < argc) {
		FILE *f = fopen(argv[optind], "r");

		if (!f)
			err(EXIT_FAILURE, "%s: open failed", argv[optind]);

		parse_column_data(f, tb, n);
		optind++;
		n++;
	}

	if (scols_table_is_tree(tb) && parent_col >= 0 && id_col >= 0)
		compose_tree(tb, parent_col, id_col);

	scols_table_enable_colors(tb, isatty(STDOUT_FILENO));

	if (fltr)
		apply_filter(tb, fltr);

	scols_print_table(tb);
	rc = EXIT_SUCCESS;
done:
	scols_unref_filter(fltr);
	scols_unref_table(tb);
	return rc;
}
