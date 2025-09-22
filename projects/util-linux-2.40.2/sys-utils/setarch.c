/*
 * Copyright (C) 2003-2007 Red Hat, Inc.
 *
 * This file is part of util-linux.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Written by Elliot Lee <sopwith@redhat.com>
 * New personality options & code added by Jindrich Novy <jnovy@redhat.com>
 * ADD_NO_RANDOMIZE flag added by Arjan van de Ven <arjanv@redhat.com>
 * Help and MIPS support from Mike Frysinger (vapier@gentoo.org)
 * Better error handling from Dmitry V. Levin (ldv@altlinux.org)
 *
 * based on ideas from the ppc32 util by Guy Streeter (2002-01), based on the
 * sparc32 util by Jakub Jelinek (1998, 1999)
 */

#include <sys/personality.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <sys/utsname.h>
#include "nls.h"
#include "c.h"
#include "closestream.h"
#include "sysfs.h"
#include "strutils.h"

#ifndef HAVE_PERSONALITY
# include <syscall.h>
# define personality(pers) ((long)syscall(SYS_personality, pers))
#endif

#define turn_on(_flag, _opts) \
	do { \
		(_opts) |= _flag; \
		if (verbose) \
			printf(_("Switching on %s.\n"), #_flag); \
	} while(0)

#ifndef UNAME26
# define UNAME26                 0x0020000
#endif
#ifndef ADDR_NO_RANDOMIZE
# define ADDR_NO_RANDOMIZE       0x0040000
#endif
#ifndef FDPIC_FUNCPTRS
# define FDPIC_FUNCPTRS          0x0080000
#endif
#ifndef MMAP_PAGE_ZERO
# define MMAP_PAGE_ZERO          0x0100000
#endif
#ifndef ADDR_COMPAT_LAYOUT
# define ADDR_COMPAT_LAYOUT      0x0200000
#endif
#ifndef READ_IMPLIES_EXEC
# define READ_IMPLIES_EXEC       0x0400000
#endif
#ifndef ADDR_LIMIT_32BIT
# define ADDR_LIMIT_32BIT        0x0800000
#endif
#ifndef SHORT_INODE
# define SHORT_INODE             0x1000000
#endif
#ifndef WHOLE_SECONDS
# define WHOLE_SECONDS           0x2000000
#endif
#ifndef STICKY_TIMEOUTS
# define STICKY_TIMEOUTS         0x4000000
#endif
#ifndef ADDR_LIMIT_3GB
# define ADDR_LIMIT_3GB          0x8000000
#endif

/* fallback for old glibc-headers 2.17 */
#ifndef PER_LINUX_FDPIC
# define PER_LINUX_FDPIC	(PER_LINUX | FDPIC_FUNCPTRS)
#endif

#define ALL_PERSONALITIES \
    X(PER_LINUX) \
    X(PER_LINUX_32BIT) \
    X(PER_LINUX_FDPIC) \
    X(PER_SVR4) \
    X(PER_SVR3) \
    X(PER_SCOSVR3) \
    X(PER_OSR5) \
    X(PER_WYSEV386) \
    X(PER_ISCR4) \
    X(PER_BSD) \
    X(PER_SUNOS) \
    X(PER_XENIX) \
    X(PER_LINUX32) \
    X(PER_LINUX32_3GB) \
    X(PER_IRIX32) \
    X(PER_IRIXN32) \
    X(PER_IRIX64) \
    X(PER_RISCOS) \
    X(PER_SOLARIS) \
    X(PER_UW7) \
    X(PER_OSF4) \
    X(PER_HPUX) \


#define ALL_OPTIONS \
    X(UNAME26) \
    X(ADDR_NO_RANDOMIZE) \
    X(FDPIC_FUNCPTRS) \
    X(MMAP_PAGE_ZERO) \
    X(ADDR_COMPAT_LAYOUT) \
    X(READ_IMPLIES_EXEC) \
    X(ADDR_LIMIT_32BIT) \
    X(SHORT_INODE) \
    X(WHOLE_SECONDS) \
    X(STICKY_TIMEOUTS) \
    X(ADDR_LIMIT_3GB) \


struct arch_domain {
	int		perval;		/* PER_* */
	const char	*target_arch;
	const char	*result_arch;
};


static void __attribute__((__noreturn__)) usage(int archwrapper)
{
	fputs(USAGE_HEADER, stdout);
	if (!archwrapper)
		fprintf(stdout, _(" %s [<arch>] [options] [<program> [<argument>...]]\n"), program_invocation_short_name);
	else
		fprintf(stdout, _(" %s [options] [<program> [<argument>...]]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Change the reported architecture and set personality flags.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputs(_(" -B, --32bit              turns on ADDR_LIMIT_32BIT\n"), stdout);
	fputs(_(" -F, --fdpic-funcptrs     makes function pointers point to descriptors\n"), stdout);
	fputs(_(" -I, --short-inode        turns on SHORT_INODE\n"), stdout);
	fputs(_(" -L, --addr-compat-layout changes the way virtual memory is allocated\n"), stdout);
	fputs(_(" -R, --addr-no-randomize  disables randomization of the virtual address space\n"), stdout);
	fputs(_(" -S, --whole-seconds      turns on WHOLE_SECONDS\n"), stdout);
	fputs(_(" -T, --sticky-timeouts    turns on STICKY_TIMEOUTS\n"), stdout);
	fputs(_(" -X, --read-implies-exec  turns on READ_IMPLIES_EXEC\n"), stdout);
	fputs(_(" -Z, --mmap-page-zero     turns on MMAP_PAGE_ZERO\n"), stdout);
	fputs(_(" -3, --3gb                limits the used address space to a maximum of 3 GB\n"), stdout);
	fputs(_("     --4gb                ignored (for backward compatibility only)\n"), stdout);
	fputs(_("     --uname-2.6          turns on UNAME26\n"), stdout);
	fputs(_(" -v, --verbose            say what options are being switched on\n"), stdout);

	if (!archwrapper) {
		fputs(_("     --list               list settable architectures, and exit\n"), stdout);
		fputs(_("     --show[=personality] show current or specific personality and exit\n"), stdout);
	}

	fputs(USAGE_SEPARATOR, stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(26));
	fprintf(stdout, USAGE_MAN_TAIL("setarch(8)"));

	exit(EXIT_SUCCESS);
}

/*
 * Returns inilialized list of all available execution domains.
 */
static struct arch_domain *init_arch_domains(void)
{
	static struct utsname un;
	size_t i;

	static struct arch_domain transitions[] =
	{
		{UNAME26,	"uname26",	NULL},
		{PER_LINUX32,	"linux32",	NULL},
		{PER_LINUX,	"linux64",	NULL},
#if defined(__powerpc__) || defined(__powerpc64__)
# ifdef __BIG_ENDIAN__
		{PER_LINUX32,	"ppc32",	"ppc"},
		{PER_LINUX32,	"ppc",		"ppc"},
		{PER_LINUX,	"ppc64",	"ppc64"},
		{PER_LINUX,	"ppc64pseries",	"ppc64"},
		{PER_LINUX,	"ppc64iseries",	"ppc64"},
# else
		{PER_LINUX32,	"ppc32",	"ppcle"},
		{PER_LINUX32,	"ppc",		"ppcle"},
		{PER_LINUX32,	"ppc32le",	"ppcle"},
		{PER_LINUX32,	"ppcle",	"ppcle"},
		{PER_LINUX,	"ppc64le",	"ppc64le"},
# endif
#endif
#if defined(__x86_64__) || defined(__i386__) || defined(__ia64__)
		{PER_LINUX32,	"i386",		"i386"},
		{PER_LINUX32,	"i486",		"i386"},
		{PER_LINUX32,	"i586",		"i386"},
		{PER_LINUX32,	"i686",		"i386"},
		{PER_LINUX32,	"athlon",	"i386"},
#endif
#if defined(__x86_64__) || defined(__i386__)
		{PER_LINUX,	"x86_64",	"x86_64"},
#endif
#if defined(__ia64__) || defined(__i386__)
		{PER_LINUX,	"ia64",		"ia64"},
#endif
#if defined(__hppa__)
		{PER_LINUX32,	"parisc32",	"parisc"},
		{PER_LINUX32,	"parisc",	"parisc"},
		{PER_LINUX,	"parisc64",	"parisc64"},
#endif
#if defined(__s390x__) || defined(__s390__)
		{PER_LINUX32,	"s390",		"s390"},
		{PER_LINUX,	"s390x",	"s390x"},
#endif
#if defined(__sparc64__) || defined(__sparc__)
		{PER_LINUX32,	"sparc",	"sparc"},
		{PER_LINUX32,	"sparc32bash",	"sparc"},
		{PER_LINUX32,	"sparc32",	"sparc"},
		{PER_LINUX,	"sparc64",	"sparc64"},
#endif
#if defined(__mips64__) || defined(__mips__)
		{PER_LINUX32,	"mips32",	"mips"},
		{PER_LINUX32,	"mips",		"mips"},
		{PER_LINUX,	"mips64",	"mips64"},
#endif
#if defined(__alpha__)
		{PER_LINUX,	"alpha",	"alpha"},
		{PER_LINUX,	"alphaev5",	"alpha"},
		{PER_LINUX,	"alphaev56",	"alpha"},
		{PER_LINUX,	"alphaev6",	"alpha"},
		{PER_LINUX,	"alphaev67",	"alpha"},
#endif
#if defined(__loongarch__)
		{PER_LINUX,	"loongarch",	"loongarch64"},
		{PER_LINUX,	"loongarch64",	"loongarch64"},
#endif
#if defined(__e2k__)
		{PER_LINUX,	"e2k",      "e2k"},
		{PER_LINUX,	"e2kv4",	"e2k"},
		{PER_LINUX,	"e2kv5",	"e2k"},
		{PER_LINUX,	"e2kv6",	"e2k"},
		{PER_LINUX,	"e2k4c",	"e2k"},
		{PER_LINUX,	"e2k8c",	"e2k"},
		{PER_LINUX,	"e2k1cp",	"e2k"},
		{PER_LINUX,	"e2k8c2",	"e2k"},
		{PER_LINUX,	"e2k12c",	"e2k"},
		{PER_LINUX,	"e2k16c",	"e2k"},
		{PER_LINUX,	"e2k2c3",	"e2k"},
#endif
#if defined(__arm__) || defined(__aarch64__)
# ifdef __BIG_ENDIAN__
		{PER_LINUX32,	"armv7b",	"arm"},
		{PER_LINUX32,	"armv8b",	"arm"},
# else
		{PER_LINUX32,	"armv7l",	"arm"},
		{PER_LINUX32,	"armv8l",	"arm"},
# endif
		{PER_LINUX32,	"armh",		"arm"},
		{PER_LINUX32,	"arm",		"arm"},
		{PER_LINUX,	"arm64",	"aarch64"},
		{PER_LINUX,	"aarch64",	"aarch64"},
#endif
#if defined(__riscv)
		{PER_LINUX32,	"riscv32",	"riscv32"},
		{PER_LINUX32,	"rv32",	"riscv32"},
		{PER_LINUX,	"riscv64",	"riscv64"},
		{PER_LINUX,	"rv64",	"riscv64"},
#endif
		/* place holder, will be filled up at runtime */
		{-1,		NULL,		NULL},
		{-1,		NULL,		NULL}
	};

	/* Add the trivial transition {PER_LINUX, machine, machine} if no
	 * such target_arch is hardcoded yet.  */
	uname(&un);
	for (i = 0; transitions[i].perval >= 0; i++)
		if (!strcmp(un.machine, transitions[i].target_arch))
			break;
	if (transitions[i].perval < 0) {
		int wrdsz = sysfs_get_address_bits(NULL);
		if (wrdsz < 0)
			wrdsz = CHAR_BIT * sizeof(void *);
		if (wrdsz == 32 || wrdsz == 64) {
			/* fill up the place holder */
			transitions[i].perval = wrdsz == 32 ? PER_LINUX32 : PER_LINUX;
			transitions[i].target_arch = un.machine;
			transitions[i].result_arch = un.machine;
		}
	}

	return transitions;
}

/*
 * List all execution domains from transitions
 */
static void list_arch_domains(struct arch_domain *doms)
{
	struct arch_domain *d;

	for (d = doms; d->target_arch != NULL; d++)
		printf("%s\n", d->target_arch);
}

static struct arch_domain *get_arch_domain(struct arch_domain *doms, const char *pers)
{
	struct arch_domain *d;

	for (d = doms; d && d->perval >= 0; d++) {
		if (!strcmp(pers, d->target_arch))
			break;
	}

	return !d || d->perval < 0 ? NULL : d;
}

static void verify_arch_domain(struct arch_domain *doms, struct arch_domain *target, const char *wanted)
{
	struct utsname un;

	if (!doms || !target || !target->result_arch)
		return;

	uname(&un);

	if (!strcmp(un.machine, target->result_arch))
		return;

	if (!strcmp(target->result_arch, "i386") ||
	    !strcmp(target->result_arch, "arm")) {
		struct arch_domain *dom;
		for (dom = doms; dom->target_arch != NULL; dom++) {
			if (!dom->result_arch || strcmp(dom->result_arch, target->result_arch))
				continue;
			if (!strcmp(dom->target_arch, un.machine))
				return;
		}
	}

	errx(EXIT_FAILURE, _("Kernel cannot set architecture to %s"), wanted);
}

static const struct { int value; const char * const name; } all_personalities[] = {
#define X(opt) { .value = opt, .name = #opt },
	ALL_PERSONALITIES
#undef X
};

static const struct { int value; const char * const name; } all_options[] = {
#define X(opt) { .value = opt, .name = #opt },
	ALL_OPTIONS
#undef X
};

static void show_personality(int pers)
{
	int options;
	size_t i;

	/* Test for exact matches including options */
	for (i = 0; i < ARRAY_SIZE(all_personalities); i++) {
		if (pers == all_personalities[i].value) {
			printf("%s\n", all_personalities[i].name);
			return;
		}
	}

	options = pers & ~PER_MASK;
	pers &= PER_MASK;

	/* Second test for type-only matches */
	for (i = 0; i < ARRAY_SIZE(all_personalities); i++) {
		if (pers == all_personalities[i].value) {
			printf("%s", all_personalities[i].name);
			break;
		}
	}

	if (i == ARRAY_SIZE(all_personalities))
		printf("0x%02x", pers);

	if (options) {
		printf(" (");

		for (i = 0; i < ARRAY_SIZE(all_options); i++) {
			if (options & all_options[i].value) {
				printf("%s", all_options[i].name);

				options &= ~all_options[i].value;
				if (options)
					printf(" ");
			}
		}
		if (options)
			printf("0x%08x", options);
		printf(")");
	}
	printf("\n");
}

static void show_current_personality(void)
{
	int pers = personality(0xffffffff);
	if (pers == -1)
		err(EXIT_FAILURE, _("Can not get current kernel personality"));

	show_personality(pers);
}

int main(int argc, char *argv[])
{
	const char *arch = NULL;
	unsigned long options = 0;
	int verbose = 0;
	int archwrapper;
	int c;
	struct arch_domain *doms = NULL, *target = NULL;
	unsigned long pers_value = 0;
	char *shell = NULL, *shell_arg = NULL;

	/* Options without equivalent short options */
	enum {
		OPT_4GB = CHAR_MAX + 1,
		OPT_UNAME26,
		OPT_LIST,
		OPT_SHOW,
	};

	/* Options --3gb and --4gb are for compatibility with an old
	 * Debian setarch implementation.  */
	static const struct option longopts[] = {
		{"help",		no_argument,		NULL,	'h'},
		{"version",		no_argument,		NULL,	'V'},
		{"verbose",		no_argument,		NULL,	'v'},
		{"addr-no-randomize",	no_argument,		NULL,	'R'},
		{"fdpic-funcptrs",	no_argument,		NULL,	'F'},
		{"mmap-page-zero",	no_argument,		NULL,	'Z'},
		{"addr-compat-layout",	no_argument,		NULL,	'L'},
		{"read-implies-exec",	no_argument,		NULL,	'X'},
		{"32bit",		no_argument,		NULL,	'B'},
		{"short-inode",		no_argument,		NULL,	'I'},
		{"whole-seconds",	no_argument,		NULL,	'S'},
		{"sticky-timeouts",	no_argument,		NULL,	'T'},
		{"3gb",			no_argument,		NULL,	'3'},
		{"4gb",			no_argument,		NULL,	OPT_4GB},
		{"uname-2.6",		no_argument,		NULL,	OPT_UNAME26},
		{"list",		no_argument,		NULL,	OPT_LIST},
		{"show",		optional_argument,	NULL,	OPT_SHOW},
		{NULL,			0,			NULL,	0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	if (argc < 1) {
		warnx(_("Not enough arguments"));
		errtryhelp(EXIT_FAILURE);
	}
	archwrapper = strcmp(program_invocation_short_name, "setarch") != 0;
	if (archwrapper) {
		arch = program_invocation_short_name;	/* symlinks to setarch */

		/* Don't use ifdef sparc here, we get "Unrecognized architecture"
		 * error message later if necessary */
		if (strcmp(arch, "sparc32bash") == 0) {
			shell = "/bin/bash";
			shell_arg = "";
			goto set_arch;
		}
	} else {
		if (1 < argc && *argv[1] != '-') {
			arch = argv[1];
			argv[1] = argv[0];	/* for getopt_long() to get the program name */
			argv++;
			argc--;
		}
	}

	while ((c = getopt_long(argc, argv, "+hVv3BFILRSTXZ", longopts, NULL)) != -1) {
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'R':
			turn_on(ADDR_NO_RANDOMIZE, options);
			break;
		case 'F':
			turn_on(FDPIC_FUNCPTRS, options);
			break;
		case 'Z':
			turn_on(MMAP_PAGE_ZERO, options);
			break;
		case 'L':
			turn_on(ADDR_COMPAT_LAYOUT, options);
			break;
		case 'X':
			turn_on(READ_IMPLIES_EXEC, options);
			break;
		case 'B':
			turn_on(ADDR_LIMIT_32BIT, options);
			break;
		case 'I':
			turn_on(SHORT_INODE, options);
			break;
		case 'S':
			turn_on(WHOLE_SECONDS, options);
			break;
		case 'T':
			turn_on(STICKY_TIMEOUTS, options);
			break;
		case '3':
			turn_on(ADDR_LIMIT_3GB, options);
			break;
		case OPT_4GB:	/* just ignore this one */
			break;
		case OPT_UNAME26:
			turn_on(UNAME26, options);
			break;
		case OPT_LIST:
			if (!archwrapper) {
				list_arch_domains(init_arch_domains());
				return EXIT_SUCCESS;
			} else
				warnx(_("unrecognized option '--list'"));
			goto error_getopts;
		case OPT_SHOW:
			if (!archwrapper) {
				if (!optarg || strcmp(optarg, "current") == 0)
					show_current_personality();
				else
					show_personality(str2num_or_err(
						optarg, 16,
						_("could not parse personality"),
						0, INT_MAX));
				return EXIT_SUCCESS;
			} else
				warnx(_("unrecognized option '--show'"));
			goto error_getopts;

error_getopts:
		default:
			errtryhelp(EXIT_FAILURE);
		case 'h':
			usage(archwrapper);
		case 'V':
			print_version(EXIT_SUCCESS);
		}
	}

	if (!arch && !options)
		errx(EXIT_FAILURE, _("no architecture argument or personality flags specified"));

	argc -= optind;
	argv += optind;

set_arch:
	/* get execution domain (architecture) */
	if (arch) {
		doms = init_arch_domains();
		target = get_arch_domain(doms, arch);

		if (!target)
			errx(EXIT_FAILURE, _("%s: Unrecognized architecture"), arch);
		pers_value = target->perval;
	}

	/* add personality flags */
	pers_value |= options;

	/* call kernel */
	if (personality(pers_value) < 0) {
		/*
		 * Depending on architecture and kernel version, personality
		 * syscall is either capable or incapable of returning an error.
		 * If the return value is not an error, then it's the previous
		 * personality value, which can be an arbitrary value
		 * undistinguishable from an error value.
		 * To make things clear, a second call is needed.
		 */
		if (personality(pers_value) < 0)
			err(EXIT_FAILURE, _("failed to set personality to %s"), arch);
	}

	/* make sure architecture is set as expected */
	if (arch)
		verify_arch_domain(doms, target, arch);

	if (!argc) {
		shell = "/bin/sh";
		shell_arg = "-sh";
	}
	if (verbose) {
		printf(_("Execute command `%s'.\n"), shell ? shell : argv[0]);
		/* flush all output streams before exec */
		fflush(NULL);
	}

	/* Execute shell */
	if (shell) {
		execl(shell, shell_arg, (char *)NULL);
		errexec(shell);
	}

	/* Execute on command line specified command */
	execvp(argv[0], argv);
	errexec(argv[0]);
}
