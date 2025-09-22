/*
 * fincore - count pages of file contents in core
 *
 * Copyright (C) 2017 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"
#include "xalloc.h"
#include "strutils.h"
#include "blkdev.h"

#include "libsmartcols.h"

/* For large files, mmap is called in iterative way.
   Window is the unit of vma prepared in each mmap
   calling.

   Window size depends on page size.
   e.g. 128MB on x86_64. ( = N_PAGES_IN_WINDOW * 4096 ). */
#define N_PAGES_IN_WINDOW ((size_t)(32 * 1024))

#ifndef HAVE_CACHESTAT

#ifndef SYS_cachestat
#define SYS_cachestat 451
#endif

struct cachestat_range {
	uint64_t off;
	uint64_t len;
};

struct cachestat {
	uint64_t nr_cache;
	uint64_t nr_dirty;
	uint64_t nr_writeback;
	uint64_t nr_evicted;
	uint64_t nr_recently_evicted;
};

static inline int cachestat(unsigned int fd,
			    const struct cachestat_range *cstat_range,
			    struct cachestat *cstat, unsigned int flags)
{
	return syscall(SYS_cachestat, fd, cstat_range, cstat, flags);
}

#endif // HAVE_CACHESTAT

struct colinfo {
	const char * const name;
	double whint;
	int flags;
	const char *help;
	unsigned int pages : 1;
};

enum {
	COL_PAGES,
	COL_SIZE,
	COL_FILE,
	COL_RES,
	COL_DIRTY_PAGES,
	COL_DIRTY,
	COL_WRITEBACK_PAGES,
	COL_WRITEBACK,
	COL_EVICTED_PAGES,
	COL_EVICTED,
	COL_RECENTLY_EVICTED_PAGES,
	COL_RECENTLY_EVICTED,
};

static const struct colinfo infos[] = {
	[COL_PAGES]                  = { "PAGES",                  1, SCOLS_FL_RIGHT, N_("file data resident in memory in pages"), 1},
	[COL_RES]                    = { "RES",                    5, SCOLS_FL_RIGHT, N_("file data resident in memory in bytes")},
	[COL_SIZE]                   = { "SIZE",                   5, SCOLS_FL_RIGHT, N_("size of the file")},
	[COL_FILE]                   = { "FILE",                   4, 0, N_("file name")},
	[COL_DIRTY_PAGES]            = { "DIRTY_PAGES",            1, SCOLS_FL_RIGHT, N_("number of dirty pages"), 1},
	[COL_DIRTY]                  = { "DIRTY",                  5, SCOLS_FL_RIGHT, N_("number of dirty bytes")},
	[COL_WRITEBACK_PAGES]        = { "WRITEBACK_PAGES",        1, SCOLS_FL_RIGHT, N_("number of pages marked for writeback"), 1},
	[COL_WRITEBACK]              = { "WRITEBACK",              5, SCOLS_FL_RIGHT, N_("number of bytes marked for writeback")},
	[COL_EVICTED_PAGES]          = { "EVICTED_PAGES",          1, SCOLS_FL_RIGHT, N_("number of evicted pages"), 1},
	[COL_EVICTED]                = { "EVICTED",                5, SCOLS_FL_RIGHT, N_("number of evicted bytes")},
	[COL_RECENTLY_EVICTED_PAGES] = { "RECENTLY_EVICTED_PAGES", 1, SCOLS_FL_RIGHT, N_("number of recently evicted pages"), 1},
	[COL_RECENTLY_EVICTED]       = { "RECENTLY_EVICTED",       5, SCOLS_FL_RIGHT, N_("number of recently evicted bytes")},
};

static int columns[ARRAY_SIZE(infos) * 2] = {-1};
static size_t ncolumns;

struct fincore_control {
	const size_t pagesize;

	struct libscols_table *tb;		/* output */

	unsigned int bytes : 1,
		     noheadings : 1,
		     raw : 1,
		     json : 1;

};

struct fincore_state {
	const char * const name;
	long long unsigned int file_size;

	struct cachestat cstat;
	struct {
		unsigned int dirty : 1,
			     writeback : 1,
			     evicted : 1,
			     recently_evicted : 1;
	} cstat_fields;
};


static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static int get_column_id(int num)
{
	assert(num >= 0);
	assert((size_t) num < ncolumns);
	assert(columns[num] < (int) ARRAY_SIZE(infos));
	return columns[num];
}

static const struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

static int get_cstat_value(const struct fincore_state *st, int column_id,
			   uint64_t *value)
{
	switch(column_id) {
	case COL_PAGES:
	case COL_RES:
		*value = st->cstat.nr_cache;
		return 1;
	case COL_DIRTY_PAGES:
	case COL_DIRTY:
		if (!st->cstat_fields.dirty)
			break;
		*value = st->cstat.nr_dirty;
		return 1;
	case COL_WRITEBACK_PAGES:
	case COL_WRITEBACK:
		if (!st->cstat_fields.writeback)
		*value = st->cstat.nr_writeback;
		return 1;
	case COL_EVICTED_PAGES:
	case COL_EVICTED:
		if (!st->cstat_fields.evicted)
			break;
		*value = st->cstat.nr_evicted;
		return 1;
	case COL_RECENTLY_EVICTED_PAGES:
	case COL_RECENTLY_EVICTED:
		if (!st->cstat_fields.recently_evicted)
			break;
		*value = st->cstat.nr_recently_evicted;
		return 1;
	default:
		assert(0);
	}
	return 0;
}

static int add_output_data(struct fincore_control *ctl,
			   struct fincore_state *st)
{
	size_t i;
	char *tmp;
	uint64_t value = 0;
	struct libscols_line *ln;

	assert(ctl);
	assert(ctl->tb);

	ln = scols_table_new_line(ctl->tb, NULL);
	if (!ln)
		err(EXIT_FAILURE, _("failed to allocate output line"));

	for (i = 0; i < ncolumns; i++) {
		int rc = 0;
		int column_id = get_column_id(i);
		int format_value = 0;

		switch(column_id) {
		case COL_FILE:
			rc = scols_line_set_data(ln, i, st->name);
			break;
		case COL_SIZE:
			if (ctl->bytes)
				xasprintf(&tmp, "%jd", (intmax_t) st->file_size);
			else
				tmp = size_to_human_string(SIZE_SUFFIX_1LETTER, st->file_size);
			rc = scols_line_refer_data(ln, i, tmp);
			break;
		case COL_PAGES:
		case COL_RES:
		case COL_DIRTY_PAGES:
		case COL_DIRTY:
		case COL_WRITEBACK_PAGES:
		case COL_WRITEBACK:
		case COL_EVICTED:
		case COL_EVICTED_PAGES:
		case COL_RECENTLY_EVICTED:
		case COL_RECENTLY_EVICTED_PAGES:
			format_value = get_cstat_value(st, column_id, &value);
			break;
		default:
			return -EINVAL;
		}

		if (format_value) {
			if (get_column_info(i)->pages) {
				xasprintf(&tmp, "%ju", (uintmax_t) value);
			} else {
				value *= ctl->pagesize;
				if (ctl->bytes)
					xasprintf(&tmp, "%ju", (uintmax_t) value);
				else
					tmp = size_to_human_string(SIZE_SUFFIX_1LETTER, value);
			}
			rc = scols_line_refer_data(ln, i, tmp);
		}

		if (rc)
			err(EXIT_FAILURE, _("failed to add output data"));
	}

	return 0;
}

static int do_mincore(struct fincore_control *ctl,
		      void *window, const size_t len,
		      struct fincore_state *st)
{
	static unsigned char vec[N_PAGES_IN_WINDOW];
	int n = (len / ctl->pagesize) + ((len % ctl->pagesize)? 1: 0);

	if (mincore (window, len, vec) < 0) {
		warn(_("failed to do mincore: %s"), st->name);
		return -errno;
	}

	while (n > 0)
	{
		if (vec[--n] & 0x1)
		{
			vec[n] = 0;
			st->cstat.nr_cache++;
		}
	}

	return 0;
}

static int mincore_fd (struct fincore_control *ctl,
		       int fd,
		       struct fincore_state *st)
{
	size_t window_size = N_PAGES_IN_WINDOW * ctl->pagesize;
	long long unsigned int file_offset, len;
	int rc = 0;

	for (file_offset = 0; file_offset < st->file_size; file_offset += len) {
		void  *window = NULL;

		len = st->file_size - file_offset;
		if (len >= window_size)
			len = window_size;

		/* PROT_NONE is enough for Linux, but qemu-user wants PROT_READ */
		window = mmap(window, len, PROT_READ, MAP_PRIVATE, fd, file_offset);
		if (window == MAP_FAILED) {
			rc = -EINVAL;
			warn(_("failed to do mmap: %s"), st->name);
			break;
		}

		rc = do_mincore(ctl, window, len, st);
		if (rc)
			break;

		munmap (window, len);
	}

	return rc;
}

static int fincore_fd (struct fincore_control *ctl,
		       int fd,
		       struct fincore_state *st)
{
	int rc;
	const struct cachestat_range cstat_range = { 0 };

	rc = cachestat(fd, &cstat_range, &st->cstat, 0);
	if (!rc) {
		st->cstat_fields.dirty = 1;
		st->cstat_fields.writeback = 1;
		st->cstat_fields.evicted = 1;
		st->cstat_fields.recently_evicted = 1;
		return 0;
	}

	if (errno != ENOSYS)
		warn(_("failed to do cachestat: %s"), st->name);

	return mincore_fd(ctl, fd, st);
}

/*
 * Returns: <0 on error, 0 success, 1 ignore.
 */
static int fincore_name(struct fincore_control *ctl,
			struct fincore_state *st)
{
	int fd;
	int rc = 0;
	struct stat sb;

	if ((fd = open (st->name, O_RDONLY)) < 0) {
		warn(_("failed to open: %s"), st->name);
		return -errno;
	}

	if (fstat (fd, &sb) < 0) {
		warn(_("failed to do fstat: %s"), st->name);
		close (fd);
		return -errno;
	}
	st->file_size = sb.st_size;

	if (S_ISBLK(sb.st_mode)) {
		rc = blkdev_get_size(fd, &st->file_size);
		if (rc)
			warn(_("failed ioctl to get size: %s"), st->name);
	} else if (S_ISREG(sb.st_mode)) {
		st->file_size = sb.st_size;
	} else {
		rc = 1;			/* ignore things like symlinks
					 * and directories*/
	}

	if (!rc)
		rc = fincore_fd(ctl, fd, st);

	close (fd);
	return rc;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] file...\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -J, --json            use JSON output format\n"), out);
	fputs(_(" -b, --bytes           print sizes in bytes rather than in human readable format\n"), out);
	fputs(_(" -n, --noheadings      don't print headings\n"), out);
	fputs(_(" -o, --output <list>   output columns\n"), out);
	fputs(_("     --output-all      output all columns\n"), out);
	fputs(_(" -r, --raw             use raw output format\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(23));

	fputs(USAGE_COLUMNS, out);

	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(out, " %22s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, USAGE_MAN_TAIL("fincore(1)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char ** argv)
{
	int c;
	size_t i;
	int rc = EXIT_SUCCESS;
	char *outarg = NULL;

	struct fincore_control ctl = {
		.pagesize = getpagesize()
	};

	enum {
		OPT_OUTPUT_ALL = CHAR_MAX + 1
	};
	static const struct option longopts[] = {
		{ "bytes",      no_argument, NULL, 'b' },
		{ "noheadings", no_argument, NULL, 'n' },
		{ "output",     required_argument, NULL, 'o' },
		{ "output-all",	no_argument,       NULL, OPT_OUTPUT_ALL },
		{ "version",    no_argument, NULL, 'V' },
		{ "help",	no_argument, NULL, 'h' },
		{ "json",       no_argument, NULL, 'J' },
		{ "raw",        no_argument, NULL, 'r' },
		{ NULL, 0, NULL, 0 },
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long (argc, argv, "bno:JrVh", longopts, NULL)) != -1) {
		switch (c) {
		case 'b':
			ctl.bytes = 1;
			break;
		case 'n':
			ctl.noheadings = 1;
			break;
		case 'o':
			outarg = optarg;
			break;
		case OPT_OUTPUT_ALL:
			for (ncolumns = 0; ncolumns < ARRAY_SIZE(infos); ncolumns++)
				columns[ncolumns] = ncolumns;
			break;
		case 'J':
			ctl.json = 1;
			break;
		case 'r':
			ctl.raw = 1;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind == argc) {
		warnx(_("no file specified"));
		errtryhelp(EXIT_FAILURE);
	}

	if (!ncolumns) {
		columns[ncolumns++] = COL_RES;
		columns[ncolumns++] = COL_PAGES;
		columns[ncolumns++] = COL_SIZE;
		columns[ncolumns++] = COL_FILE;
	}

	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
					 &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	scols_init_debug(0);
	ctl.tb = scols_new_table();
	if (!ctl.tb)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	scols_table_enable_noheadings(ctl.tb, ctl.noheadings);
	scols_table_enable_raw(ctl.tb, ctl.raw);
	scols_table_enable_json(ctl.tb, ctl.json);
	if (ctl.json)
		scols_table_set_name(ctl.tb, "fincore");

	for (i = 0; i < ncolumns; i++) {
		const struct colinfo *col = get_column_info(i);
		struct libscols_column *cl;

		cl = scols_table_new_column(ctl.tb, col->name, col->whint, col->flags);
		if (!cl)
			err(EXIT_FAILURE, _("failed to allocate output column"));

		if (ctl.json) {
			int id = get_column_id(i);

			switch (id) {
			case COL_FILE:
				scols_column_set_json_type(cl, SCOLS_JSON_STRING);
				break;
			case COL_SIZE:
			case COL_RES:
				if (!ctl.bytes)
					break;
				/* fallthrough */
			default:
				scols_column_set_json_type(cl, SCOLS_JSON_NUMBER);
				break;
			}
		}
	}

	for(; optind < argc; optind++) {
		struct fincore_state st = {
			.name = argv[optind],
		};

		switch (fincore_name(&ctl, &st)) {
		case 0:
			add_output_data(&ctl, &st);
			break;
		case 1:
			break; /* ignore */
		default:
			rc = EXIT_FAILURE;
			break;
		}
	}

	scols_print_table(ctl.tb);
	scols_unref_table(ctl.tb);

	return rc;
}
