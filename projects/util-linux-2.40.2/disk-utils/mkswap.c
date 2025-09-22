/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mkswap.c - set up a linux swap device
 *
 * Copyright (C) 1991 Linus Torvalds
 *               20.12.91 - time began. Got VM working yesterday by doing this by hand.
 *
 * Copyright (C) 1999 Jakub Jelinek <jj@ultra.linux.cz>
 * Copyright (C) 2007-2014 Karel Zak <kzak@redhat.com>
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>
#ifdef HAVE_LIBSELINUX
# include <selinux/selinux.h>
# include <selinux/context.h>
# include "selinux-utils.h"
#endif
#ifdef HAVE_LINUX_FIEMAP_H
# include <linux/fs.h>
# include <linux/fiemap.h>
#endif

#include "linux_version.h"
#include "swapheader.h"
#include "strutils.h"
#include "nls.h"
#include "blkdev.h"
#include "pathnames.h"
#include "all-io.h"
#include "xalloc.h"
#include "c.h"
#include "closestream.h"
#include "ismounted.h"
#include "optutils.h"
#include "bitops.h"

#ifdef HAVE_LIBUUID
# include <uuid.h>
#endif

#ifdef HAVE_LIBBLKID
# include <blkid.h>
#endif

#define MIN_GOODPAGES	10

#define SELINUX_SWAPFILE_TYPE	"swapfile_t"

enum ENDIANNESS {
	ENDIANNESS_NATIVE,
	ENDIANNESS_LITTLE,
	ENDIANNESS_BIG,
};

struct mkswap_control {
	struct swap_header_v1_2	*hdr;		/* swap header */
	void			*signature_page;/* buffer with swap header */

	char			*devname;	/* device or file name */
	const char		*lockmode;	/* as specified by --lock */
	struct stat		devstat;	/* stat() result */
	int			fd;		/* swap file descriptor */

	unsigned long long	npages;		/* number of pages */
	unsigned long		nbadpages;	/* number of bad pages */

	int			user_pagesize;	/* --pagesize */
	int			pagesize;	/* final pagesize used for the header */
	off_t			offset;         /* offset of the header in the target */

	char			*opt_label;	/* LABEL as specified on command line */
	unsigned char		*uuid;		/* UUID parsed by libbuuid */

	unsigned long long	filesz;		/* desired swap file size */

	size_t			nbad_extents;

	enum ENDIANNESS         endianness;

	unsigned int		check:1,	/* --check */
				verbose:1,      /* --verbose */
				quiet:1,        /* --quiet */
				force:1,	/* --force */
				file:1;		/* --file */
};

static uint32_t cpu32_to_endianness(uint32_t v, enum ENDIANNESS e)
{
	switch (e) {
		case ENDIANNESS_NATIVE: return v;
		case ENDIANNESS_LITTLE: return cpu_to_le32(v);
		case ENDIANNESS_BIG: return cpu_to_be32(v);
	}
	abort();
}

static void init_signature_page(struct mkswap_control *ctl)
{
	const int kernel_pagesize = getpagesize();

	if (ctl->user_pagesize) {
		if (ctl->user_pagesize < 0 || !is_power_of_2(ctl->user_pagesize) ||
		    (size_t) ctl->user_pagesize < sizeof(struct swap_header_v1_2) + 10)
			errx(EXIT_FAILURE,
			     _("Bad user-specified page size %u"),
			       ctl->user_pagesize);
		if (!ctl->quiet && ctl->user_pagesize != kernel_pagesize)
			warnx(_("Using user-specified page size %d, "
				"instead of the system value %d"),
				ctl->user_pagesize, kernel_pagesize);
		ctl->pagesize = ctl->user_pagesize;
	} else
		ctl->pagesize = kernel_pagesize;

	ctl->signature_page = xcalloc(1, ctl->pagesize);
	ctl->hdr = (struct swap_header_v1_2 *) ctl->signature_page;
}

static void deinit_signature_page(struct mkswap_control *ctl)
{
	free(ctl->signature_page);

	ctl->hdr = NULL;
	ctl->signature_page = NULL;
}

static void set_signature(const struct mkswap_control *ctl)
{
	char *sp = (char *) ctl->signature_page;

	assert(sp);
	memcpy(sp + ctl->pagesize - SWAP_SIGNATURE_SZ, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ);
}

static void set_uuid_and_label(const struct mkswap_control *ctl)
{
	assert(ctl);
	assert(ctl->hdr);

	/* set UUID */
	if (ctl->uuid)
		memcpy(ctl->hdr->uuid, ctl->uuid, sizeof(ctl->hdr->uuid));

	/* set LABEL */
	if (ctl->opt_label) {
		xstrncpy(ctl->hdr->volume_name,
			 ctl->opt_label, sizeof(ctl->hdr->volume_name));
		if (!ctl->quiet
		    && strlen(ctl->opt_label) > strlen(ctl->hdr->volume_name))
			warnx(_("Label was truncated."));
	}

	/* report results */
	if (!ctl->quiet && (ctl->uuid || ctl->opt_label)) {
		if (ctl->opt_label)
			printf("LABEL=%s, ", ctl->hdr->volume_name);
		else
			printf(_("no label, "));
#ifdef HAVE_LIBUUID
		if (ctl->uuid) {
			char uuid_string[UUID_STR_LEN];
			uuid_unparse(ctl->uuid, uuid_string);
			printf("UUID=%s\n", uuid_string);
		} else
#endif
			printf(_("no uuid\n"));
	}
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] device [size]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Set up a Linux swap area.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -c, --check               check bad blocks before creating the swap area\n"), out);
	fputs(_(" -f, --force               allow swap size area be larger than device\n"), out);
	fputs(_(" -q, --quiet               suppress output and warning messages\n"), out);
	fputs(_(" -p, --pagesize SIZE       specify page size in bytes\n"), out);
	fputs(_(" -L, --label LABEL         specify label\n"), out);
	fputs(_(" -v, --swapversion NUM     specify swap-space version number\n"), out);
	fputs(_(" -U, --uuid UUID           specify the uuid to use\n"), out);
	fprintf(out,
	      _(" -e, --endianness=<value>  specify the endianness to use "
	                                    "(%s, %s or %s)\n"), "native", "little", "big");
	fputs(_(" -o, --offset OFFSET       specify the offset in the device\n"), out);
	fputs(_(" -s, --size SIZE           specify the size of a swap file in bytes\n"), out);
	fputs(_(" -F, --file                create a swap file\n"), out);
	fputs(_("     --verbose             verbose output\n"), out);

	fprintf(out,
	      _("     --lock[=<mode>]       use exclusive device lock (%s, %s or %s)\n"), "yes", "no", "nonblock");

	fprintf(out, USAGE_HELP_OPTIONS(27));

	fprintf(out, USAGE_MAN_TAIL("mkswap(8)"));
	exit(EXIT_SUCCESS);
}

static void page_bad(struct mkswap_control *ctl, unsigned int page)
{
	const unsigned long max_badpages =
		(ctl->pagesize - 1024 - 128 * sizeof(int) - 10) / sizeof(int);

	if (ctl->nbadpages == max_badpages)
		errx(EXIT_FAILURE, _("too many bad pages: %lu"), max_badpages);

	ctl->hdr->badpages[ctl->nbadpages] = page;
	ctl->nbadpages++;
}

static void check_blocks(struct mkswap_control *ctl)
{
	unsigned int current_page = 0;
	int do_seek = 1;
	char *buffer;

	assert(ctl);
	assert(ctl->fd > -1);

	buffer = xmalloc(ctl->pagesize);
	while (current_page < ctl->npages) {
		ssize_t rc;
		off_t offset = (off_t) current_page * ctl->pagesize;

		if (do_seek && lseek(ctl->fd, offset, SEEK_SET) != offset)
			errx(EXIT_FAILURE, _("seek failed in check_blocks"));

		rc = read(ctl->fd, buffer, ctl->pagesize);
		do_seek = (rc < 0 || rc != ctl->pagesize);
		if (do_seek)
			page_bad(ctl, current_page);
		current_page++;
	}

	if (!ctl->quiet)
		printf(P_("%lu bad page\n", "%lu bad pages\n", ctl->nbadpages), ctl->nbadpages);
	free(buffer);
}


#ifdef HAVE_LINUX_FIEMAP_H
static void warn_extent(struct mkswap_control *ctl, const char *msg, uint64_t off)
{
	if (ctl->nbad_extents == 0) {
		fputc('\n', stderr);
		fprintf(stderr, _(

	"mkswap: %s contains holes or other unsupported extents.\n"
	"        This swap file can be rejected by kernel on swap activation!\n"),
				ctl->devname);

		if (ctl->verbose)
			fputc('\n', stderr);
		else
			fprintf(stderr, _(
	"        Use --verbose for more details.\n"));

	}
	if (ctl->verbose) {
		fputs(" - ", stderr);
		fprintf(stderr, msg, off);
		fputc('\n', stderr);
	}
	ctl->nbad_extents++;
}

static void check_extents(struct mkswap_control *ctl)
{
	char buf[BUFSIZ] = { 0 };
	struct fiemap *fiemap = (struct fiemap *) buf;
	int last = 0;
	uint64_t last_logical = 0;

	memset(fiemap, 0, sizeof(struct fiemap));

	do {
		int rc;
		size_t n, i;

		fiemap->fm_length = ~0ULL;
		fiemap->fm_flags = FIEMAP_FLAG_SYNC;
		fiemap->fm_extent_count =
			(sizeof(buf) - sizeof(*fiemap)) / sizeof(struct fiemap_extent);

		rc = ioctl(ctl->fd, FS_IOC_FIEMAP, (unsigned long) fiemap);
		if (rc < 0)
			return;

		n = fiemap->fm_mapped_extents;
		if (n == 0)
			break;

		for (i = 0; i < n; i++) {
			struct fiemap_extent *e = &fiemap->fm_extents[i];

			if (e->fe_logical > last_logical)
				warn_extent(ctl, _("hole detected at offset %ju"),
						(uintmax_t) last_logical);

			last_logical = (e->fe_logical + e->fe_length);

			if (e->fe_flags & FIEMAP_EXTENT_LAST)
				last = 1;
			if (e->fe_flags & FIEMAP_EXTENT_DATA_INLINE)
				warn_extent(ctl, _("data inline extent at offset %ju"),
						(uintmax_t) e->fe_logical);
			if (e->fe_flags & FIEMAP_EXTENT_SHARED)
				warn_extent(ctl, _("shared extent at offset %ju"),
						(uintmax_t) e->fe_logical);
			if (e->fe_flags & FIEMAP_EXTENT_DELALLOC)
				warn_extent(ctl, _("unallocated extent at offset %ju"),
						(uintmax_t) e->fe_logical);

			if (!ctl->verbose && ctl->nbad_extents)
				goto done;
		}
		fiemap->fm_start = fiemap->fm_extents[n - 1].fe_logical
				 + fiemap->fm_extents[n - 1].fe_length;
	} while (last == 0);

	if (last_logical < (uint64_t) ctl->devstat.st_size)
		warn_extent(ctl, _("hole detected at offset %ju"),
				(uintmax_t) last_logical);
done:
	if (ctl->nbad_extents)
		fputc('\n', stderr);
}
#endif /* HAVE_LINUX_FIEMAP_H */

/* return size in pages */
static unsigned long long get_size(const struct mkswap_control *ctl)
{
	unsigned long long size;

	if (ctl->file && ctl->filesz)
		size = ctl->filesz;
	else {
		int fd = open(ctl->devname, O_RDONLY);
		if (fd < 0)
			err(EXIT_FAILURE, _("cannot open %s"), ctl->devname);
		if (blkdev_get_size(fd, &size) < 0)
			err(EXIT_FAILURE, _("cannot determine size of %s"), ctl->devname);
		if ((unsigned long long) ctl->offset > size)
			errx(EXIT_FAILURE, _("offset larger than file size"));
		close(fd);
	}
	size -= ctl->offset;
	size /= ctl->pagesize;
	return size;
}

#ifdef HAVE_LIBBLKID
static blkid_probe new_prober(const struct mkswap_control *ctl)
{
	blkid_probe pr = blkid_new_probe();
	if (!pr)
		errx(EXIT_FAILURE, _("unable to alloc new libblkid probe"));
	if (blkid_probe_set_device(pr, ctl->fd, 0, 0))
		errx(EXIT_FAILURE, _("unable to assign device to libblkid probe"));
	return pr;
}
#endif

static void open_device(struct mkswap_control *ctl)
{
	assert(ctl);
	assert(ctl->devname);

	if (ctl->file) {
		if (stat(ctl->devname, &ctl->devstat) == 0) {
			if (!S_ISREG(ctl->devstat.st_mode))
				err(EXIT_FAILURE, _("cannot create swap file %s: node isn't regular file"), ctl->devname);
			if (chmod(ctl->devname, 0600) < 9)
				err(EXIT_FAILURE, _("cannot set permissions on swap file %s"), ctl->devname);
		}
		ctl->fd = open(ctl->devname, O_RDWR | O_CREAT, 0600);
	} else {
		if (stat(ctl->devname, &ctl->devstat) < 0)
			err(EXIT_FAILURE, _("stat of %s failed"), ctl->devname);
		ctl->fd = open_blkdev_or_file(&ctl->devstat, ctl->devname, O_RDWR);
	}
	if (ctl->fd < 0)
		err(EXIT_FAILURE, _("cannot open %s"), ctl->devname);
	if (ctl->file && ctl->filesz) {
		if (ftruncate(ctl->fd, ctl->filesz) < 0)
			err(EXIT_FAILURE, _("couldn't allocate swap file %s"), ctl->devname);
#ifdef HAVE_POSIX_FALLOCATE
		errno = posix_fallocate(ctl->fd, 0, ctl->filesz);
		if (errno)
			err(EXIT_FAILURE, _("couldn't allocate swap file %s"), ctl->devname);
#elif defined(HAVE_FALLOCATE)
		if (fallocate(ctl->fd, 0, 0, ctl->filesz) < 0)
			err(EXIT_FAILURE, _("couldn't allocate swap file %s"), ctl->devname);
#endif
	}

	if (blkdev_lock(ctl->fd, ctl->devname, ctl->lockmode) != 0)
		exit(EXIT_FAILURE);

	if (ctl->check && S_ISREG(ctl->devstat.st_mode)) {
		ctl->check = 0;
		if (!ctl->quiet)
			warnx(_("warning: checking bad blocks from swap file is not supported: %s"),
			       ctl->devname);
	}
}

static void wipe_device(struct mkswap_control *ctl)
{
	char *type = NULL;
	int zap = 1;
#ifdef HAVE_LIBBLKID
	blkid_probe pr = NULL;
	const char *v = NULL;
#endif
	if (!ctl->force) {
		if (lseek(ctl->fd, 0, SEEK_SET) != 0)
			errx(EXIT_FAILURE, _("unable to rewind swap-device"));

#ifdef HAVE_LIBBLKID
		pr = new_prober(ctl);
		blkid_probe_enable_partitions(pr, 1);
		blkid_probe_enable_superblocks(pr, 0);

		if (blkid_do_fullprobe(pr) == 0 &&
		    blkid_probe_lookup_value(pr, "PTTYPE", &v, NULL) == 0 && v) {
			type = xstrdup(v);
			zap = 0;
		}
#else
		/* don't zap if compiled without libblkid */
		zap = 0;
#endif
	}

	if (zap) {
		/*
		 * Wipe bootbits
		 */
		char buf[1024] = { '\0' };

		if (lseek(ctl->fd, 0, SEEK_SET) != 0)
			errx(EXIT_FAILURE, _("unable to rewind swap-device"));

		if (write_all(ctl->fd, buf, sizeof(buf)))
			errx(EXIT_FAILURE, _("unable to erase bootbits sectors"));
#ifdef HAVE_LIBBLKID
		/*
		 * Wipe rest of the device
		 */
		if (!pr)
			pr = new_prober(ctl);

		blkid_probe_enable_superblocks(pr, 1);
		blkid_probe_enable_partitions(pr, 0);
		blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_MAGIC|BLKID_SUBLKS_TYPE);

		while (blkid_do_probe(pr) == 0) {
			const char *data = NULL;

			if (!ctl->quiet
			    && blkid_probe_lookup_value(pr, "TYPE", &data, NULL) == 0 && data)
				warnx(_("%s: warning: wiping old %s signature."), ctl->devname, data);
			blkid_do_wipe(pr, 0);
		}
#endif
	} else if (!ctl->quiet) {
		warnx(_("%s: warning: don't erase bootbits sectors"),
			ctl->devname);
		if (type)
			fprintf(stderr, _("        (%s partition table detected). "), type);
		else
			fprintf(stderr, _("        (compiled without libblkid). "));
		fprintf(stderr, _("Use -f to force.\n"));
	}
	free(type);
#ifdef HAVE_LIBBLKID
	blkid_free_probe(pr);
#endif
}

#define SIGNATURE_OFFSET	1024

static void write_header_to_device(struct mkswap_control *ctl)
{
	off_t offset;

	assert(ctl);
	assert(ctl->fd > -1);
	assert(ctl->signature_page);

	offset = SIGNATURE_OFFSET + ctl->offset;

	if (lseek(ctl->fd, offset, SEEK_SET) != offset)
		errx(EXIT_FAILURE, _("unable to rewind swap-device"));

	if (write_all(ctl->fd, (char *) ctl->signature_page + SIGNATURE_OFFSET,
		      ctl->pagesize - SIGNATURE_OFFSET) == -1)
		err(EXIT_FAILURE,
			_("%s: unable to write signature page"),
			ctl->devname);
}

int main(int argc, char **argv)
{
	struct mkswap_control ctl = { .fd = -1, .endianness = ENDIANNESS_NATIVE };
	int c, permMask;
	uint64_t sz;
	int version = SWAP_VERSION;
	char *block_count = NULL, *strsz = NULL;
#ifdef HAVE_LIBUUID
	const char *opt_uuid = NULL;
	uuid_t uuid_dat;
#endif
	enum {
		OPT_LOCK = CHAR_MAX + 1,
		OPT_VERBOSE
	};
	static const struct option longopts[] = {
		{ "check",       no_argument,       NULL, 'c' },
		{ "force",       no_argument,       NULL, 'f' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "pagesize",    required_argument, NULL, 'p' },
		{ "label",       required_argument, NULL, 'L' },
		{ "swapversion", required_argument, NULL, 'v' },
		{ "uuid",        required_argument, NULL, 'U' },
		{ "endianness",  required_argument, NULL, 'e' },
		{ "offset",      required_argument, NULL, 'o' },
		{ "size",        required_argument, NULL, 's' },
		{ "file",        no_argument,       NULL, 'F' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "help",        no_argument,       NULL, 'h' },
		{ "lock",        optional_argument, NULL, OPT_LOCK },
		{ "verbose",    no_argument,        NULL, OPT_VERBOSE },
		{ NULL,          0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'c', 'q' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while((c = getopt_long(argc, argv, "cfp:qL:v:U:e:o:s:FVh", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'c':
			ctl.check = 1;
			break;
		case 'f':
			ctl.force = 1;
			break;
		case 'p':
			ctl.user_pagesize = strtou32_or_err(optarg, _("parsing page size failed"));
			break;
		case 'q':
			ctl.quiet = 1;
			break;
		case 'L':
			ctl.opt_label = optarg;
			break;
		case 'v':
			version = strtos32_or_err(optarg, _("parsing version number failed"));
			if (version != SWAP_VERSION)
				errx(EXIT_FAILURE,
					_("swapspace version %d is not supported"), version);
			break;
		case 'U':
#ifdef HAVE_LIBUUID
			opt_uuid = optarg;
#else
			warnx(_("warning: ignoring -U (UUIDs are unsupported by %s)"),
				program_invocation_short_name);
#endif
			break;
		case 'e':
			if (strcmp(optarg, "native") == 0) {
				ctl.endianness = ENDIANNESS_NATIVE;
			} else if (strcmp(optarg, "little") == 0) {
				ctl.endianness = ENDIANNESS_LITTLE;
			} else if (strcmp(optarg, "big") == 0) {
				ctl.endianness = ENDIANNESS_BIG;
			} else {
				errx(EXIT_FAILURE,
					_("invalid endianness %s is not supported"), optarg);
			}
			break;
		case 'o':
			ctl.offset = str2unum_or_err(optarg,
					10, _("Invalid offset"), SINT_MAX(off_t));
			break;
		case 's':
			ctl.filesz = strtosize_or_err(optarg, _("Invalid size"));
			break;
		case 'F':
			ctl.file = 1;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
			break;
		case OPT_LOCK:
			ctl.lockmode = "1";
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				ctl.lockmode = optarg;
			}
			break;
		case OPT_VERBOSE:
			ctl.verbose = 1;
			break;
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind < argc)
		ctl.devname = argv[optind++];
	if (optind < argc)
		block_count = argv[optind++];
	if (optind != argc) {
		warnx(_("only one device argument is currently supported"));
		errtryhelp(EXIT_FAILURE);
	}

#ifdef HAVE_LIBUUID
	if(opt_uuid) {
		if (strcmp(opt_uuid, "clear") == 0)
			uuid_clear(uuid_dat);
		else if (strcmp(opt_uuid, "random") == 0)
			uuid_generate_random(uuid_dat);
		else if (strcmp(opt_uuid, "time") == 0)
			uuid_generate_time(uuid_dat);
		else if (uuid_parse(opt_uuid, uuid_dat) != 0)
			errx(EXIT_FAILURE, _("error: parsing UUID failed"));
	} else
		uuid_generate(uuid_dat);
	ctl.uuid = uuid_dat;
#endif

	init_signature_page(&ctl);	/* get pagesize and allocate signature page */

	if (!ctl.devname) {
		warnx(_("error: Nowhere to set up swap on?"));
		errtryhelp(EXIT_FAILURE);
	}
	if (block_count) {
		/* this silly user specified the number of blocks explicitly */
		uint64_t blks = strtou64_or_err(block_count,
					_("invalid block count argument"));
		ctl.npages = blks / (ctl.pagesize / 1024);
	}

	sz = get_size(&ctl);
	if (!ctl.npages)
		ctl.npages = sz;
	else if (ctl.npages > sz && !ctl.force)
		errx(EXIT_FAILURE,
			_("error: "
			  "size %llu KiB is larger than device size %"PRIu64" KiB"),
			ctl.npages * (ctl.pagesize / 1024), sz * (ctl.pagesize / 1024));

	if (ctl.npages < MIN_GOODPAGES)
		errx(EXIT_FAILURE,
		     _("error: swap area needs to be at least %ld KiB"),
		     (long)(MIN_GOODPAGES * ctl.pagesize / 1024));
	if (ctl.npages > UINT32_MAX) {
		/* true when swap is bigger than 17.59 terabytes */
		ctl.npages = UINT32_MAX;
		if (!ctl.quiet)
			warnx(_("warning: truncating swap area to %llu KiB"),
				ctl.npages * ctl.pagesize / 1024);
	}

	if (is_mounted(ctl.devname))
		errx(EXIT_FAILURE, _("error: "
			"%s is mounted; will not make swapspace"),
			ctl.devname);

	open_device(&ctl);
	permMask = S_ISBLK(ctl.devstat.st_mode) ? 07007 : 07077;
	if (!ctl.quiet && (ctl.devstat.st_mode & permMask) != 0)
		warnx(_("%s: insecure permissions %04o, fix with: chmod %04o %s"),
			ctl.devname, ctl.devstat.st_mode & 07777,
			~permMask & 0666, ctl.devname);
	if (!ctl.quiet
	    && getuid() == 0 && S_ISREG(ctl.devstat.st_mode) && ctl.devstat.st_uid != 0)
		warnx(_("%s: insecure file owner %d, fix with: chown 0:0 %s"),
			ctl.devname, ctl.devstat.st_uid, ctl.devname);


	if (ctl.check)
		check_blocks(&ctl);
#ifdef HAVE_LINUX_FIEMAP_H
	if (!ctl.quiet && S_ISREG(ctl.devstat.st_mode))
		check_extents(&ctl);
#endif

	wipe_device(&ctl);

	assert(ctl.hdr);
	ctl.hdr->version = cpu32_to_endianness(version, ctl.endianness);
	ctl.hdr->last_page = cpu32_to_endianness(ctl.npages - 1, ctl.endianness);
	ctl.hdr->nr_badpages = cpu32_to_endianness(ctl.nbadpages, ctl.endianness);

	if ((ctl.npages - MIN_GOODPAGES) < ctl.nbadpages)
		errx(EXIT_FAILURE, _("Unable to set up swap-space: unreadable"));

	sz = (ctl.npages - ctl.nbadpages - 1) * ctl.pagesize;
	strsz = size_to_human_string(SIZE_SUFFIX_SPACE | SIZE_SUFFIX_3LETTER, sz);

	if (!ctl.quiet)
		printf(_("Setting up swapspace version %d, size = %s (%"PRIu64" bytes)\n"),
			version, strsz, sz);
	free(strsz);

	set_signature(&ctl);
	set_uuid_and_label(&ctl);

	write_header_to_device(&ctl);

	deinit_signature_page(&ctl);

#ifdef HAVE_LIBSELINUX
	if (S_ISREG(ctl.devstat.st_mode) && is_selinux_enabled() > 0) {
		const char *context_string;
		char *oldcontext;
		context_t newcontext;

		if (fgetfilecon(ctl.fd, &oldcontext) < 0) {
			if (errno != ENODATA)
				err(EXIT_FAILURE,
					_("%s: unable to obtain selinux file label"),
					ctl.devname);
			if (ul_selinux_get_default_context(ctl.devname,
						ctl.devstat.st_mode, &oldcontext))
				errx(EXIT_FAILURE,
					_("%s: unable to obtain default selinux file label"),
					ctl.devname);
		}
		if (!(newcontext = context_new(oldcontext)))
			errx(EXIT_FAILURE, _("unable to create new selinux context"));
		if (context_type_set(newcontext, SELINUX_SWAPFILE_TYPE))
			errx(EXIT_FAILURE, _("couldn't compute selinux context"));

		context_string = context_str(newcontext);

		if (strcmp(context_string, oldcontext)!=0) {
			if (fsetfilecon(ctl.fd, context_string) && errno != ENOTSUP)
				err(EXIT_FAILURE, _("unable to relabel %s to %s"),
						ctl.devname, context_string);
		}
		context_free(newcontext);
		freecon(oldcontext);
	}
#endif
	/*
	 * A subsequent swapon() will fail if the signature
	 * is not actually on disk. (This is a kernel bug.)
	 * The fsync() in close_fd() will take care of writing.
	 */
	if (close_fd(ctl.fd) != 0)
		err(EXIT_FAILURE, _("write failed"));
	return EXIT_SUCCESS;
}
