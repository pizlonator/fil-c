/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2012-2023 Karel Zak <kzak@redhat.com>
 *
 * Original implementation from Linux 0.99, without License and copyright in
 * the code. Karel Zak rewrote the code under GPL-2.0-or-later.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>
#include <ctype.h>

#include <libsmartcols.h>

#include "c.h"
#include "nls.h"
#include "bitops.h"
#include "blkdev.h"
#include "pathnames.h"
#include "xalloc.h"
#include "strutils.h"
#include "optutils.h"
#include "closestream.h"

#include "swapheader.h"
#include "swapprober.h"
#include "swapon-common.h"

#ifdef HAVE_SYS_SWAP_H
# include <sys/swap.h>
#endif

#ifndef SWAP_FLAG_DISCARD
# define SWAP_FLAG_DISCARD	0x10000 /* enable discard for swap */
#endif

#ifndef SWAP_FLAG_DISCARD_ONCE
# define SWAP_FLAG_DISCARD_ONCE 0x20000 /* discard swap area at swapon-time */
#endif

#ifndef SWAP_FLAG_DISCARD_PAGES
# define SWAP_FLAG_DISCARD_PAGES 0x40000 /* discard page-clusters after use */
#endif

#define SWAP_FLAGS_DISCARD_VALID (SWAP_FLAG_DISCARD | SWAP_FLAG_DISCARD_ONCE | \
				  SWAP_FLAG_DISCARD_PAGES)

#ifndef SWAP_FLAG_PREFER
# define SWAP_FLAG_PREFER	0x8000	/* set if swap priority specified */
#endif

#ifndef SWAP_FLAG_PRIO_MASK
# define SWAP_FLAG_PRIO_MASK	0x7fff
#endif

#ifndef SWAP_FLAG_PRIO_SHIFT
# define SWAP_FLAG_PRIO_SHIFT	0
#endif

#if !defined(HAVE_SWAPON) && defined(SYS_swapon)
# include <sys/syscall.h>
# define swapon(path, flags) syscall(SYS_swapon, path, flags)
#endif

#define MAX_PAGESIZE	(64 * 1024)

#ifndef UUID_STR_LEN
# define UUID_STR_LEN	37
#endif

enum {
	SIG_SWAPSPACE = 1,
	SIG_SWSUSPEND
};

/* column names */
struct colinfo {
        const char * const	name; /* header */
        double			whint; /* width hint (N < 1 is in percent of termwidth) */
	int			flags; /* SCOLS_FL_* */
        const char		*help;
};

enum {
	COL_PATH,
	COL_TYPE,
	COL_SIZE,
	COL_USED,
	COL_PRIO,
	COL_UUID,
	COL_LABEL
};
static const struct colinfo infos[] = {
	[COL_PATH]     = { "NAME",	0.20, 0, N_("device file or partition path") },
	[COL_TYPE]     = { "TYPE",	0.20, SCOLS_FL_TRUNC, N_("type of the device")},
	[COL_SIZE]     = { "SIZE",	0.20, SCOLS_FL_RIGHT, N_("size of the swap area")},
	[COL_USED]     = { "USED",	0.20, SCOLS_FL_RIGHT, N_("bytes in use")},
	[COL_PRIO]     = { "PRIO",	0.20, SCOLS_FL_RIGHT, N_("swap priority")},
	[COL_UUID]     = { "UUID",	0.20, 0, N_("swap uuid")},
	[COL_LABEL]    = { "LABEL",	0.20, 0, N_("swap label")},
};


/* swap area properties */
struct swap_prop {
	int discard;			/* discard policy */
	int priority;			/* non-prioritized swap by default */
	int no_fail;			/* skip device if not exist */
};

/* device description */
struct swap_device {
	const char *path;		/* device or file to be turned on */
	const char *label;		/* swap label */
	const char *uuid;		/* unique identifier */
	unsigned int pagesize;
};

/* control struct */
struct swapon_ctl {
	int columns[ARRAY_SIZE(infos) * 2];	/* --show columns */
	int ncolumns;				/* number of columns */

	struct swap_prop props;		/* global settings for all devices */

	unsigned int
		all:1,			/* turn on all swap devices */
		bytes:1,		/* display --show in bytes */
		fix_page_size:1,	/* reinitialize page size */
		no_heading:1,		/* toggle --show headers */
		raw:1,			/* toggle --show alignment */
		show:1,			/* display --show information */
		verbose:1;		/* be chatty */
};

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	assert(name);

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static inline int get_column_id(const struct swapon_ctl *ctl, int num)
{
	assert(num < ctl->ncolumns);
	assert(ctl->columns[num] < (int) ARRAY_SIZE(infos));

	return ctl->columns[num];
}

static inline const struct colinfo *get_column_info(const struct swapon_ctl *ctl, unsigned num)
{
	return &infos[get_column_id(ctl, num)];
}

static void add_scols_line(const struct swapon_ctl *ctl, struct libscols_table *table, struct libmnt_fs *fs)
{
	int i;
	struct libscols_line *line;
	blkid_probe pr = NULL;
	const char *data;

	assert(table);
	assert(fs);

	line = scols_table_new_line(table, NULL);
	if (!line)
		err(EXIT_FAILURE, _("failed to allocate output line"));

	data = mnt_fs_get_source(fs);
	if (access(data, R_OK) == 0)
		pr = get_swap_prober(data);
	for (i = 0; i < ctl->ncolumns; i++) {
		char *str = NULL;
		off_t size;

		switch (get_column_id(ctl, i)) {
		case COL_PATH:
			xasprintf(&str, "%s", mnt_fs_get_source(fs));
			break;
		case COL_TYPE:
			xasprintf(&str, "%s", mnt_fs_get_swaptype(fs));
			break;
		case COL_SIZE:
			size = mnt_fs_get_size(fs);
			size *= 1024;	/* convert to bytes */
			if (ctl->bytes)
				xasprintf(&str, "%jd", size);
			else
				str = size_to_human_string(SIZE_SUFFIX_1LETTER, size);
			break;
		case COL_USED:
			size = mnt_fs_get_usedsize(fs);
			size *= 1024;	/* convert to bytes */
			if (ctl->bytes)
				xasprintf(&str, "%jd", size);
			else
				str = size_to_human_string(SIZE_SUFFIX_1LETTER, size);
			break;
		case COL_PRIO:
			xasprintf(&str, "%d", mnt_fs_get_priority(fs));
			break;
		case COL_UUID:
			if (pr && !blkid_probe_lookup_value(pr, "UUID", &data, NULL))
				xasprintf(&str, "%s", data);
			break;
		case COL_LABEL:
			if (pr && !blkid_probe_lookup_value(pr, "LABEL", &data, NULL))
				xasprintf(&str, "%s", data);
			break;
		default:
			break;
		}

		if (str && scols_line_refer_data(line, i, str))
			err(EXIT_FAILURE, _("failed to add output data"));
	}
	if (pr)
		blkid_free_probe(pr);
}

static int display_summary(void)
{
	struct libmnt_table *st = get_swaps();
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;

	if (!st)
		return -1;

	if (mnt_table_is_empty(st))
		return 0;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		err(EXIT_FAILURE, _("failed to initialize libmount iterator"));

	/* TRANSLATORS: The tabs make each field a multiple of 8 characters. Keep aligned with each entry below. */
	printf(_("Filename\t\t\t\tType\t\tSize\t\tUsed\t\tPriority\n"));

	while (mnt_table_next_fs(st, itr, &fs) == 0) {
		const char *src = mnt_fs_get_source(fs);
		const char *type = mnt_fs_get_swaptype(fs);
		int srclen = strlen(src);
		int typelen = strlen(type);
		off_t size = mnt_fs_get_size(fs);
		off_t used = mnt_fs_get_usedsize(fs);

		/* TRANSLATORS: Keep each field a multiple of 8 characters and aligned with the header above. */
		printf("%s%*s%s%s\t%jd%s\t%jd%s\t%d\n",
			src,
			srclen < 40 ? 40 - srclen : 1, " ",
			type,
			typelen < 8 ? "\t" : "",
			size,
			size < 10000000 ? "\t" : "",
			used,
			used < 10000000 ? "\t" : "",
			mnt_fs_get_priority(fs));
	}

	mnt_free_iter(itr);
	return 0;
}

static int show_table(struct swapon_ctl *ctl)
{
	struct libmnt_table *st = get_swaps();
	struct libmnt_iter *itr = NULL;
	struct libmnt_fs *fs;
	int i;
	struct libscols_table *table = NULL;

	if (!st)
		return -1;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		err(EXIT_FAILURE, _("failed to initialize libmount iterator"));

	scols_init_debug(0);

	table = scols_new_table();
	if (!table)
		err(EXIT_FAILURE, _("failed to allocate output table"));

	scols_table_enable_raw(table, ctl->raw);
	scols_table_enable_noheadings(table, ctl->no_heading);

	for (i = 0; i < ctl->ncolumns; i++) {
		const struct colinfo *col = get_column_info(ctl, i);

		if (!scols_table_new_column(table, col->name, col->whint, col->flags))
			err(EXIT_FAILURE, _("failed to allocate output column"));
	}

	while (mnt_table_next_fs(st, itr, &fs) == 0)
		add_scols_line(ctl, table, fs);

	scols_print_table(table);
	scols_unref_table(table);
	mnt_free_iter(itr);
	return 0;
}

/* calls mkswap */
static int swap_reinitialize(struct swap_device *dev)
{
	pid_t pid;
	int status, ret;
	char const *cmd[7];
	int idx=0;

	assert(dev);
	assert(dev->path);

	warnx(_("%s: reinitializing the swap."), dev->path);

	switch ((pid=fork())) {
	case -1: /* fork error */
		warn(_("fork failed"));
		return -1;

	case 0:	/* child */
		if (geteuid() != getuid() && drop_permissions() != 0)
			exit(EXIT_FAILURE);

		cmd[idx++] = "mkswap";
		if (dev->label) {
			cmd[idx++] = "-L";
			cmd[idx++] = dev->label;
		}
		if (dev->uuid) {
			cmd[idx++] = "-U";
			cmd[idx++] = dev->uuid;
		}
		cmd[idx++] = dev->path;
		cmd[idx++] = NULL;
		execvp(cmd[0], (char * const *) cmd);
		errexec(cmd[0]);

	default: /* parent */
		do {
			ret = waitpid(pid, &status, 0);
		} while (ret == -1 && errno == EINTR);

		if (ret < 0) {
			warn(_("waitpid failed"));
			return -1;
		}

		/* mkswap returns: 0=suss, >0 error */
		if (WIFEXITED(status) && WEXITSTATUS(status)==0)
			return 0; /* ok */
		break;
	}
	return -1; /* error */
}

/* Replaces unwanted SWSUSPEND signature with swap signature */
static int swap_rewrite_signature(const struct swap_device *dev)
{
	int fd, rc = -1;

	assert(dev);
	assert(dev->path);
	assert(dev->pagesize);

	fd = open(dev->path, O_WRONLY);
	if (fd == -1) {
		warn(_("cannot open %s"), dev->path);
		return -1;
	}

	if (lseek(fd, dev->pagesize - SWAP_SIGNATURE_SZ, SEEK_SET) < 0) {
		warn(_("%s: lseek failed"), dev->path);
		goto err;
	}

	if (write(fd, (void *) SWAP_SIGNATURE,
			SWAP_SIGNATURE_SZ) != SWAP_SIGNATURE_SZ) {
		warn(_("%s: write signature failed"), dev->path);
		goto err;
	}

	rc  = 0;
err:
	if (close_fd(fd) != 0) {
		warn(_("write failed: %s"), dev->path);
		rc = -1;
	}
	return rc;
}

static int swap_detect_signature(const char *buf, int *sig)
{
	assert(buf);
	assert(sig);

	if (memcmp(buf, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ) == 0)
		*sig = SIG_SWAPSPACE;

	else if (memcmp(buf, "S1SUSPEND", 9) == 0 ||
		 memcmp(buf, "S2SUSPEND", 9) == 0 ||
		 memcmp(buf, "ULSUSPEND", 9) == 0 ||
		 memcmp(buf, "\xed\xc3\x02\xe9\x98\x56\xe5\x0c", 8) == 0 ||
		 memcmp(buf, "LINHIB0001", 10) == 0)
		*sig = SIG_SWSUSPEND;
	else
		return 0;

	return 1;
}

static char *swap_get_header(int fd, int *sig, unsigned int *pagesize)
{
	char *buf;
	ssize_t datasz;
	unsigned int page;

	assert(sig);
	assert(pagesize);

	*pagesize = 0;
	*sig = 0;

	buf = xmalloc(MAX_PAGESIZE);

	datasz = read(fd, buf, MAX_PAGESIZE);
	if (datasz == (ssize_t) -1)
		goto err;

	for (page = 0x1000; page <= MAX_PAGESIZE; page <<= 1) {
		/* skip 32k pagesize since this does not seem to
		 * be supported */
		if (page == 0x8000)
			continue;
		/* the smallest swap area is PAGE_SIZE*10, it means
		 * 40k, that's less than MAX_PAGESIZE */
		if (datasz < 0 || (size_t) datasz < (page - SWAP_SIGNATURE_SZ))
			break;
		if (swap_detect_signature(buf + page - SWAP_SIGNATURE_SZ, sig)) {
			*pagesize = page;
			break;
		}
	}

	if (*pagesize)
		return buf;
err:
	free(buf);
	return NULL;
}

/* returns real size of swap space */
static unsigned long long swap_get_size(const struct swap_device *dev,
					const char *hdr)
{
	unsigned int last_page = 0;
	const unsigned int swap_version = SWAP_VERSION;
	const struct swap_header_v1_2 *s;

	assert(dev);
	assert(dev->pagesize > 0);

	s = (const struct swap_header_v1_2 *) hdr;

	if (s->version == swap_version)
		last_page = s->last_page;
	else if (swab32(s->version) == swap_version)
		last_page = swab32(s->last_page);

	return ((unsigned long long) last_page + 1) * dev->pagesize;
}

static void swap_get_info(struct swap_device *dev, const char *hdr)
{
	const struct swap_header_v1_2 *s = (const struct swap_header_v1_2 *) hdr;

	assert(dev);

	if (s && *s->volume_name)
		dev->label = xstrdup(s->volume_name);

	if (s && *s->uuid) {
		const unsigned char *u = s->uuid;
		char str[UUID_STR_LEN];

		snprintf(str, sizeof(str),
			"%02x%02x%02x%02x-"
			"%02x%02x-%02x%02x-"
			"%02x%02x-%02x%02x%02x%02x%02x%02x",
			u[0], u[1], u[2], u[3],
			u[4], u[5], u[6], u[7],
			u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
		dev->uuid = xstrdup(str);
	}
}

static int swapon_checks(const struct swapon_ctl *ctl, struct swap_device *dev)
{
	struct stat st;
	int fd, sig;
	char *hdr = NULL;
	unsigned long long devsize = 0;
	int permMask;

	assert(ctl);
	assert(dev);
	assert(dev->path);

	fd = open(dev->path, O_RDONLY);
	if (fd == -1) {
		warn(_("cannot open %s"), dev->path);
		goto err;
	}

	if (fstat(fd, &st) < 0) {
		warn(_("stat of %s failed"), dev->path);
		goto err;
	}

	permMask = S_ISBLK(st.st_mode) ? 07007 : 07077;
	if ((st.st_mode & permMask) != 0)
		warnx(_("%s: insecure permissions %04o, %04o suggested."),
				dev->path, st.st_mode & 07777,
				~permMask & 0666);

	if (S_ISREG(st.st_mode) && st.st_uid != 0)
		warnx(_("%s: insecure file owner %d, 0 (root) suggested."),
				dev->path, st.st_uid);

	/* test for holes by LBT */
	if (S_ISREG(st.st_mode)) {
		if (st.st_blocks * 512L < st.st_size) {
			warnx(_("%s: skipping - it appears to have holes."),
				dev->path);
			goto err;
		}
		devsize = st.st_size;
	}

	if (S_ISBLK(st.st_mode) && blkdev_get_size(fd, &devsize)) {
		warnx(_("%s: get size failed"), dev->path);
		goto err;
	}

	hdr = swap_get_header(fd, &sig, &dev->pagesize);
	if (!hdr) {
		warnx(_("%s: read swap header failed"), dev->path);
		goto err;
	}

	if (ctl->verbose)
		warnx(_("%s: found signature [pagesize=%d, signature=%s]"),
			dev->path,
			dev->pagesize,
			sig == SIG_SWAPSPACE ? "swap" :
			sig == SIG_SWSUSPEND ? "suspend" : "unknown");

	if (sig == SIG_SWAPSPACE && dev->pagesize) {
		unsigned long long swapsize = swap_get_size(dev, hdr);
		int syspg = getpagesize();

		if (ctl->verbose)
			warnx(_("%s: pagesize=%d, swapsize=%llu, devsize=%llu"),
				dev->path, dev->pagesize, swapsize, devsize);

		if (swapsize > devsize) {
			if (ctl->verbose)
				warnx(_("%s: last_page 0x%08llx is larger"
					" than actual size of swapspace"),
					dev->path, swapsize);

		} else if (syspg < 0 || (unsigned int) syspg != dev->pagesize) {
			if (ctl->fix_page_size) {
				int rc;

				swap_get_info(dev, hdr);

				warnx(_("%s: swap format pagesize does not match."),
					dev->path);
				rc = swap_reinitialize(dev);
				if (rc < 0)
					goto err;
			} else
				warnx(_("%s: swap format pagesize does not match. "
					"(Use --fixpgsz to reinitialize it.)"),
					dev->path);
		}
	} else if (sig == SIG_SWSUSPEND) {
		/* We have to reinitialize swap with old (=useless) software suspend
		 * data. The problem is that if we don't do it, then we get data
		 * corruption the next time an attempt at unsuspending is made.
		 */
		warnx(_("%s: software suspend data detected. "
				"Rewriting the swap signature."),
			dev->path);
		if (swap_rewrite_signature(dev) < 0)
			goto err;
	}

	free(hdr);
	close(fd);
	return 0;
err:
	if (fd != -1)
		close(fd);
	free(hdr);
	return -1;
}

static int do_swapon(const struct swapon_ctl *ctl,
		     const struct swap_prop *prop,
		     const char *spec,
		     int canonic)
{
	struct swap_device dev = { .path = NULL };
	int status;
	int flags = 0;
	int priority;

	assert(ctl);
	assert(prop);

	if (!canonic) {
		dev.path = mnt_resolve_spec(spec, mntcache);
		if (!dev.path)
			return cannot_find(spec);
	} else
		dev.path = spec;

	priority = prop->priority;

	if (swapon_checks(ctl, &dev))
		return -1;

#ifdef SWAP_FLAG_PREFER
	if (priority >= 0) {
		if (priority > SWAP_FLAG_PRIO_MASK)
			priority = SWAP_FLAG_PRIO_MASK;

		flags = SWAP_FLAG_PREFER
			| ((priority & SWAP_FLAG_PRIO_MASK)
			   << SWAP_FLAG_PRIO_SHIFT);
	}
#endif
	/*
	 * Validate the discard flags passed and set them
	 * accordingly before calling sys_swapon.
	 */
	if (prop->discard && !(prop->discard & ~SWAP_FLAGS_DISCARD_VALID)) {
		/*
		 * If we get here with both discard policy flags set,
		 * we just need to tell the kernel to enable discards
		 * and it will do correctly, just as we expect.
		 */
		if ((prop->discard & SWAP_FLAG_DISCARD_ONCE) &&
		    (prop->discard & SWAP_FLAG_DISCARD_PAGES))
			flags |= SWAP_FLAG_DISCARD;
		else
			flags |= prop->discard;
	}

	if (ctl->verbose)
		printf(_("swapon %s\n"), dev.path);

	status = swapon(dev.path, flags);
	if (status < 0)
		warn(_("%s: swapon failed"), dev.path);

	return status;
}

static int swapon_by_label(struct swapon_ctl *ctl, const char *label)
{
	char *device = mnt_resolve_tag("LABEL", label, mntcache);
	return device ? do_swapon(ctl, &ctl->props, device, TRUE) :  cannot_find(label);
}

static int swapon_by_uuid(struct swapon_ctl *ctl, const char *uuid)
{
	char *device = mnt_resolve_tag("UUID", uuid, mntcache);
	return device ? do_swapon(ctl, &ctl->props, device, TRUE) : cannot_find(uuid);
}

/* -o <options> or fstab */
static int parse_options(struct swap_prop *props, const char *options)
{
	char *arg = NULL;
	size_t argsz = 0;

	assert(props);
	assert(options);

	if (mnt_optstr_get_option(options, "nofail", NULL, NULL) == 0)
		props->no_fail = 1;

	if (mnt_optstr_get_option(options, "discard", &arg, &argsz) == 0) {
		props->discard |= SWAP_FLAG_DISCARD;

		if (arg) {
			/* only single-time discards are wanted */
			if (strncmp(arg, "once", argsz) == 0)
				props->discard |= SWAP_FLAG_DISCARD_ONCE;

			/* do discard for every released swap page */
			if (strncmp(arg, "pages", argsz) == 0)
				props->discard |= SWAP_FLAG_DISCARD_PAGES;
		}
	}

	arg = NULL;
	if (mnt_optstr_get_option(options, "pri", &arg, &argsz) == 0 && arg) {
		char *end = NULL;
		int n;

		errno = 0;
		n = (int) strtol(arg, &end, 10);
		if (errno == 0 && end && end > arg)
			props->priority = n;
	}
	return 0;
}


static int swapon_all(struct swapon_ctl *ctl, const char *filename)
{
	struct libmnt_table *tb = get_fstab(filename);
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int status = 0;

	if (!tb)
		err(EXIT_FAILURE, _("failed to parse %s"), mnt_get_fstab_path());

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		err(EXIT_FAILURE, _("failed to initialize libmount iterator"));

	while (mnt_table_find_next_fs(tb, itr, match_swap, NULL, &fs) == 0) {
		/* defaults */
		const char *opts;
		const char *device;
		struct swap_prop prop;		/* per device setting */

		if (mnt_fs_get_option(fs, "noauto", NULL, NULL) == 0) {
			if (ctl->verbose)
				warnx(_("%s: noauto option -- ignored"), mnt_fs_get_source(fs));
			continue;
		}

		/* default setting */
		prop = ctl->props;

		/* overwrite default by setting from fstab */
		opts = mnt_fs_get_options(fs);
		if (opts)
			parse_options(&prop, opts);

		/* convert LABEL=, UUID= etc. from fstab to device name */
		device = mnt_resolve_spec(mnt_fs_get_source(fs), mntcache);
		if (!device) {
			if (!prop.no_fail)
				status |= cannot_find(mnt_fs_get_source(fs));
			continue;
		}

		if (is_active_swap(device)) {
			if (ctl->verbose)
				warnx(_("%s: already active -- ignored"), device);
			continue;
		}

		if (prop.no_fail && access(device, R_OK) != 0) {
			if (ctl->verbose)
				warnx(_("%s: inaccessible -- ignored"), device);
			continue;
		}

		/* swapon */
		status |= do_swapon(ctl, &prop, device, TRUE);
	}

	mnt_free_iter(itr);
	return status;
}


static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [<spec>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Enable devices and files for paging and swapping.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all                enable all swaps from /etc/fstab\n"), out);
	fputs(_(" -d, --discard[=<policy>] enable swap discards, if supported by device\n"), out);
	fputs(_(" -e, --ifexists           silently skip devices that do not exist\n"), out);
	fputs(_(" -f, --fixpgsz            reinitialize the swap space if necessary\n"), out);
	fputs(_(" -o, --options <list>     comma-separated list of swap options\n"), out);
	fputs(_(" -p, --priority <prio>    specify the priority of the swap device\n"), out);
	fputs(_(" -s, --summary            display summary about used swap devices (DEPRECATED)\n"), out);
	fputs(_(" -T, --fstab <path>       alternative file to /etc/fstab\n"), out);
	fputs(_("     --show[=<columns>]   display summary in definable table\n"), out);
	fputs(_("     --noheadings         don't print table heading (with --show)\n"), out);
	fputs(_("     --raw                use the raw output format (with --show)\n"), out);
	fputs(_("     --bytes              display swap size in bytes in --show output\n"), out);
	fputs(_(" -v, --verbose            verbose mode\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));

	fputs(_("\nThe <spec> parameter:\n" \
		" -L <label>             synonym for LABEL=<label>\n"
		" -U <uuid>              synonym for UUID=<uuid>\n"
		" LABEL=<label>          specifies device by swap area label\n"
		" UUID=<uuid>            specifies device by swap area UUID\n"
		" PARTLABEL=<label>      specifies device by partition label\n"
		" PARTUUID=<uuid>        specifies device by partition UUID\n"
		" <device>               name of device to be used\n"
		" <file>                 name of file to be used\n"), out);

	fputs(_("\nAvailable discard policy types (for --discard):\n"
		" once    : only single-time area discards are issued\n"
		" pages   : freed pages are discarded before they are reused\n"
		"If no policy is selected, both discard types are enabled (default).\n"), out);

	fputs(USAGE_COLUMNS, out);
	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(out, " %-5s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, USAGE_MAN_TAIL("swapon(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	int status = 0, c;
	size_t i;
	char *options = NULL, *fstab_filename = NULL;

	enum {
		BYTES_OPTION = CHAR_MAX + 1,
		NOHEADINGS_OPTION,
		RAW_OPTION,
		SHOW_OPTION,
		OPT_LIST_TYPES
	};

	static const struct option long_opts[] = {
		{ "priority",   required_argument, NULL, 'p'               },
		{ "discard",    optional_argument, NULL, 'd'               },
		{ "ifexists",   no_argument,       NULL, 'e'               },
		{ "options",    optional_argument, NULL, 'o'               },
		{ "summary",    no_argument,       NULL, 's'               },
		{ "fixpgsz",    no_argument,       NULL, 'f'               },
		{ "all",        no_argument,       NULL, 'a'               },
		{ "help",       no_argument,       NULL, 'h'               },
		{ "verbose",    no_argument,       NULL, 'v'               },
		{ "version",    no_argument,       NULL, 'V'               },
		{ "show",       optional_argument, NULL, SHOW_OPTION       },
		{ "output-all", no_argument,       NULL, OPT_LIST_TYPES    },
		{ "noheadings", no_argument,       NULL, NOHEADINGS_OPTION },
		{ "raw",        no_argument,       NULL, RAW_OPTION        },
		{ "bytes",      no_argument,       NULL, BYTES_OPTION      },
		{ "fstab",      required_argument, NULL, 'T'               },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'a','o','s', SHOW_OPTION },
		{ 'a','o', BYTES_OPTION },
		{ 'a','o', NOHEADINGS_OPTION },
		{ 'a','o', RAW_OPTION },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	struct swapon_ctl ctl;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	memset(&ctl, 0, sizeof(struct swapon_ctl));
	ctl.props.priority = -1;

	mnt_init_debug(0);
	mntcache = mnt_new_cache();

	while ((c = getopt_long(argc, argv, "ahd::efo:p:svVL:U:T:",
				long_opts, NULL)) != -1) {

		err_exclusive_options(c, long_opts, excl, excl_st);

		switch (c) {
		case 'a':		/* all */
			ctl.all = 1;
			break;
		case 'o':
			options = optarg;
			break;
		case 'p':		/* priority */
			ctl.props.priority = strtos16_or_err(optarg,
					   _("failed to parse priority"));
			break;
		case 'L':
			add_label(optarg);
			break;
		case 'U':
			add_uuid(optarg);
			break;
		case 'T':
			fstab_filename = optarg;
			break;
		case 'd':
			ctl.props.discard |= SWAP_FLAG_DISCARD;
			if (optarg) {
				if (*optarg == '=')
					optarg++;

				if (strcmp(optarg, "once") == 0)
					ctl.props.discard |= SWAP_FLAG_DISCARD_ONCE;
				else if (strcmp(optarg, "pages") == 0)
					ctl.props.discard |= SWAP_FLAG_DISCARD_PAGES;
				else
					errx(EXIT_FAILURE, _("unsupported discard policy: %s"), optarg);
			}
			break;
		case 'e':               /* ifexists */
			ctl.props.no_fail = 1;
			break;
		case 'f':
			ctl.fix_page_size = 1;
			break;
		case 's':		/* status report */
			status = display_summary();
			return status;
		case 'v':		/* be chatty */
			ctl.verbose = 1;
			break;
		case SHOW_OPTION:
			if (optarg) {
				ctl.ncolumns = string_to_idarray(optarg,
							     ctl.columns,
							     ARRAY_SIZE(ctl.columns),
							     column_name_to_id);
				if (ctl.ncolumns < 0)
					return EXIT_FAILURE;
			}
			ctl.show = 1;
			break;
		case OPT_LIST_TYPES:
			for (ctl.ncolumns = 0; (size_t)ctl.ncolumns < ARRAY_SIZE(infos); ctl.ncolumns++)
				ctl.columns[ctl.ncolumns] = ctl.ncolumns;
			break;
		case NOHEADINGS_OPTION:
			ctl.no_heading = 1;
			break;
		case RAW_OPTION:
			ctl.raw = 1;
			break;
		case BYTES_OPTION:
			ctl.bytes = 1;
			break;
		case 0:
			break;

		case 'h':		/* help */
			usage();
		case 'V':		/* version */
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}
	argv += optind;

	if (ctl.show || (!ctl.all && !numof_labels() && !numof_uuids() && *argv == NULL)) {
		if (!ctl.ncolumns) {
			/* default columns */
			ctl.columns[ctl.ncolumns++] = COL_PATH;
			ctl.columns[ctl.ncolumns++] = COL_TYPE;
			ctl.columns[ctl.ncolumns++] = COL_SIZE;
			ctl.columns[ctl.ncolumns++] = COL_USED;
			ctl.columns[ctl.ncolumns++] = COL_PRIO;
		}
		status = show_table(&ctl);
		return status;
	}

	if (ctl.props.no_fail && !ctl.all) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	if (ctl.all)
		status |= swapon_all(&ctl, fstab_filename);

	if (options)
		parse_options(&ctl.props, options);

	for (i = 0; i < numof_labels(); i++)
		status |= swapon_by_label(&ctl, get_label(i));

	for (i = 0; i < numof_uuids(); i++)
		status |= swapon_by_uuid(&ctl, get_uuid(i));

	while (*argv != NULL)
		status |= do_swapon(&ctl, &ctl.props, *argv++, FALSE);

	free_tables();
	mnt_unref_cache(mntcache);

	return status;
}
