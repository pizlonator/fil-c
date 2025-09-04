/* vi: set sw=4 ts=4: */
/*
 * Mini touch implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Previous version called open() and then utime().  While this will be
 * be necessary to implement -r and -t, it currently only makes things bigger.
 * Also, exiting on a failure was a bug.  All args should be processed.
 */
//config:config TOUCH
//config:	bool "touch (6.1 kb)"
//config:	default y
//config:	help
//config:	touch is used to create or change the access and/or
//config:	modification timestamp of specified files.
//config:
//config:config FEATURE_TOUCH_SUSV3
//config:	bool "Add support for SUSV3 features (-a -d -m -t -r)"
//config:	default y
//config:	depends on TOUCH
//config:	help
//config:	Enable touch to use a reference file or a given date/time argument.

//applet:IF_TOUCH(APPLET_NOFORK(touch, touch, BB_DIR_BIN, BB_SUID_DROP, touch))

//kbuild:lib-$(CONFIG_TOUCH) += touch.o

//usage:#define touch_trivial_usage
//usage:       "[-ch" IF_FEATURE_TOUCH_SUSV3("am") "]"
//usage:       IF_FEATURE_TOUCH_SUSV3(" [-d DATE] [-t DATE] [-r FILE]")
//usage:       " FILE..."
//usage:#define touch_full_usage "\n\n"
//usage:       "Update mtime of FILEs\n"
//usage:     "\n	-c	Don't create files"
//usage:     "\n	-h	Don't follow links"
//usage:	IF_FEATURE_TOUCH_SUSV3(
//usage:     "\n	-a	Change only atime"
//usage:     "\n	-m	Change only mtime"
//usage:     "\n	-d DT	Date/time to use"
//usage:     "\n	-t DT	Date/time to use"
//usage:     "\n	-r FILE	Use FILE's date/time"
//usage:	)
//usage:
//usage:#define touch_example_usage
//usage:       "$ ls -l /tmp/foo\n"
//usage:       "/bin/ls: /tmp/foo: No such file or directory\n"
//usage:       "$ touch /tmp/foo\n"
//usage:       "$ ls -l /tmp/foo\n"
//usage:       "-rw-rw-r--    1 andersen andersen        0 Apr 15 01:11 /tmp/foo\n"

/* coreutils implements:
 * -a   change only the access time
 * -c, --no-create
 *      do not create any files
 * -d, --date=STRING
 *      parse STRING and use it instead of current time
 * -f   (ignored, BSD compat)
 * -m   change only the modification time
 * -h, --no-dereference
 * -r, --reference=FILE
 *      use this file's times instead of current time
 * -t STAMP
 *      use [[CC]YY]MMDDhhmm[.ss] instead of current time
 * --time=WORD
 *      change the specified time: WORD is access, atime, or use
 */

#include "libbb.h"

int touch_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int touch_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;
	int opts;
	exitcode_t status = EXIT_SUCCESS;
#if ENABLE_FEATURE_TOUCH_SUSV3
	char *reference_file;
	char *date_str;
	/* timebuf[0] is atime, timebuf[1] is mtime */
	struct timespec timebuf[2];
#else
# define reference_file NULL
# define date_str       NULL
# define timebuf        ((struct timespec*)NULL)
#endif

	enum {
		OPT_c = (1 << 0),
		OPT_h = (1 << 1),
		OPT_r = (1 << 2) * ENABLE_FEATURE_TOUCH_SUSV3,
		OPT_d = (1 << 3) * ENABLE_FEATURE_TOUCH_SUSV3,
		OPT_t = (1 << 4) * ENABLE_FEATURE_TOUCH_SUSV3,
		OPT_a = (1 << 5) * ENABLE_FEATURE_TOUCH_SUSV3,
		OPT_m = (1 << 6) * ENABLE_FEATURE_TOUCH_SUSV3,
	};
#if ENABLE_LONG_OPTS
	static const char touch_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"no-create\0"        No_argument       "c"
		"no-dereference\0"   No_argument       "h"
		IF_FEATURE_TOUCH_SUSV3("reference\0"        Required_argument "r")
		IF_FEATURE_TOUCH_SUSV3("date\0"             Required_argument "d")
	;
#endif
	/* -d and -t both set time. In coreutils,
	 * accepted data format differs a bit between -d and -t.
	 * We accept the same formats for both
	 */
	opts = getopt32long(argv, "^"
		"ch"
		IF_FEATURE_TOUCH_SUSV3("r:d:t:am")
		/*ignored:*/ "f" IF_NOT_FEATURE_TOUCH_SUSV3("am")
		"\0" /* opt_complementary: */
		/* at least one arg: */ "-1"
		/* coreutils forbids -r and -t at once: */ IF_FEATURE_TOUCH_SUSV3(":r--t:t--r")
		/* but allows these combinations: "r--d:d--r:t--d:d--t" */
		, touch_longopts
#if ENABLE_FEATURE_TOUCH_SUSV3
		, &reference_file
		, &date_str
		, &date_str
#endif
	);

#if ENABLE_FEATURE_TOUCH_SUSV3
	timebuf[0].tv_nsec = timebuf[1].tv_nsec = UTIME_NOW;
	if (opts & OPT_r) {
		struct stat stbuf;
		xstat(reference_file, &stbuf);
		timebuf[0].tv_sec = stbuf.st_atime;
		timebuf[1].tv_sec = stbuf.st_mtime;
		timebuf[0].tv_nsec = stbuf.st_atim.tv_nsec;
		timebuf[1].tv_nsec = stbuf.st_mtim.tv_nsec;
	}
	if (opts & (OPT_d|OPT_t)) {
		struct tm tm_time;
		time_t t;
		int check_dst;

		//memset(&tm_time, 0, sizeof(tm_time));
		/* Better than memset: makes "HH:MM" dates meaningful */
		time(&t);
		localtime_r(&t, &tm_time);
		check_dst = parse_datestr(date_str, &tm_time);

		/* Correct any day of week and day of year etc. fields */
		if (check_dst)
			tm_time.tm_isdst = -1;  /* recheck dst unless date is UTC */
		t = validate_tm_time(date_str, &tm_time);

		timebuf[1].tv_sec = timebuf[0].tv_sec = t;
		timebuf[1].tv_nsec = timebuf[0].tv_nsec = 0;
	}
	/* If both -a and -m specified, both times should be set.
	 * IOW: set OMIT only if one, not both, of them is given!
	 */
	if ((opts & (OPT_a|OPT_m)) == OPT_a)
		timebuf[1].tv_nsec = UTIME_OMIT;
	if ((opts & (OPT_a|OPT_m)) == OPT_m)
		timebuf[0].tv_nsec = UTIME_OMIT;
#endif

	argv += optind;
	do {
		int result = utimensat(AT_FDCWD, *argv, timebuf,
				(opts & OPT_h) ? AT_SYMLINK_NOFOLLOW : 0);
		if (result != 0) {
			if (errno == ENOENT) { /* no such file? */
				if (opts & OPT_c) {
					/* Creation is disabled, so ignore */
					continue;
				}
				/* Try to create the file */
				fd = open(*argv, O_RDWR | O_CREAT, 0666);
				if (fd >= 0) {
					if (opts & (OPT_r|OPT_d|OPT_t))
						futimens(fd, timebuf);
					xclose(fd);
					continue;
				}
			}
			status = EXIT_FAILURE;
			bb_simple_perror_msg(*argv);
		}
	} while (*++argv);

	return status;
}
