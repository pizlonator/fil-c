/*
 * free.c - display free memory information
 *
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2012-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2004      Albert Cahalan
 * Copyright © 2002-2003 Robert Love <rml@tech9.net>
 * Copyright © 1992      Brian Edmonds and Rafal Maszkowski
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <locale.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>

#include "config.h"
#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "fileutils.h"

#include "meminfo.h"

#ifndef SIZE_MAX
#define SIZE_MAX		32
#endif

#define FREE_HUMANREADABLE	(1 << 1)
#define FREE_LOHI		(1 << 2)
#define FREE_WIDE		(1 << 3)
#define FREE_TOTAL		(1 << 4)
#define FREE_SI			(1 << 5)
#define FREE_REPEAT		(1 << 6)
#define FREE_REPEATCOUNT	(1 << 7)
#define FREE_COMMITTED		(1 << 8)
#define FREE_LINE		(1 << 9)

struct commandline_arguments {
	int exponent;		/* demanded in kilos, magas... */
	float repeat_interval;	/* delay in seconds */
	int repeat_counter;	/* number of repeats */
};

/* function prototypes */
static void usage(FILE * out);
double power(unsigned int base, unsigned int expo);
static const char *scale_size(unsigned long size, int flags, struct commandline_arguments args);

static void __attribute__ ((__noreturn__))
    usage(FILE * out)
{
        fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -b, --bytes         show output in bytes\n"), out);
	fputs(_("     --kilo          show output in kilobytes\n"), out);
	fputs(_("     --mega          show output in megabytes\n"), out);
	fputs(_("     --giga          show output in gigabytes\n"), out);
	fputs(_("     --tera          show output in terabytes\n"), out);
	fputs(_("     --peta          show output in petabytes\n"), out);
	fputs(_(" -k, --kibi          show output in kibibytes\n"), out);
	fputs(_(" -m, --mebi          show output in mebibytes\n"), out);
	fputs(_(" -g, --gibi          show output in gibibytes\n"), out);
	fputs(_("     --tebi          show output in tebibytes\n"), out);
	fputs(_("     --pebi          show output in pebibytes\n"), out);
	fputs(_(" -h, --human         show human-readable output\n"), out);
	fputs(_("     --si            use powers of 1000 not 1024\n"), out);
	fputs(_(" -l, --lohi          show detailed low and high memory statistics\n"), out);
	fputs(_(" -L, --line          show output on a single line\n"), out);
	fputs(_(" -t, --total         show total for RAM + swap\n"), out);
	fputs(_(" -v, --committed     show committed memory and commit limit\n"), out);
	fputs(_(" -s N, --seconds N   repeat printing every N seconds\n"), out);
	fputs(_(" -c N, --count N     repeat printing N times, then exit\n"), out);
	fputs(_(" -w, --wide          wide output\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(_("     --help     display this help and exit\n"), out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("free(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

double power(unsigned int base, unsigned int expo)
{
	return (expo == 0) ? 1 : base * power(base, expo - 1);
}

/* idea of this function is copied from top size scaling */
static const char *scale_size(unsigned long size, int flags, struct commandline_arguments args)
{
	static char up[] = { 'B', 'K', 'M', 'G', 'T', 'P', 0 };
	static char buf[BUFSIZ];
	int i;
	float base;
	long long bytes;

	base = (flags & FREE_SI) ? 1000.0 : 1024.0;
	bytes = size * 1024LL;

	if (!(flags & FREE_HUMANREADABLE)) {
		switch (args.exponent) {
		case 0:
			/* default output */
			snprintf(buf, sizeof(buf), "%ld", (long int)(bytes / (long long int)base));
			return buf;
		case 1:
			/* in bytes, which can not be in SI */
			snprintf(buf, sizeof(buf), "%lld", bytes);
			return buf;
		default:
			/* In desired scale. */
			snprintf(buf, sizeof(buf), "%ld",
			        (long)(bytes / power(base, args.exponent-1)));
			return buf;
		}
	}

	/* human readable output */
	if (4 >= snprintf(buf, sizeof(buf), "%lld%c", bytes, up[0]))
		return buf;

	for (i = 1; up[i] != 0; i++) {
		if (flags & FREE_SI) {
			if (4 >= snprintf(buf, sizeof(buf), "%.1f%c",
			                  (float)(bytes / power(base, i)), up[i]))
				return buf;
			if (4 >= snprintf(buf, sizeof(buf), "%ld%c",
			                  (long)(bytes / power(base, i)), up[i]))
				return buf;
		} else {
			if (5 >= snprintf(buf, sizeof(buf), "%.1f%ci",
			                  (float)(bytes / power(base, i)), up[i]))
				return buf;
			if (5 >= snprintf(buf, sizeof(buf), "%ld%ci",
			                  (long)(bytes / power(base, i)), up[i]))
				return buf;
		}
	}
	/*
	 * On system where there is more than exbibyte of memory or swap the
	 * output does not fit to column. For incoming few years this should
	 * not be a big problem (wrote at Apr, 2015).
	 */
	return buf;
}

static void check_unit_set(int *unit_set)
{
    if (*unit_set)
	xerrx(EXIT_FAILURE,
		_("Multiple unit options don't make sense."));
    *unit_set = 1;
}

/*
 * Print the header columns.
 * We cannot simply use the second printf because the length of the
 * translated strings doesn't work with it. Instead we need to find
 * the wide length of the string and use that.
 * This method also removes the messy wprintf/printf buffering issues
 */
#define HC_WIDTH 9
static void print_head_col(const char *str)
{
    int len;
    int spaces = 9;
    wchar_t wstr[BUFSIZ];

    len = mbstowcs(wstr, str, BUFSIZ);
    if (len < 0)
        spaces = 9;
    else if (len < HC_WIDTH) {
        int width;
        if ( (width = wcswidth(wstr, 99)) > 0)
            spaces = HC_WIDTH - width;
        else
            spaces = HC_WIDTH - len;
    } else
        spaces = 0;

    printf("%s%.*s", str, spaces, "         ");
}

int main(int argc, char **argv)
{
	int c, flags = 0, unit_set = 0, rc = 0;
	struct commandline_arguments args;
	struct meminfo_info *mem_info = NULL;

	/*
	 * For long options that have no equivalent short option, use a
	 * non-character as a pseudo short option, starting with CHAR_MAX + 1.
	 */
	enum {
		SI_OPTION = CHAR_MAX + 1,
		KILO_OPTION,
		MEGA_OPTION,
		GIGA_OPTION,
		TERA_OPTION,
		PETA_OPTION,
		TEBI_OPTION,
		PEBI_OPTION,
		HELP_OPTION
	};

	static const struct option longopts[] = {
		{  "bytes",	no_argument,	    NULL,  'b'		},
		{  "kilo",	no_argument,	    NULL,  KILO_OPTION	},
		{  "mega",	no_argument,	    NULL,  MEGA_OPTION	},
		{  "giga",	no_argument,	    NULL,  GIGA_OPTION	},
		{  "tera",	no_argument,	    NULL,  TERA_OPTION	},
		{  "peta",	no_argument,	    NULL,  PETA_OPTION	},
		{  "kibi",	no_argument,	    NULL,  'k'		},
		{  "mebi",	no_argument,	    NULL,  'm'		},
		{  "gibi",	no_argument,	    NULL,  'g'		},
		{  "tebi",	no_argument,	    NULL,  TEBI_OPTION	},
		{  "pebi",	no_argument,	    NULL,  PEBI_OPTION	},
		{  "human",	no_argument,	    NULL,  'h'		},
		{  "si",	no_argument,	    NULL,  SI_OPTION	},
		{  "lohi",	no_argument,	    NULL,  'l'		},
		{  "line",	no_argument,	    NULL,  'L'		},
		{  "total",	no_argument,	    NULL,  't'		},
		{  "committed",	no_argument,	    NULL,  'v'		},
		{  "seconds",	required_argument,  NULL,  's'		},
		{  "count",	required_argument,  NULL,  'c'		},
		{  "wide",	no_argument,	    NULL,  'w'		},
		{  "help",	no_argument,	    NULL,  HELP_OPTION	},
		{  "version",	no_argument,	    NULL,  'V'		},
		{  NULL,	0,		    NULL,  0		}
	};

	/* defaults */
	args.exponent = 0;
	args.repeat_interval = 1000000;
	args.repeat_counter = 0;

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((c = getopt_long(argc, argv, "bkmghlLtvc:ws:V", longopts, NULL)) != -1)
		switch (c) {
		case 'b':
		        check_unit_set(&unit_set);
			args.exponent = 1;
			break;
		case 'k':
		        check_unit_set(&unit_set);
			args.exponent = 2;
			break;
		case 'm':
		        check_unit_set(&unit_set);
			args.exponent = 3;
			break;
		case 'g':
		        check_unit_set(&unit_set);
			args.exponent = 4;
			break;
		case TEBI_OPTION:
		        check_unit_set(&unit_set);
			args.exponent = 5;
			break;
		case PEBI_OPTION:
		        check_unit_set(&unit_set);
			args.exponent = 6;
			break;
		case KILO_OPTION:
		        check_unit_set(&unit_set);
			args.exponent = 2;
			flags |= FREE_SI;
			break;
		case MEGA_OPTION:
		        check_unit_set(&unit_set);
			args.exponent = 3;
			flags |= FREE_SI;
			break;
		case GIGA_OPTION:
		        check_unit_set(&unit_set);
			args.exponent = 4;
			flags |= FREE_SI;
			break;
		case TERA_OPTION:
		        check_unit_set(&unit_set);
			args.exponent = 5;
			flags |= FREE_SI;
			break;
		case PETA_OPTION:
		        check_unit_set(&unit_set);
			args.exponent = 6;
			flags |= FREE_SI;
			break;
		case 'h':
			flags |= FREE_HUMANREADABLE;
			break;
		case SI_OPTION:
			flags |= FREE_SI;
			break;
		case 'l':
			flags |= FREE_LOHI;
			break;
		case 'L':
			flags |= FREE_LINE;
			break;
		case 't':
			flags |= FREE_TOTAL;
			break;
		case 'v':
			flags |= FREE_COMMITTED;
			break;
		case 's':
			flags |= FREE_REPEAT;
			errno = 0;
            args.repeat_interval = (1000000 * strtod_nol_or_err(optarg, "seconds argument failed"));
			if (args.repeat_interval < 1)
				xerrx(EXIT_FAILURE,
				     _("seconds argument `%s' is not positive number"), optarg);
			break;
		case 'c':
			flags |= FREE_REPEAT;
			flags |= FREE_REPEATCOUNT;
			args.repeat_counter = strtol_or_err(optarg,
				_("failed to parse count argument"));
			if (args.repeat_counter < 1)
			  error(EXIT_FAILURE, ERANGE,
				  _("failed to parse count argument: '%s'"), optarg);
			break;
		case 'w':
			flags |= FREE_WIDE;
			break;
		case HELP_OPTION:
			usage(stdout);
		case 'V':
			printf(PROCPS_NG_VERSION);
			exit(EXIT_SUCCESS);
		default:
			usage(stderr);
		}
	if (optind != argc)
	    usage(stderr);

	if ( (rc = procps_meminfo_new(&mem_info)) < 0)
    {
        if (rc == -ENOENT)
            xerrx(EXIT_FAILURE,
                  _("Memory information file /proc/meminfo does not exist"));
        else
            xerrx(EXIT_FAILURE,
                  _("Unable to create meminfo structure"));
    }
	do {
	     if ( flags & FREE_LINE ) {
                 /* Translation Hint: These are shortened column headers
                  * that are all 7 characters long. Use spaces and right
                  * align if the translation is shorter.
                  */
		printf("%s %11s ", _("SwapUse"), scale_size(MEMINFO_GET(mem_info, MEMINFO_SWAP_USED, ul_int), flags, args));
		printf("%s %11s ", _("CachUse"), scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_BUFFERS, ul_int) +
                            MEMINFO_GET(mem_info, MEMINFO_MEM_CACHED_ALL, ul_int), flags, args));
		printf("%s %11s ", _(" MemUse"), scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_USED, ul_int), flags, args));
		printf("%s %11s ", _("MemFree"), scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_FREE, ul_int), flags, args));
		if ( (flags & FREE_REPEAT) == 0 )
			printf("\n");
		else if ( args.repeat_counter == 1 )
			printf("\n");
	     } else {
		/* Translation Hint: You can use 9 character words in
		 * the header, and the words need to be right align to
		 * beginning of a number. */
		if (flags & FREE_WIDE) {
			printf(_("               total        used        free      shared     buffers       cache   available"));
		} else {
			printf(_("               total        used        free      shared  buff/cache   available"));
		}
		printf("\n");
		print_head_col(_("Mem:"));
		printf("%11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_TOTAL, ul_int), flags, args));
		printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_USED, ul_int), flags, args));
		printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_FREE, ul_int), flags, args));
		printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_SHARED, ul_int), flags, args));
		if (flags & FREE_WIDE) {
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_BUFFERS, ul_int),
				    flags, args));
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_CACHED_ALL, ul_int)
				    , flags, args));
		} else {
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_BUFFERS, ul_int) +
				    MEMINFO_GET(mem_info, MEMINFO_MEM_CACHED_ALL, ul_int), flags, args));
		}
		printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_AVAILABLE, ul_int), flags, args));
		printf("\n");
		/*
		 * Print low vs. high information, if the user requested it.
		 * Note we check if low_total == 0: if so, then this kernel
		 * does not export the low and high stats. Note we still want
		 * to print the high info, even if it is zero.
		 */
		if (flags & FREE_LOHI) {
			print_head_col(_("Low:"));
			printf("%11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_LOW_TOTAL, ul_int), flags, args));
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_LOW_USED, ul_int), flags, args));
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_LOW_FREE, ul_int), flags, args));
			printf("\n");

			print_head_col( _("High:"));
			printf("%11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_HIGH_TOTAL, ul_int), flags, args));
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_HIGH_USED, ul_int), flags, args));
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_HIGH_FREE, ul_int), flags, args));
			printf("\n");
		}

		print_head_col(_("Swap:"));
		printf("%11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_SWAP_TOTAL, ul_int), flags, args));
		printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_SWAP_USED, ul_int), flags, args));
		printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_SWAP_FREE, ul_int), flags, args));
		printf("\n");

		if (flags & FREE_TOTAL) {
			print_head_col(_("Total:"));
			printf("%11s", scale_size(
				    MEMINFO_GET(mem_info, MEMINFO_MEM_TOTAL, ul_int) +
				    MEMINFO_GET(mem_info, MEMINFO_SWAP_TOTAL, ul_int), flags, args));
			printf(" %11s", scale_size(
				    MEMINFO_GET(mem_info, MEMINFO_MEM_USED, ul_int) +
				    MEMINFO_GET(mem_info, MEMINFO_SWAP_USED, ul_int), flags, args));
			printf(" %11s", scale_size(
				    MEMINFO_GET(mem_info, MEMINFO_MEM_FREE, ul_int) +
				    MEMINFO_GET(mem_info, MEMINFO_SWAP_FREE, ul_int), flags, args));
			printf("\n");
		}
		if (flags & FREE_COMMITTED) {
			print_head_col(_("Comm:"));
			printf("%11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_COMMIT_LIMIT, ul_int), flags, args));
			printf(" %11s", scale_size(MEMINFO_GET(mem_info, MEMINFO_MEM_COMMITTED_AS, ul_int), flags, args));
			printf(" %11s", scale_size(
				    MEMINFO_GET(mem_info, MEMINFO_MEM_COMMIT_LIMIT, ul_int) -
				    MEMINFO_GET(mem_info, MEMINFO_MEM_COMMITTED_AS, ul_int), flags, args));
			printf("\n");
		}

		} /* end else of if FREE_LINE */
		fflush(stdout);
		if (flags & FREE_REPEATCOUNT) {
			args.repeat_counter--;
			if (args.repeat_counter < 1)
				exit(EXIT_SUCCESS);
		}
		if (flags & FREE_REPEAT) {
			printf("\n");
			usleep(args.repeat_interval);
		}
	} while ((flags & FREE_REPEAT));

	exit(EXIT_SUCCESS);
}
