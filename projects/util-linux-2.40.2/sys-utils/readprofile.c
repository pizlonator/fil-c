/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 1994,1996 Alessandro Rubini (rubini@ipvvis.unipv.it)
 *
 * readprofile.c - used to read /proc/profile
 */

/*
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * 1999-09-01 Stephane Eranian <eranian@cello.hpl.hp.com>
 * - 64bit clean patch
 * 3Feb2001 Andrew Morton <andrewm@uow.edu.au>
 * - -M option to write profile multiplier.
 * 2001-11-07 Werner Almesberger <wa@almesberger.net>
 * - byte order auto-detection and -n option
 * 2001-11-09 Werner Almesberger <wa@almesberger.net>
 * - skip step size (index 0)
 * 2002-03-09 John Levon <moz@compsoc.man.ac.uk>
 * - make maplineno do something
 * 2002-11-28 Mads Martin Joergensen +
 * - also try /boot/System.map-`uname -r`
 * 2003-04-09 Werner Almesberger <wa@almesberger.net>
 * - fixed off-by eight error and improved heuristics in byte order detection
 * 2003-08-12 Nikita Danilov <Nikita@Namesys.COM>
 * - added -s option; example of use:
 * "readprofile -s -m /boot/System.map-test | grep __d_lookup | sort -n -k3"
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "c.h"
#include "strutils.h"
#include "nls.h"
#include "xalloc.h"
#include "closestream.h"

#define S_LEN 128

/* These are the defaults */
static char defaultmap[]="/boot/System.map";
static char defaultpro[]="/proc/profile";

static FILE *myopen(char *name, char *mode, int *flag)
{
	int len = strlen(name);

	if (!strcmp(name + len - 3, ".gz")) {
		FILE *res;
		char *cmdline = xmalloc(len + 6);
		snprintf(cmdline, len + 6, "zcat %s", name);
		res = popen(cmdline, mode);
		free(cmdline);
		*flag = 1;
		return res;
	}
	*flag = 0;
	return fopen(name, mode);
}

#ifndef BOOT_SYSTEM_MAP
#define BOOT_SYSTEM_MAP "/boot/System.map-"
#endif

static char *boot_uname_r_str(void)
{
	struct utsname uname_info;
	char *s;

	if (uname(&uname_info))
		return "";
	xasprintf(&s, "%s%s", BOOT_SYSTEM_MAP, uname_info.release);
	return s;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Display kernel profiling information.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out,
	      _(" -m, --mapfile <mapfile>   (defaults: \"%s\" and\n"), defaultmap);
	fprintf(out,
	      _("                                      \"%s\")\n"), boot_uname_r_str());
	fprintf(out,
	      _(" -p, --profile <pro-file>  (default:  \"%s\")\n"), defaultpro);
	fputs(_(" -M, --multiplier <mult>   set the profiling multiplier to <mult>\n"), out);
	fputs(_(" -i, --info                print only info about the sampling step\n"), out);
	fputs(_(" -v, --verbose             print verbose data\n"), out);
	fputs(_(" -a, --all                 print all symbols, even if count is 0\n"), out);
	fputs(_(" -b, --histbin             print individual histogram-bin counts\n"), out);
	fputs(_(" -s, --counters            print individual counters within functions\n"), out);
	fputs(_(" -r, --reset               reset all the counters (root only)\n"), out);
	fputs(_(" -n, --no-auto             disable byte order auto-detection\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(27));
	fprintf(out, USAGE_MAN_TAIL("readprofile(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	FILE *map;
	int proFd, has_mult = 0, multiplier = 0;
	char *mapFile, *proFile;
	size_t len = 0, indx = 1;
	unsigned long long add0 = 0;
	unsigned int step;
	unsigned int *buf, total, fn_len;
	unsigned long long fn_add = 0, next_add; /* current and next address */
	char fn_name[S_LEN], next_name[S_LEN];	/* current and next name */
	char mode[8];
	int c;
	ssize_t rc;
	int optAll = 0, optInfo = 0, optReset = 0, optVerbose = 0, optNative = 0;
	int optBins = 0, optSub = 0;
	char mapline[S_LEN];
	int maplineno = 1;
	int popenMap;		/* flag to tell if popen() has been used */
	int header_printed;
	double rep = 0;

	static const struct option longopts[] = {
		{"mapfile", required_argument, NULL, 'm'},
		{"profile", required_argument, NULL, 'p'},
		{"multiplier", required_argument, NULL, 'M'},
		{"info", no_argument, NULL, 'i'},
		{"verbose", no_argument, NULL, 'v'},
		{"all", no_argument, NULL, 'a'},
		{"histbin", no_argument, NULL, 'b'},
		{"counters", no_argument, NULL, 's'},
		{"reset", no_argument, NULL, 'r'},
		{"no-auto", no_argument, NULL, 'n'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

#define next (current^1)

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	proFile = defaultpro;
	mapFile = defaultmap;

	while ((c = getopt_long(argc, argv, "m:p:M:ivabsrnVh", longopts, NULL)) != -1) {
		switch (c) {
		case 'm':
			mapFile = optarg;
			break;
		case 'n':
			optNative++;
			break;
		case 'p':
			proFile = optarg;
			break;
		case 'a':
			optAll++;
			break;
		case 'b':
			optBins++;
			break;
		case 's':
			optSub++;
			break;
		case 'i':
			optInfo++;
			break;
		case 'M':
			multiplier = strtol_or_err(optarg, _("failed to parse multiplier"));
			has_mult = 1;
			break;
		case 'r':
			optReset++;
			break;
		case 'v':
			optVerbose++;
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optReset || has_mult) {
		int fd, to_write;

		/* When writing the multiplier, if the length of the
		 * write is not sizeof(int), the multiplier is not
		 * changed. */
		if (has_mult) {
			to_write = sizeof(int);
		} else {
			multiplier = 0;
			/* sth different from sizeof(int) */
			to_write = 1;
		}
		/* try to become root, just in case */
		ignore_result( setuid(0) );
		fd = open(defaultpro, O_WRONLY);
		if (fd < 0)
			err(EXIT_FAILURE, "%s", defaultpro);
		if (write(fd, &multiplier, to_write) != to_write)
			err(EXIT_FAILURE, _("error writing %s"), defaultpro);
		close(fd);
		exit(EXIT_SUCCESS);
	}

	/* Use an fd for the profiling buffer, to skip stdio overhead */
	if (((proFd = open(proFile, O_RDONLY)) < 0)
	    || ((int)(len = lseek(proFd, 0, SEEK_END)) < 0)
	    || (lseek(proFd, 0, SEEK_SET) < 0))
		err(EXIT_FAILURE, "%s", proFile);
	if (!len)
		errx(EXIT_FAILURE, "%s: %s", proFile, _("input file is empty"));

	buf = xmalloc(len);

	rc = read(proFd, buf, len);
	if (rc < 0 || (size_t) rc != len)
		err(EXIT_FAILURE, "%s", proFile);
	close(proFd);

	if (!optNative) {
		int entries = len / sizeof(*buf);
		int big = 0, small = 0;
		unsigned *p;
		size_t i;

		for (p = buf + 1; p < buf + entries; p++) {
			if (*p & ~0U << ((unsigned) sizeof(*buf) * 4U))
				big++;
			if (*p & ((1U << ((unsigned) sizeof(*buf) * 4U)) - 1U))
				small++;
		}
		if (big > small) {
			warnx(_("Assuming reversed byte order. "
				"Use -n to force native byte order."));
			for (p = buf; p < buf + entries; p++)
				for (i = 0; i < sizeof(*buf) / 2; i++) {
					unsigned char *b = (unsigned char *)p;
					unsigned char tmp;
					tmp = b[i];
					b[i] = b[sizeof(*buf) - i - 1];
					b[sizeof(*buf) - i - 1] = tmp;
				}
		}
	}

	step = buf[0];
	if (optInfo) {
		printf(_("Sampling_step: %u\n"), step);
		exit(EXIT_SUCCESS);
	}

	total = 0;

	map = myopen(mapFile, "r", &popenMap);
	if (map == NULL && mapFile == defaultmap) {
		mapFile = boot_uname_r_str();
		map = myopen(mapFile, "r", &popenMap);
	}
	if (map == NULL)
		err(EXIT_FAILURE, "%s", mapFile);

	while (fgets(mapline, S_LEN, map)) {
		if (sscanf(mapline, "%llx %7[^\n ] %127[^\n ]", &fn_add, mode, fn_name) != 3)
			errx(EXIT_FAILURE, _("%s(%i): wrong map line"), mapFile,
			     maplineno);
		/* only elf works like this */
		if (!strcmp(fn_name, "_stext") || !strcmp(fn_name, "__stext")) {
			add0 = fn_add;
			break;
		}
		maplineno++;
	}

	if (!add0)
		errx(EXIT_FAILURE, _("can't find \"_stext\" in %s"), mapFile);

	/*
	 * Main loop.
	 */
	while (fgets(mapline, S_LEN, map)) {
		unsigned int this = 0;
		int done = 0;

		if (sscanf(mapline, "%llx %7[^\n ] %127[^\n ]", &next_add, mode, next_name) != 3)
			errx(EXIT_FAILURE, _("%s(%i): wrong map line"), mapFile,
			     maplineno);
		header_printed = 0;

		/* the kernel only profiles up to _etext */
		if (!strcmp(next_name, "_etext") ||
		    !strcmp(next_name, "__etext"))
			done = 1;
		else {
			/* ignore any LEADING (before a '[tT]' symbol
			 * is found) Absolute symbols and __init_end
			 * because some architectures place it before
			 * .text section */
			if ((*mode == 'A' || *mode == '?')
			    && (total == 0 || !strcmp(next_name, "__init_end")))
				continue;
			if (*mode != 'T' && *mode != 't' &&
			    *mode != 'W' && *mode != 'w')
				break;	/* only text is profiled */
		}

		if (indx >= len / sizeof(*buf))
			errx(EXIT_FAILURE,
			     _("profile address out of range. Wrong map file?"));

		while (step > 0 && indx < (next_add - add0) / step) {
			if (optBins && (buf[indx] || optAll)) {
				if (!header_printed) {
					printf("%s:\n", fn_name);
					header_printed = 1;
				}
				printf("\t%llx\t%u\n", (indx - 1) * step + add0,
				       buf[indx]);
			}
			this += buf[indx++];
		}
		total += this;

		if (optBins) {
			if (optVerbose || this > 0)
				printf("  total\t\t\t\t%u\n", this);
		} else if ((this || optAll) &&
			   (fn_len = next_add - fn_add) != 0) {
			if (optVerbose)
				printf("%016llx %-40s %6u %8.4f\n", fn_add,
				       fn_name, this, this / (double)fn_len);
			else
				printf("%6u %-40s %8.4f\n",
				       this, fn_name, this / (double)fn_len);
			if (optSub && step > 0) {
				unsigned long long scan;

				for (scan = (fn_add - add0) / step + 1;
				     scan < (next_add - add0) / step;
				     scan++) {
					unsigned long long addr;
					addr = (scan - 1) * step + add0;
					printf("\t%#llx\t%s+%#llx\t%u\n",
					       addr, fn_name, addr - fn_add,
					       buf[scan]);
				}
			}
		}

		fn_add = next_add;
		strcpy(fn_name, next_name);

		maplineno++;
		if (done)
			break;
	}

	/* clock ticks, out of kernel text - probably modules */
	printf("%6u %s\n", buf[len / sizeof(*buf) - 1], "*unknown*");

	if (fn_add > add0)
		rep = total / (double)(fn_add - add0);

	/* trailer */
	if (optVerbose)
		printf("%016x %-40s %6u %8.4f\n",
		       0, "total", total, rep);
	else
		printf("%6u %-40s %8.4f\n",
		       total, _("total"), rep);

	popenMap ? pclose(map) : fclose(map);
	exit(EXIT_SUCCESS);
}
