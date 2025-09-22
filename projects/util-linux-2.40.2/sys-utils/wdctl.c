/*
 * wdctl(8) - show hardware watchdog status
 *
 * Copyright (C) 2012 Lennart Poettering
 * Copyright (C) 2012 Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <sys/ioctl.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <linux/watchdog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libsmartcols.h>

#include "nls.h"
#include "c.h"
#include "xalloc.h"
#include "closestream.h"
#include "optutils.h"
#include "pathnames.h"
#include "strutils.h"
#include "carefulputc.h"
#include "path.h"
#include "strv.h"

/*
 * since 2.6.18
 */
#ifndef WDIOC_SETPRETIMEOUT
# define WDIOC_SETPRETIMEOUT    _IOWR(WATCHDOG_IOCTL_BASE, 8, int)
# define WDIOC_GETPRETIMEOUT	_IOR(WATCHDOG_IOCTL_BASE, 9, int)
# define WDIOC_GETTIMELEFT	_IOR(WATCHDOG_IOCTL_BASE, 10, int)
# define WDIOF_POWEROVER	0x0040	/* Power over voltage */
# define WDIOF_SETTIMEOUT	0x0080  /* Set timeout (in seconds) */
# define WDIOF_MAGICCLOSE	0x0100	/* Supports magic close char */
# define WDIOF_PRETIMEOUT	0x0200  /* Pretimeout (in seconds), get/set */
# define WDIOF_KEEPALIVEPING	0x8000	/* Keep alive ping reply */
#endif

/*
 * since 3.5
 */
#ifndef WDIOF_ALARMONLY
# define WDIOF_ALARMONLY	0x0400	/* Watchdog triggers a management or
					   other external alarm not a reboot */
#endif

struct wdflag {
	uint32_t	flag;
	const char	*name;
	const char	*description;
};

static const struct wdflag wdflags[] = {
	{ WDIOF_CARDRESET,     "CARDRESET",  N_("Card previously reset the CPU") },
	{ WDIOF_EXTERN1,       "EXTERN1",    N_("External relay 1") },
	{ WDIOF_EXTERN2,       "EXTERN2",    N_("External relay 2") },
	{ WDIOF_FANFAULT,      "FANFAULT",   N_("Fan failed") },
	{ WDIOF_KEEPALIVEPING, "KEEPALIVEPING", N_("Keep alive ping reply") },
	{ WDIOF_MAGICCLOSE,    "MAGICCLOSE", N_("Supports magic close char") },
	{ WDIOF_OVERHEAT,      "OVERHEAT",   N_("Reset due to CPU overheat") },
	{ WDIOF_POWEROVER,     "POWEROVER",  N_("Power over voltage") },
	{ WDIOF_POWERUNDER,    "POWERUNDER", N_("Power bad/power fault") },
	{ WDIOF_PRETIMEOUT,    "PRETIMEOUT", N_("Pretimeout (in seconds)") },
	{ WDIOF_SETTIMEOUT,    "SETTIMEOUT", N_("Set timeout (in seconds)") },
	{ WDIOF_ALARMONLY,     "ALARMONLY",  N_("Not trigger reboot") }
};


/* column names */
struct colinfo {
	const char * const	name; /* header */
	double			whint; /* width hint (N < 1 is in percent of termwidth) */
	int			flags; /* SCOLS_FL_* */
	const char		*help;
};

enum { COL_FLAG, COL_DESC, COL_STATUS, COL_BSTATUS, COL_DEVICE };

/* columns descriptions */
static const struct colinfo infos[] = {
	[COL_FLAG]    = { "FLAG",        14,  0, N_("flag name") },
	[COL_DESC]    = { "DESCRIPTION", 0.1, SCOLS_FL_TRUNC, N_("flag description") },
	[COL_STATUS]  = { "STATUS",      1,   SCOLS_FL_RIGHT, N_("flag status") },
	[COL_BSTATUS] = { "BOOT-STATUS", 1,   SCOLS_FL_RIGHT, N_("flag boot status") },
	[COL_DEVICE]  = { "DEVICE",      0.1, 0, N_("watchdog device name") }

};

static int columns[ARRAY_SIZE(infos) * 2];
static int ncolumns;

struct wd_device {
	const char	*devpath;
	struct path_cxt	*sysfs;

	char		*governor;
	char		**available_governors;

	int		timeout;
	int		timeleft;
	int		pretimeout;

	uint32_t	status;
	uint32_t	bstatus;
	int		nowayout;

	struct watchdog_info ident;

	unsigned int	has_identity : 1,
			has_fw_version : 1,
			has_options : 1,
			has_status : 1,
			has_bootstatus : 1,
			has_timeout : 1,
			has_timeleft : 1,
			has_pretimeout : 1,
			has_nowayout : 1,
			no_sysfs : 1;
};

struct wd_control {
	/* set */
	int		timeout;			/* --settimeout */
	int		pretimeout;			/* --setpretimeout */
	const char      *governor;			/* --setpregovernor */
	unsigned int	set_timeout : 1,
			set_pretimeout : 1;

	/* output */
	unsigned int	show_oneline : 1,
			show_raw : 1,
			hide_headings : 1,
			hide_flags : 1,
			hide_ident : 1,
			hide_timeouts : 1;
};

#define want_set(_ctl)		((_ctl)->set_timeout \
				  || (_ctl)->set_pretimeout \
				  || (_ctl)->governor)

/* converts flag name to flag bit */
static long name2bit(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(wdflags); i++) {
		const char *cn = wdflags[i].name;
		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return wdflags[i].flag;
	}
	warnx(_("unknown flag: %s"), name);
	return -1;
}

static int column2id(const char *name, size_t namesz)
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
	assert(num < ncolumns);
	assert(columns[num] < (int) ARRAY_SIZE(infos));

	return columns[num];
}

static const struct colinfo *get_column_info(unsigned num)
{
	return &infos[ get_column_id(num) ];
}

/* We preffer cdev /dev/watchdog0 as this device has node in
 * /sys/class/watchdog/. The old miscdev /dev/watchdog is fallback for old
 * systemds only.
 */
static const char *get_default_device(void)
{
	const char **p;
	static const char *devs[] = {
		"/dev/watchdog0",
		"/dev/watchdog",
		NULL
	};

	for (p = devs; *p; p++) {
		if (access(*p, F_OK) == 0)
			return *p;
	}

	return NULL;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;
	const char *dflt = get_default_device();

	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] [<device> ...]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Show the status of the hardware watchdog.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -f, --flags <list>     print selected flags only\n"
		" -F, --noflags          don't print information about flags\n"
		" -I, --noident          don't print watchdog identity information\n"
		" -n, --noheadings       don't print headings for flags table\n"
		" -O, --oneline          print all information on one line\n"
		" -o, --output <list>    output columns of the flags\n"
		" -p, --setpretimeout <sec> set watchdog pre-timeout\n"
		" -g, --setpregovernor <name> set pre-timeout governor\n"
		" -r, --raw              use raw output format for flags table\n"
		" -T, --notimeouts       don't print watchdog timeouts\n"
		" -s, --settimeout <sec> set watchdog timeout\n"
		" -x, --flags-only       print only flags table (same as -I -T)\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(24));
	fputs(USAGE_SEPARATOR, out);

	if (dflt)
		fprintf(out, _("The default device is %s.\n"), dflt);
	else
		fprintf(out, _("No default device is available.\n"));

	fputs(USAGE_COLUMNS, out);
	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(out, " %13s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, USAGE_MAN_TAIL("wdctl(8)"));

	exit(EXIT_SUCCESS);
}

static struct path_cxt *get_sysfs(struct wd_device *wd)
{
	struct path_cxt *sys;
	struct stat st;

	if (wd->no_sysfs)
		return NULL;
	if (wd->sysfs)
		return wd->sysfs;
	if (stat(wd->devpath, &st) != 0)
		goto nosysfs;

	sys = ul_new_path(_PATH_SYS_DEVCHAR "/%u:%u",
			major(st.st_rdev), minor(st.st_rdev));
	if (!sys)
		return NULL;

	if (ul_path_get_dirfd(sys) < 0)
		goto nosysfs;		/* device not in /sys */

	if (ul_path_access(sys, F_OK, "identity") != 0)
		goto nosysfs;		/* no info in /sys (old miscdev?) */

	wd->sysfs = sys;
	return sys;
nosysfs:
	wd->no_sysfs = 1;
	return NULL;
}

static void add_flag_line(struct libscols_table *table, struct wd_device *wd, const struct wdflag *fl)
{
	int i;
	struct libscols_line *line;

	line = scols_table_new_line(table, NULL);
	if (!line) {
		warn(_("failed to allocate output line"));
		return;
	}

	for (i = 0; i < ncolumns; i++) {
		const char *str = NULL;

		switch (get_column_id(i)) {
		case COL_FLAG:
			str = fl->name;
			break;
		case COL_DESC:
			str = fl->description;
			break;
		case COL_STATUS:
			str = wd->status & fl->flag ? "1" : "0";
			break;
		case COL_BSTATUS:
			str = wd->bstatus & fl->flag ? "1" : "0";
			break;
		case COL_DEVICE:
			str = wd->devpath;
			break;
		default:
			break;
		}

		if (str && scols_line_set_data(line, i, str)) {
			warn(_("failed to add output data"));
			break;
		}
	}
}

static int show_flags(struct wd_control *ctl, struct wd_device *wd, uint32_t wanted)
{
	size_t i;
	int rc = -1;
	struct libscols_table *table;
	uint32_t flags;

	/* information about supported bits is probably missing in /sys */
	if (!wd->ident.options)
		return 0;

	scols_init_debug(0);

	/* create output table */
	table = scols_new_table();
	if (!table) {
		warn(_("failed to allocate output table"));
		return -1;
	}
	scols_table_enable_raw(table, ctl->show_raw);
	scols_table_enable_noheadings(table, ctl->hide_headings);

	/* define columns */
	for (i = 0; i < (size_t) ncolumns; i++) {
		const struct colinfo *col = get_column_info(i);

		if (!scols_table_new_column(table, col->name, col->whint, col->flags)) {
			warnx(_("failed to allocate output column"));
			goto done;
		}
	}

	/* fill-in table with data
	 * -- one line for each supported flag (option)	 */
	flags = wd->ident.options;

	for (i = 0; i < ARRAY_SIZE(wdflags); i++) {
		if (wanted && !(wanted & wdflags[i].flag))
			; /* ignore */
		else if (flags & wdflags[i].flag)
			add_flag_line(table, wd, &wdflags[i]);

		flags &= ~wdflags[i].flag;
	}

	if (flags)
		warnx(_("%s: unknown flags 0x%x\n"), wd->devpath, flags);

	scols_print_table(table);
	rc = 0;
done:
	scols_unref_table(table);
	return rc;
}

/*
 * Warning: successfully opened watchdog has to be properly closed with magic
 * close character otherwise the machine will be rebooted!
 *
 * Don't use err() or exit() here!
 */
static int set_watchdog(struct wd_control *ctl, struct wd_device *wd)
{
	int fd;
	sigset_t sigs, oldsigs;
	int rc = 0;

	assert(wd);
	assert(wd->devpath);
	assert(ctl);

	if (!ctl->set_timeout && !ctl->set_pretimeout)
		goto sysfs_only;

	sigemptyset(&oldsigs);
	sigfillset(&sigs);
	sigprocmask(SIG_BLOCK, &sigs, &oldsigs);

	fd = open(wd->devpath, O_WRONLY|O_CLOEXEC);

	if (fd < 0) {
		if (errno == EBUSY)
			warnx(_("%s: watchdog already in use, terminating."),
					wd->devpath);
		warn(_("cannot open %s"), wd->devpath);
		return -1;
	}

	for (;;) {
		/* We just opened this to query the state, not to arm
		 * it hence use the magic close character */
		static const char v = 'V';

		if (write(fd, &v, 1) >= 0)
			break;
		if (errno != EINTR) {
			warn(_("%s: failed to disarm watchdog"), wd->devpath);
			break;
		}
		/* Let's try hard, since if we don't get this right
		 * the machine might end up rebooting. */
	}

	if (ctl->set_timeout) {
		if (ioctl(fd, WDIOC_SETTIMEOUT, &ctl->timeout) != 0) {
			rc += errno;
			warn(_("cannot set timeout for %s"), wd->devpath);
		} else
			printf(P_("Timeout has been set to %d second.\n",
				  "Timeout has been set to %d seconds.\n",
				  ctl->timeout), ctl->timeout);
	}

	if (ctl->set_pretimeout) {
		if (ioctl(fd, WDIOC_SETPRETIMEOUT, &ctl->pretimeout) != 0) {
			rc += errno;
			warn(_("cannot set pretimeout for %s"), wd->devpath);
		} else
			printf(P_("Pre-timeout has been set to %d second.\n",
				  "Pre-timeout has been set to %d seconds.\n",
				  ctl->pretimeout), ctl->pretimeout);
	}

	if (close(fd))
		warn(_("write failed"));

	sigprocmask(SIG_SETMASK, &oldsigs, NULL);

sysfs_only:
	if (ctl->governor) {
		struct path_cxt *sys = get_sysfs(wd);
		int xrc;

		xrc = !sys ? errno :
			     ul_path_write_string(sys, ctl->governor,
						"pretimeout_governor");
		if (xrc)
			warn(_("cannot set pre-timeout governor"));
		rc += xrc;
	}

	return rc;
}

/*
 * Warning: successfully opened watchdog has to be properly closed with magic
 * close character otherwise the machine will be rebooted!
 *
 * Don't use err() or exit() here!
 */
static int read_watchdog_from_device(struct wd_device *wd)
{
	int fd;
	sigset_t sigs, oldsigs;

	assert(wd->devpath);

	sigemptyset(&oldsigs);
	sigfillset(&sigs);
	sigprocmask(SIG_BLOCK, &sigs, &oldsigs);

	fd = open(wd->devpath, O_WRONLY|O_CLOEXEC);

	if (fd < 0)
		return -errno;

	if (ioctl(fd, WDIOC_GETSUPPORT, &wd->ident) < 0)
		warn(_("%s: failed to get information about watchdog"), wd->devpath);
	else {
		ioctl(fd, WDIOC_GETSTATUS, &wd->status);
		ioctl(fd, WDIOC_GETBOOTSTATUS, &wd->bstatus);

		/*
		 * Sometimes supported options like WDIOF_CARDRESET are missing from
		 * ident.options, add anything set in status/bstatus to ident.options.
		 */
		wd->ident.options |= wd->status;
		wd->ident.options |= wd->bstatus;

		if (ioctl(fd, WDIOC_GETTIMEOUT, &wd->timeout) >= 0)
			wd->has_timeout = 1;
		if (ioctl(fd, WDIOC_GETPRETIMEOUT, &wd->pretimeout) >= 0)
			wd->has_pretimeout = 1;
		if (ioctl(fd, WDIOC_GETTIMELEFT, &wd->timeleft) >= 0)
			wd->has_timeleft = 1;
	}

	for (;;) {
		/* We just opened this to query the state, not to arm
		 * it hence use the magic close character */
		static const char v = 'V';

		if (write(fd, &v, 1) >= 0)
			break;
		if (errno != EINTR) {
			warn(_("%s: failed to disarm watchdog"), wd->devpath);
			break;
		}
		/* Let's try hard, since if we don't get this right
		 * the machine might end up rebooting. */
	}

	if (close(fd))
		warn(_("write failed"));
	sigprocmask(SIG_SETMASK, &oldsigs, NULL);

	return 0;
}


/* Returns: <0 error, 0 success, 1 unssuported */
static int read_watchdog_from_sysfs(struct wd_device *wd)
{
	struct path_cxt *sys;

	sys = get_sysfs(wd);
	if (!sys)
		return 1;

	if (ul_path_read_buffer(sys, (char *) wd->ident.identity, sizeof(wd->ident.identity), "identity") >= 0)
		wd->has_identity = 1;
	if (ul_path_read_u32(sys, &wd->ident.firmware_version, "fw_version") == 0)
		wd->has_fw_version = 1;
	if (ul_path_scanf(sys, "options", "%x", &wd->ident.options) == 1)
		wd->has_options = 1;
	if (ul_path_scanf(sys, "status", "%x", &wd->status) == 1)
		wd->has_status = 1;
	if (ul_path_read_u32(sys, &wd->bstatus, "bootstatus") == 0)
		wd->has_bootstatus = 1;
	if (ul_path_read_s32(sys, &wd->nowayout, "nowayout") == 0)
		wd->has_nowayout = 1;
	if (ul_path_read_s32(sys, &wd->timeout, "timeout") == 0)
		wd->has_timeout = 1;
	if (ul_path_read_s32(sys, &wd->pretimeout, "pretimeout") == 0)
		wd->has_pretimeout = 1;
	if (ul_path_read_s32(sys, &wd->timeleft, "timeleft") == 0)
		wd->has_timeleft = 1;

	return 0;
}

static int read_governors(struct wd_device *wd)
{
	struct path_cxt *sys;
	FILE *f;

	sys = get_sysfs(wd);
	if (!sys)
		return 1;

	f = ul_path_fopen(sys, "r", "pretimeout_available_governors");
	if (f) {
		char *line = NULL;
		size_t dummy = 0;
		ssize_t sz;

		while ((sz = getline(&line, &dummy, f)) >= 0) {
			if (rtrim_whitespace((unsigned char *) line) == 0)
				continue;
			strv_consume(&wd->available_governors, line);
			dummy = 0;
			line = NULL;
		}
		free(line);
		fclose(f);
	}

	ul_path_read_string(sys, &wd->governor,  "pretimeout_governor");
	return 0;
}

static bool should_read_from_device(struct wd_device *wd)
{
	if (wd->no_sysfs)
		return true;

	if (!wd->has_nowayout)
		return false;

	if (wd->nowayout)
		return false;

	return !wd->has_identity ||
	       !wd->has_fw_version ||
	       !wd->has_options ||
	       !wd->has_status ||
	       !wd->has_bootstatus ||
	       !wd->has_timeout ||
	       !wd->has_timeleft;
	       // pretimeout attribute may be hidden in sysfs
}

static int read_watchdog(struct wd_device *wd)
{
	int rc;

	rc = read_watchdog_from_sysfs(wd);

	if (rc && should_read_from_device(wd))
		rc = read_watchdog_from_device(wd);

	if (rc) {
		warn(_("cannot read information about %s"), wd->devpath);
		return -1;
	}

	read_governors(wd);
	return 0;
}

static void show_timeouts(struct wd_device *wd)
{
	if (wd->has_timeout)
		printf(P_("%-14s %2i second\n", "%-14s %2i seconds\n", wd->timeout),
			  _("Timeout:"), wd->timeout);
	if (wd->has_timeleft)
		printf(P_("%-14s %2i second\n", "%-14s %2i seconds\n", wd->timeleft),
			  _("Timeleft:"), wd->timeleft);
	if (wd->has_pretimeout)
		printf(P_("%-14s %2i second\n", "%-14s %2i seconds\n", wd->pretimeout),
			  _("Pre-timeout:"), wd->pretimeout);
}

static void show_governors(struct wd_device *wd)
{
	if (wd->governor)
		printf(_("%-14s %s\n"), _("Pre-timeout governor:"), wd->governor);
	if (wd->available_governors) {
		char *tmp = strv_join(wd->available_governors, " ");

		if (tmp)
			printf(_("%-14s %s\n"),
				_("Available pre-timeout governors:"), tmp);
		free(tmp);
	}
}

static void print_oneline(struct wd_control *ctl, struct wd_device *wd, uint32_t wanted)
{
	printf("%s:", wd->devpath);

	if (!ctl->hide_ident) {
		printf(" VERSION=\"%x\"", wd->ident.firmware_version);

		printf(" IDENTITY=");
		fputs_quoted((char *) wd->ident.identity, stdout);
	}
	if (!ctl->hide_timeouts) {
		if (wd->has_timeout)
			printf(" TIMEOUT=\"%i\"", wd->timeout);
		if (wd->has_pretimeout)
			printf(" PRETIMEOUT=\"%i\"", wd->pretimeout);
		if (wd->has_timeleft)
			printf(" TIMELEFT=\"%i\"", wd->timeleft);
	}

	if (!ctl->hide_flags) {
		size_t i;
		uint32_t flags = wd->ident.options;

		for (i = 0; i < ARRAY_SIZE(wdflags); i++) {
			const struct wdflag *fl;

			if ((wanted && !(wanted & wdflags[i].flag)) ||
			    !(flags & wdflags[i].flag))
				continue;

			fl= &wdflags[i];

			printf(" %s=\"%s\"", fl->name,
					     wd->status & fl->flag ? "1" : "0");
			printf(" %s_BOOT=\"%s\"", fl->name,
					     wd->bstatus & fl->flag ? "1" : "0");

		}
	}

	fputc('\n', stdout);
}

static void print_device(struct wd_control *ctl, struct wd_device *wd, uint32_t wanted)
{
	/* NAME=value one line output */
	if (ctl->show_oneline) {
		print_oneline(ctl, wd, wanted);
		return;
	}

	/* pretty output */
	if (!ctl->hide_ident) {
		printf("%-15s%s\n", _("Device:"), wd->devpath);
		printf("%-15s%s [%s %x]\n",
				_("Identity:"),
				wd->ident.identity,
				_("version"),
				wd->ident.firmware_version);
	}
	if (!ctl->hide_timeouts)
		show_timeouts(wd);

	show_governors(wd);

	if (!ctl->hide_flags)
		show_flags(ctl, wd, wanted);
}

int main(int argc, char *argv[])
{
	struct wd_device wd;
	struct wd_control ctl = { .hide_headings = 0 };
	int c, res = EXIT_SUCCESS, count = 0;
	unsigned long wanted = 0;
	const char *dflt_device = NULL;

	static const struct option long_opts[] = {
		{ "flags",      required_argument, NULL, 'f' },
		{ "flags-only", no_argument,       NULL, 'x' },
		{ "help",	no_argument,       NULL, 'h' },
		{ "noflags",    no_argument,       NULL, 'F' },
		{ "noheadings", no_argument,       NULL, 'n' },
		{ "noident",	no_argument,       NULL, 'I' },
		{ "notimeouts", no_argument,       NULL, 'T' },
		{ "settimeout", required_argument, NULL, 's' },
		{ "setpretimeout", required_argument, NULL, 'p' },
		{ "setpregovernor", required_argument, NULL, 'g' },
		{ "output",     required_argument, NULL, 'o' },
		{ "oneline",    no_argument,       NULL, 'O' },
		{ "raw",        no_argument,       NULL, 'r' },
		{ "version",    no_argument,       NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'F','f' },			/* noflags,flags*/
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv,
				"d:f:g:hFnITp:o:s:OrVx", long_opts, NULL)) != -1) {

		err_exclusive_options(c, long_opts, excl, excl_st);

		switch(c) {
		case 'o':
			ncolumns = string_to_idarray(optarg,
						     columns, ARRAY_SIZE(columns),
						     column2id);
			if (ncolumns < 0)
				return EXIT_FAILURE;
			break;
		case 's':
			ctl.timeout = strtos32_or_err(optarg, _("invalid timeout argument"));
			ctl.set_timeout = 1;
			break;
		case 'p':
			ctl.pretimeout = strtos32_or_err(optarg, _("invalid pretimeout argument"));
			ctl.set_pretimeout = 1;
			break;
		case 'f':
			if (string_to_bitmask(optarg, &wanted, name2bit) != 0)
				return EXIT_FAILURE;
			break;
		case 'F':
			ctl.hide_flags = 1;
			break;
		case 'g':
			ctl.governor = optarg;
			break;
		case 'I':
			ctl.hide_ident = 1;
			break;
		case 'T':
			ctl.hide_timeouts = 1;
			break;
		case 'n':
			ctl.hide_headings = 1;
			break;
		case 'r':
			ctl.show_raw = 1;
			break;
		case 'O':
			ctl.show_oneline = 1;
			break;
		case 'x':
			ctl.hide_ident = 1;
			ctl.hide_timeouts = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (!ncolumns) {
		/* default columns */
		columns[ncolumns++] = COL_FLAG;
		columns[ncolumns++] = COL_DESC;
		columns[ncolumns++] = COL_STATUS;
		columns[ncolumns++] = COL_BSTATUS;
	}

	/* Device no specified, use default. */
	if (optind == argc) {
		dflt_device = get_default_device();
		if (!dflt_device)
			err(EXIT_FAILURE, _("No default device is available."));
	}

	do {
		int rc;

		memset(&wd, 0, sizeof(wd));
		wd.devpath = dflt_device ? dflt_device : argv[optind++];

		if (count)
			fputc('\n', stdout);
		count++;

		if (want_set(&ctl)) {
			rc = set_watchdog(&ctl, &wd);
			if (rc) {
				res = EXIT_FAILURE;
			}
		}

		rc = read_watchdog(&wd);
		if (rc) {
			res = EXIT_FAILURE;
			continue;
		}

		print_device(&ctl, &wd, wanted);
		ul_unref_path(wd.sysfs);
	} while (optind < argc);

	return res;
}
