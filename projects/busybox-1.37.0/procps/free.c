/* vi: set sw=4 ts=4: */
/*
 * Mini free implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config FREE
//config:	bool "free (3.8 kb)"
//config:	default y
//config:	help
//config:	free displays the total amount of free and used physical and swap
//config:	memory in the system, as well as the buffers used by the kernel.
//config:	The shared memory column should be ignored; it is obsolete.

//applet:IF_FREE(APPLET_NOFORK(free, free, BB_DIR_USR_BIN, BB_SUID_DROP, free))

//kbuild:lib-$(CONFIG_FREE) += free.o

//usage:#define free_trivial_usage
//usage:       "" IF_DESKTOP("[-bkmgh]")
//usage:#define free_full_usage "\n\n"
//usage:       "Display free and used memory"
//usage:
//usage:#define free_example_usage
//usage:       "$ free\n"
//usage:       "              total         used         free       shared      buffers\n"
//usage:       "  Mem:       257628       248724         8904        59644        93124\n"
//usage:       " Swap:       128516         8404       120112\n"
//usage:       "Total:       386144       257128       129016\n"
//procps-ng 3.3.15:
// -b, --bytes         show output in bytes
//     --kilo          show output in kilobytes
//     --mega          show output in megabytes
//     --giga          show output in gigabytes
//     --tera          show output in terabytes
//     --peta          show output in petabytes
// -k, --kibi          show output in kibibytes
// -m, --mebi          show output in mebibytes
// -g, --gibi          show output in gibibytes
//     --tebi          show output in tebibytes
//     --pebi          show output in pebibytes
// -h, --human         show human-readable output
//     --si            use powers of 1000 not 1024
// -l, --lohi          show detailed low and high memory statistics
// -t, --total         show total for RAM + swap
// -s N, --seconds N   repeat printing every N seconds
// -c N, --count N     repeat printing N times, then exit
// -w, --wide          wide output
//
//NB: if we implement -s or -c, need to stop being NOFORK!

#include "libbb.h"
#ifdef __linux__
# include <sys/sysinfo.h>
#endif

struct globals {
	unsigned mem_unit;
#if ENABLE_DESKTOP
	unsigned unit;
# define G_unit g->unit
#else
# define G_unit (1 << 10)
#endif
	unsigned long cached_kb, available_kb, reclaimable_kb;
};
/* Because of NOFORK, "globals" are not in global data */

static const char *scale(struct globals *g, unsigned long d)
{
	/* Display (size * block_size) with one decimal digit.
	 * If display_unit == 0, show value no bigger than 1024 with suffix (K,M,G...),
	 * else divide by display_unit and do not use suffix.
	 * Returns "auto pointer" */
	return make_human_readable_str(d, g->mem_unit, G_unit);
}

/* NOINLINE reduces main() stack usage, which makes code smaller (on x86 at least) */
static NOINLINE unsigned int parse_meminfo(struct globals *g)
{
	char buf[60]; /* actual lines we expect are ~30 chars or less */
	FILE *fp;
	int seen_cached_and_available_and_reclaimable;

	fp = xfopen_for_read("/proc/meminfo");
	g->cached_kb = g->available_kb = g->reclaimable_kb = 0;
	seen_cached_and_available_and_reclaimable = 3;
	while (fgets(buf, sizeof(buf), fp)) {
		if (sscanf(buf, "Cached: %lu %*s\n", &g->cached_kb) == 1)
			if (--seen_cached_and_available_and_reclaimable == 0)
				break;
		if (sscanf(buf, "MemAvailable: %lu %*s\n", &g->available_kb) == 1)
			if (--seen_cached_and_available_and_reclaimable == 0)
				break;
		if (sscanf(buf, "SReclaimable: %lu %*s\n", &g->reclaimable_kb) == 1)
			if (--seen_cached_and_available_and_reclaimable == 0)
				break;
	}
	/* Have to close because of NOFORK */
	fclose(fp);

	return seen_cached_and_available_and_reclaimable == 0;
}

int free_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int free_main(int argc UNUSED_PARAM, char **argv IF_NOT_DESKTOP(UNUSED_PARAM))
{
	struct globals G;
	struct sysinfo info;
	unsigned long long cached, cached_plus_free, available;
	int seen_available;

#if ENABLE_DESKTOP
	G.unit = 1 << 10;
	if (argv[1] && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 'b':
			G.unit = 1;
			break;
		case 'k': /* 2^10 */
			/* G.unit = 1 << 10; - already is */
			break;
		case 'm': /* 2^20 */
			G.unit = 1 << 20;
			break;
		case 'g': /* 2^30 */
			G.unit = 1 << 30;
			break;
//		case 't':
// -- WRONG, -t is not "terabytes" in procps-ng, it's --total
//			G.unit = 1 << 40;
//			break;
		case 'h':
			G.unit = 0; /* human readable */
			break;
		default:
			bb_show_usage();
		}
	}
#endif
	printf("       %12s%12s%12s%12s%12s%12s\n"
	"Mem:   ",
		"total",
		"used",
		"free",
		"shared", "buff/cache", "available" /* swap and total don't have these columns */
	);

	sysinfo(&info);
	/* Extract cached and memavailable from /proc/meminfo and convert to mem_units */
	seen_available = parse_meminfo(&G);
	G.mem_unit = (info.mem_unit ? info.mem_unit : 1); /* kernels < 2.4.x return mem_unit==0, so cope */
	available = ((unsigned long long) G.available_kb * 1024) / G.mem_unit;
	cached = ((unsigned long long) G.cached_kb * 1024) / G.mem_unit;
	cached += info.bufferram;
	cached += ((unsigned long long) G.reclaimable_kb * 1024) / G.mem_unit;
	cached_plus_free = cached + info.freeram;

	printf("%12s%12s%12s",
		scale(&G, info.totalram),                //total
		scale(&G, info.totalram - cached_plus_free), //used
		scale(&G, info.freeram)                  //free
	);
	/* using two printf's: only 4 auto strings are supported, we need 6 */
	printf("%12s%12s%12s\n",
		scale(&G, info.sharedram),               //shared
		scale(&G, cached),                       //buff/cache
		scale(&G, available)                     //available
	);
	/* On kernels < 3.14, MemAvailable is not provided.
	 * Show alternate, more meaningful busy/free numbers by counting
	 * buffer cache as free memory. */
	if (!seen_available) {
		printf("-/+ buffers/cache: ");
		printf("%12s%12s%12s\n" + 4,
			scale(&G, info.totalram - cached_plus_free), //used
			scale(&G, cached_plus_free)                  //free
		);
	}
#if BB_MMU
	printf("Swap:  ");
	printf("%12s%12s%12s\n",
		scale(&G, info.totalswap),                 //total
		scale(&G, info.totalswap - info.freeswap), //used
		scale(&G, info.freeswap)                   //free
	);
#endif
	return EXIT_SUCCESS;
}
