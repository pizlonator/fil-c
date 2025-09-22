/*
 * pipesz(1) - Set or examine pipe buffer sizes.
 *
 * Copyright (c) 2022 Nathan Sharp
 * Written by Nathan Sharp <nwsharp@live.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
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

#include <getopt.h>
#include <sys/ioctl.h>		/* FIONREAD */
#include <fcntl.h>		/* F_GETPIPE_SZ F_SETPIPE_SZ */

#include "c.h"
#include "nls.h"

#include "closestream.h"	/* close_stdout_atexit */
#include "optutils.h"		/* err_exclusive_options */
#include "path.h"		/* ul_path_read_s32 */
#include "pathnames.h"		/* _PATH_PROC_PIPE_MAX_SIZE */
#include "strutils.h"		/* strtos32_or_err strtosize_or_err */

static char opt_check = 0;	/* --check */
static char opt_get = 0;	/* --get */
static char opt_quiet = 0;	/* --quiet */
static int opt_size = -1;	/* --set <size> */
static char opt_verbose = 0;	/* --verbose */

/* fallback file for default size */
#ifndef PIPESZ_DEFAULT_SIZE_FILE
#define PIPESZ_DEFAULT_SIZE_FILE _PATH_PROC_PIPE_MAX_SIZE
#endif

/* convenience macros, since pipesz is by default very lenient */
#define check(FMT...) do {			\
	if (opt_check) {			\
		err(EXIT_FAILURE, FMT);		\
	} else if (!opt_quiet)	{		\
		warn(FMT);			\
	}					\
} while (0)

#define checkx(FMT...) do {			\
	if (opt_check) {			\
		errx(EXIT_FAILURE, FMT);	\
	} else if (!opt_quiet) {		\
		warnx(FMT);			\
	}					\
} while (0)

static void __attribute__((__noreturn__)) usage(void)
{
	fputs(USAGE_HEADER, stdout);
	fprintf(stdout, _(" %s [options] [--set <size>] [--] [command]\n"), program_invocation_short_name);
	fprintf(stdout, _(" %s [options] --get\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	/* TRANSLATORS: 'command' refers to a program argument */
	fputsln(_("Set or examine pipe buffer sizes and optionally execute command."), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputsln(_(" -g, --get          examine pipe buffers"), stdout);
	/* TRANSLATORS: '%s' refers to a system file */
	fprintf(stdout,
	     _(" -s, --set <size>   set pipe buffer sizes\n"
	       "                      size defaults to %s\n"),
		PIPESZ_DEFAULT_SIZE_FILE);

	fputs(USAGE_SEPARATOR, stdout);
	fputsln(_(" -f, --file <path>  act on a file"), stdout);
	fputsln(_(" -n, --fd <num>     act on a file descriptor"), stdout);
	fputsln(_(" -i, --stdin        act on standard input"), stdout);
	fputsln(_(" -o, --stdout       act on standard output"), stdout);
	fputsln(_(" -e, --stderr       act on standard error"), stdout);

	fputs(USAGE_SEPARATOR, stdout);
	fputsln(_(" -c, --check        do not continue after an error"), stdout);
	fputsln(_(" -q, --quiet        do not warn of non-fatal errors"), stdout);
	fputsln(_(" -v, --verbose      provide detailed output"), stdout);

	fputs(USAGE_SEPARATOR, stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(20));

	fprintf(stdout, USAGE_MAN_TAIL("pipesz(1)"));

	exit(EXIT_SUCCESS);
}

/*
 * performs F_GETPIPE_SZ and FIONREAD
 * outputs a table row
 */
static void do_get(int fd, const char *name)
{
	int sz, used;

	sz = fcntl(fd, F_GETPIPE_SZ);
	if (sz < 0) {
		/* TRANSLATORS: '%s' refers to a file */
		check(_("cannot get pipe buffer size of %s"), name);
		return;
	}

	if (ioctl(fd, FIONREAD, &used))
		used = 0;

	printf("%s\t%d\t%d\n", name, sz, used);
}

/*
 * performs F_SETPIPE_SZ
 */
static void do_set(int fd, const char *name)
{
	int sz;

	sz = fcntl(fd, F_SETPIPE_SZ, opt_size);
	if (sz < 0)
		/* TRANSLATORS: '%s' refers to a file */
		check(_("cannot set pipe buffer size of %s"), name);
	else if (opt_verbose)
		/* TRANSLATORS: '%s' refers to a file, '%d' to a buffer size in bytes */
		warnx(_("%s pipe buffer size set to %d"), name, sz);
}

/*
 * does the requested operation on an fd
 */
static void do_fd(int fd)
{
	char name[sizeof(stringify(INT_MIN)) + 3];

	sprintf(name, "fd %d", fd);

	if (opt_get)
		do_get(fd, name);
	else
		do_set(fd, name);
}

/*
 * does the requested operation on a file
 */
static void do_file(const char *path)
{
	int fd;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		/* TRANSLATORS: '%s' refers to a file */
		check(_("cannot open %s"), path);
		return;
	}

	if (opt_get)
		do_get(fd, path);
	else
		do_set(fd, path);

	close(fd);
}

/*
 * if necessary, determines a default buffer size and places it in opt_size
 * returns FALSE if this could not be done
 */
static char set_size_default(void)
{
	if (opt_size >= 0)
		return TRUE;

	if (ul_path_read_s32(NULL, &opt_size, PIPESZ_DEFAULT_SIZE_FILE)) {
		/* TRANSLATORS: '%s' refers to a system file */
		check(_("cannot parse %s"), PIPESZ_DEFAULT_SIZE_FILE);
		return FALSE;
	}

	if (opt_size < 0) {
		/* TRANSLATORS: '%s' refers to a system file */
		checkx(_("cannot parse %s"), PIPESZ_DEFAULT_SIZE_FILE);
		return FALSE;
	}

	return TRUE;
}

int main(int argc, char **argv)
{
	static const char shortopts[] = "+cef:ghin:oqs:vV";
	static const struct option longopts[] = {
		{ "check",     no_argument,       NULL, 'c' },
		{ "fd",        required_argument, NULL, 'n' },
		{ "file",      required_argument, NULL, 'f' },
		{ "get",       no_argument,       NULL, 'g' },
		{ "help",      no_argument,       NULL, 'h' },
		{ "quiet",     no_argument,       NULL, 'q' },
		{ "set",       required_argument, NULL, 's' },
		{ "stdin",     no_argument,       NULL, 'i' },
		{ "stdout",    no_argument,       NULL, 'o' },
		{ "stderr",    no_argument,       NULL, 'e' },
		{ "verbose",   no_argument,       NULL, 'v' },
		{ "version",   no_argument,       NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	static const ul_excl_t excl[] = {
		{ 'g', 's' },
		{ 0 }
	};

	int c, fd, n_opt_pipe = 0, n_opt_size = 0;
	uintmax_t sz;

	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	/* check for --help or --version */
	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		}
	}

	/* gather normal options */
	optind = 1;
	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'c':
			opt_check = TRUE;
			break;
		case 'e':
			++n_opt_pipe;
			break;
		case 'f':
			++n_opt_pipe;
			break;
		case 'g':
			opt_get = TRUE;
			break;
		case 'i':
			++n_opt_pipe;
			break;
		case 'n':
			(void) strtos32_or_err(optarg, _("invalid fd argument"));
			++n_opt_pipe;
			break;
		case 'o':
			++n_opt_pipe;
			break;
		case 'q':
			opt_quiet = TRUE;
			break;
		case 's':
			sz = strtosize_or_err(optarg, _("invalid size argument"));
			opt_size = sz >= INT_MAX ? INT_MAX : (int)sz;
			++n_opt_size;
			break;
		case 'v':
			opt_verbose = TRUE;
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	/* check arguments */
	if (opt_get) {
		if (argv[optind])
			errx(EXIT_FAILURE, _("cannot specify a command with --get"));

		/* print column headers, if requested */
		if (opt_verbose)
			printf("%s\t%s\t%s\n",
/* TRANSLATORS: a column that contains the names of files that are unix pipes */
				_("pipe"),
/* TRANSLATORS: a column that contains buffer sizes in bytes */
				_("size"),
/* TRANSLATORS: a column that contains an amount of data which has not been used by a program */
				_("unread")
			);

		/* special behavior for --get */
		if (!n_opt_pipe) {
			do_fd(STDIN_FILENO);
			return EXIT_SUCCESS;
		}
	} else {
		if (!set_size_default())
			goto execute_command;

		if (!opt_quiet && n_opt_size > 1)
			warnx(_("using last specified size"));

		/* special behavior for --set */
		if (!n_opt_pipe) {
			do_fd(STDOUT_FILENO);
			goto execute_command;
		}
	}

	/* go through the arguments again and do the requested operations */
	optind = 1;
	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
		switch (c) {
		case 'e':
			do_fd(STDERR_FILENO);
			break;
		case 'f':
			do_file(optarg);
			break;
		case 'i':
			do_fd(STDIN_FILENO);
			break;
		case 'n':
			/* optarg was checked before, but it's best to be safe */
			fd = strtos32_or_err(optarg, _("invalid fd argument"));
			do_fd(fd);
			break;
		case 'o':
			do_fd(STDOUT_FILENO);
			break;
		}

execute_command:
	/* exec the command, if it's present */
	if (!argv[optind])
		return EXIT_SUCCESS;

	execvp(argv[optind], &argv[optind]);
	errexec(argv[optind]);
}
