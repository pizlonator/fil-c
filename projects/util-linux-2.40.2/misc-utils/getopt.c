/*
 *  getopt.c - Enhanced implementation of BSD getopt(1)
 *  Copyright (c) 1997-2014 Frodo Looijaard <frodo@frodo.looijaard.name>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Version 1.0-b4: Tue Sep 23 1997. First public release.
 * Version 1.0: Wed Nov 19 1997.
 *   Bumped up the version number to 1.0
 *   Fixed minor typo (CSH instead of TCSH)
 * Version 1.0.1: Tue Jun 3 1998
 *   Fixed sizeof instead of strlen bug
 *   Bumped up the version number to 1.0.1
 * Version 1.0.2: Thu Jun 11 1998 (not present)
 *   Fixed gcc-2.8.1 warnings
 *   Fixed --version/-V option (not present)
 * Version 1.0.5: Tue Jun 22 1999
 *   Make -u option work (not present)
 * Version 1.0.6: Tue Jun 27 2000
 *   No important changes
 * Version 1.1.0: Tue Jun 30 2000
 *   Added NLS support (partly written by Arkadiusz Miśkiewicz
 *     <misiek@pld.org.pl>)
 * Version 1.1.4: Mon Nov 7 2005
 *   Fixed a few type's in the manpage
 * Version 1.1.5: Sun Aug 12 2012
 *  Sync with util-linux-2.21, fixed build problems, many new translations
 * Version 1.1.6: Mon Nov 24 2014
 *  Sync with util-linux git 20141120, detect ambiguous long options, fix
 *  backslash problem in tcsh
 */

/* Exit codes:
 *   0) No errors, successful operation.
 *   1) getopt(3) returned an error.
 *   2) A problem with parameter parsing for getopt(1).
 *   3) Internal error, out of memory
 *   4) Returned for -T
 */
#define GETOPT_EXIT_CODE	1
#define PARAMETER_EXIT_CODE	2
#define XALLOC_EXIT_CODE	3
#define CLOSE_EXIT_CODE		XALLOC_EXIT_CODE
#define TEST_EXIT_CODE		4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h> /* BSD */
#endif

#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"

/* NON_OPT is the code that is returned getopt(3) when a non-option is
 * found in 'char optstring[]="-abc...";', e.g., it begins by '-' */
#define NON_OPT 1
/* LONG_OPT is the code that is returned when a long option is found. */
#define LONG_OPT 0

/* The shells recognized. */
typedef enum { BASH, TCSH } shell_t;

struct getopt_control {
	shell_t shell;			/* the shell we generate output for */
	char *optstr;			/* getopt(3) optstring */
	char *name;
	struct option *long_options;	/* long options */
	int long_options_length;	/* length of options array */
	int long_options_nr;		/* number of used elements in array */
	unsigned int
		compatible:1,		/* compatibility mode for 'difficult' programs */
		quiet_errors:1,		/* print errors */
		quiet_output:1,		/* print output */
		quote:1;		/* quote output */
};

enum { REALLOC_INCREMENT = 8 };

/* Allow changing which getopt is in use with function pointer. */
static int (*getopt_long_fp) (int argc, char *const *argv, const char *optstr,
			      const struct option * longopts, int *longindex);

/*
 * This function 'normalizes' a single argument: it puts single quotes
 * around it and escapes other special characters. If quote is false, it
 * just returns its argument.
 *
 * Bash only needs special treatment for single quotes; tcsh also recognizes
 * exclamation marks within single quotes, and nukes whitespace. This
 * function returns a pointer to a buffer that is overwritten by each call.
 */
static void print_normalized(const struct getopt_control *ctl, const char *arg)
{
	char *buf;
	const char *argptr = arg;
	char *bufptr;

	if (!ctl->quote) {
		printf(" %s", arg);
		return;
	}

	/*
	 * Each character in arg may take up to four characters in the
	 * result: For a quote we need a closing quote, a backslash, a quote
	 * and an opening quote! We need also the global opening and closing
	 * quote, and one extra character for '\0'.
	 */
	buf = xmalloc(strlen(arg) * 4 + 3);
	bufptr = buf;

	for (*bufptr++ = '\''; *argptr; argptr++) {
		if (ctl->shell == TCSH) {
			switch (*argptr) {
			case '\\':
				/* Backslash: replace it with: '\\' */
				*bufptr++ = '\\';
				*bufptr++ = '\\';
				continue;
			case '!':
				/* Exclamation mark: replace it with: \! */
				*bufptr++ = '\'';
				*bufptr++ = '\\';
				*bufptr++ = '!';
				*bufptr++ = '\'';
				continue;
			case '\n':
				/* Newline: replace it with: \n */
				*bufptr++ = '\\';
				*bufptr++ = 'n';
				continue;
			}
			if (isspace(*argptr)) {
				/* Non-newline whitespace: replace it with \<ws> */
				*bufptr++ = '\'';
				*bufptr++ = '\\';
				*bufptr++ = *argptr;
				*bufptr++ = '\'';
				continue;
			}
		}
		if (*argptr == '\'') {
			/* Quote: replace it with: '\'' */
			*bufptr++ = '\'';
			*bufptr++ = '\\';
			*bufptr++ = '\'';
			*bufptr++ = '\'';
		} else
			/* Just copy */
			*bufptr++ = *argptr;
	}

	*bufptr++ = '\'';
	*bufptr++ = '\0';
	printf(" %s", buf);
	free(buf);
}

/*
 * Generate the output. argv[0] is the program name (used for reporting errors).
 * argv[1..] contains the options to be parsed. argc must be the number of
 * elements in argv (ie. 1 if there are no options, only the program name),
 * optstr must contain the short options, and longopts the long options.
 * Other settings are found in global variables.
 */
static int generate_output(struct getopt_control *ctl, char *argv[], int argc)
{
	int exit_code = EXIT_SUCCESS;	/* Assume everything will be OK */
	int opt;
	int longindex;
	const char *charptr;

	if (ctl->quiet_errors)
		/* No error reporting from getopt(3) */
		opterr = 0;
	/* Reset getopt(3) */
	optind = 0;

	while ((opt =
		(getopt_long_fp
		 (argc, argv, ctl->optstr,
		  (const struct option *)ctl->long_options, &longindex)))
	       != EOF) {
		if (opt == '?' || opt == ':')
			exit_code = GETOPT_EXIT_CODE;
		else if (!ctl->quiet_output) {
			switch (opt) {
			case LONG_OPT:
				printf(" --%s", ctl->long_options[longindex].name);
				if (ctl->long_options[longindex].has_arg)
					print_normalized(ctl, optarg ? optarg : "");
				break;
			case NON_OPT:
				print_normalized(ctl, optarg ? optarg : "");
				break;
			default:
				printf(" -%c", opt);
				charptr = strchr(ctl->optstr, opt);
				if (charptr != NULL && *++charptr == ':')
					print_normalized(ctl, optarg ? optarg : "");
			}
		}
	}
	if (!ctl->quiet_output) {
		printf(" --");
		while (optind < argc)
			print_normalized(ctl, argv[optind++]);
		printf("\n");
	}
	for (longindex = 0; longindex < ctl->long_options_nr; longindex++)
		free((char *)ctl->long_options[longindex].name);
	free(ctl->long_options);
	free(ctl->optstr);
	free(ctl->name);
	return exit_code;
}

/*
 * Report an error when parsing getopt's own arguments. If message is NULL,
 * we already sent a message, we just exit with a helpful hint.
 */
static void __attribute__ ((__noreturn__)) parse_error(const char *message)
{
	if (message)
		warnx("%s", message);
	errtryhelp(PARAMETER_EXIT_CODE);
}


/* Register a long option. The contents of name is copied. */
static void add_longopt(struct getopt_control *ctl, const char *name, int has_arg)
{
	static int flag;
	int nr = ctl->long_options_nr;

	if (ctl->long_options_nr == ctl->long_options_length) {
		ctl->long_options_length += REALLOC_INCREMENT;
		ctl->long_options = xreallocarray(ctl->long_options,
						  ctl->long_options_length,
						  sizeof(struct option));
	}
	if (name) {
		/* Not for init! */
		ctl->long_options[nr].has_arg = has_arg;
		ctl->long_options[nr].flag = &flag;
		ctl->long_options[nr].val = ctl->long_options_nr;
		ctl->long_options[nr].name = xstrdup(name);
	} else {
		/* lets use add_longopt(ct, NULL, 0) to terminate the array */
		ctl->long_options[nr].name = NULL;
		ctl->long_options[nr].has_arg = 0;
		ctl->long_options[nr].flag = NULL;
		ctl->long_options[nr].val = 0;
	}
}


static void add_short_options(struct getopt_control *ctl, char *options)
{
	free(ctl->optstr);
	if (*options != '+' && getenv("POSIXLY_CORRECT"))
		ctl->optstr = strconcat("+", options);
	else
		ctl->optstr = xstrdup(options);
	if (!ctl->optstr)
		err_oom();
}


/*
 * Register several long options. options is a string of long options,
 * separated by commas or whitespace. This nukes options!
 */
static void add_long_options(struct getopt_control *ctl, char *options)
{
	int arg_opt;
	char *tokptr = strtok(options, ", \t\n");

	while (tokptr) {
		size_t len = strlen(tokptr);

		arg_opt = no_argument;
		if (len > 0) {
			if (tokptr[len - 1] == ':') {
				if (tokptr[len - 2] == ':') {
					tokptr[len - 2] = '\0';
					arg_opt = optional_argument;
				} else {
					tokptr[len - 1] = '\0';
					arg_opt = required_argument;
				}
				if (!*tokptr)
					parse_error(_
						    ("empty long option after "
						     "-l or --long argument"));
			}
			add_longopt(ctl, tokptr, arg_opt);
			ctl->long_options_nr++;
		}
		tokptr = strtok(NULL, ", \t\n");
	}
	add_longopt(ctl, NULL, 0);	/* ensure long_options[] is not full */
}

static shell_t shell_type(const char *new_shell)
{
	if (!strcmp(new_shell, "bash"))
		return BASH;
	if (!strcmp(new_shell, "sh"))
		return BASH;
	if (!strcmp(new_shell, "tcsh"))
		return TCSH;
	if (!strcmp(new_shell, "csh"))
		return TCSH;
	parse_error(_("unknown shell after -s or --shell argument"));
}

static void __attribute__((__noreturn__)) usage(void)
{
	fputs(USAGE_HEADER, stdout);
	printf(_(
		" %1$s <optstring> <parameters>\n"
		" %1$s [options] [--] <optstring> <parameters>\n"
		" %1$s [options] -o|--options <optstring> [options] [--] <parameters>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Parse command options.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputs(_(" -a, --alternative             allow long options starting with single -\n"), stdout);
	fputs(_(" -l, --longoptions <longopts>  the long options to be recognized\n"), stdout);
	fputs(_(" -n, --name <progname>         the name under which errors are reported\n"), stdout);
	fputs(_(" -o, --options <optstring>     the short options to be recognized\n"), stdout);
	fputs(_(" -q, --quiet                   disable error reporting by getopt(3)\n"), stdout);
	fputs(_(" -Q, --quiet-output            no normal output\n"), stdout);
	fputs(_(" -s, --shell <shell>           set quoting conventions to those of <shell>\n"), stdout);
	fputs(_(" -T, --test                    test for getopt(1) version\n"), stdout);
	fputs(_(" -u, --unquoted                do not quote the output\n"), stdout);
	fputs(USAGE_SEPARATOR, stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(31));
	fprintf(stdout, USAGE_MAN_TAIL("getopt(1)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct getopt_control ctl = {
		.shell = BASH,
		.quote = 1
	};
	int opt;

	/* Stop scanning as soon as a non-option argument is found! */
	static const char *shortopts = "+ao:l:n:qQs:TuhV";
	static const struct option longopts[] = {
		{"options", required_argument, NULL, 'o'},
		{"longoptions", required_argument, NULL, 'l'},
		{"quiet", no_argument, NULL, 'q'},
		{"quiet-output", no_argument, NULL, 'Q'},
		{"shell", required_argument, NULL, 's'},
		{"test", no_argument, NULL, 'T'},
		{"unquoted", no_argument, NULL, 'u'},
		{"help", no_argument, NULL, 'h'},
		{"alternative", no_argument, NULL, 'a'},
		{"name", required_argument, NULL, 'n'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	if (getenv("GETOPT_COMPATIBLE"))
		ctl.compatible = 1;

	if (argc == 1) {
		if (ctl.compatible) {
			/*
			 * For some reason, the original getopt gave no
			 * error when there were no arguments.
			 */
			printf(" --\n");
			return EXIT_SUCCESS;
		}
		parse_error(_("missing optstring argument"));
	}

	add_longopt(&ctl, NULL, 0);	/* init */
	getopt_long_fp = getopt_long;

	if (argv[1][0] != '-' || ctl.compatible) {
		ctl.quote = 0;
		ctl.optstr = xmalloc(strlen(argv[1]) + 1);
		strcpy(ctl.optstr, argv[1] + strspn(argv[1], "-+"));
		argv[1] = argv[0];
		return generate_output(&ctl, argv + 1, argc - 1);
	}

	while ((opt =
		getopt_long(argc, argv, shortopts, longopts, NULL)) != EOF)
		switch (opt) {
		case 'a':
			getopt_long_fp = getopt_long_only;
			break;
		case 'o':
			add_short_options(&ctl, optarg);
			break;
		case 'l':
			add_long_options(&ctl, optarg);
			break;
		case 'n':
			free(ctl.name);
			ctl.name = xstrdup(optarg);
			break;
		case 'q':
			ctl.quiet_errors = 1;
			break;
		case 'Q':
			ctl.quiet_output = 1;
			break;
		case 's':
			ctl.shell = shell_type(optarg);
			break;
		case 'T':
			return TEST_EXIT_CODE;
		case 'u':
			ctl.quote = 0;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case '?':
		case ':':
			parse_error(NULL);
		case 'h':
			usage();
		default:
			parse_error(_("internal error, contact the author."));
		}

	if (!ctl.optstr) {
		if (optind >= argc)
			parse_error(_("missing optstring argument"));
		else {
			add_short_options(&ctl, argv[optind]);
			optind++;
		}
	}

	if (ctl.name) {
		argv[optind - 1] = ctl.name;
#if defined (HAVE_SETPROGNAME) && !defined (__linux__)
		setprogname(ctl.name);
#endif
	} else
		argv[optind - 1] = argv[0];

	return generate_output(&ctl, argv + optind - 1, argc - optind + 1);
}
