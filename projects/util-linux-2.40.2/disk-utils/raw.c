/*
 * raw.c: User mode tool to bind and query raw character devices.
 *
 * Stephen Tweedie, 1999, 2000
 *
 * This file may be redistributed under the terms of the GNU General
 * Public License, version 2.
 *
 * Copyright Red Hat Software, 1999, 2000
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/major.h>
#include <linux/raw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "pathnames.h"

#define EXIT_RAW_ACCESS 3
#define EXIT_RAW_IOCTL 4

#define RAW_NR_MINORS 8192

static int do_query;
static int do_query_all;

static int master_fd;
static int raw_minor;

void open_raw_ctl(void);
static int query(int minor_raw, const char *raw_name, int quiet);
static int bind(int minor_raw, int block_major, int block_minor);

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
		_(" %1$s %2$srawN <major> <minor>\n"
		  " %1$s %2$srawN /dev/<blockdevice>\n"
		  " %1$s -q %2$srawN\n"
		  " %1$s -qa\n"), program_invocation_short_name,
		_PATH_RAWDEVDIR);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Bind a raw character device to a block device.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -q, --query    set query mode\n"), out);
	fputs(_(" -a, --all      query all raw devices\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(16));
	fprintf(out, USAGE_MAN_TAIL("raw(8)"));
	exit(EXIT_SUCCESS);
}

static long strtol_octal_or_err(const char *str, const char *errmesg)
{
	long num;
	char *end = NULL;

	if (str == NULL || *str == '\0')
		goto err;
	errno = 0;
	num = strtol(str, &end, 0);

	if (errno || str == end || (end && *end))
		goto err;

	return num;
 err:
	if (errno)
		err(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	else
		errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	return 0;
}

int main(int argc, char *argv[])
{
	int c;
	char *raw_name;
	char *block_name;
	int retval;
	int block_major, block_minor;
	int i, rc;

	struct stat statbuf;

	static const struct option longopts[] = {
		{"query",   no_argument, NULL, 'q'},
		{"all",     no_argument, NULL, 'a'},
		{"version", no_argument, NULL, 'V'},
		{"help",    no_argument, NULL, 'h'},
		{NULL, 0, NULL, '0'},
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "qaVh", longopts, NULL)) != -1)
		switch (c) {
		case 'q':
			do_query = 1;
			break;
		case 'a':
			do_query_all = 1;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	/*
	 * Check for, and open, the master raw device, /dev/raw
	 */
	open_raw_ctl();

	if (do_query_all) {
		if (optind < argc) {
			warnx(_("bad usage"));
			errtryhelp(EXIT_FAILURE);
		}
		for (i = 1; i < RAW_NR_MINORS; i++)
			query(i, NULL, 1);
		exit(EXIT_SUCCESS);
	}

	/*
	 * It's a bind or a single query.  Either way we need a raw device.
	 */

	if (optind >= argc) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}
	raw_name = argv[optind++];

	/*
	 * try to check the device name before stat(), because on systems with
	 * udev the raw0 causes a create udev event for char 162/0, which
	 * causes udev to *remove* /dev/rawctl
	 */
	rc = sscanf(raw_name, _PATH_RAWDEVDIR "raw%d", &raw_minor);
	if (rc != 1) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}
	if (raw_minor == 0)
		errx(EXIT_RAW_ACCESS,
		     _("Device '%s' is the control raw device "
		       "(use raw<N> where <N> is greater than zero)"),
		     raw_name);

	if (do_query)
		return query(raw_minor, raw_name, 0);

	/*
	 * It's not a query, so we still have some parsing to do.  Have we been
	 * given a block device filename or a major/minor pair?
	 */
	switch (argc - optind) {
	case 1:
		block_name = argv[optind];
		retval = stat(block_name, &statbuf);
		if (retval)
			err(EXIT_RAW_ACCESS,
			    _("Cannot locate block device '%s'"), block_name);
		if (!S_ISBLK(statbuf.st_mode))
			errx(EXIT_RAW_ACCESS,
			     _("Device '%s' is not a block device"),
			     block_name);
		block_major = major(statbuf.st_rdev);
		block_minor = minor(statbuf.st_rdev);
		break;

	case 2:
		block_major =
		    strtol_octal_or_err(argv[optind],
					_("failed to parse argument"));
		block_minor =
		    strtol_octal_or_err(argv[optind + 1],
					_("failed to parse argument"));
		break;

	default:
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	return bind(raw_minor, block_major, block_minor);
}

void open_raw_ctl(void)
{
	master_fd = open(_PATH_RAWDEVCTL, O_RDWR, 0);
	if (master_fd < 0) {
		master_fd = open(_PATH_RAWDEVCTL_OLD, O_RDWR, 0);
		if (master_fd < 0)
			err(EXIT_RAW_ACCESS,
			    _("Cannot open master raw device '%s'"),
			    _PATH_RAWDEVCTL);
	}
}

static int query(int minor_raw, const char *raw_name, int quiet)
{
	struct raw_config_request rq;
	static int has_worked = 0;

	if (raw_name) {
		struct stat statbuf;

		if (stat(raw_name, &statbuf) != 0)
			err(EXIT_RAW_ACCESS,
			    _("Cannot locate raw device '%s'"), raw_name);
		if (!S_ISCHR(statbuf.st_mode))
			errx(EXIT_RAW_ACCESS,
			     _("Raw device '%s' is not a character dev"),
			     raw_name);
		if (major(statbuf.st_rdev) != RAW_MAJOR)
			errx(EXIT_RAW_ACCESS,
			     _("Device '%s' is not a raw dev"), raw_name);
		minor_raw = minor(statbuf.st_rdev);
	}

	rq.raw_minor = minor_raw;
	if (ioctl(master_fd, RAW_GETBIND, &rq) < 0) {
		if (quiet && errno == ENODEV)
			return 3;
		if (has_worked && errno == EINVAL)
			return 0;
		err(EXIT_RAW_IOCTL, _("Error querying raw device"));
	}

	/* If one query has worked, mark that fact so that we don't report
	 * spurious fatal errors if raw(8) has been built to support more raw
	 * minor numbers than the kernel has. */
	has_worked = 1;
	if (quiet && !rq.block_major && !rq.block_minor)
		return 0;
	printf(_("%sraw%d:  bound to major %d, minor %d\n"),
	       _PATH_RAWDEVDIR, minor_raw, (int)rq.block_major,
	       (int)rq.block_minor);
	return 0;
}

static int bind(int minor_raw, int block_major, int block_minor)
{
	struct raw_config_request rq;

	rq.raw_minor = minor_raw;
	rq.block_major = block_major;
	rq.block_minor = block_minor;
	if (ioctl(master_fd, RAW_SETBIND, &rq) < 0)
		err(EXIT_RAW_IOCTL, _("Error setting raw device"));
	printf(_("%sraw%d:  bound to major %d, minor %d\n"),
	       _PATH_RAWDEVDIR, raw_minor, (int)rq.block_major,
	       (int)rq.block_minor);
	return 0;
}
