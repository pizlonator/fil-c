/*
 * uuidparse.c --- Interpret uuid encoded information.  This program
 *	violates the UUID abstraction barrier by reaching into the
 *	guts of a UUID.
 *
 * Based on libuuid/src/uuid_time.c
 * Copyright (C) 1998, 1999 Theodore Ts'o.
 *
 * All alterations (C) 2017 Sami Kerola
 * The 3-Clause BSD License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <assert.h>
#include <getopt.h>
#include <libsmartcols.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <uuid.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "optutils.h"
#include "strutils.h"
#include "timeutils.h"
#include "xalloc.h"

/* column IDs */
enum {
	COL_UUID = 0,
	COL_VARIANT,
	COL_TYPE,
	COL_TIME
};

/* column names */
struct colinfo {
	const char *name;	/* header */
	double whint;		/* width hint (N < 1 is in percent of termwidth) */
	int flags;		/* SCOLS_FL_* */
	const char *help;
};

/* columns descriptions */
static const struct colinfo infos[] = {
	[COL_UUID]    = {"UUID",    UUID_STR_LEN, 0, N_("unique identifier")},
	[COL_VARIANT] = {"VARIANT", 9,  0, N_("variant name")},
	[COL_TYPE]    = {"TYPE",    10, 0, N_("type name")},
	[COL_TIME]    = {"TIME",    31, 0, N_("timestamp")}
};

static int columns[ARRAY_SIZE(infos) * 2];
static size_t ncolumns;

struct control {
	unsigned int
		json:1,
		no_headings:1,
		raw:1;
};

static void __attribute__((__noreturn__)) usage(void)
{
	size_t i;

	fputs(USAGE_HEADER, stdout);
	fprintf(stdout, _(" %s [options] <uuid ...>\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, stdout);
	fputsln(_(" -J, --json             use JSON output format"), stdout);
	fputsln(_(" -n, --noheadings       don't print headings"), stdout);
	fputsln(_(" -o, --output <list>    COLUMNS to display (see below)"), stdout);
	fputsln(_(" -r, --raw              use the raw output format"), stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(24));

	fputs(USAGE_COLUMNS, stdout);
	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(stdout, " %8s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(stdout, USAGE_MAN_TAIL("uuidparse(1)"));
	exit(EXIT_SUCCESS);
}

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	assert(name);

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;
		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static int get_column_id(size_t num)
{
	assert(num < ncolumns);
	assert(columns[num] < (int)ARRAY_SIZE(infos));
	return columns[num];
}

static const struct colinfo *get_column_info(int num)
{
	return &infos[get_column_id(num)];
}

static void fill_table_row(struct libscols_table *tb, char const *const uuid)
{
	static struct libscols_line *ln;
	size_t i;
	uuid_t buf;
	int invalid = 0;
	int variant = -1, type = -1;

	assert(tb);
	assert(uuid);

	ln = scols_table_new_line(tb, NULL);
	if (!ln)
		errx(EXIT_FAILURE, _("failed to allocate output line"));

	if (uuid_parse(uuid, buf))
		invalid = 1;
	else {
		variant = uuid_variant(buf);
		type = uuid_type(buf);
	}

	for (i = 0; i < ncolumns; i++) {
		char *str = NULL;

		switch (get_column_id(i)) {
		case COL_UUID:
			str = xstrdup(uuid);
			break;
		case COL_VARIANT:
			if (invalid) {
				str = xstrdup(_("invalid"));
				break;
			}
			switch (variant) {
			case UUID_VARIANT_NCS:
				str = xstrdup("NCS");
				break;
			case UUID_VARIANT_DCE:
				str = xstrdup("DCE");
				break;
			case UUID_VARIANT_MICROSOFT:
				str = xstrdup("Microsoft");
				break;
			default:
				str = xstrdup(_("other"));
			}
			break;
		case COL_TYPE:
			if (invalid) {
				str = xstrdup(_("invalid"));
				break;
			}
			switch (type) {
			case UUID_TYPE_DCE_NIL:
				if (uuid_is_null(buf))
					str = xstrdup(_("nil"));
				else
					str = xstrdup(_("unknown"));
				break;
			case UUID_TYPE_DCE_TIME:
				str = xstrdup(_("time-based"));
				break;
			case UUID_TYPE_DCE_SECURITY:
				str = xstrdup("DCE");
				break;
			case UUID_TYPE_DCE_MD5:
				str = xstrdup(_("name-based"));
				break;
			case UUID_TYPE_DCE_RANDOM:
				str = xstrdup(_("random"));
				break;
			case UUID_TYPE_DCE_SHA1:
				str = xstrdup(_("sha1-based"));
				break;
			default:
				str = xstrdup(_("unknown"));
			}
			break;
		case COL_TIME:
			if (invalid) {
				str = xstrdup(_("invalid"));
				break;
			}
			if (variant == UUID_VARIANT_DCE && type == UUID_TYPE_DCE_TIME) {
				struct timeval tv;
				char date_buf[ISO_BUFSIZ];

				uuid_time(buf, &tv);
				strtimeval_iso(&tv, ISO_TIMESTAMP_COMMA,
					       date_buf, sizeof(date_buf));
				str = xstrdup(date_buf);
			}
			break;
		default:
			abort();
		}
		if (str && scols_line_refer_data(ln, i, str))
			errx(EXIT_FAILURE, _("failed to add output data"));
	}
}

static void print_output(struct control const *const ctrl, int argc,
			 char **argv)
{
	struct libscols_table *tb;
	size_t i;

	scols_init_debug(0);
	tb = scols_new_table();
	if (!tb)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	if (ctrl->json) {
		scols_table_enable_json(tb, 1);
		scols_table_set_name(tb, "uuids");
	}
	scols_table_enable_noheadings(tb, ctrl->no_headings);
	scols_table_enable_raw(tb, ctrl->raw);

	for (i = 0; i < ncolumns; i++) {
		const struct colinfo *col = get_column_info(i);

		if (!scols_table_new_column(tb, col->name, col->whint,
					    col->flags))
			err(EXIT_FAILURE,
			    _("failed to initialize output column"));
	}

	for (i = 0; i < (size_t) argc; i++)
		fill_table_row(tb, argv[i]);

	if (i == 0) {
		char uuid[UUID_STR_LEN];

		while (scanf(" %36[^ \t\n]%*c", uuid) && !feof(stdin))
			fill_table_row(tb, uuid);
	}
	scols_print_table(tb);
	scols_unref_table(tb);
}

int main(int argc, char **argv)
{
	struct control ctrl = { 0 };
	char *outarg = NULL;
	int c;

	static const struct option longopts[] = {
		{"json",       no_argument,       NULL, 'J'},
		{"noheadings", no_argument,       NULL, 'n'},
		{"output",     required_argument, NULL, 'o'},
		{"raw",        no_argument,       NULL, 'r'},
		{"version",    no_argument,       NULL, 'V'},
		{"help",       no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	static const ul_excl_t excl[] = {
		{'J', 'r'},
		{0}
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "Jno:rVh", longopts, NULL)) != -1) {
		err_exclusive_options(c, longopts, excl, excl_st);
		switch (c) {
		case 'J':
			ctrl.json = 1;
			break;
		case 'n':
			ctrl.no_headings = 1;
			break;
		case 'o':
			outarg = optarg;
			break;
		case 'r':
			ctrl.raw = 1;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	columns[ncolumns++] = COL_UUID;
	columns[ncolumns++] = COL_VARIANT;
	columns[ncolumns++] = COL_TYPE;
	columns[ncolumns++] = COL_TIME;

	if (outarg
	    && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
				     &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	print_output(&ctrl, argc, argv);

	return EXIT_SUCCESS;
}
