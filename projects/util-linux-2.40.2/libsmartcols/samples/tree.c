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

#include "libsmartcols.h"

static int add_children(struct libscols_table *tb,
			struct libscols_line *ln, int fd);


enum { COL_MODE, COL_SIZE, COL_NAME };

struct libscols_column *sort_column;

/* add columns to the @tb */
static void setup_columns(struct libscols_table *tb, int notree)
{
	if (!scols_table_new_column(tb, "MODE", 0.3, 0))
		goto fail;
	if (!scols_table_new_column(tb, "SIZE", 5, SCOLS_FL_RIGHT))
		goto fail;

	sort_column = scols_table_new_column(tb, "NAME", 0.5,
			(notree ? 0 : SCOLS_FL_TREE) | SCOLS_FL_NOEXTREMES);
	if (!sort_column)
		goto fail;
	scols_column_set_cmpfunc(sort_column, scols_cmpstr_cells, NULL);

	return;
fail:
	scols_unref_table(tb);
	err(EXIT_FAILURE, "failed to create output columns");
}

/* add a new line to @tb, the content is based on @st */
static int add_line_from_stat(struct libscols_table *tb,
			      struct libscols_line *parent,
			      int parent_fd,
			      struct stat *st,
			      const char *name)
{
	struct libscols_line *ln;
	char modbuf[11], *p;
	mode_t mode = st->st_mode;
	int rc = 0;

	ln = scols_table_new_line(tb, parent);
	if (!ln)
		err(EXIT_FAILURE, "failed to create output line");

	/* MODE; local buffer, use scols_line_set_data() that calls strdup() */
	xstrmode(mode, modbuf);
	if (scols_line_set_data(ln, COL_MODE, modbuf))
		goto fail;

	/* SIZE; already allocated string, use scols_line_refer_data() */
	p = size_to_human_string(0, st->st_size);
	if (!p || scols_line_refer_data(ln, COL_SIZE, p))
		goto fail;

	/* NAME */
	if (scols_line_set_data(ln, COL_NAME, name))
		goto fail;

	/* colors */
	if (scols_table_colors_wanted(tb)) {
		struct libscols_cell *ce = scols_line_get_cell(ln, COL_NAME);

		if (S_ISDIR(mode))
			scols_cell_set_color(ce, "blue");
		else if (S_ISLNK(mode))
			scols_cell_set_color(ce, "cyan");
		else if (S_ISBLK(mode))
			scols_cell_set_color(ce, "magenta");
		else if ((mode & S_IXOTH) || (mode & S_IXGRP) || (mode & S_IXUSR))
			scols_cell_set_color(ce, "green");
	}

	if (S_ISDIR(st->st_mode)) {
		int fd;

		if (parent_fd >= 0)
			fd = openat(parent_fd, name, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC);
		else
			fd = open(name, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC);
		if (fd >= 0) {
			rc = add_children(tb, ln, fd);
			close(fd);
		}
	}
	return rc;
fail:
	err(EXIT_FAILURE, "failed to create cell data");
	return -1;
}

/* read all entries from directory addressed by @fd */
static int add_children(struct libscols_table *tb,
			struct libscols_line *ln,
			int fd)
{
	DIR *dir;
	struct dirent *d;

	dir = fdopendir(fd);
	if (!dir)
		return -errno;

	while ((d = readdir(dir))) {
		struct stat st;

		if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;
		if (fstatat(fd, d->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0)
			continue;
		add_line_from_stat(tb, ln, fd, &st, d->d_name);
	}
	closedir(dir);
	return 0;
}

static void add_lines(struct libscols_table *tb, const char *dirname)
{
	struct stat st;

	if (lstat(dirname, &st))
		err(EXIT_FAILURE, "%s", dirname);

	add_line_from_stat(tb, NULL, -1, &st, dirname);
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fprintf(out, " %s [options] [<dir> ...]\n\n", program_invocation_short_name);
	fputs(" -c, --csv               display a csv-like output\n", out);
	fputs(" -i, --ascii             use ascii characters only\n", out);
	fputs(" -l, --list              use list format output\n", out);
	fputs(" -n, --noheadings        don't print headings\n", out);
	fputs(" -p, --pairs             use key=\"value\" output format\n", out);
	fputs(" -J, --json              use JSON output format\n", out);
	fputs(" -r, --raw               use raw output format\n", out);
	fputs(" -s, --sort              sort by NAME\n", out);
	fputs(" -x, --tree-sort         keep tree-like order (also for --list)\n", out);
	fputs(" -S, --range-start <n>   first line to print\n", out);
	fputs(" -E, --range-end <n>     last line to print\n", out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct libscols_table *tb;
	int c, notree = 0, nstart = -1, nend = -1, sort = 0, force_tree_sort = 0;


	static const struct option longopts[] = {
		{ "ascii",	0, NULL, 'i' },
		{ "csv",        0, NULL, 'c' },
		{ "list",       0, NULL, 'l' },
		{ "noheadings",	0, NULL, 'n' },
		{ "pairs",      0, NULL, 'p' },
		{ "json",       0, NULL, 'J' },
		{ "raw",        0, NULL, 'r' },
		{ "range-start",1, NULL, 'S' },
		{ "range-end",  1, NULL, 'E' },
		{ "sort",       0, NULL, 's' },
		{ "tree-sort",  0, NULL, 'x' },
		{ NULL, 0, NULL, 0 },
	};

	setlocale(LC_ALL, "");	/* just to have enable UTF8 chars */

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, "failed to create output table");

	while((c = getopt_long(argc, argv, "ciJlnprS:sE:x", longopts, NULL)) != -1) {
		switch(c) {
		case 'c':
			scols_table_set_column_separator(tb, ",");
			scols_table_enable_raw(tb, 1);
			notree = 1;
			break;
		case 'i':
			scols_table_enable_ascii(tb, 1);
			break;
		case 'J':
			scols_table_set_name(tb, "scolstest");
			scols_table_enable_json(tb, 1);
			break;
		case 'l':
			notree = 1;
			break;
		case 'n':
			scols_table_enable_noheadings(tb, 1);
			break;
		case 'p':
			scols_table_enable_export(tb, 1);
			notree = 1;
			break;
		case 'r':
			scols_table_enable_raw(tb, 1);
			notree = 1;
			break;
		case 'S':
			nstart = strtos32_or_err(optarg, "failed to parse range start") - 1;
			break;
		case 's':
			sort = 1;
			break;
		case 'E':
			nend = strtos32_or_err(optarg, "failed to parse range end") - 1;
			break;
		case 'x':
			force_tree_sort = 1;
			break;
		default:
			usage(stderr);
		}
	}

	scols_table_enable_colors(tb, isatty(STDOUT_FILENO));
	setup_columns(tb, notree);

	if (optind == argc)
		add_lines(tb, ".");
	else while (optind < argc)
		add_lines(tb, argv[optind++]);

	if (sort)
		scols_sort_table(tb, sort_column);
	if (force_tree_sort)
		scols_sort_table_by_tree(tb);

	if (nstart >= 0 || nend >= 0) {
		/* print subset */
		struct libscols_line *start = NULL, *end = NULL;

		if (nstart >= 0)
			start = scols_table_get_line(tb, nstart);
		if (nend >= 0)
			end = scols_table_get_line(tb, nend);

		if (start || end)
			scols_table_print_range(tb, start, end);
	} else
		/* print all table */
		scols_print_table(tb);

	scols_unref_table(tb);
	return EXIT_SUCCESS;
}
