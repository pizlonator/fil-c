/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * lsmem - Show memory configuration
 *
 * Copyright IBM Corp. 2016
 * Copyright (C) 2016 Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <c.h>
#include <nls.h>
#include <path.h>
#include <strutils.h>
#include <closestream.h>
#include <xalloc.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <assert.h>
#include <optutils.h>
#include <libsmartcols.h>

#define _PATH_SYS_MEMORY		"/sys/devices/system/memory"

#define MEMORY_STATE_ONLINE		0
#define MEMORY_STATE_OFFLINE		1
#define MEMORY_STATE_GOING_OFFLINE	2
#define MEMORY_STATE_UNKNOWN		3

enum zone_id {
	ZONE_DMA = 0,
	ZONE_DMA32,
	ZONE_NORMAL,
	ZONE_HIGHMEM,
	ZONE_MOVABLE,
	ZONE_DEVICE,
	ZONE_NONE,
	ZONE_UNKNOWN,
	MAX_NR_ZONES,
};

struct memory_block {
	uint64_t	index;
	uint64_t	count;
	int		state;
	int		node;
	int		nr_zones;
	int		zones[MAX_NR_ZONES];
	unsigned int	removable:1;
};

struct lsmem {
	struct path_cxt		*sysmem;		/* _PATH_SYS_MEMORY directory handler */
	struct dirent		**dirs;
	int			ndirs;
	struct memory_block	*blocks;
	int			nblocks;
	uint64_t		block_size;
	uint64_t		mem_online;
	uint64_t		mem_offline;

	struct libscols_table	*table;
	unsigned int		have_nodes : 1,
				raw : 1,
				export : 1,
				json : 1,
				noheadings : 1,
				summary : 1,
				list_all : 1,
				bytes : 1,
				want_summary : 1,
				want_table : 1,
				split_by_node : 1,
				split_by_state : 1,
				split_by_removable : 1,
				split_by_zones : 1,
				have_zones : 1;
};


enum {
	COL_RANGE,
	COL_SIZE,
	COL_STATE,
	COL_REMOVABLE,
	COL_BLOCK,
	COL_NODE,
	COL_ZONES,
};

static char *zone_names[] = {
	[ZONE_DMA]	= "DMA",
	[ZONE_DMA32]	= "DMA32",
	[ZONE_NORMAL]	= "Normal",
	[ZONE_HIGHMEM]	= "Highmem",
	[ZONE_MOVABLE]	= "Movable",
	[ZONE_DEVICE]	= "Device",
	[ZONE_NONE]	= "None",	/* block contains more than one zone, can't be offlined */
	[ZONE_UNKNOWN]	= "Unknown",
};

/* column names */
struct coldesc {
	const char	*name;		/* header */
	double		whint;		/* width hint (N < 1 is in percent of termwidth) */
	int		flags;		/* SCOLS_FL_* */
	const char      *help;
};

/* columns descriptions */
static struct coldesc coldescs[] = {
	[COL_RANGE]	= { "RANGE", 0, 0, N_("start and end address of the memory range")},
	[COL_SIZE]	= { "SIZE", 5, SCOLS_FL_RIGHT, N_("size of the memory range")},
	[COL_STATE]	= { "STATE", 0, SCOLS_FL_RIGHT, N_("online status of the memory range")},
	[COL_REMOVABLE]	= { "REMOVABLE", 0, SCOLS_FL_RIGHT, N_("memory is removable")},
	[COL_BLOCK]	= { "BLOCK", 0, SCOLS_FL_RIGHT, N_("memory block number or blocks range")},
	[COL_NODE]	= { "NODE", 0, SCOLS_FL_RIGHT, N_("numa node of memory")},
	[COL_ZONES]	= { "ZONES", 0, SCOLS_FL_RIGHT, N_("valid zones for the memory range")},
};

/* columns[] array specifies all currently wanted output column. The columns
 * are defined by coldescs[] array and you can specify (on command line) each
 * column twice. That's enough, dynamically allocated array of the columns is
 * unnecessary overkill and over-engineering in this case */
static int columns[ARRAY_SIZE(coldescs) * 2];
static size_t ncolumns;

static inline size_t err_columns_index(size_t arysz, size_t idx)
{
	if (idx >= arysz)
		errx(EXIT_FAILURE, _("too many columns specified, "
				     "the limit is %zu columns"),
				arysz - 1);
	return idx;
}

/*
 * name must be null-terminated
 */
static int zone_name_to_id(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(zone_names); i++) {
		if (!strcasecmp(name, zone_names[i]))
			return i;
	}
	return ZONE_UNKNOWN;
}

#define add_column(ary, n, id)	\
		((ary)[ err_columns_index(ARRAY_SIZE(ary), (n)) ] = (id))

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(coldescs); i++) {
		const char *cn = coldescs[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static inline int get_column_id(int num)
{
	assert(num >= 0);
	assert((size_t) num < ncolumns);
	assert(columns[num] < (int) ARRAY_SIZE(coldescs));

	return columns[num];
}

static inline struct coldesc *get_column_desc(int num)
{
	return &coldescs[ get_column_id(num) ];
}

static inline void reset_split_policy(struct lsmem *l, int enable)
{
	l->split_by_state = enable;
	l->split_by_node = enable;
	l->split_by_removable = enable;
	l->split_by_zones = enable;
}

static void set_split_policy(struct lsmem *l, int cols[], size_t ncols)
{
	size_t i;

	reset_split_policy(l, 0);

	for (i = 0; i < ncols; i++) {
		switch (cols[i]) {
		case COL_STATE:
			l->split_by_state = 1;
			break;
		case COL_NODE:
			l->split_by_node = 1;
			break;
		case COL_REMOVABLE:
			l->split_by_removable = 1;
			break;
		case COL_ZONES:
			l->split_by_zones = 1;
			break;
		default:
			break;
		}
	}
}

static void add_scols_line(struct lsmem *lsmem, struct memory_block *blk)
{
	size_t i;
	struct libscols_line *line;

	line = scols_table_new_line(lsmem->table, NULL);
	if (!line)
		err_oom();

	for (i = 0; i < ncolumns; i++) {
		char *str = NULL;

		switch (get_column_id(i)) {
		case COL_RANGE:
		{
			uint64_t start = blk->index * lsmem->block_size;
			uint64_t size = blk->count * lsmem->block_size;
			xasprintf(&str, "0x%016"PRIx64"-0x%016"PRIx64, start, start + size - 1);
			break;
		}
		case COL_SIZE:
			if (lsmem->bytes)
				xasprintf(&str, "%"PRId64, (uint64_t) blk->count * lsmem->block_size);
			else
				str = size_to_human_string(SIZE_SUFFIX_1LETTER,
						(uint64_t) blk->count * lsmem->block_size);
			break;
		case COL_STATE:
			str = xstrdup(
				blk->state == MEMORY_STATE_ONLINE ? _("online") :
				blk->state == MEMORY_STATE_OFFLINE ? _("offline") :
				blk->state == MEMORY_STATE_GOING_OFFLINE ? _("on->off") :
				"?");
			break;
		case COL_REMOVABLE:
			if (blk->state == MEMORY_STATE_ONLINE)
				str = xstrdup(blk->removable ? _("yes") : _("no"));
			break;
		case COL_BLOCK:
			if (blk->count == 1)
				xasprintf(&str, "%"PRId64, blk->index);
			else
				xasprintf(&str, "%"PRId64"-%"PRId64,
					 blk->index, blk->index + blk->count - 1);
			break;
		case COL_NODE:
			if (lsmem->have_nodes)
				xasprintf(&str, "%d", blk->node);
			break;
		case COL_ZONES:
			if (lsmem->have_zones) {
				char valid_zones[BUFSIZ];
				int j, zone_id;

				valid_zones[0] = '\0';
				for (j = 0; j < blk->nr_zones; j++) {
					zone_id = blk->zones[j];
					if (strlen(valid_zones) +
					    strlen(zone_names[zone_id]) > BUFSIZ - 2)
						break;
					strcat(valid_zones, zone_names[zone_id]);
					if (j + 1 < blk->nr_zones)
						strcat(valid_zones, "/");
				}
				str = xstrdup(valid_zones);
			}
			break;
		}

		if (str && scols_line_refer_data(line, i, str) != 0)
			err_oom();
	}
}

static void fill_scols_table(struct lsmem *lsmem)
{
	int i;

	for (i = 0; i < lsmem->nblocks; i++)
		add_scols_line(lsmem, &lsmem->blocks[i]);
}

static void print_summary(struct lsmem *lsmem)
{
	if (lsmem->bytes) {
		printf("%-23s %15"PRId64"\n",_("Memory block size:"), lsmem->block_size);
		printf("%-23s %15"PRId64"\n",_("Total online memory:"), lsmem->mem_online);
		printf("%-23s %15"PRId64"\n",_("Total offline memory:"), lsmem->mem_offline);
	} else {
		char *p;

		if ((p = size_to_human_string(SIZE_SUFFIX_1LETTER, lsmem->block_size)))
			printf("%-23s %5s\n",_("Memory block size:"), p);
		free(p);

		if ((p = size_to_human_string(SIZE_SUFFIX_1LETTER, lsmem->mem_online)))
			printf("%-23s %5s\n",_("Total online memory:"), p);
		free(p);

		if ((p = size_to_human_string(SIZE_SUFFIX_1LETTER, lsmem->mem_offline)))
			printf("%-23s %5s\n",_("Total offline memory:"), p);
		free(p);
	}
}

static int memory_block_get_node(struct lsmem *lsmem, char *name)
{
	struct dirent *de;
	DIR *dir;
	int node;

	dir = ul_path_opendir(lsmem->sysmem, name);
	if (!dir)
		err(EXIT_FAILURE, _("Failed to open %s"), name);

	node = -1;
	while ((de = readdir(dir)) != NULL) {
		if (strncmp("node", de->d_name, 4) != 0)
			continue;
		if (!isdigit_string(de->d_name + 4))
			continue;
		errno = 0;
		node = strtol(de->d_name + 4, NULL, 10);
		if (errno)
			continue;
		break;
	}
	closedir(dir);
	return node;
}

static int memory_block_read_attrs(struct lsmem *lsmem, char *name,
				    struct memory_block *blk)
{
	char *line = NULL;
	int i, x = 0, rc = 0;

	memset(blk, 0, sizeof(*blk));

	errno = 0;
	blk->count = 1;
	blk->state = MEMORY_STATE_UNKNOWN;
	blk->index = strtoumax(name + 6, NULL, 10); /* get <num> of "memory<num>" */

	if (errno)
		rc = -errno;

	if (ul_path_readf_s32(lsmem->sysmem, &x, "%s/removable", name) == 0)
		blk->removable = x == 1;

	if (ul_path_readf_string(lsmem->sysmem, &line, "%s/state", name) > 0 && line) {
		if (strcmp(line, "offline") == 0)
			blk->state = MEMORY_STATE_OFFLINE;
		else if (strcmp(line, "online") == 0)
			blk->state = MEMORY_STATE_ONLINE;
		else if (strcmp(line, "going-offline") == 0)
			blk->state = MEMORY_STATE_GOING_OFFLINE;
		free(line);
	}

	if (lsmem->have_nodes)
		blk->node = memory_block_get_node(lsmem, name);

	blk->nr_zones = 0;
	if (lsmem->have_zones
	    && ul_path_readf_string(lsmem->sysmem, &line, "%s/valid_zones", name) > 0
	    && line) {

		char *token = strtok(line, " ");

		for (i = 0; token && i < MAX_NR_ZONES; i++) {
			blk->zones[i] = zone_name_to_id(token);
			blk->nr_zones++;
			token = strtok(NULL, " ");
		}
		free(line);
	}

	return rc;
}

static int is_mergeable(struct lsmem *lsmem, struct memory_block *blk)
{
	struct memory_block *curr;
	int i;

	if (!lsmem->nblocks)
		return 0;
	curr = &lsmem->blocks[lsmem->nblocks - 1];
	if (lsmem->list_all)
		return 0;
	if (curr->index + curr->count != blk->index)
		return 0;
	if (lsmem->split_by_state && curr->state != blk->state)
		return 0;
	if (lsmem->split_by_removable && curr->removable != blk->removable)
		return 0;
	if (lsmem->split_by_node && lsmem->have_nodes) {
		if (curr->node != blk->node)
			return 0;
	}
	if (lsmem->split_by_zones && lsmem->have_zones) {
		if (curr->nr_zones != blk->nr_zones)
			return 0;
		for (i = 0; i < curr->nr_zones; i++) {
			if (curr->zones[i] == ZONE_UNKNOWN ||
			    curr->zones[i] != blk->zones[i])
				return 0;
		}
	}
	return 1;
}

static void free_info(struct lsmem *lsmem)
{
	int i;

	if (!lsmem)
		return;
	free(lsmem->blocks);
	for (i = 0; i < lsmem->ndirs; i++)
		free(lsmem->dirs[i]);
	free(lsmem->dirs);
}

static void read_info(struct lsmem *lsmem)
{
	struct memory_block blk;
	char buf[128];
	int i;

	if (ul_path_read_buffer(lsmem->sysmem, buf, sizeof(buf), "block_size_bytes") <= 0)
		err(EXIT_FAILURE, _("failed to read memory block size"));

	errno = 0;
	lsmem->block_size = strtoumax(buf, NULL, 16);
	if (errno)
		err(EXIT_FAILURE, _("failed to read memory block size"));

	for (i = 0; i < lsmem->ndirs; i++) {
		memory_block_read_attrs(lsmem, lsmem->dirs[i]->d_name, &blk);
		if (blk.state == MEMORY_STATE_ONLINE)
			lsmem->mem_online += lsmem->block_size;
		else
			lsmem->mem_offline += lsmem->block_size;
		if (is_mergeable(lsmem, &blk)) {
			lsmem->blocks[lsmem->nblocks - 1].count++;
			continue;
		}
		lsmem->nblocks++;
		lsmem->blocks = xreallocarray(lsmem->blocks, lsmem->nblocks, sizeof(blk));
		*&lsmem->blocks[lsmem->nblocks - 1] = blk;
	}
}

static int memory_block_filter(const struct dirent *de)
{
	if (strncmp("memory", de->d_name, 6) != 0)
		return 0;
	return isdigit_string(de->d_name + 6);
}

static void read_basic_info(struct lsmem *lsmem)
{
	char dir[PATH_MAX];

	if (ul_path_access(lsmem->sysmem, F_OK, "block_size_bytes") != 0)
		errx(EXIT_FAILURE, _("This system does not support memory blocks"));

	ul_path_get_abspath(lsmem->sysmem, dir, sizeof(dir), NULL);

	lsmem->ndirs = scandir(dir, &lsmem->dirs, memory_block_filter, versionsort);
	if (lsmem->ndirs <= 0)
		err(EXIT_FAILURE, _("Failed to read %s"), dir);

	if (memory_block_get_node(lsmem, lsmem->dirs[0]->d_name) != -1)
		lsmem->have_nodes = 1;

	/* The valid_zones sysmem attribute was introduced with kernel 3.18 */
	if (ul_path_access(lsmem->sysmem, F_OK, "memory0/valid_zones") == 0)
		lsmem->have_zones = 1;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("List the ranges of available memory with their online status.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -J, --json           use JSON output format\n"), out);
	fputs(_(" -P, --pairs          use key=\"value\" output format\n"), out);
	fputs(_(" -a, --all            list each individual memory block\n"), out);
	fputs(_(" -b, --bytes          print SIZE in bytes rather than in human readable format\n"), out);
	fputs(_(" -n, --noheadings     don't print headings\n"), out);
	fputs(_(" -o, --output <list>  output columns\n"), out);
	fputs(_("     --output-all     output all columns\n"), out);
	fputs(_(" -r, --raw            use raw output format\n"), out);
	fputs(_(" -S, --split <list>   split ranges by specified columns\n"), out);
	fputs(_(" -s, --sysroot <dir>  use the specified directory as system root\n"), out);
	fputs(_("     --summary[=when] print summary information (never,always or only)\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(22));

	fputs(USAGE_COLUMNS, out);
	for (i = 0; i < ARRAY_SIZE(coldescs); i++)
		fprintf(out, " %10s  %s\n", coldescs[i].name, _(coldescs[i].help));

	fprintf(out, USAGE_MAN_TAIL("lsmem(1)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct lsmem _lsmem = {
			.want_table = 1,
			.want_summary = 1
		}, *lsmem = &_lsmem;

	const char *outarg = NULL, *splitarg = NULL, *prefix = NULL;
	int c;
	size_t i;

	enum {
		LSMEM_OPT_SUMARRY = CHAR_MAX + 1,
		OPT_OUTPUT_ALL
	};

	static const struct option longopts[] = {
		{"all",		no_argument,		NULL, 'a'},
		{"bytes",	no_argument,		NULL, 'b'},
		{"help",	no_argument,		NULL, 'h'},
		{"json",	no_argument,		NULL, 'J'},
		{"noheadings",	no_argument,		NULL, 'n'},
		{"output",	required_argument,	NULL, 'o'},
		{"output-all",	no_argument,		NULL, OPT_OUTPUT_ALL},
		{"pairs",	no_argument,		NULL, 'P'},
		{"raw",		no_argument,		NULL, 'r'},
		{"sysroot",	required_argument,	NULL, 's'},
		{"split",       required_argument,      NULL, 'S'},
		{"version",	no_argument,		NULL, 'V'},
		{"summary",     optional_argument,	NULL, LSMEM_OPT_SUMARRY },
		{NULL,		0,			NULL, 0}
	};
	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'J', 'P', 'r' },
		{ 'S', 'a' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "abhJno:PrS:s:V", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'a':
			lsmem->list_all = 1;
			break;
		case 'b':
			lsmem->bytes = 1;
			break;
		case 'J':
			lsmem->json = 1;
			lsmem->want_summary = 0;
			break;
		case 'n':
			lsmem->noheadings = 1;
			break;
		case 'o':
			outarg = optarg;
			break;
		case OPT_OUTPUT_ALL:
			for (ncolumns = 0; (size_t)ncolumns < ARRAY_SIZE(coldescs); ncolumns++)
				columns[ncolumns] = ncolumns;
			break;
		case 'P':
			lsmem->export = 1;
			lsmem->want_summary = 0;
			break;
		case 'r':
			lsmem->raw = 1;
			lsmem->want_summary = 0;
			break;
		case 's':
			prefix = optarg;
			break;
		case 'S':
			splitarg = optarg;
			break;
		case LSMEM_OPT_SUMARRY:
			if (optarg) {
				if (strcmp(optarg, "never") == 0)
					lsmem->want_summary = 0;
				else if (strcmp(optarg, "only") == 0)
					lsmem->want_table = 0;
				else if (strcmp(optarg, "always") == 0)
					lsmem->want_summary = 1;
				else
					errx(EXIT_FAILURE, _("unsupported --summary argument"));
			} else
				lsmem->want_table = 0;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (argc != optind) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	if (lsmem->want_table + lsmem->want_summary == 0)
		errx(EXIT_FAILURE, _("options --{raw,json,pairs} and --summary=only are mutually exclusive"));

	ul_path_init_debug();

	lsmem->sysmem = ul_new_path(_PATH_SYS_MEMORY);
	if (!lsmem->sysmem)
		err(EXIT_FAILURE, _("failed to initialize %s handler"), _PATH_SYS_MEMORY);
	if (prefix && ul_path_set_prefix(lsmem->sysmem, prefix) != 0)
		err(EXIT_FAILURE, _("invalid argument to --sysroot"));
	if (!ul_path_is_accessible(lsmem->sysmem))
		err(EXIT_FAILURE, _("cannot open %s"), _PATH_SYS_MEMORY);

	/* Shortcut to avoid scols machinery on --summary=only */
	if (lsmem->want_table == 0 && lsmem->want_summary) {
		read_basic_info(lsmem);
		read_info(lsmem);
		print_summary(lsmem);
		return EXIT_SUCCESS;
	}

	/*
	 * Default columns
	 */
	if (!ncolumns) {
		add_column(columns, ncolumns++, COL_RANGE);
		add_column(columns, ncolumns++, COL_SIZE);
		add_column(columns, ncolumns++, COL_STATE);
		add_column(columns, ncolumns++, COL_REMOVABLE);
		add_column(columns, ncolumns++, COL_BLOCK);
	}

	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
					 &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	/*
	 * Initialize output
	 */
	scols_init_debug(0);

	if (!(lsmem->table = scols_new_table()))
		errx(EXIT_FAILURE, _("failed to initialize output table"));
	scols_table_enable_raw(lsmem->table, lsmem->raw);
	scols_table_enable_export(lsmem->table, lsmem->export);
	scols_table_enable_json(lsmem->table, lsmem->json);
	scols_table_enable_noheadings(lsmem->table, lsmem->noheadings);

	if (lsmem->json)
		scols_table_set_name(lsmem->table, "memory");

	for (i = 0; i < ncolumns; i++) {
		struct coldesc *ci = get_column_desc(i);
		struct libscols_column *cl;

		cl = scols_table_new_column(lsmem->table, ci->name, ci->whint, ci->flags);
		if (!cl)
			err(EXIT_FAILURE, _("Failed to initialize output column"));

		if (lsmem->json) {
			int id = get_column_id(i);

			switch (id) {
			case COL_SIZE:
				if (!lsmem->bytes)
					break;
				/* fallthrough */
			case COL_NODE:
				scols_column_set_json_type(cl, SCOLS_JSON_NUMBER);
				break;
			case COL_REMOVABLE:
				scols_column_set_json_type(cl, SCOLS_JSON_BOOLEAN);
				break;
			}
		}
	}

	if (splitarg) {
		int split[ARRAY_SIZE(coldescs)] = { 0 };
		static size_t nsplits = 0;

		if (strcasecmp(splitarg, "none") == 0)
			;
		else if (string_add_to_idarray(splitarg, split, ARRAY_SIZE(split),
					&nsplits, column_name_to_id) < 0)
			return EXIT_FAILURE;

		set_split_policy(lsmem, split, nsplits);

	} else
		/* follow output columns */
		set_split_policy(lsmem, columns, ncolumns);

	/*
	 * Read data and print output
	 */
	read_basic_info(lsmem);
	read_info(lsmem);

	if (lsmem->want_table) {
		fill_scols_table(lsmem);
		scols_print_table(lsmem->table);

		if (lsmem->want_summary)
			fputc('\n', stdout);
	}

	if (lsmem->want_summary)
		print_summary(lsmem);

	scols_unref_table(lsmem->table);
	ul_unref_path(lsmem->sysmem);
	free_info(lsmem);
	return 0;
}
