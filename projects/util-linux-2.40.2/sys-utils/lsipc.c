/*
 * lsipc - List information about IPC instances employed in the system
 *
 * Copyright (C) 2015 Ondrej Oprala <ooprala@redhat.com>
 * Copyright (C) 2015 Karel Zak <ooprala@redhat.com>
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
 *
 *
 * lsipc is inspired by the ipcs utility. The aim is to create
 * a utility unencumbered by a standard to provide more flexible
 * means of controlling the output.
 */

#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <unistd.h>

#include <libsmartcols.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"
#include "strutils.h"
#include "optutils.h"
#include "xalloc.h"
#include "procfs.h"
#include "ipcutils.h"
#include "timeutils.h"

/*
 * time modes
 * */
enum {
	TIME_INVALID = 0,
	TIME_SHORT,
	TIME_FULL,
	TIME_ISO
};

/*
 * IDs
 */
enum {
	/* generic */
	COLDESC_IDX_GEN_FIRST = 0,
		COL_KEY = COLDESC_IDX_GEN_FIRST,
		COL_ID,
		COL_OWNER,
		COL_PERMS,
		COL_CUID,
		COL_CUSER,
		COL_CGID,
		COL_CGROUP,
		COL_UID,
		COL_USER,
		COL_GID,
		COL_GROUP,
		COL_CTIME,
	COLDESC_IDX_GEN_LAST = COL_CTIME,

	/* msgq-specific */
	COLDESC_IDX_MSG_FIRST,
		COL_USEDBYTES = COLDESC_IDX_MSG_FIRST,
		COL_MSGS,
		COL_SEND,
		COL_RECV,
		COL_LSPID,
		COL_LRPID,
	COLDESC_IDX_MSG_LAST = COL_LRPID,

	/* shm-specific */
	COLDESC_IDX_SHM_FIRST,
		COL_SIZE = COLDESC_IDX_SHM_FIRST,
		COL_NATTCH,
		COL_STATUS,
		COL_ATTACH,
		COL_DETACH,
		COL_COMMAND,
		COL_CPID,
		COL_LPID,
	COLDESC_IDX_SHM_LAST = COL_LPID,

	/* sem-specific */
	COLDESC_IDX_SEM_FIRST,
		COL_NSEMS = COLDESC_IDX_SEM_FIRST,
		COL_OTIME,
	COLDESC_IDX_SEM_LAST = COL_OTIME,

	/* summary (--global) */
	COLDESC_IDX_SUM_FIRST,
		COL_RESOURCE = COLDESC_IDX_SUM_FIRST,
		COL_DESC,
		COL_LIMIT,
		COL_USED,
		COL_USEPERC,
	COLDESC_IDX_SUM_LAST = COL_USEPERC
};

/* not all columns apply to all options, so we specify a legal range for each */
static size_t LOWER, UPPER;

/*
 * output modes
 */
enum {
	OUT_EXPORT = 1,
	OUT_NEWLINE,
	OUT_RAW,
	OUT_JSON,
	OUT_PRETTY,
	OUT_LIST
};

struct lsipc_control {
	int outmode;
	unsigned int noheadings : 1,		/* don't print header line */
		     notrunc : 1,		/* don't truncate columns */
		     shellvar : 1,              /* use shell compatible colum names */
		     bytes : 1,			/* SIZE in bytes */
		     numperms : 1,		/* numeric permissions */
		     time_mode : 2;
};

struct lsipc_coldesc {
	const char * const name;
	const char *help;
	const char *pretty_name;

	double whint;	/* width hint */
	long flag;
};

static const struct lsipc_coldesc coldescs[] =
{
	/* common */
	[COL_KEY]	= { "KEY",	N_("Resource key"), N_("Key"), 1},
	[COL_ID]	= { "ID",	N_("Resource ID"), N_("ID"), 1},
	[COL_OWNER]	= { "OWNER",	N_("Owner's username or UID"), N_("Owner"), 1, SCOLS_FL_RIGHT},
	[COL_PERMS]	= { "PERMS",	N_("Permissions"), N_("Permissions"), 1, SCOLS_FL_RIGHT},
	[COL_CUID]	= { "CUID",	N_("Creator UID"), N_("Creator UID"), 1, SCOLS_FL_RIGHT},
	[COL_CUSER]     = { "CUSER",    N_("Creator user"), N_("Creator user"), 1 },
	[COL_CGID]	= { "CGID",	N_("Creator GID"), N_("Creator GID"), 1, SCOLS_FL_RIGHT},
	[COL_CGROUP]    = { "CGROUP",   N_("Creator group"), N_("Creator group"), 1 },
	[COL_UID]	= { "UID",	N_("User ID"), N_("UID"), 1, SCOLS_FL_RIGHT},
	[COL_USER]	= { "USER",	N_("User name"), N_("User name"), 1},
	[COL_GID]	= { "GID",	N_("Group ID"), N_("GID"), 1, SCOLS_FL_RIGHT},
	[COL_GROUP]	= { "GROUP",	N_("Group name"), N_("Group name"), 1},
	[COL_CTIME]	= { "CTIME",	N_("Time of the last change"), N_("Last change"), 1, SCOLS_FL_RIGHT},

	/* msgq-specific */
	[COL_USEDBYTES]	= { "USEDBYTES",N_("Bytes used"), N_("Bytes used"), 1, SCOLS_FL_RIGHT},
	[COL_MSGS]	= { "MSGS",	N_("Number of messages"), N_("Messages"), 1},
	[COL_SEND]	= { "SEND",	N_("Time of last msg sent"), N_("Msg sent"), 1, SCOLS_FL_RIGHT},
	[COL_RECV]	= { "RECV",	N_("Time of last msg received"), N_("Msg received"), 1, SCOLS_FL_RIGHT},
	[COL_LSPID]	= { "LSPID",	N_("PID of the last msg sender"), N_("Msg sender"), 1, SCOLS_FL_RIGHT},
	[COL_LRPID]	= { "LRPID",	N_("PID of the last msg receiver"), N_("Msg receiver"), 1, SCOLS_FL_RIGHT},

	/* shm-specific */
	[COL_SIZE]	= { "SIZE",	N_("Segment size"), N_("Segment size"), 1, SCOLS_FL_RIGHT},
	[COL_NATTCH]	= { "NATTCH",	N_("Number of attached processes"), N_("Attached processes"), 1, SCOLS_FL_RIGHT},
	[COL_STATUS]	= { "STATUS",	N_("Status"), N_("Status"), 1, SCOLS_FL_NOEXTREMES},
	[COL_ATTACH]	= { "ATTACH",	N_("Attach time"), N_("Attach time"), 1, SCOLS_FL_RIGHT},
	[COL_DETACH]	= { "DETACH",	N_("Detach time"), N_("Detach time"), 1, SCOLS_FL_RIGHT},
	[COL_COMMAND]	= { "COMMAND",  N_("Creator command line"), N_("Creator command"), 0, SCOLS_FL_TRUNC},
	[COL_CPID]	= { "CPID",	N_("PID of the creator"), N_("Creator PID"), 1, SCOLS_FL_RIGHT},
	[COL_LPID]	= { "LPID",	N_("PID of last user"), N_("Last user PID"), 1, SCOLS_FL_RIGHT},

	/* sem-specific */
	[COL_NSEMS]	= { "NSEMS",	N_("Number of semaphores"), N_("Semaphores"), 1, SCOLS_FL_RIGHT},
	[COL_OTIME]	= { "OTIME",	N_("Time of the last operation"), N_("Last operation"), 1, SCOLS_FL_RIGHT},

	/* cols for summarized information */
	[COL_RESOURCE]  = { "RESOURCE", N_("Resource name"), N_("Resource"), 1 },
	[COL_DESC]      = { "DESCRIPTION",N_("Resource description"), N_("Description"), 1 },
	[COL_USED]      = { "USED",     N_("Currently used"), N_("Used"), 1, SCOLS_FL_RIGHT },
	[COL_USEPERC]	= { "USE%",     N_("Currently use percentage"), N_("Use"), 1, SCOLS_FL_RIGHT },
	[COL_LIMIT]     = { "LIMIT",    N_("System-wide limit"), N_("Limit"), 1, SCOLS_FL_RIGHT },
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

#define add_column(ary, n, id)	\
		((ary)[ err_columns_index(ARRAY_SIZE(ary), (n)) ] = (id))

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(coldescs); i++) {
		const char *cn = coldescs[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz)) {
			if (i > COL_CTIME) {
				if (i >= LOWER && i <= UPPER)
					return i;

				warnx(_("column %s does not apply to the specified IPC"), name);
				return -1;
			}

			return i;
		}
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static int get_column_id(int num)
{
	assert(num >= 0);
	assert((size_t) num < ncolumns);
	assert((size_t) columns[num] < ARRAY_SIZE(coldescs));
	return columns[num];
}

static const struct lsipc_coldesc *get_column_desc(int num)
{
	return &coldescs[ get_column_id(num) ];
}

static char *get_username(struct passwd **pw, uid_t id)
{
	if (!*pw || (*pw)->pw_uid != id)
		*pw = getpwuid(id);

	return *pw ? xstrdup((*pw)->pw_name) : NULL;
}

static char *get_groupname(struct group **gr, gid_t id)
{
	if (!*gr || (*gr)->gr_gid != id)
		*gr = getgrgid(id);

	return *gr ? xstrdup((*gr)->gr_name) : NULL;
}

static int parse_time_mode(const char *s)
{
	struct lsipc_timefmt {
		const char *name;
		const int val;
	};
	static const struct lsipc_timefmt timefmts[] = {
		{"iso", TIME_ISO},
		{"full", TIME_FULL},
		{"short", TIME_SHORT},
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(timefmts); i++) {
		if (strcmp(timefmts[i].name, s) == 0)
			return timefmts[i].val;
	}
	errx(EXIT_FAILURE, _("unknown time format: %s"), s);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Show information on IPC facilities.\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Resource options:\n"), out);
	fputs(_(" -m, --shmems      shared memory segments\n"), out);
	fputs(_(" -q, --queues      message queues\n"), out);
	fputs(_(" -s, --semaphores  semaphores\n"), out);
	fputs(_(" -g, --global      info about system-wide usage (may be used with -m, -q and -s)\n"), out);
	fputs(_(" -i, --id <id>     print details on resource identified by <id>\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_("     --noheadings         don't print headings\n"), out);
	fputs(_("     --notruncate         don't truncate output\n"), out);
	fputs(_("     --time-format=<type> display dates in short, full or iso format\n"), out);
	fputs(_(" -b, --bytes              print SIZE in bytes rather than in human readable format\n"), out);
	fputs(_(" -c, --creator            show creator and owner\n"), out);
	fputs(_(" -e, --export             display in an export-able output format\n"), out);
	fputs(_(" -J, --json               use the JSON output format\n"), out);
	fputs(_(" -n, --newline            display each piece of information on a new line\n"), out);
	fputs(_(" -l, --list               force list output format (for example with --id)\n"), out);
	fputs(_(" -o, --output[=<list>]    define the columns to output\n"), out);
	fputs(_(" -P, --numeric-perms      print numeric permissions (PERMS column)\n"), out);
	fputs(_(" -r, --raw                display in raw mode\n"), out);
	fputs(_(" -t, --time               show attach, detach and change times\n"), out);
	fputs(_(" -y, --shell              use column names to be usable as shell variable identifiers\n"), out);


	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));

	fprintf(out, _("\nGeneric columns:\n"));
	for (i = COLDESC_IDX_GEN_FIRST; i <= COLDESC_IDX_GEN_LAST; i++)
		fprintf(out, " %14s  %s\n", coldescs[i].name, _(coldescs[i].help));

	fprintf(out, _("\nShared-memory columns (--shmems):\n"));
	for (i = COLDESC_IDX_SHM_FIRST; i <= COLDESC_IDX_SHM_LAST; i++)
		fprintf(out, " %14s  %s\n", coldescs[i].name, _(coldescs[i].help));

	fprintf(out, _("\nMessage-queue columns (--queues):\n"));
	for (i = COLDESC_IDX_MSG_FIRST; i <= COLDESC_IDX_MSG_LAST; i++)
		fprintf(out, " %14s  %s\n", coldescs[i].name, _(coldescs[i].help));

	fprintf(out, _("\nSemaphore columns (--semaphores):\n"));
	for (i = COLDESC_IDX_SEM_FIRST; i <= COLDESC_IDX_SEM_LAST; i++)
		fprintf(out, " %14s  %s\n", coldescs[i].name, _(coldescs[i].help));

	fprintf(out, _("\nSummary columns (--global):\n"));
	for (i = COLDESC_IDX_SUM_FIRST; i <= COLDESC_IDX_SUM_LAST; i++)
		fprintf(out, " %14s  %s\n", coldescs[i].name, _(coldescs[i].help));

	fprintf(out, USAGE_MAN_TAIL("lsipc(1)"));
	exit(EXIT_SUCCESS);
}

static struct libscols_table *new_table(struct lsipc_control *ctl)
{
	struct libscols_table *table = scols_new_table();

	if (!table)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	if (ctl->noheadings)
		scols_table_enable_noheadings(table, 1);
	if (ctl->shellvar)
		scols_table_enable_shellvar(table, 1);

	switch(ctl->outmode) {
	case OUT_NEWLINE:
		scols_table_set_column_separator(table, "\n");
		/* fallthrough */
	case OUT_EXPORT:
		scols_table_enable_export(table, 1);
		break;
	case OUT_RAW:
		scols_table_enable_raw(table, 1);
		break;
	case OUT_PRETTY:
		scols_table_enable_noheadings(table, 1);
		break;
	case OUT_JSON:
		scols_table_enable_json(table, 1);
		break;
	default:
		break;
	}
	return table;
}

static struct libscols_table *setup_table(struct lsipc_control *ctl)
{
	struct libscols_table *table = new_table(ctl);
	size_t n;

	for (n = 0; n < ncolumns; n++) {
		const struct lsipc_coldesc *desc = get_column_desc(n);
		int flags = desc->flag;

		if (ctl->notrunc)
			flags &= ~SCOLS_FL_TRUNC;
		if (!scols_table_new_column(table, desc->name, desc->whint, flags))
			goto fail;
	}
	return table;
fail:
	scols_unref_table(table);
	return NULL;
}

static int print_pretty(struct libscols_table *table)
{
	struct libscols_iter *itr = scols_new_iter(SCOLS_ITER_FORWARD);
	struct libscols_column *col;
	struct libscols_cell *data;
	struct libscols_line *ln;
	const char *hstr, *dstr;
	int n = 0;

	ln = scols_table_get_line(table, 0);
	while (!scols_table_next_column(table, itr, &col)) {

		data = scols_line_get_cell(ln, n);

		hstr = N_(get_column_desc(n)->pretty_name);
		dstr = scols_cell_get_data(data);

		if (dstr)
			printf("%s:%*c%-36s\n", hstr, 35 - (int)strlen(hstr), ' ', dstr);
		++n;
	}

	/* this is used to pretty-print detailed info about a semaphore array */
	if (ln) {
		struct libscols_table *subtab = scols_line_get_userdata(ln);
		if (subtab) {
			printf(_("Elements:\n\n"));
			scols_print_table(subtab);
		}
	}

	scols_free_iter(itr);
	return 0;

}

static int print_table(struct lsipc_control *ctl, struct libscols_table *tb)
{
	if (ctl->outmode == OUT_PRETTY)
		print_pretty(tb);
	else
		scols_print_table(tb);
	return 0;
}
static struct timeval now;

static char *make_time(int mode, time_t time)
{
	char buf[64] = {0};

	switch(mode) {
	case TIME_FULL:
	{
		struct tm tm;
		char *s;

		localtime_r(&time, &tm);
		asctime_r(&tm, buf);
		if (*(s = buf + strlen(buf) - 1) == '\n')
			*s = '\0';
		break;
	}
	case TIME_SHORT:
		strtime_short(&time, &now, 0, buf, sizeof(buf));
		break;
	case TIME_ISO:
		strtime_iso(&time, ISO_TIMESTAMP_T, buf, sizeof(buf));
		break;
	default:
		errx(EXIT_FAILURE, _("unsupported time type"));
	}
	return xstrdup(buf);
}

static void global_set_data(struct lsipc_control *ctl, struct libscols_table *tb,
			    const char *resource, const char *desc, uintmax_t used,
			    uintmax_t limit, int usage, int byte_unit)
{
	struct libscols_line *ln;
	size_t n;

	ln = scols_table_new_line(tb, NULL);
	if (!ln)
		err(EXIT_FAILURE, _("failed to allocate output line"));

	for (n = 0; n < ncolumns; n++) {
		int rc = 0;
		char *arg = NULL;

		switch (get_column_id(n)) {
		case COL_RESOURCE:
			rc = scols_line_set_data(ln, n, resource);
			break;
		case COL_DESC:
			rc = scols_line_set_data(ln, n, desc);
			break;
		case COL_USED:
			if (usage) {
				if (!byte_unit || ctl->bytes)
					xasprintf(&arg, "%ju", used);
				else
					arg = size_to_human_string(SIZE_SUFFIX_1LETTER, used);
				rc = scols_line_refer_data(ln, n, arg);
			} else
				rc = scols_line_set_data(ln, n, "-");
			break;
		case COL_USEPERC:
			if (usage) {
				xasprintf(&arg, "%2.2f%%", (double) used / limit * 100);
				rc = scols_line_refer_data(ln, n, arg);
			} else
				rc = scols_line_set_data(ln, n, "-");
			break;
		case COL_LIMIT:
			if (!byte_unit || ctl->bytes)
				xasprintf(&arg, "%ju", limit);
			else
				arg = size_to_human_string(SIZE_SUFFIX_1LETTER, limit);
			rc = scols_line_refer_data(ln, n, arg);
			break;
		}

		if (rc != 0)
			err(EXIT_FAILURE, _("failed to add output data"));
	}
}

static void setup_sem_elements_columns(struct libscols_table *tb)
{
	scols_table_set_name(tb, "elements");
	if (!scols_table_new_column(tb, "SEMNUM", 0, SCOLS_FL_RIGHT))
		err_oom();
	if (!scols_table_new_column(tb, "VALUE", 0, SCOLS_FL_RIGHT))
		err_oom();
	if (!scols_table_new_column(tb, "NCOUNT", 0, SCOLS_FL_RIGHT))
		err_oom();
	if (!scols_table_new_column(tb, "ZCOUNT", 0, SCOLS_FL_RIGHT))
		err_oom();
	if (!scols_table_new_column(tb, "PID", 0, SCOLS_FL_RIGHT))
		err_oom();
	if (!scols_table_new_column(tb, "COMMAND", 0, SCOLS_FL_RIGHT))
		err_oom();
}

static void do_sem(int id, struct lsipc_control *ctl, struct libscols_table *tb)
{
	struct libscols_line *ln;
	struct passwd *pw = NULL, *cpw = NULL;
	struct group *gr = NULL, *cgr = NULL;
	struct sem_data *semds, *semdsp;
	char *arg = NULL;

	scols_table_set_name(tb, "semaphores");

	if (ipc_sem_get_info(id, &semds) < 1) {
		if (id > -1)
			warnx(_("id %d not found"), id);
		return;
	}
	for (semdsp = semds;  semdsp->next != NULL || id > -1; semdsp = semdsp->next) {
		size_t n;

		ln = scols_table_new_line(tb, NULL);
		if (!ln)
			err(EXIT_FAILURE, _("failed to allocate output line"));

		for (n = 0; n < ncolumns; n++) {
			int rc = 0;
			switch (get_column_id(n)) {
			case COL_KEY:
				xasprintf(&arg, "0x%08x",semdsp->sem_perm.key);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_ID:
				xasprintf(&arg, "%d",semdsp->sem_perm.id);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_OWNER:
				arg = get_username(&pw, semdsp->sem_perm.uid);
				if (!arg)
					xasprintf(&arg, "%u", semdsp->sem_perm.uid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_PERMS:
				if (ctl->numperms)
					xasprintf(&arg, "%#o", semdsp->sem_perm.mode & 0777);
				else {
					arg = xmalloc(11);
					xstrmode(semdsp->sem_perm.mode & 0777, arg);
				}
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CUID:
				xasprintf(&arg, "%u", semdsp->sem_perm.cuid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CUSER:
				arg = get_username(&cpw, semdsp->sem_perm.cuid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CGID:
				xasprintf(&arg, "%u", semdsp->sem_perm.cgid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CGROUP:
				arg = get_groupname(&cgr, semdsp->sem_perm.cgid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_UID:
				xasprintf(&arg, "%u", semdsp->sem_perm.uid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_USER:
				arg = get_username(&pw, semdsp->sem_perm.uid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_GID:
				xasprintf(&arg, "%u", semdsp->sem_perm.gid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_GROUP:
				arg = get_groupname(&gr, semdsp->sem_perm.gid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CTIME:
				if (semdsp->sem_ctime != 0) {
					rc = scols_line_refer_data(ln, n,
							make_time(ctl->time_mode,
							  (time_t)semdsp->sem_ctime));
				}
				break;
			case COL_NSEMS:
				xasprintf(&arg, "%ju", semdsp->sem_nsems);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_OTIME:
				if (semdsp->sem_otime != 0) {
					rc = scols_line_refer_data(ln, n,
							make_time(ctl->time_mode,
							  (time_t)semdsp->sem_otime));
				}
				break;
			}
			if (rc != 0)
				err(EXIT_FAILURE, _("failed to add output data"));
			arg = NULL;
		}

		if (id > -1 && semds->sem_nsems) {
			/* Create extra table with ID specific semaphore elements */
			struct libscols_table *sub = new_table(ctl);
			size_t i;
			int rc = 0;

			scols_table_enable_noheadings(sub, 0);
			setup_sem_elements_columns(sub);

			for (i = 0; i < semds->sem_nsems; i++) {
				struct sem_elem *e = &semds->elements[i];
				struct libscols_line *sln = scols_table_new_line(sub, NULL);

				if (!sln)
					err(EXIT_FAILURE, _("failed to allocate output line"));

				/* SEMNUM */
				xasprintf(&arg, "%zu", i);
				rc = scols_line_refer_data(sln, 0, arg);
				if (rc)
					break;

				/* VALUE */
				xasprintf(&arg, "%d", e->semval);
				rc = scols_line_refer_data(sln, 1, arg);
				if (rc)
					break;

				/* NCOUNT */
				xasprintf(&arg, "%d", e->ncount);
				rc = scols_line_refer_data(sln, 2, arg);
				if (rc)
					break;

				/* ZCOUNT */
				xasprintf(&arg, "%d", e->zcount);
				rc = scols_line_refer_data(sln, 3, arg);
				if (rc)
					break;

				/* PID */
				xasprintf(&arg, "%d", e->pid);
				rc = scols_line_refer_data(sln, 4, arg);
				if (rc)
					break;

				/* COMMAND */
				arg = pid_get_cmdline(e->pid);
				rc = scols_line_refer_data(sln, 5, arg);
				if (rc)
					break;
			}

			if (rc != 0)
				err(EXIT_FAILURE, _("failed to set data"));

			scols_line_set_userdata(ln, (void *)sub);
			break;
		}
	}
	ipc_sem_free_info(semds);
}

static void do_sem_global(struct lsipc_control *ctl, struct libscols_table *tb)
{
	struct sem_data *semds;
	struct ipc_limits lim;
	int nsems = 0, nsets = 0;

	ipc_sem_get_limits(&lim);

	if (ipc_sem_get_info(-1, &semds) > 0) {
		struct sem_data *p;

		for (p = semds; p->next != NULL; p = p->next) {
			++nsets;
			nsems += p->sem_nsems;
		}
		ipc_sem_free_info(semds);
	}

	global_set_data(ctl, tb, "SEMMNI", _("Number of semaphore identifiers"), nsets, lim.semmni, 1, 0);
	global_set_data(ctl, tb, "SEMMNS", _("Total number of semaphores"), nsems, lim.semmns, 1, 0);
	global_set_data(ctl, tb, "SEMMSL", _("Max semaphores per semaphore set."), 0, lim.semmsl, 0, 0);
	global_set_data(ctl, tb, "SEMOPM", _("Max number of operations per semop(2)"), 0, lim.semopm, 0, 0);
	global_set_data(ctl, tb, "SEMVMX", _("Semaphore max value"), 0, lim.semvmx, 0, 0);
}

static void do_msg(int id, struct lsipc_control *ctl, struct libscols_table *tb)
{
	struct libscols_line *ln;
	struct passwd *pw = NULL;
	struct group *gr = NULL;
	struct msg_data *msgds, *msgdsp;
	char *arg = NULL;

	if (ipc_msg_get_info(id, &msgds) < 1) {
		if (id > -1)
			warnx(_("id %d not found"), id);
		return;
	}
	scols_table_set_name(tb, "messages");

	for (msgdsp = msgds; msgdsp->next != NULL || id > -1 ; msgdsp = msgdsp->next) {
		size_t n;
		ln = scols_table_new_line(tb, NULL);

		if (!ln)
			err(EXIT_FAILURE, _("failed to allocate output line"));

		/* no need to call getpwuid() for the same user */
		if (!(pw && pw->pw_uid == msgdsp->msg_perm.uid))
			pw = getpwuid(msgdsp->msg_perm.uid);

		/* no need to call getgrgid() for the same user */
		if (!(gr && gr->gr_gid == msgdsp->msg_perm.gid))
			gr = getgrgid(msgdsp->msg_perm.gid);

		for (n = 0; n < ncolumns; n++) {
			int rc = 0;

			switch (get_column_id(n)) {
			case COL_KEY:
				xasprintf(&arg, "0x%08x",msgdsp->msg_perm.key);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_ID:
				xasprintf(&arg, "%d",msgdsp->msg_perm.id);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_OWNER:
				arg = get_username(&pw, msgdsp->msg_perm.uid);
				if (!arg)
					xasprintf(&arg, "%u", msgdsp->msg_perm.uid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_PERMS:
				if (ctl->numperms)
					xasprintf(&arg, "%#o", msgdsp->msg_perm.mode & 0777);
				else {
					arg = xmalloc(11);
					xstrmode(msgdsp->msg_perm.mode & 0777, arg);
					rc = scols_line_refer_data(ln, n, arg);
				}
				break;
			case COL_CUID:
				xasprintf(&arg, "%u", msgdsp->msg_perm.cuid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CUSER:
				arg = get_username(&pw, msgdsp->msg_perm.cuid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CGID:
				xasprintf(&arg, "%u", msgdsp->msg_perm.cuid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CGROUP:
				arg = get_groupname(&gr, msgdsp->msg_perm.cgid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_UID:
				xasprintf(&arg, "%u", msgdsp->msg_perm.uid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_USER:
				arg = get_username(&pw, msgdsp->msg_perm.uid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_GID:
				xasprintf(&arg, "%u", msgdsp->msg_perm.gid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_GROUP:
				arg = get_groupname(&gr,msgdsp->msg_perm.gid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CTIME:
				if (msgdsp->q_ctime != 0)
					rc = scols_line_refer_data(ln, n,
						make_time(ctl->time_mode,
							  (time_t)msgdsp->q_ctime));
				break;
			case COL_USEDBYTES:
				if (ctl->bytes)
					xasprintf(&arg, "%ju", msgdsp->q_cbytes);
				else
					arg = size_to_human_string(SIZE_SUFFIX_1LETTER, msgdsp->q_cbytes);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_MSGS:
				xasprintf(&arg, "%ju", msgdsp->q_qnum);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_SEND:
				if (msgdsp->q_stime != 0)
					rc = scols_line_refer_data(ln, n,
						make_time(ctl->time_mode,
							  (time_t)msgdsp->q_stime));
				break;
			case COL_RECV:
				if (msgdsp->q_rtime != 0)
					rc = scols_line_refer_data(ln, n,
						make_time(ctl->time_mode,
							  (time_t)msgdsp->q_rtime));
				break;
			case COL_LSPID:
				xasprintf(&arg, "%u", msgdsp->q_lspid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_LRPID:
				xasprintf(&arg, "%u", msgdsp->q_lrpid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			}
			if (rc != 0)
				err(EXIT_FAILURE, _("failed to set data"));
			arg = NULL;
		}
		if (id > -1)
			break;
	}
	ipc_msg_free_info(msgds);
}


static void do_msg_global(struct lsipc_control *ctl, struct libscols_table *tb)
{
	struct msg_data *msgds, *msgdsp;
	struct ipc_limits lim;
	int msgqs = 0;

	ipc_msg_get_limits(&lim);

	/* count number of used queues */
	if (ipc_msg_get_info(-1, &msgds) > 0) {
		for (msgdsp = msgds; msgdsp->next != NULL; msgdsp = msgdsp->next)
			++msgqs;
		ipc_msg_free_info(msgds);
	}

	global_set_data(ctl, tb, "MSGMNI", _("Number of message queues"), msgqs, lim.msgmni, 1, 0);
	global_set_data(ctl, tb, "MSGMAX", _("Max size of message (bytes)"),	0, lim.msgmax, 0, 1);
	global_set_data(ctl, tb, "MSGMNB", _("Default max size of queue (bytes)"), 0, lim.msgmnb, 0, 1);
}


static void do_shm(int id, struct lsipc_control *ctl, struct libscols_table *tb)
{
	struct libscols_line *ln;
	struct passwd *pw = NULL;
	struct group *gr = NULL;
	struct shm_data *shmds, *shmdsp;
	char *arg = NULL;

	if (ipc_shm_get_info(id, &shmds) < 1) {
		if (id > -1)
			warnx(_("id %d not found"), id);
		return;
	}

	scols_table_set_name(tb, "sharedmemory");

	for (shmdsp = shmds; shmdsp->next != NULL || id > -1 ; shmdsp = shmdsp->next) {
		size_t n;
		ln = scols_table_new_line(tb, NULL);

		if (!ln)
			err(EXIT_FAILURE, _("failed to allocate output line"));

		for (n = 0; n < ncolumns; n++) {
			int rc = 0;

			switch (get_column_id(n)) {
			case COL_KEY:
				xasprintf(&arg, "0x%08x",shmdsp->shm_perm.key);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_ID:
				xasprintf(&arg, "%d",shmdsp->shm_perm.id);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_OWNER:
				arg = get_username(&pw, shmdsp->shm_perm.uid);
				if (!arg)
					xasprintf(&arg, "%u", shmdsp->shm_perm.uid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_PERMS:
				if (ctl->numperms)
					xasprintf(&arg, "%#o", shmdsp->shm_perm.mode & 0777);
				else {
					arg = xmalloc(11);
					xstrmode(shmdsp->shm_perm.mode & 0777, arg);
				}
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CUID:
				xasprintf(&arg, "%u", shmdsp->shm_perm.cuid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CUSER:
				arg = get_username(&pw, shmdsp->shm_perm.cuid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CGID:
				xasprintf(&arg, "%u", shmdsp->shm_perm.cuid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CGROUP:
				arg = get_groupname(&gr, shmdsp->shm_perm.cgid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_UID:
				xasprintf(&arg, "%u", shmdsp->shm_perm.uid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_USER:
				arg = get_username(&pw, shmdsp->shm_perm.uid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_GID:
				xasprintf(&arg, "%u", shmdsp->shm_perm.gid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_GROUP:
				arg = get_groupname(&gr, shmdsp->shm_perm.gid);
				if (arg)
					rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_CTIME:
				if (shmdsp->shm_ctim != 0)
					rc = scols_line_refer_data(ln, n,
						make_time(ctl->time_mode,
							  (time_t)shmdsp->shm_ctim));
				break;
			case COL_SIZE:
				if (ctl->bytes)
					xasprintf(&arg, "%ju", shmdsp->shm_segsz);
				else
					arg = size_to_human_string(SIZE_SUFFIX_1LETTER, shmdsp->shm_segsz);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_NATTCH:
				xasprintf(&arg, "%ju", shmdsp->shm_nattch);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_STATUS: {
					int comma = 0;
					size_t offt = 0;

					free(arg);
					arg = xcalloc(1, sizeof(char) * strlen(_("dest"))
							+ strlen(_("locked"))
							+ strlen(_("hugetlb"))
							+ strlen(_("noreserve")) + 4);
#ifdef SHM_DEST
					if (shmdsp->shm_perm.mode & SHM_DEST) {
						offt += sprintf(arg, "%s", _("dest"));
						comma++;
					}
#endif
#ifdef SHM_LOCKED
					if (shmdsp->shm_perm.mode & SHM_LOCKED) {
						if (comma)
							arg[offt++] = ',';
						offt += sprintf(arg + offt, "%s", _("locked"));
					}
#endif
#ifdef SHM_HUGETLB
					if (shmdsp->shm_perm.mode & SHM_HUGETLB) {
						if (comma)
							arg[offt++] = ',';
						offt += sprintf(arg + offt, "%s", _("hugetlb"));
					}
#endif
#ifdef SHM_NORESERVE
					if (shmdsp->shm_perm.mode & SHM_NORESERVE) {
						if (comma)
							arg[offt++] = ',';
						sprintf(arg + offt, "%s", _("noreserve"));
					}
#endif
					rc = scols_line_refer_data(ln, n, arg);
				}
				break;
			case COL_ATTACH:
				if (shmdsp->shm_atim != 0)
					rc = scols_line_refer_data(ln, n,
							make_time(ctl->time_mode,
							  (time_t)shmdsp->shm_atim));
				break;
			case COL_DETACH:
				if (shmdsp->shm_dtim != 0)
					rc = scols_line_refer_data(ln, n,
							make_time(ctl->time_mode,
							  (time_t)shmdsp->shm_dtim));
				break;
			case COL_CPID:
				xasprintf(&arg, "%u", shmdsp->shm_cprid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_LPID:
				xasprintf(&arg, "%u", shmdsp->shm_lprid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			case COL_COMMAND:
				arg = pid_get_cmdline(shmdsp->shm_cprid);
				rc = scols_line_refer_data(ln, n, arg);
				break;
			}
			if (rc != 0)
				err(EXIT_FAILURE, _("failed to set data"));
			arg = NULL;
		}
		if (id > -1)
			break;
	}
	ipc_shm_free_info(shmds);
}

static void do_shm_global(struct lsipc_control *ctl, struct libscols_table *tb)
{
	struct shm_data *shmds, *shmdsp;
	uint64_t nsegs = 0, sum_segsz = 0;
	struct ipc_limits lim;

	ipc_shm_get_limits(&lim);

	if (ipc_shm_get_info(-1, &shmds) > 0) {
		for (shmdsp = shmds; shmdsp->next != NULL; shmdsp = shmdsp->next) {
			++nsegs;
			sum_segsz += shmdsp->shm_segsz;
		}
		ipc_shm_free_info(shmds);
	}

	global_set_data(ctl, tb, "SHMMNI", _("Shared memory segments"), nsegs, lim.shmmni, 1, 0);
	global_set_data(ctl, tb, "SHMALL", _("Shared memory pages"), sum_segsz / getpagesize(), lim.shmall, 1, 0);
	global_set_data(ctl, tb, "SHMMAX", _("Max size of shared memory segment (bytes)"), 0, lim.shmmax, 0, 1);
	global_set_data(ctl, tb, "SHMMIN", _("Min size of shared memory segment (bytes)"), 0, lim.shmmin, 0, 1);
}

int main(int argc, char *argv[])
{
	int opt, msg = 0, sem = 0, shm = 0, id = -1;
	int show_time = 0, show_creat = 0, global = 0;
	size_t i;
	struct lsipc_control *ctl = xcalloc(1, sizeof(struct lsipc_control));
	static struct libscols_table *tb;
	char *outarg = NULL;

	/* long only options. */
	enum {
		OPT_NOTRUNC = CHAR_MAX + 1,
		OPT_NOHEAD,
		OPT_TIME_FMT
	};

	static const struct option longopts[] = {
		{ "bytes",          no_argument,        NULL, 'b' },
		{ "creator",        no_argument,	NULL, 'c' },
		{ "export",         no_argument,	NULL, 'e' },
		{ "global",         no_argument,	NULL, 'g' },
		{ "help",           no_argument,	NULL, 'h' },
		{ "id",             required_argument,	NULL, 'i' },
		{ "json",           no_argument,	NULL, 'J' },
		{ "list",           no_argument,        NULL, 'l' },
		{ "newline",        no_argument,	NULL, 'n' },
		{ "noheadings",     no_argument,	NULL, OPT_NOHEAD },
		{ "notruncate",     no_argument,	NULL, OPT_NOTRUNC },
		{ "numeric-perms",  no_argument,	NULL, 'P' },
		{ "output",         required_argument,	NULL, 'o' },
		{ "queues",         no_argument,	NULL, 'q' },
		{ "raw",            no_argument,	NULL, 'r' },
		{ "semaphores",     no_argument,	NULL, 's' },
		{ "shmems",         no_argument,	NULL, 'm' },
		{ "time",           no_argument,	NULL, 't' },
		{ "time-format",    required_argument,	NULL, OPT_TIME_FMT },
		{ "version",        no_argument,	NULL, 'V' },
		{ "shell",          no_argument,	NULL, 'y' },
		{NULL, 0, NULL, 0}
	};

	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'J', 'e', 'l', 'n', 'r' },
		{ 'g', 'i' },
		{ 'c', 'o', 't' },
		{ 'm', 'q', 's' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	ctl->time_mode = 0;

	scols_init_debug(0);

	while ((opt = getopt_long(argc, argv, "bceghi:Jlmno:PqrstVy", longopts, NULL)) != -1) {

		err_exclusive_options(opt, longopts, excl, excl_st);

		switch (opt) {
		case 'b':
			ctl->bytes = 1;
			break;
		case 'i':
			id = strtos32_or_err(optarg, _("failed to parse IPC identifier"));
			break;
		case 'e':
			ctl->outmode = OUT_EXPORT;
			break;
		case 'r':
			ctl->outmode = OUT_RAW;
			break;
		case 'o':
			outarg = optarg;
			break;
		case 'g':
			global = 1;
			break;
		case 'q':
			msg = 1;
			add_column(columns, ncolumns++, COL_KEY);
			add_column(columns, ncolumns++, COL_ID);
			add_column(columns, ncolumns++, COL_PERMS);
			add_column(columns, ncolumns++, COL_OWNER);
			add_column(columns, ncolumns++, COL_USEDBYTES);
			add_column(columns, ncolumns++, COL_MSGS);
			add_column(columns, ncolumns++, COL_LSPID);
			add_column(columns, ncolumns++, COL_LRPID);
			LOWER = COLDESC_IDX_MSG_FIRST;
			UPPER = COLDESC_IDX_MSG_LAST;
			break;
		case 'l':
			ctl->outmode = OUT_LIST;
			break;
		case 'm':
			shm = 1;
			add_column(columns, ncolumns++, COL_KEY);
			add_column(columns, ncolumns++, COL_ID);
			add_column(columns, ncolumns++, COL_PERMS);
			add_column(columns, ncolumns++, COL_OWNER);
			add_column(columns, ncolumns++, COL_SIZE);
			add_column(columns, ncolumns++, COL_NATTCH);
			add_column(columns, ncolumns++, COL_STATUS);
			add_column(columns, ncolumns++, COL_CTIME);
			add_column(columns, ncolumns++, COL_CPID);
			add_column(columns, ncolumns++, COL_LPID);
			add_column(columns, ncolumns++, COL_COMMAND);
			LOWER = COLDESC_IDX_SHM_FIRST;
			UPPER = COLDESC_IDX_SHM_LAST;
			break;
		case 'n':
			ctl->outmode = OUT_NEWLINE;
			break;
		case 'P':
			ctl->numperms = 1;
			break;
		case 's':
			sem = 1;
			add_column(columns, ncolumns++, COL_KEY);
			add_column(columns, ncolumns++, COL_ID);
			add_column(columns, ncolumns++, COL_PERMS);
			add_column(columns, ncolumns++, COL_OWNER);
			add_column(columns, ncolumns++, COL_NSEMS);
			LOWER = COLDESC_IDX_SEM_FIRST;
			UPPER = COLDESC_IDX_SEM_LAST;
			break;
		case OPT_NOTRUNC:
			ctl->notrunc = 1;
			break;
		case OPT_NOHEAD:
			ctl->noheadings = 1;
			break;
		case OPT_TIME_FMT:
			ctl->time_mode = parse_time_mode(optarg);
			break;
		case 'J':
			ctl->outmode = OUT_JSON;
			break;
		case 't':
			show_time = 1;
			break;
		case 'c':
			show_creat = 1;
			break;
		case 'y':
			ctl->shellvar = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	/* default is global */
	if (msg + shm + sem == 0) {
		msg = shm = sem = global = 1;
		if (show_time || show_creat || id != -1)
			errx(EXIT_FAILURE, _("--global is mutually exclusive with --creator, --id and --time"));
	}
	if (global) {
		add_column(columns, ncolumns++, COL_RESOURCE);
		add_column(columns, ncolumns++, COL_DESC);
		add_column(columns, ncolumns++, COL_LIMIT);
		add_column(columns, ncolumns++, COL_USED);
		add_column(columns, ncolumns++, COL_USEPERC);
		LOWER = COLDESC_IDX_SUM_FIRST;
		UPPER = COLDESC_IDX_SUM_LAST;
	}

	/* default to pretty-print if --id specified */
	if (id != -1 && !ctl->outmode)
		ctl->outmode = OUT_PRETTY;

	if (!ctl->time_mode)
		ctl->time_mode = ctl->outmode == OUT_PRETTY ? TIME_FULL : TIME_SHORT;

	if (ctl->outmode == OUT_PRETTY && !(optarg || show_creat || show_time)) {
		/* all columns for lsipc --<RESOURCE> --id <ID> */
		for (ncolumns = 0, i = 0; i < ARRAY_SIZE(coldescs); i++)
			 columns[ncolumns++] = i;
	} else {
		if (show_creat) {
			add_column(columns, ncolumns++, COL_CUID);
			add_column(columns, ncolumns++, COL_CGID);
			add_column(columns, ncolumns++, COL_UID);
			add_column(columns, ncolumns++, COL_GID);
		}
		if (msg && show_time) {
			add_column(columns, ncolumns++, COL_SEND);
			add_column(columns, ncolumns++, COL_RECV);
			add_column(columns, ncolumns++, COL_CTIME);
		}
		if (shm && show_time) {
			/* keep "COMMAND" as last column */
			size_t cmd = columns[ncolumns - 1] == COL_COMMAND;

			if (cmd)
				ncolumns--;
			add_column(columns, ncolumns++, COL_ATTACH);
			add_column(columns, ncolumns++, COL_DETACH);
			if (cmd)
				add_column(columns, ncolumns++, COL_COMMAND);
		}
		if (sem && show_time) {
			add_column(columns, ncolumns++, COL_OTIME);
			add_column(columns, ncolumns++, COL_CTIME);
		}
	}

	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
					 &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	tb = setup_table(ctl);
	if (!tb)
		return EXIT_FAILURE;

	if (global)
		scols_table_set_name(tb, "ipclimits");

	if (msg) {
		if (global)
			do_msg_global(ctl, tb);
		else
			do_msg(id, ctl, tb);
	}
	if (shm) {
		if (global)
			do_shm_global(ctl ,tb);
		else
			do_shm(id, ctl, tb);
	}
	if (sem) {
		if (global)
			do_sem_global(ctl, tb);
		else
			do_sem(id, ctl, tb);
	}

	print_table(ctl, tb);

	scols_unref_table(tb);
	free(ctl);

	return EXIT_SUCCESS;
}
