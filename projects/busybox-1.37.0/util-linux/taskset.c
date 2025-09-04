/* vi: set sw=4 ts=4: */
/*
 * taskset - retrieve or set a processes' CPU affinity
 * Copyright (c) 2006 Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config TASKSET
//config:	bool "taskset (5.6 kb)"
//config:	default y
//config:	help
//config:	Retrieve or set a processes's CPU affinity.
//config:	This requires sched_{g,s}etaffinity support in your libc.
//config:
//config:config FEATURE_TASKSET_FANCY
//config:	bool "Fancy output"
//config:	default y
//config:	depends on TASKSET
//config:	help
//config:	Needed for machines with more than 32-64 CPUs:
//config:	affinity parameter 0xHHHHHHHHHHHHHHHHHHHH can be arbitrarily long
//config:	in this case. Otherwise, it is limited to sizeof(long).
//config:
//config:config FEATURE_TASKSET_CPULIST
//config:	bool "CPU list support (-c option)"
//config:	default y
//config:	depends on FEATURE_TASKSET_FANCY
//config:	help
//config:	Add support for taking/printing affinity as CPU list when '-c'
//config:	option is used. For example, it prints '0-3,7' instead of mask '8f'.

//applet:IF_TASKSET(APPLET_NOEXEC(taskset, taskset, BB_DIR_USR_BIN, BB_SUID_DROP, taskset))

//kbuild:lib-$(CONFIG_TASKSET) += taskset.o

//usage:#define taskset_trivial_usage
//usage:       "[-ap] [HEXMASK"IF_FEATURE_TASKSET_CPULIST(" | -c LIST")"] { PID | PROG ARGS }"
//usage:#define taskset_full_usage "\n\n"
//usage:       "Set or get CPU affinity\n"
//usage:     "\n	-p	Operate on PID"
//usage:     "\n	-a	Operate on all threads"
//usage:     "\n	-c	Affinity is a list, not mask"
//usage:
//usage:#define taskset_example_usage
//usage:       "$ taskset 0x7 ./dgemm_test&\n"
//usage:       "$ taskset -p 0x1 $!\n"
//usage:       "pid 4790's current affinity mask: 7\n"
//usage:       "pid 4790's new affinity mask: 1\n"
//usage:       "$ taskset 0x7 /bin/sh -c './taskset -p 0x1 $$'\n"
//usage:       "pid 6671's current affinity mask: 1\n"
//usage:       "pid 6671's new affinity mask: 1\n"
//usage:       "$ taskset -p 1\n"
//usage:       "pid 1's current affinity mask: 3\n"
/*
 * Not yet implemented:
 * -a/--all-tasks (affect all threads)
 *	needs to get TIDs from /proc/PID/task/ and use _them_ as "pid" in sched_setaffinity(pid)
 */
#include <sched.h>
#include "libbb.h"

typedef unsigned long ul;
#define SZOF_UL (unsigned)(sizeof(ul))
#define BITS_UL (unsigned)(sizeof(ul)*8)
#define MASK_UL (unsigned)(sizeof(ul)*8 - 1)

#if ENABLE_FEATURE_TASKSET_FANCY
#define TASKSET_PRINTF_MASK "%s"
/* craft a string from the mask */
static char *from_mask(const ul *mask, unsigned sz_in_bytes)
{
	char *str = xzalloc((sz_in_bytes+1) * 2); /* we will leak it */
	char *p = str;
	for (;;) {
		ul v = *mask++;
		if (SZOF_UL == 4)
			p += sprintf(p, "%08lx", v);
		if (SZOF_UL == 8)
			p += sprintf(p, "%016lx", v);
		if (SZOF_UL == 16)
			p += sprintf(p, "%032lx", v); /* :) */
		sz_in_bytes -= SZOF_UL;
		if ((int)sz_in_bytes <= 0)
			break;
	}
	while (str[0] == '0' && str[1])
		str++;
	return str;
}
#else
#define TASKSET_PRINTF_MASK "%lx"
static unsigned long from_mask(ul *mask, unsigned sz_in_bytes UNUSED_PARAM)
{
	return *mask;
}
#endif

#if ENABLE_FEATURE_TASKSET_CPULIST
/*
 * Parse the CPU list and set the mask accordingly.
 *
 * The list element can be either a CPU index or a range of CPU indices.
 * Example: "1,3,5-7". Stride can be specified: "0-7:2" is "0,2,4,6".
 * Note: leading and trailing whitespace is not allowed.
 *  util-linux 2.31 allows leading and sometimes trailing whitespace:
 *  ok:     taskset -c ' 1,  2'
 *  ok:     taskset -c ' 1 , 2'
 *  ok:     taskset -c ' 1-7: 2 ,8'
 *  not ok: taskset -c ' 1 '
 *  not ok: taskset -c ' 1-7: 2 '
 */
static void parse_cpulist(ul *mask, unsigned max, char *s)
{
	char *aff = s;
	for (;;) {
		unsigned bit, end;
		unsigned stride = 1;

		bit = end = bb_strtou(s, &s, 10);
		if (*s == '-') {
			s++;
			end = bb_strtou(s, &s, 10);
			if (*s == ':') {
				s++;
				stride = bb_strtou(s, &s, 10);
			}
		}
		if ((*s != ',' && *s != '\0')
		 || bit > end
		 || end == UINT_MAX /* bb_strtou returns this on malformed / ERANGE numbers */
		 || (stride - 1) > (UINT_MAX / 4)
		/* disallow 0, malformed input, and too large stride prone to overflows */
		) {
			bb_error_msg_and_die("bad affinity '%s'", aff);
		}
		while (bit <= end && bit < max) {
			mask[bit / BITS_UL] |= (1UL << (bit & MASK_UL));
			bit += stride;
		}
		if (*s == '\0')
			break;
		s++;
	}
}
static void print_cpulist(const ul *mask, unsigned mask_size_in_bytes)
{
	const ul *mask_end;
	const char *delim;
	unsigned pos;
	ul bit;

	mask_end = mask + mask_size_in_bytes / sizeof(mask[0]);
	delim = "";
	pos = 0;
	bit = 1;
	for (;;) {
		if (*mask & bit) {
			unsigned onebit = pos + 1;
			printf("%s%u", delim, pos);
			do {
				pos++;
				bit <<= 1;
				if (bit == 0) {
					mask++;
					if (mask >= mask_end)
						break;
					bit = 1;
				}
			} while (*mask & bit);
			if (onebit != pos)
				printf("-%u", pos - 1);
			delim = ",";
		}
		pos++;
		bit <<= 1;
		if (bit == 0) {
			mask++;
			if (mask >= mask_end)
				break;
			bit = 1;
		}
	}
	bb_putchar('\n');
}
#endif

enum {
	OPT_p = 1 << 0,
	OPT_a = 1 << 1,
	OPT_c = (1 << 2) * ENABLE_FEATURE_TASKSET_CPULIST,
};

static int process_pid_str(const char *pid_str, unsigned opts, char *aff)
{
	ul *mask;
	unsigned mask_size_in_bytes;
	const char *current_new;
	pid_t pid = !pid_str ? 0 : xatou_range(pid_str, 1, INT_MAX); /* disallow "0": "taskset -p 0" should fail */

	mask_size_in_bytes = SZOF_UL;
	current_new = "current";
 print_aff:
	mask = get_malloc_cpu_affinity(pid, &mask_size_in_bytes);
	if (opts & OPT_p) {
#if ENABLE_FEATURE_TASKSET_CPULIST
		if (opts & OPT_c) {
			printf("pid %d's %s affinity list: ", pid, current_new);
			print_cpulist(mask, mask_size_in_bytes);
		} else
#endif
			printf("pid %d's %s affinity mask: "TASKSET_PRINTF_MASK"\n",
				pid, current_new, from_mask(mask, mask_size_in_bytes));
		if (!aff) {
			/* Either it was just "-p <pid>",
			 * or it was "-p <aff> <pid>" and we came here
			 * for the second time (see goto below) */
			return 0;
		}
		current_new = "new";
	}
	memset(mask, 0, mask_size_in_bytes);

	if (!ENABLE_FEATURE_TASKSET_FANCY) {
		/* Affinity was specified, translate it into mask */
		/* it is always in hex, skip "0x" if it exists */
		if (aff[0] == '0' && (aff[1]|0x20) == 'x')
			aff += 2;
		mask[0] = xstrtoul(aff, 16);
	}
#if ENABLE_FEATURE_TASKSET_CPULIST
	else if (opts & OPT_c) {
		parse_cpulist(mask, mask_size_in_bytes * 8, aff);
	}
#endif
	else {
		unsigned i;
		char *last_char;

		/* Affinity was specified, translate it into mask */
		/* it is always in hex, skip "0x" if it exists */
		if (aff[0] == '0' && (aff[1]|0x20) == 'x')
			aff += 2;

		i = 0; /* bit pos in mask[] */

		/* aff is ASCII hex string, accept very long masks in this form.
		 * Process hex string AABBCCDD... to ulong mask[]
		 * from the rightmost nibble, which is least-significant.
		 * Bits not fitting into mask[] are ignored: (example: 1234
		 * in 12340000000000000000000000000000000000000ff)
		 */
		last_char = strchrnul(aff, '\0');
		while (last_char > aff) {
			char c;
			ul val;

			last_char--;
			c = *last_char;
			if (isdigit(c))
				val = c - '0';
			else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
				val = (c|0x20) - ('a' - 10);
			else
				bb_error_msg_and_die("bad affinity '%s'", aff);

			if (i < mask_size_in_bytes * 8) {
				mask[i / BITS_UL] |= val << (i & MASK_UL);
				//bb_error_msg("bit %d set", i);
			}
			/* else:
			 * We can error out here, but we don't.
			 * For one, kernel itself ignores bits in mask[]
			 * which do not map to any CPUs:
			 * if mask[] has one 32-bit long element,
			 * but you have only 8 CPUs, all bits beyond first 8
			 * are ignored, silently.
			 * No point in making bits past 31th to be errors.
			 */
			i += 4;
		}
	}

	/* Set pid's or our own (pid==0) affinity */
	if (sched_setaffinity(pid, mask_size_in_bytes, (void*)mask))
		bb_perror_msg_and_die("can't %cet pid %d's affinity", 's', pid);
	//bb_error_msg("set mask[0]:%lx", mask[0]);

	if ((opts & OPT_p) && aff) { /* "-p <aff> <pid> [...ignored...]" */
		aff = NULL;
		goto print_aff; /* print new affinity and exit */
	}
	return 0;
}

static int FAST_FUNC iter(const char *dn UNUSED_PARAM, struct dirent *ent, void *aff)
{
	if (isdigit(ent->d_name[0]))
		return process_pid_str(ent->d_name, option_mask32, aff);
	return 0;
}

int taskset_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int taskset_main(int argc UNUSED_PARAM, char **argv)
{
	const char *pid_str;
	char *aff;
	unsigned opts;

	/* NB: we mimic util-linux's taskset: -p does not take
	 * an argument, i.e., "-pN" is NOT valid, only "-p N"!
	 * Indeed, util-linux-2.13-pre7 uses:
	 * getopt_long(argc, argv, "+pchV", ...), not "...p:..." */

	opts = getopt32(argv, "^+" "pa"IF_FEATURE_TASKSET_CPULIST("c")
			"\0" "-1" /* at least 1 arg */);
	argv += optind;

	aff = *argv++;
	if (!(opts & OPT_p)) {
		/* <aff> <cmd...> */
		if (!*argv)
			bb_show_usage();
		process_pid_str(NULL, opts, aff);
		BB_EXECVP_or_die(argv);
	}

	pid_str = aff;
	if (*argv) /* "-p <aff> <pid> ...rest.is.ignored..." */
		pid_str = *argv;
	else
		aff = NULL;

	if (opts & OPT_a) {
		char *dn;
		int r;

		dn = xasprintf("/proc/%s/task", pid_str);
		r = iterate_on_dir(dn, iter, aff);
		IF_FEATURE_CLEAN_UP(free(dn);)
		if (r == 0)
			return r; /* EXIT_SUCCESS */
		/* else: no /proc/PID/task, act as if no -a was given */
	}
	return process_pid_str(pid_str, opts, aff);
}
