/*
 * SPDX-License-Identifier: ISC
 *
 * /dev/rfkill userspace tool
 *
 * Copyright 2009 Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2009 Marcel Holtmann <marcel@holtmann.org>
 * Copyright 2009 Tim Gardner <tim.gardner@canonical.com>
 * Copyright 2017 Sami Kerola <kerolasa@iki.fi>
 * Copyright (C) 2017 Karel Zak <kzak@redhat.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <ctype.h>
#include <getopt.h>
#include <libsmartcols.h>
#include <linux/rfkill.h>
#include <poll.h>
#include <sys/syslog.h>
#include <sys/time.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "optutils.h"
#include "pathnames.h"
#include "strutils.h"
#include "timeutils.h"
#include "widechar.h"
#include "xalloc.h"
#include "all-io.h"


/*
 * NFC supported by kernel since v3.10 (year 2013); FM and another types are from
 * year 2009 (2.6.33) or older.
 */
#ifndef RFKILL_TYPE_NFC
# ifndef RFKILL_TYPE_FM
#  define RFKILL_TYPE_FM	RFKILL_TYPE_GPS + 1
# endif
# define RFKILL_TYPE_NFC	RFKILL_TYPE_FM + 1
# undef NUM_RFKILL_TYPES
# define NUM_RFKILL_TYPES	RFKILL_TYPE_NFC + 1
#endif

struct rfkill_type_str {
	enum rfkill_type type;	/* ID */
	const char *name;	/* generic name */
	const char *desc;	/* human readable name */
};

static const struct rfkill_type_str rfkill_type_strings[] = {
	{ .type = RFKILL_TYPE_ALL,       .name = "all"           },
	{ .type = RFKILL_TYPE_WLAN,      .name = "wlan",         .desc = "Wireless LAN" },
	{ .type = RFKILL_TYPE_WLAN,      .name = "wifi"          },				/* alias */
	{ .type = RFKILL_TYPE_BLUETOOTH, .name = "bluetooth",    .desc = "Bluetooth" },
	{ .type = RFKILL_TYPE_UWB,       .name = "uwb",          .desc = "Ultra-Wideband" },
	{ .type = RFKILL_TYPE_UWB,       .name = "ultrawideband" }, /* alias */
	{ .type = RFKILL_TYPE_WIMAX,     .name = "wimax",        .desc = "WiMAX" },
	{ .type = RFKILL_TYPE_WWAN,      .name = "wwan",         .desc = "Wireless WAN" },
	{ .type = RFKILL_TYPE_GPS,       .name = "gps",          .desc = "GPS" },
	{ .type = RFKILL_TYPE_FM,        .name = "fm",           .desc = "FM" },
	{ .type = RFKILL_TYPE_NFC,       .name = "nfc",          .desc = "NFC" },
	{ .type = NUM_RFKILL_TYPES,      .name = NULL            }
};

struct rfkill_id {
	union {
		enum rfkill_type type;
		uint32_t index;
	};
	enum {
		RFKILL_IS_INVALID,
		RFKILL_IS_TYPE,
		RFKILL_IS_INDEX,
		RFKILL_IS_ALL
	} result;
};

/* supported actions */
enum {
	ACT_LIST,
	ACT_HELP,
	ACT_EVENT,
	ACT_BLOCK,
	ACT_UNBLOCK,
	ACT_TOGGLE,

	ACT_LIST_OLD
};

static char *rfkill_actions[] = {
	[ACT_LIST]	= "list",
	[ACT_HELP]	= "help",
	[ACT_EVENT]	= "event",
	[ACT_BLOCK]	= "block",
	[ACT_UNBLOCK]	= "unblock",
	[ACT_TOGGLE]	= "toggle"
};

/* column IDs */
enum {
	COL_DEVICE,
	COL_ID,
	COL_TYPE,
	COL_DESC,
	COL_SOFT,
	COL_HARD
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
	[COL_DEVICE] = {"DEVICE", 0, 0, N_("kernel device name")},
	[COL_ID]     = {"ID",	  2, SCOLS_FL_RIGHT, N_("device identifier value")},
	[COL_TYPE]   = {"TYPE",	  0, 0, N_("device type name that can be used as identifier")},
	[COL_DESC]   = {"TYPE-DESC",   0, 0, N_("device type description")},
	[COL_SOFT]   = {"SOFT",	  0, SCOLS_FL_RIGHT, N_("status of software block")},
	[COL_HARD]   = {"HARD",	  0, SCOLS_FL_RIGHT, N_("status of hardware block")}
};

static int columns[ARRAY_SIZE(infos) * 2];
static size_t ncolumns;

struct control {
	struct libscols_table *tb;
	unsigned int
		json:1,
		no_headings:1,
		raw:1;
};

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

static int string_to_action(const char *str)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rfkill_actions); i++)
		if (strcmp(str, rfkill_actions[i]) == 0)
			return i;

	return -EINVAL;
}

static int rfkill_open(int rdonly, int nonblock)
{
	int fd;

	fd = open(_PATH_DEV_RFKILL, rdonly ? O_RDONLY : O_RDWR);
	if (fd < 0) {
		warn(_("cannot open %s"), _PATH_DEV_RFKILL);
		return -errno;
	}

	if (nonblock && fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		warn(_("cannot set non-blocking %s"), _PATH_DEV_RFKILL);
		close(fd);
		return -errno;
	}

	return fd;
}

/* returns: 0 success, 1 read again, < 0 error */
static int rfkill_read_event(int fd, struct rfkill_event *event)
{
	ssize_t	len = read(fd, event, sizeof(*event));

	if (len < 0) {
		if (errno == EAGAIN)
			return 1;
		warn(_("cannot read %s"), _PATH_DEV_RFKILL);
		return -errno;
	}

	if ((size_t) len < (size_t) RFKILL_EVENT_SIZE_V1) {
		warnx(_("wrong size of rfkill event: %zu < %zu"),
				(size_t) len, (size_t) RFKILL_EVENT_SIZE_V1);
		return 1;
	}

	return 0;
}


static int rfkill_event(void)
{
	enum {
		POLLFD_RFKILL,
		POLLFD_STDOUT,
		POLLFD_COUNT
	};
	struct rfkill_event event;
	struct timeval tv;
	char date_buf[ISO_BUFSIZ];
	struct pollfd p[POLLFD_COUNT];
	int fd, n;

	fd = rfkill_open(1, 0);
	if (fd < 0)
		return fd;

	memset(&p, 0, sizeof(p));
	p[POLLFD_RFKILL].fd = fd;
	p[POLLFD_RFKILL].events = POLLIN | POLLHUP;
	p[POLLFD_STDOUT].fd = STDOUT_FILENO;
	p[POLLFD_STDOUT].events = 0;

	/* interrupted by signal only */
	while (1) {
		int rc = 1;	/* recover-able error */

		n = poll(p, ARRAY_SIZE(p), -1);
		if (n < 0) {
			warn(_("failed to poll %s"), _PATH_DEV_RFKILL);
			goto failed;
		}

		if (p[POLLFD_STDOUT].revents)
			goto failed; /* read end of stdout closed */
		if (p[POLLFD_RFKILL].revents)
			rc = rfkill_read_event(fd, &event);
		if (rc < 0)
			goto failed;
		if (rc)
			continue;

		gettimeofday(&tv, NULL);
		strtimeval_iso(&tv, ISO_TIMESTAMP_COMMA, date_buf,
			       sizeof(date_buf));
		printf("%s: idx %u type %u op %u soft %u hard %u\n",
		       date_buf,
		       event.idx, event.type, event.op, event.soft, event.hard);
		fflush(stdout);
	}

failed:
	close(fd);
	return -1;
}

static const char *get_sys_attr(uint32_t idx, const char *attr)
{
	static char name[128];
	char path[PATH_MAX];
	FILE *f;
	char *p;

	snprintf(path, sizeof(path), _PATH_SYS_RFKILL "/rfkill%u/%s", idx, attr);
	f = fopen(path, "r");
	if (!f)
		goto done;
	if (!fgets(name, sizeof(name), f))
		goto done;
	p = strchr(name, '\n');
	if (p)
		*p = '\0';
done:
	if (f)
		fclose(f);
	return name;
}

static struct rfkill_id rfkill_id_to_type(const char *s)
{
	const struct rfkill_type_str *p;
	struct rfkill_id ret = { .result = 0 };

	if (islower(*s)) {
		for (p = rfkill_type_strings; p->name != NULL; p++) {
			if (!strcmp(s, p->name)) {
				ret.type = p->type;
				if (!strcmp(s, "all"))
					ret.result = RFKILL_IS_ALL;
				else
					ret.result = RFKILL_IS_TYPE;
				return ret;
			}
		}
	} else if (isdigit(*s)) {
		/* assume a numeric character implies an index. */
		char filename[64];

		ret.index = strtou32_or_err(s, _("invalid identifier"));
		snprintf(filename, sizeof(filename) - 1,
			 _PATH_SYS_RFKILL "/rfkill%" PRIu32 "/name", ret.index);
		if (access(filename, F_OK) == 0)
			ret.result = RFKILL_IS_INDEX;
		else
			ret.result = RFKILL_IS_INVALID;
		return ret;
	}

	ret.result = RFKILL_IS_INVALID;
	return ret;
}

static const char *rfkill_type_to_desc(enum rfkill_type type)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rfkill_type_strings); i++) {
		if (type == rfkill_type_strings[i].type)
			return rfkill_type_strings[i].desc;
	}

	return NULL;
}


static int event_match(struct rfkill_event *event, struct rfkill_id *id)
{
	if (event->op != RFKILL_OP_ADD)
		return 0;

	/* filter out unwanted results */
	switch (id->result) {
	case RFKILL_IS_TYPE:
		if (event->type != id->type)
			return 0;
		break;
	case RFKILL_IS_INDEX:
		if (event->idx != id->index)
			return 0;
		break;
	case RFKILL_IS_ALL:
		break;
	default:
		abort();
	}

	return 1;
}

static void fill_table_row(struct libscols_table *tb, struct rfkill_event *event)
{
	static struct libscols_line *ln;
	size_t i;

	assert(tb);

	ln = scols_table_new_line(tb, NULL);
	if (!ln) {
		errno = ENOMEM;
		errx(EXIT_FAILURE, _("failed to allocate output line"));
	}

	for (i = 0; i < (size_t)ncolumns; i++) {
		char *str = NULL;
		switch (get_column_id(i)) {
		case COL_DEVICE:
			str = xstrdup(get_sys_attr(event->idx, "name"));
			break;
		case COL_ID:
			xasprintf(&str, "%" PRIu32, event->idx);
			break;
		case COL_TYPE:
			str = xstrdup(get_sys_attr(event->idx, "type"));
			break;
		case COL_DESC:
			str = xstrdup(rfkill_type_to_desc(event->type));
			break;
		case COL_SOFT:
			str = xstrdup(event->soft ? _("blocked") : _("unblocked"));
			break;
		case COL_HARD:
			str = xstrdup(event->hard ? _("blocked") : _("unblocked"));
			break;
		default:
			abort();
		}
		if (str && scols_line_refer_data(ln, i, str))
			errx(EXIT_FAILURE, _("failed to add output data"));
	}
}

static int rfkill_list_old(const char *param)
{
	struct rfkill_id id = { .result = RFKILL_IS_ALL };
	struct rfkill_event event;
	int fd, rc = 0;

	if (param) {
		id = rfkill_id_to_type(param);
		if (id.result == RFKILL_IS_INVALID) {
			warnx(_("invalid identifier: %s"), param);
			return -EINVAL;
		}
	}

	fd = rfkill_open(1, 1);
	if (fd < 0)
		return fd;

	while (1) {
		rc = rfkill_read_event(fd, &event);
		if (rc < 0)
			break;
		if (rc == 1 && errno == EAGAIN) {
			rc = 0;		/* done */
			break;
		}
		if (rc == 0 && event_match(&event, &id)) {
			char *name = xstrdup(get_sys_attr(event.idx, "name")),
			     *type = xstrdup(rfkill_type_to_desc(event.type));

			if (!type)
				type = xstrdup(get_sys_attr(event.idx, "type"));

			printf("%u: %s: %s\n", event.idx, name, type);
			printf("\tSoft blocked: %s\n", event.soft ? "yes" : "no");
			printf("\tHard blocked: %s\n", event.hard ? "yes" : "no");

			free(name);
			free(type);
		}
	}
	close(fd);
	return rc;
}

static void rfkill_list_init(struct control *ctrl)
{
	size_t i;

	scols_init_debug(0);

	ctrl->tb = scols_new_table();
	if (!ctrl->tb)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	scols_table_enable_json(ctrl->tb, ctrl->json);
	scols_table_set_name(ctrl->tb, "rfkilldevices");
	scols_table_enable_noheadings(ctrl->tb, ctrl->no_headings);
	scols_table_enable_raw(ctrl->tb, ctrl->raw);

	for (i = 0; i < (size_t) ncolumns; i++) {
		const struct colinfo *col = get_column_info(i);
		struct libscols_column *cl;

		cl = scols_table_new_column(ctrl->tb, col->name, col->whint, col->flags);
		if (!cl)
			err(EXIT_FAILURE, _("failed to allocate output column"));
		if (ctrl->json) {
			int id = get_column_id(i);
			if (id == COL_ID)
				scols_column_set_json_type(cl, SCOLS_JSON_NUMBER);
		}
	}
}

static int rfkill_list_fill(struct control const *ctrl, const char *param)
{
	struct rfkill_id id = { .result = RFKILL_IS_ALL };
	struct rfkill_event event;
	int fd, rc = 0;

	if (param) {
		id = rfkill_id_to_type(param);
		if (id.result == RFKILL_IS_INVALID) {
			warnx(_("invalid identifier: %s"), param);
			return -EINVAL;
		}
	}

	fd = rfkill_open(1, 1);
	if (fd < 0)
		return fd;

	while (1) {
		rc = rfkill_read_event(fd, &event);
		if (rc < 0)
			break;
		if (rc == 1 && errno == EAGAIN) {
			rc = 0;		/* done */
			break;
		}
		if (rc == 0 && event_match(&event, &id))
			fill_table_row(ctrl->tb, &event);
	}
	close(fd);
	return rc;
}

static void rfkill_list_output(struct control const *ctrl)
{
	scols_print_table(ctrl->tb);
	scols_unref_table(ctrl->tb);
}

static int __rfkill_block(int fd, struct rfkill_id *id, uint8_t block, const char *param)
{
	struct rfkill_event event = {
		.op = RFKILL_OP_CHANGE_ALL,
		.soft = block,
		0
	};
	char *message = NULL;

	switch (id->result) {
	case RFKILL_IS_INVALID:
		warnx(_("invalid identifier: %s"), param);
		return -1;
	case RFKILL_IS_TYPE:
		event.type = id->type;
		xasprintf(&message, "type %s", param);
		break;
	case RFKILL_IS_INDEX:
		event.op = RFKILL_OP_CHANGE;
		event.idx = id->index;
		xasprintf(&message, "id %d", id->index);
		break;
	case RFKILL_IS_ALL:
		message = xstrdup("all");
		break;
	default:
		abort();
	}

	if (write_all(fd, &event, sizeof(event)) != 0)
		warn(_("write failed: %s"), _PATH_DEV_RFKILL);
	else {
		openlog("rfkill", 0, LOG_USER);
		syslog(LOG_NOTICE, "%s set for %s", block ? "block" : "unblock", message);
		closelog();
	}
	free(message);
	return 0;
}

static int rfkill_block(uint8_t block, const char *param)
{
	struct rfkill_id id;
	int fd;

	id = rfkill_id_to_type(param);
	if (id.result == RFKILL_IS_INVALID) {
		warnx(_("invalid identifier: %s"), param);
		return -EINVAL;
	}

	fd = rfkill_open(0, 0);
	if (fd < 0)
		return fd;

	__rfkill_block(fd, &id, block, param);

	return close(fd);
}

static int rfkill_toggle(const char *param)
{
	struct rfkill_id id = { .result = RFKILL_IS_ALL };
	struct rfkill_event event;
	int fd, rc = 0;

	id = rfkill_id_to_type(param);
	if (id.result == RFKILL_IS_INVALID) {
		warnx(_("invalid identifier: %s"), param);
		return -EINVAL;
	}

	fd = rfkill_open(0, 1);
	if (fd < 0)
		return fd;

	while (1) {
		rc = rfkill_read_event(fd, &event);
		if (rc < 0)
			break;
		if (rc == 1 && errno == EAGAIN) {
			rc = 0;		/* done */
			break;
		}
		if (rc == 0 && event_match(&event, &id))
			__rfkill_block(fd, &id, event.soft ? 0 : 1, param);
	}

	close(fd);
	return rc;
}


static void __attribute__((__noreturn__)) usage(void)
{
	size_t i;

	fputs(USAGE_HEADER, stdout);
	fprintf(stdout, _(" %s [options] command [identifier ...]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Tool for enabling and disabling wireless devices.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputs(_(" -J, --json             use JSON output format\n"), stdout);
	fputs(_(" -n, --noheadings       don't print headings\n"), stdout);
	fputs(_(" -o, --output <list>    define which output columns to use\n"), stdout);
	fputs(_("     --output-all       output all columns\n"), stdout);
	fputs(_(" -r, --raw              use the raw output format\n"), stdout);

	fputs(USAGE_SEPARATOR, stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(24));

	fputs(USAGE_COLUMNS, stdout);
	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(stdout, " %-10s  %s\n", infos[i].name, _(infos[i].help));

	fputs(USAGE_COMMANDS, stdout);

	/*
	 * TRANSLATORS: command names should not be translated, explaining
	 * them as additional field after identifier is fine, for example
	 *
	 * list   [identifier]   (lista [tarkenne])
	 */
	fputs(_(" help\n"), stdout);
	fputs(_(" event\n"), stdout);
	fputs(_(" list   [identifier]\n"), stdout);
	fputs(_(" block   identifier\n"), stdout);
	fputs(_(" unblock identifier\n"), stdout);
	fputs(_(" toggle  identifier\n"), stdout);

	fprintf(stdout, USAGE_MAN_TAIL("rfkill(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct control ctrl = { 0 };
	int c, act = ACT_LIST, list_all = 0;
	char *outarg = NULL;
	enum {
		OPT_LIST_TYPES = CHAR_MAX + 1
	};
	static const struct option longopts[] = {
		{ "json",	no_argument,	   NULL, 'J' },
		{ "noheadings", no_argument,	   NULL, 'n' },
		{ "output",	required_argument, NULL, 'o' },
		{ "output-all",	no_argument,	   NULL, OPT_LIST_TYPES },
		{ "raw",	no_argument,	   NULL, 'r' },
		{ "version",	no_argument,	   NULL, 'V' },
		{ "help",	no_argument,	   NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};
	static const ul_excl_t excl[] = {
		{'J', 'r'},
		{0}
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;
	int ret = 0;

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
		case OPT_LIST_TYPES:
			list_all = 1;
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

	if (argc > 0) {
		act = string_to_action(*argv);
		if (act < 0)
			errtryhelp(EXIT_FAILURE);
		argv++;
		argc--;

		/*
		 * For backward compatibility we use old output format if
		 * "list" explicitly specified and --output not defined.
		 */
		if (!outarg && act == ACT_LIST)
			act = ACT_LIST_OLD;
	}

	switch (act) {
	case ACT_LIST_OLD:
		/* Deprecated in favour of ACT_LIST */
		if (!argc)
			ret |= rfkill_list_old(NULL);	/* ALL */
		else while (argc) {
			ret |= rfkill_list_old(*argv);
			argc--;
			argv++;
		}
		break;

	case ACT_LIST:
		columns[ncolumns++] = COL_ID;
		columns[ncolumns++] = COL_TYPE;
		columns[ncolumns++] = COL_DEVICE;
		if (list_all)
			columns[ncolumns++] = COL_DESC;
		columns[ncolumns++] = COL_SOFT;
		columns[ncolumns++] = COL_HARD;

		if (outarg
		    && string_add_to_idarray(outarg, columns,
					     ARRAY_SIZE(columns), &ncolumns,
					     column_name_to_id) < 0)
			return EXIT_FAILURE;

		rfkill_list_init(&ctrl);
		if (!argc)
			ret |= rfkill_list_fill(&ctrl, NULL);	/* ALL */
		else while (argc) {
			ret |= rfkill_list_fill(&ctrl, *argv);
			argc--;
			argv++;
		}
		rfkill_list_output(&ctrl);
		break;

	case ACT_EVENT:
		ret = rfkill_event();
		break;

	case ACT_HELP:
		usage();
		break;

	case ACT_BLOCK:
		while (argc) {
			ret |= rfkill_block(1, *argv);
			argc--;
			argv++;
		}
		break;

	case ACT_UNBLOCK:
		while (argc) {
			ret |= rfkill_block(0, *argv);
			argv++;
			argc--;
		}
		break;

	case ACT_TOGGLE:
		while (argc) {
			ret |= rfkill_toggle(*argv);
			argv++;
			argc--;
		}
		break;
	}

	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
