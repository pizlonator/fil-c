/*
 * lsclocks(1) - display system clocks
 *
 * Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
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

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include <getopt.h>
#include <glob.h>
#include <sys/ioctl.h>

#include <libsmartcols.h>

#include <linux/rtc.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "timeutils.h"
#include "closestream.h"
#include "xalloc.h"
#include "pathnames.h"
#include "all-io.h"
#include "list.h"

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME			0
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC			1
#endif

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW		4
#endif

#ifndef CLOCK_REALTIME_COARSE
#define CLOCK_REALTIME_COARSE		5
#endif

#ifndef CLOCK_MONOTONIC_COARSE
#define CLOCK_MONOTONIC_COARSE		6
#endif

#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME			7
#endif

#ifndef CLOCK_REALTIME_ALARM
#define CLOCK_REALTIME_ALARM		8
#endif

#ifndef CLOCK_BOOTTIME_ALARM
#define CLOCK_BOOTTIME_ALARM		9
#endif

#ifndef CLOCK_TAI
#define CLOCK_TAI			11
#endif

enum CLOCK_TYPE {
	CT_SYS,
	CT_PTP,
	CT_CPU,
	CT_RTC,
};

static const char *clock_type_name(enum CLOCK_TYPE type)
{
	switch (type) {
	case CT_SYS:
		return "sys";
	case CT_PTP:
		return "ptp";
	case CT_CPU:
		return "cpu";
	case CT_RTC:
		return "rtc";
	}
	errx(EXIT_FAILURE, _("Unknown clock type %d"), type);
}

struct clockinfo {
	enum CLOCK_TYPE type;
	clockid_t id;
	const char * const id_name;
	const char * const name;
	const char * const ns_offset_name;
	bool no_id;
};

static const struct clockinfo clocks[] = {
	{ CT_SYS, CLOCK_REALTIME,         "CLOCK_REALTIME",         "realtime"         },
	{ CT_SYS, CLOCK_MONOTONIC,        "CLOCK_MONOTONIC",        "monotonic",
	  .ns_offset_name = "monotonic"						       },
	{ CT_SYS, CLOCK_MONOTONIC_RAW,    "CLOCK_MONOTONIC_RAW",    "monotonic-raw"    },
	{ CT_SYS, CLOCK_REALTIME_COARSE,  "CLOCK_REALTIME_COARSE",  "realtime-coarse"  },
	{ CT_SYS, CLOCK_MONOTONIC_COARSE, "CLOCK_MONOTONIC_COARSE", "monotonic-coarse" },
	{ CT_SYS, CLOCK_BOOTTIME,         "CLOCK_BOOTTIME",         "boottime",
	  .ns_offset_name = "boottime"						       },
	{ CT_SYS, CLOCK_REALTIME_ALARM,   "CLOCK_REALTIME_ALARM",   "realtime-alarm"   },
	{ CT_SYS, CLOCK_BOOTTIME_ALARM,   "CLOCK_BOOTTIME_ALARM",   "boottime-alarm"   },
	{ CT_SYS, CLOCK_TAI,              "CLOCK_TAI",              "tai"              },
};

/* column IDs */
enum {
	COL_TYPE,
	COL_ID,
	COL_CLOCK,
	COL_NAME,
	COL_TIME,
	COL_ISO_TIME,
	COL_RESOL,
	COL_RESOL_RAW,
	COL_REL_TIME,
	COL_NS_OFFSET,
};

/* column names */
struct colinfo {
	const char * const	name; /* header */
	double			whint; /* width hint (N < 1 is in percent of termwidth) */
	int			flags; /* SCOLS_FL_* */
	int			json_type; /* SCOLS_JSON_* */
	const char * const	help;
};

/* columns descriptions */
static const struct colinfo infos[] = {
	[COL_TYPE]       = { "TYPE",       1, 0,              SCOLS_JSON_STRING, N_("type") },
	[COL_ID]         = { "ID",         1, SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER, N_("numeric id") },
	[COL_CLOCK]      = { "CLOCK",      1, 0,              SCOLS_JSON_STRING, N_("symbolic name") },
	[COL_NAME]       = { "NAME",       1, 0,              SCOLS_JSON_STRING, N_("readable name") },
	[COL_TIME]       = { "TIME",       1, SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER, N_("numeric time") },
	[COL_ISO_TIME]   = { "ISO_TIME",   1, SCOLS_FL_RIGHT, SCOLS_JSON_STRING, N_("human readable ISO time") },
	[COL_RESOL]      = { "RESOL",      1, SCOLS_FL_RIGHT, SCOLS_JSON_STRING, N_("human readable resolution") },
	[COL_RESOL_RAW]  = { "RESOL_RAW",  1, SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER, N_("resolution") },
	[COL_REL_TIME]   = { "REL_TIME",   1, SCOLS_FL_RIGHT, SCOLS_JSON_STRING, N_("human readable relative time") },
	[COL_NS_OFFSET]  = { "NS_OFFSET",  1, SCOLS_FL_RIGHT, SCOLS_JSON_NUMBER, N_("namespace offset") },
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

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -J, --json                 use JSON output format\n"), out);
	fputs(_(" -n, --noheadings           don't print headings\n"), out);
	fputs(_(" -o, --output <list>        output columns\n"), out);
	fputs(_("     --output-all           output all columns\n"), out);
	fputs(_(" -r, --raw                  use raw output format\n"), out);
	fputs(_(" -t, --time <clock>         show current time of single clock\n"), out);
	fputs(_(" --no-discover-dynamic      do not try to discover dynamic clocks\n"), out);
	fputs(_(" -d, --dynamic-clock <path> also display specified dynamic clock\n"), out);
	fputs(_(" -c, --cpu-clock <pid>      also display CPU clock of specified process\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(29));

	fputs(USAGE_COLUMNS, out);

	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(out, " %16s  %-10s%s\n", infos[i].name,
			infos[i].json_type == SCOLS_JSON_STRING?  "<string>":
			infos[i].json_type == SCOLS_JSON_ARRAY_STRING?  "<string>":
			infos[i].json_type == SCOLS_JSON_ARRAY_NUMBER?  "<string>":
			infos[i].json_type == SCOLS_JSON_NUMBER?  "<number>":
			"<boolean>",
			_(infos[i].help));

	fprintf(out, USAGE_MAN_TAIL("lsclocks(1)"));

	exit(EXIT_SUCCESS);
}

__attribute__ ((__format__ (__printf__, 3, 4)))
static void scols_line_asprintf(struct libscols_line *ln, size_t n, const char *format, ...)
{
	char *data;
	va_list args;

	va_start(args, format);
	xvasprintf(&data, format, args);
	va_end(args);

	scols_line_refer_data(ln, n, data);
}

static void scols_line_format_timespec(struct libscols_line *ln, size_t n, const struct timespec *ts)
{
	scols_line_asprintf(ln, n, "%ju.%09" PRId32, (uintmax_t) ts->tv_sec, (uint32_t) ts->tv_nsec);
}

static clockid_t parse_clock(const char *name)
{
	size_t i;
	uint32_t id = -1;
	int rc;

	rc = ul_strtou32(name, &id, 10);

	for (i = 0; i < ARRAY_SIZE(clocks); i++) {
		if (!strcmp(name, clocks[i].id_name)
		    || !strcmp(name, clocks[i].name))
			return clocks[i].id;
		if (rc == 0 && (clockid_t) id == clocks[i].id)
			return id;
	}

	errx(EXIT_FAILURE, _("Unknown clock: %s"), name);
}

static int64_t get_namespace_offset(const char *name)
{
	char *tokstr, *buf, *saveptr, *line, *space;
	uint64_t ret;
	int fd;

	fd = open(_PATH_PROC_TIMENS_OFF, O_RDONLY);
	if (fd == -1)
		err(EXIT_FAILURE, _("Could not open %s"), _PATH_PROC_TIMENS_OFF);

	read_all_alloc(fd, &buf);

	for (tokstr = buf; ; tokstr = NULL) {
		line = strtok_r(tokstr, "\n", &saveptr);
		if (!line)
			continue;
		line = (char *) startswith(line, name);
		if (!line || line[0] != ' ')
			continue;

		line = (char *) skip_blank(line);
		space = strchr(line, ' ');
		if (space)
			*space = '\0';
		ret = strtos64_or_err(line, _("Invalid offset"));
		break;
	}

	free(buf);
	close(fd);
	return ret;
}

static void add_clock_line(struct libscols_table *tb, const int *columns,
			   size_t ncolumns, const struct clockinfo *clockinfo,
			   const struct timespec *now, const struct timespec *resolution)
{
	char buf[FORMAT_TIMESTAMP_MAX];
	struct libscols_line *ln;
	size_t i;
	int rc;

	ln = scols_table_new_line(tb, NULL);
	if (!ln)
		errx(EXIT_FAILURE, _("failed to allocate output line"));

	for (i = 0; i < ncolumns; i++) {
		switch (columns[i]) {
			case COL_TYPE:
				scols_line_set_data(ln, i, clock_type_name(clockinfo->type));
				break;
			case COL_ID:
				if (!clockinfo->no_id)
					scols_line_asprintf(ln, i, "%ju", (uintmax_t) clockinfo->id);
				break;
			case COL_CLOCK:
				scols_line_set_data(ln, i, clockinfo->id_name);
				break;
			case COL_NAME:
				scols_line_set_data(ln, i, clockinfo->name);
				break;
			case COL_TIME:
				if (now->tv_nsec == -1)
					break;

				scols_line_format_timespec(ln, i, now);
				break;
			case COL_ISO_TIME:
				if (now->tv_nsec == -1)
					break;

				rc = strtimespec_iso(now,
						ISO_GMTIME | ISO_DATE | ISO_TIME | ISO_T | ISO_DOTNSEC | ISO_TIMEZONE,
						buf, sizeof(buf));
				if (rc)
					errx(EXIT_FAILURE, _("failed to format iso time"));
				scols_line_set_data(ln, i, buf);
				break;
			case COL_RESOL:
				if (resolution->tv_nsec == -1)
					break;

				rc = strtimespec_relative(resolution, buf, sizeof(buf));
				if (rc)
					errx(EXIT_FAILURE, _("failed to format relative time"));
				scols_line_set_data(ln, i, buf);
				break;
			case COL_RESOL_RAW:
				if (resolution->tv_nsec == -1)
					break;
				scols_line_format_timespec(ln, i, resolution);
				break;
			case COL_REL_TIME:
				if (now->tv_nsec == -1)
					break;
				rc = strtimespec_relative(now, buf, sizeof(buf));
				if (rc)
					errx(EXIT_FAILURE, _("failed to format relative time"));
				scols_line_set_data(ln, i, buf);
				break;
			case COL_NS_OFFSET:
				if (clockinfo->ns_offset_name)
					scols_line_asprintf(ln, i, "%"PRId64,
							    get_namespace_offset(clockinfo->ns_offset_name));
				break;
		}
	}
}

static void add_posix_clock_line(struct libscols_table *tb, const int *columns,
			         size_t ncolumns, const struct clockinfo *clockinfo)
{
	struct timespec resolution, now;
	int rc;

	rc = clock_gettime(clockinfo->id, &now);
	if (rc)
		now.tv_nsec = -1;

	rc = clock_getres(clockinfo->id, &resolution);
	if (rc)
		resolution.tv_nsec = -1;

	add_clock_line(tb, columns, ncolumns, clockinfo, &now, &resolution);
}

struct path_clock {
	struct list_head head;
	const char * path;
};

static void add_dynamic_clock_from_path(struct libscols_table *tb,
					const int *columns, size_t ncolumns,
					const char *path, bool explicit)
{
	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		if (explicit)
			err(EXIT_FAILURE, _("Could not open %s"), path);
		else
			return;
	}

	struct clockinfo clockinfo = {
		.type = CT_PTP,
		.no_id = true,
		.id_name = path,
		.name = path,
	};
	add_posix_clock_line(tb, columns, ncolumns, &clockinfo);
	close(fd);
}

static void add_dynamic_clocks_from_discovery(struct libscols_table *tb,
					      const int *columns, size_t ncolumns)
{
	int rc;
	size_t i;
	glob_t state;

	rc = glob("/dev/ptp*", 0, NULL, &state);
	if (rc == GLOB_NOMATCH)
		return;
	else if (rc)
		errx(EXIT_FAILURE, _("Could not glob: %d"), rc);

	for (i = 0; i < state.gl_pathc; i++)
		add_dynamic_clock_from_path(tb, columns, ncolumns,
					    state.gl_pathv[i], false);

	globfree(&state);
}

static void add_rtc_clock_from_path(struct libscols_table *tb,
				    const int *columns, size_t ncolumns,
				    const char *path, bool explicit)
{
	int fd, rc;
	struct rtc_time rtc_time;
	struct tm tm = { 0 };
	struct timespec now = { 0 }, resolution = { .tv_nsec = -1 };

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		if (explicit)
			err(EXIT_FAILURE, _("Could not open %s"), path);
		else
			return;
	}

	rc = ioctl(fd, RTC_RD_TIME, &rtc_time);
	if (rc)
		err(EXIT_FAILURE,
		    _("ioctl(RTC_RD_NAME) to %s to read the time failed"), path);

	tm.tm_sec  = rtc_time.tm_sec;
	tm.tm_min  = rtc_time.tm_min;
	tm.tm_hour = rtc_time.tm_hour;
	tm.tm_mday = rtc_time.tm_mday;
	tm.tm_mon  = rtc_time.tm_mon;
	tm.tm_year = rtc_time.tm_year;
	tm.tm_wday = rtc_time.tm_wday;
	tm.tm_yday = rtc_time.tm_yday;

	now.tv_sec = mktime(&tm);

	struct clockinfo clockinfo = {
		.type = CT_RTC,
		.no_id = true,
		.id_name = path,
		.name = path,
	};
	add_clock_line(tb, columns, ncolumns, &clockinfo, &now, &resolution);

	close(fd);
}

static void add_rtc_clocks_from_discovery(struct libscols_table *tb,
					  const int *columns, size_t ncolumns)
{
	int rc;
	size_t i;
	glob_t state;

	rc = glob("/dev/rtc*", 0, NULL, &state);
	if (rc == GLOB_NOMATCH)
		return;
	if (rc)
		errx(EXIT_FAILURE, _("Could not glob: %d"), rc);

	for (i = 0; i < state.gl_pathc; i++)
		add_rtc_clock_from_path(tb, columns, ncolumns,
					    state.gl_pathv[i], false);

	globfree(&state);
}

struct cpu_clock {
	struct list_head head;
	pid_t pid;
	char name[sizeof(stringify_value(SINT_MAX(pid_t)))];
};

static void add_cpu_clock(struct libscols_table *tb,
			  const int *columns, size_t ncolumns,
			  pid_t pid, const char *name)
{
	int rc;
	clockid_t clockid;

	rc = clock_getcpuclockid(pid, &clockid);
	if (rc)
		errx(EXIT_FAILURE, _("Could not get CPU clock of process %jd: %s"),
				   (intmax_t) pid, strerror(rc));

	struct clockinfo clockinfo = {
		.type = CT_CPU,
		.id = clockid,
		.name = name,
		.no_id = true,
	};
	add_posix_clock_line(tb, columns, ncolumns, &clockinfo);
}


int main(int argc, char **argv)
{
	size_t i;
	int c, rc;
	const struct colinfo *colinfo;

	struct libscols_table *tb;
	struct libscols_column *col;

	bool noheadings = false, raw = false, json = false,
	     disc_dynamic = true, disc_rtc = true;
	const char *outarg = NULL;
	int columns[ARRAY_SIZE(infos) * 2];
	size_t ncolumns = 0;
	clockid_t clock = -1;
	struct path_clock *path_clock;
	struct cpu_clock *cpu_clock;
	struct list_head *current_path_clock, *current_cpu_clock;
	struct list_head dynamic_clocks, cpu_clocks, rtc_clocks;

	struct timespec now;

	enum {
		OPT_OUTPUT_ALL = CHAR_MAX + 1,
		OPT_NO_DISC_DYN,
		OPT_NO_DISC_RTC,
	};
	static const struct option longopts[] = {
		{ "noheadings",          no_argument,       NULL, 'n' },
		{ "output",              required_argument, NULL, 'o' },
		{ "output-all",	         no_argument,       NULL, OPT_OUTPUT_ALL },
		{ "version",             no_argument,       NULL, 'V' },
		{ "help",	         no_argument,       NULL, 'h' },
		{ "json",                no_argument,       NULL, 'J' },
		{ "raw",                 no_argument,       NULL, 'r' },
		{ "time",                required_argument, NULL, 't' },
		{ "no-discover-dynamic", no_argument,       NULL, OPT_NO_DISC_DYN },
		{ "dynamic-clock",       required_argument, NULL, 'd' },
		{ "cpu-clock",           required_argument, NULL, 'c' },
		{ "no-discover-rtc",     no_argument,       NULL, OPT_NO_DISC_RTC },
		{ "rtc",                 required_argument, NULL, 'x' },
		{ 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	INIT_LIST_HEAD(&dynamic_clocks);
	INIT_LIST_HEAD(&cpu_clocks);
	INIT_LIST_HEAD(&rtc_clocks);

	while ((c = getopt_long(argc, argv, "no:Jrt:d:c:x:Vh", longopts, NULL)) != -1) {
		switch (c) {
		case 'n':
			noheadings = true;
			break;
		case 'o':
			outarg = optarg;
			break;
		case OPT_OUTPUT_ALL:
			for (ncolumns = 0; ncolumns < ARRAY_SIZE(infos); ncolumns++)
				columns[ncolumns] = ncolumns;
			break;
		case 'J':
			json = true;
			break;
		case 'r':
			raw = true;
			break;
		case 't':
			clock = parse_clock(optarg);
			break;
		case 'd':
			path_clock = xmalloc(sizeof(*path_clock));
			path_clock->path = optarg;
			list_add(&path_clock->head, &dynamic_clocks);
			break;
		case OPT_NO_DISC_DYN:
			disc_dynamic = false;
			break;
		case 'c':
			cpu_clock = xmalloc(sizeof(*cpu_clock));
			cpu_clock->pid = strtopid_or_err(optarg, _("failed to parse pid"));
			snprintf(cpu_clock->name, sizeof(cpu_clock->name),
				 "%jd", (intmax_t) cpu_clock->pid);
			list_add(&cpu_clock->head, &cpu_clocks);
			break;
		case 'x':
			path_clock = xmalloc(sizeof(*path_clock));
			path_clock->path = optarg;
			list_add(&path_clock->head, &rtc_clocks);
			break;
		case OPT_NO_DISC_RTC:
			disc_rtc = false;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (argv[optind])
		errtryhelp(EXIT_FAILURE);

	if (clock != -1) {
		rc = clock_gettime(clock, &now);
		if (rc)
			err(EXIT_FAILURE, _("failed to get time"));
		printf("%ju.%09"PRId32"\n", (uintmax_t) now.tv_sec, (uint32_t) now.tv_nsec);
		return EXIT_SUCCESS;
	}

	if (!ncolumns) {
		columns[ncolumns++] = COL_ID;
		columns[ncolumns++] = COL_NAME;
		columns[ncolumns++] = COL_TYPE;
		columns[ncolumns++] = COL_TIME;
		columns[ncolumns++] = COL_RESOL;
		columns[ncolumns++] = COL_ISO_TIME;
	}

	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
					    &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	scols_init_debug(0);

	tb = scols_new_table();
	if (!tb)
		errx(EXIT_FAILURE, _("failed to allocate output table"));
	scols_table_set_name(tb, "clocks");

	for (i = 0; i < ncolumns; i++) {
		colinfo = &infos[columns[i]];

		col = scols_table_new_column(tb, colinfo->name, colinfo->whint, colinfo->flags);
		if (!col)
			errx(EXIT_FAILURE, _("failed to allocate output column"));

		scols_column_set_json_type(col, colinfo->json_type);
	}

	for (i = 0; i < ARRAY_SIZE(clocks); i++)
		add_posix_clock_line(tb, columns, ncolumns, &clocks[i]);

	if (disc_dynamic)
		add_dynamic_clocks_from_discovery(tb, columns, ncolumns);

	list_for_each(current_path_clock, &dynamic_clocks) {
		path_clock = list_entry(current_path_clock, struct path_clock, head);
		add_dynamic_clock_from_path(tb, columns, ncolumns, path_clock->path, true);
	}

	list_free(&dynamic_clocks, struct path_clock, head, free);

	if (disc_rtc)
		add_rtc_clocks_from_discovery(tb, columns, ncolumns);

	list_for_each(current_path_clock, &rtc_clocks) {
		path_clock = list_entry(current_path_clock, struct path_clock, head);
		add_rtc_clock_from_path(tb, columns, ncolumns, path_clock->path, true);
	}

	list_free(&rtc_clocks, struct path_clock, head, free);

	list_for_each(current_cpu_clock, &cpu_clocks) {
		cpu_clock = list_entry(current_cpu_clock, struct cpu_clock, head);
		add_cpu_clock(tb, columns, ncolumns, cpu_clock->pid, cpu_clock->name);
	}

	list_free(&cpu_clocks, struct cpu_clock, head, free);

	scols_table_enable_json(tb, json);
	scols_table_enable_raw(tb, raw);
	scols_table_enable_noheadings(tb, noheadings);
	scols_print_table(tb);
	scols_unref_table(tb);
}
