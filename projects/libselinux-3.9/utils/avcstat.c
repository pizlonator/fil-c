/*
 * avcstat - Display SELinux avc statistics.
 *
 * Copyright (C) 2004 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2,
 * as published by the Free Software Foundation.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/limits.h>

#define DEF_STAT_FILE	"/avc/cache_stats"
#define DEF_BUF_SIZE	8192
#define HEADERS		"lookups hits misses allocations reclaims frees"

struct avc_cache_stats {
	unsigned long long lookups;
	unsigned long long hits;
	unsigned long long misses;
	unsigned long long allocations;
	unsigned long long reclaims;
	unsigned long long frees;
};

static int interval;
static int rows;
static char *progname;
static char buf[DEF_BUF_SIZE];

/* selinuxfs mount point */
extern char *selinux_mnt;

static __attribute__((__format__(printf,1,2),__noreturn__)) void die(const char *msg, ...)
{
	va_list args;

	fputs("ERROR: ", stderr);

	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);

	if (errno)
		fprintf(stderr, ": %s", strerror(errno));

	fputc('\n', stderr);
	exit(1);
}

static void usage(void)
{
	printf("\nUsage: %s [-c] [-f status_file] [interval]\n\n", progname);
	printf
	    ("Display SELinux AVC statistics.  If the interval parameter is specified, the\n");
	printf
	    ("program will loop, displaying updated statistics every \'interval\' seconds.\n");
	printf
	    ("Relative values are displayed by default. Use the -c option to specify the\n");
	printf
	    ("display of cumulative values.  The -f option specifies the location of the\n");
	printf("AVC statistics file, defaulting to \'%s%s\'.\n\n", selinux_mnt,
	       DEF_STAT_FILE);
}

static void set_window_rows(void)
{
	int ret;
	struct winsize ws;

	ret = ioctl(fileno(stdout), TIOCGWINSZ, &ws);
	if (ret < 0 || ws.ws_row < 3)
		ws.ws_row = 24;
	rows = ws.ws_row;
}

static void sighandler(int num)
{
	if (num == SIGWINCH)
		set_window_rows();
}

int main(int argc, char **argv)
{
	struct avc_cache_stats tot, rel, last;
	int fd, i, cumulative = 0;
	struct sigaction sa;
	char avcstatfile[PATH_MAX];
	snprintf(avcstatfile, sizeof avcstatfile, "%s%s", selinux_mnt,
		 DEF_STAT_FILE);
	progname = basename(argv[0]);

	memset(&last, 0, sizeof(last));

	while ((i = getopt(argc, argv, "cf:h?-")) != -1) {
		switch (i) {
		case 'c':
			cumulative = 1;
			break;
		case 'f':
			strncpy(avcstatfile, optarg, sizeof(avcstatfile) - 1);
			avcstatfile[sizeof(avcstatfile)-1] = '\0';
			break;
		case 'h':
		case '-':
			usage();
			exit(EXIT_SUCCESS);
		default:
			usage();
			die("unrecognized parameter '%c'", i);
		}
	}

	if (optind < argc) {
		char *arg = argv[optind];
		unsigned int n = strtoul(arg, NULL, 10);

		if (errno == ERANGE) {
			usage();
			die("invalid interval \'%s\'", arg);
		}
		if (n == 0) {
			usage();
			exit(EXIT_SUCCESS);
		}
		interval = n;
	}

	sa.sa_handler = sighandler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);

	i = sigaction(SIGWINCH, &sa, NULL);
	if (i < 0)
		die("sigaction");

	set_window_rows();
	fd = open(avcstatfile, O_RDONLY);
	if (fd < 0)
		die("open: \'%s\'", avcstatfile);

	for (i = 0;; i++) {
		char *line;
		ssize_t ret, parsed = 0;

		memset(buf, 0, DEF_BUF_SIZE);
		ret = read(fd, buf, DEF_BUF_SIZE-1);
		if (ret < 0)
			die("read");

		if (ret == 0)
			die("read: \'%s\': unexpected end of file",
			    avcstatfile);

		line = strtok(buf, "\n");
		if (!line)
			die("unable to parse \'%s\': end of line not found",
			    avcstatfile);

		if (strcmp(line, HEADERS))
			die("unable to parse \'%s\': invalid headers",
			    avcstatfile);

		if (!i || !(i % (rows - 2)))
			printf("%10s %10s %10s %10s %10s %10s\n", "lookups",
			       "hits", "misses", "allocs", "reclaims", "frees");

		memset(&tot, 0, sizeof(tot));

		while ((line = strtok(NULL, "\n"))) {
			struct avc_cache_stats tmp;

			ret = sscanf(line, "%llu %llu %llu %llu %llu %llu",
				     &tmp.lookups,
				     &tmp.hits,
				     &tmp.misses,
				     &tmp.allocations,
				     &tmp.reclaims, &tmp.frees);
			if (ret != 6)
				die("unable to parse \'%s\': scan error",
				    avcstatfile);

			tot.lookups += tmp.lookups;
			tot.hits += tmp.hits;
			tot.misses += tmp.misses;
			tot.allocations += tmp.allocations;
			tot.reclaims += tmp.reclaims;
			tot.frees += tmp.frees;
			parsed = 1;
		}

		if (!parsed)
			die("unable to parse \'%s\': no data", avcstatfile);

		if (cumulative || !i)
			printf("%10llu %10llu %10llu %10llu %10llu %10llu\n",
			       tot.lookups, tot.hits, tot.misses,
			       tot.allocations, tot.reclaims, tot.frees);
		else {
			rel.lookups = tot.lookups - last.lookups;
			rel.hits = tot.hits - last.hits;
			rel.misses = tot.misses - last.misses;
			rel.allocations = tot.allocations - last.allocations;
			rel.reclaims = tot.reclaims - last.reclaims;
			rel.frees = tot.frees - last.frees;
			printf("%10llu %10llu %10llu %10llu %10llu %10llu\n",
			       rel.lookups, rel.hits, rel.misses,
			       rel.allocations, rel.reclaims, rel.frees);
		}

		if (!interval)
			break;

		memcpy(&last, &tot, sizeof(last));
		sleep(interval);

		ret = lseek(fd, 0, 0);
		if (ret < 0)
			die("lseek");
	}

	close(fd);
	return 0;
}
