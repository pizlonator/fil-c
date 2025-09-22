/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * fsck.minix.c - a file system consistency checker for Linux.
 *
 * (C) 1991, 1992 Linus Torvalds. This file may be redistributed
 * as per the GNU copyleft.
 *
 *
 * 09.11.91  -  made the first rudimentary functions
 *
 * 10.11.91  -  updated, does checking, no repairs yet.
 *		Sent out to the mailing-list for testing.
 *
 * 14.11.91  -	Testing seems to have gone well. Added some
 *		correction-code, and changed some functions.
 *
 * 15.11.91  -  More correction code. Hopefully it notices most
 *		cases now, and tries to do something about them.
 *
 * 16.11.91  -  More corrections (thanks to Mika Jalava). Most
 *		things seem to work now. Yeah, sure.
 *
 *
 * 19.04.92  -	Had to start over again from this old version, as a
 *		kernel bug ate my enhanced fsck in February.
 *
 * 28.02.93  -	added support for different directory entry sizes..
 *
 * Sat Mar  6 18:59:42 1993, faith@cs.unc.edu: Output namelen with
 *                           super-block information
 *
 * Sat Oct  9 11:17:11 1993, faith@cs.unc.edu: make exit status conform
 *                           to that required by fsutil
 *
 * Mon Jan  3 11:06:52 1994 - Dr. Wettstein (greg%wind.uucp@plains.nodak.edu)
 *			      Added support for file system valid flag.  Also
 *			      added program_version variable and output of
 *			      program name and version number when program
 *			      is executed.
 *
 * 30.10.94 - added support for v2 filesystem
 *            (Andreas Schwab, schwab@issan.informatik.uni-dortmund.de)
 *
 * 10.12.94  -  added test to prevent checking of mounted fs adapted
 *              from Theodore Ts'o's (tytso@athena.mit.edu) e2fsck
 *              program.  (Daniel Quinlan, quinlan@yggdrasil.com)
 *
 * 01.07.96  - Fixed the v2 fs stuff to use the right #defines and such
 *	       for modern libcs (janl@math.uio.no, Nicolai Langfeldt)
 *
 * 02.07.96  - Added C bit fiddling routines from rmk@ecs.soton.ac.uk
 *             (Russell King).  He made them for ARM.  It would seem
 *	       that the ARM is powerful enough to do this in C whereas
 *             i386 and m64k must use assembly to get it fast >:-)
 *	       This should make minix fsck systemindependent.
 *	       (janl@math.uio.no, Nicolai Langfeldt)
 *
 * 04.11.96  - Added minor fixes from Andreas Schwab to avoid compiler
 *             warnings.  Added mc68k bitops from
 *	       Joerg Dorchain <dorchain@mpi-sb.mpg.de>.
 *
 * 06.11.96  - Added v2 code submitted by Joerg Dorchain, but written by
 *             Andreas Schwab.
 *
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2008-04-06 James Youngman <jay@gnu.org>
 * - Issue better error message if we fail to open the device.
 * - Restore terminal state if we get a fatal signal.
 *
 *
 * I've had no time to add comments - hopefully the function names
 * are comments enough. As with all file system checkers, this assumes
 * the file system is quiescent - don't use it on a mounted device
 * unless you can be sure nobody is writing to it (and remember that the
 * kernel can write to it when it searches for files).
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/stat.h>
#include <signal.h>
#include <getopt.h>

#include "c.h"
#include "exitcodes.h"
#include "minix_programs.h"
#include "nls.h"
#include "pathnames.h"
#include "bitops.h"
#include "ismounted.h"
#include "all-io.h"
#include "closestream.h"
#include "rpmatch.h"
#include "strutils.h"

#define ROOT_INO 1
#define YESNO_LENGTH 64

/* Global variables used in minix_programs.h inline functions */
int fs_version = 1;
char *super_block_buffer;

static char *inode_buffer;

#define Inode (((struct minix_inode *) inode_buffer) - 1)
#define Inode2 (((struct minix2_inode *) inode_buffer) - 1)

static char *device_name;
static int device_fd;
static int repair, automatic, verbose, list, show, warn_mode, force;
static int directory, regular, blockdev, chardev, links, symlinks, total;

static int changed;		/* flags if the filesystem has been changed */
static int errors_uncorrected;	/* flag if some error was not corrected */
static size_t dirsize = 16;
static size_t namelen = 14;
static struct termios termios;
static volatile sig_atomic_t termios_set;

/* File-name data */
#define MAX_DEPTH 50
static int name_depth;
static char name_list[MAX_DEPTH][MINIX_NAME_MAX + 1];

/* Copy of the previous, just for error reporting - see get_current_name.  This
 * is a waste of 12kB or so.  */
static char current_name[MAX_DEPTH * (MINIX_NAME_MAX + 1) + 1];

static unsigned char *inode_count = NULL;
static unsigned char *zone_count = NULL;

static void recursive_check(unsigned int ino);
static void recursive_check2(unsigned int ino);

static char *inode_map;
static char *zone_map;

#define inode_in_use(x) (isset(inode_map,(x)) != 0)
#define zone_in_use(x) (isset(zone_map,(x)-get_first_zone()+1) != 0)

#define mark_inode(x) (setbit(inode_map,(x)),changed=1)
#define unmark_inode(x) (clrbit(inode_map,(x)),changed=1)

#define mark_zone(x) (setbit(zone_map,(x)-get_first_zone()+1),changed=1)
#define unmark_zone(x) (clrbit(zone_map,(x)-get_first_zone()+1),changed=1)

static void
reset(void) {
	if (termios_set)
		tcsetattr(STDIN_FILENO, TCSANOW, &termios);
}

static void
fatalsig(int sig) {
	/* We received a fatal signal.  Reset the terminal.  Also reset the
	 * signal handler and re-send the signal, so that the parent process
	 * knows which signal actually caused our death.  */
	signal(sig, SIG_DFL);
	reset();
	raise(sig);
}

static void __attribute__((__noreturn__))
leave(int status) {
	reset();
	exit(status);
}

static void __attribute__((__noreturn__))
usage(void) {
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] <device>\n"), program_invocation_short_name);
	fputs(USAGE_SEPARATOR, out);
	fputs(_("Check the consistency of a Minix filesystem.\n"), out);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -l, --list       list all filenames\n"), out);
	fputs(_(" -a, --auto       automatic repair\n"), out);
	fputs(_(" -r, --repair     interactive repair\n"), out);
	fputs(_(" -v, --verbose    be verbose\n"), out);
	fputs(_(" -s, --super      output super-block information\n"), out);
	fputs(_(" -m, --uncleared  activate mode not cleared warnings\n"), out);
	fputs(_(" -f, --force      force check\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(18));
	fprintf(out, USAGE_MAN_TAIL("fsck.minix(8)"));
	exit(FSCK_EX_OK);
}

static void die(const char *fmt, ...)
    __attribute__ ((__format__(__printf__, 1, 2)));

static void
die(const char *fmt, ...) {
	va_list ap;

	fprintf(stderr, UTIL_LINUX_VERSION);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	leave(FSCK_EX_ERROR);
}

/* This simply goes through the file-name data and prints out the current file.  */
static void
get_current_name(void) {
	int i = 0, ct;
	char *p, *q;

	q = current_name;
	while (i < name_depth) {
		p = name_list[i++];
		ct = namelen;
		*q++ = '/';
		while (ct-- && *p)
			*q++ = *p++;
	}
	if (i == 0)
		*q++ = '/';
	*q = 0;
}

static int
ask(const char *string, int def) {
	int resp;
	char input[YESNO_LENGTH];

	if (!repair) {
		printf("\n");
		errors_uncorrected = 1;
		return 0;
	}
	if (automatic) {
		printf("\n");
		if (!def)
			errors_uncorrected = 1;
		return def;
	}
	/* TRANSLATORS: these yes no questions uses rpmatch(), and should be
	 * translated.  */
	printf(def ? _("%s (y/n)? ") : _("%s (n/y)? "), string);
	fflush(stdout);
	ignore_result( fgets(input, YESNO_LENGTH, stdin) );
	resp = rpmatch(input);
	switch (resp) {
	case RPMATCH_INVALID:
		/* def = def */
		break;
	case RPMATCH_NO:
	case RPMATCH_YES:
		def = resp;
		break;
	default:
		/* rpmatch bug? */
		abort();
	}
	if (def)
		printf(_("y\n"));
	else {
		printf(_("n\n"));
		errors_uncorrected = 1;
	}
	return def;
}

/* Make certain that we aren't checking a filesystem that is on a mounted
 * partition.  Code adapted from e2fsck, Copyright (C) 1993, 1994 Theodore
 * Ts'o.  Also licensed under GPL.  */
static void
check_mount(void) {
	int cont;

	if (!is_mounted(device_name))
		return;

	printf(_("%s is mounted.	 "), device_name);
	if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
		cont = ask(_("Do you really want to continue"), 0);
	else
		cont = 0;
	if (!cont) {
		printf(_("check aborted.\n"));
		exit(FSCK_EX_OK);
	}
}


static int is_valid_zone_nr(unsigned short nr)
{
	if (nr < get_first_zone())
		return 0;
	if (nr >= get_nzones())
		return 0;
	return 1;
}

/* check_zone_nr checks to see that *nr is a valid zone nr.  If it isn't, it
 * will possibly be repaired.  Check_zone_nr sets *corrected if an error was
 * corrected, and returns the zone (0 for no zone or a bad zone-number).  */
static int
check_zone_nr(unsigned short *nr, int *corrected) {
	if (!*nr)
		return 0;

	if (*nr < get_first_zone()) {
		get_current_name();
		printf(_("Zone nr < FIRSTZONE in file `%s'."), current_name);
	} else if (*nr >= get_nzones()) {
		get_current_name();
		printf(_("Zone nr >= ZONES in file `%s'."), current_name);
	} else
		return *nr;

	if (ask(_("Remove block"), 1)) {
		*nr = 0;
		*corrected = 1;
	}
	return 0;
}

static int
check_zone_nr2(unsigned int *nr, int *corrected) {
	if (!*nr)
		return 0;

	if (*nr < get_first_zone()) {
		get_current_name();
		printf(_("Zone nr < FIRSTZONE in file `%s'."), current_name);
	} else if (*nr >= get_nzones()) {
		get_current_name();
		printf(_("Zone nr >= ZONES in file `%s'."), current_name);
	} else
		return *nr;

	if (ask(_("Remove block"), 1)) {
		*nr = 0;
		*corrected = 1;
	}
	return 0;
}

/* read-block reads block nr into the buffer at addr.  */
static void
read_block(unsigned int nr, char *addr) {
	if (!nr) {
		memset(addr, 0, MINIX_BLOCK_SIZE);
		return;
	}
	if (MINIX_BLOCK_SIZE * nr != lseek(device_fd, MINIX_BLOCK_SIZE * nr, SEEK_SET)) {
		get_current_name();
		printf(_("Read error: unable to seek to block in file '%s'\n"),
		       current_name);
		memset(addr, 0, MINIX_BLOCK_SIZE);
		errors_uncorrected = 1;
	} else if (MINIX_BLOCK_SIZE != read(device_fd, addr, MINIX_BLOCK_SIZE)) {
		get_current_name();
		printf(_("Read error: bad block in file '%s'\n"), current_name);
		memset(addr, 0, MINIX_BLOCK_SIZE);
		errors_uncorrected = 1;
	}
}

/* write_block writes block nr to disk.  */
static void
write_block(unsigned int nr, char *addr) {
	if (!nr)
		return;
	if (nr < get_first_zone() || nr >= get_nzones()) {
		printf(_("Internal error: trying to write bad block\n"
			 "Write request ignored\n"));
		errors_uncorrected = 1;
		return;
	}
	if (MINIX_BLOCK_SIZE * nr != lseek(device_fd, MINIX_BLOCK_SIZE * nr, SEEK_SET))
		die(_("seek failed in write_block"));
	if (MINIX_BLOCK_SIZE != write(device_fd, addr, MINIX_BLOCK_SIZE)) {
		get_current_name();
		printf(_("Write error: bad block in file '%s'\n"),
		       current_name);
		errors_uncorrected = 1;
	}
}

/* map-block calculates the absolute block nr of a block in a file.  It sets
 * 'changed' if the inode has needed changing, and re-writes any indirect
 * blocks with errors.  */
static int
map_block(struct minix_inode *inode, unsigned int blknr) {
	unsigned short ind[MINIX_BLOCK_SIZE >> 1];
	unsigned short dind[MINIX_BLOCK_SIZE >> 1];
	int blk_chg, block, result;
	size_t range;

	if (blknr < 7)
		return check_zone_nr(inode->i_zone + blknr, &changed);
	blknr -= 7;
	if (blknr < 512) {
		block = check_zone_nr(inode->i_zone + 7, &changed);
		read_block(block, (char *)ind);
		blk_chg = 0;
		result = check_zone_nr(blknr + ind, &blk_chg);
		if (blk_chg)
			write_block(block, (char *)ind);
		return result;
	}
	blknr -= 512;
	block = check_zone_nr(inode->i_zone + 8, &changed);
	read_block(block, (char *)dind);
	blk_chg = 0;
	range = blknr / 512;
	if (ARRAY_SIZE(dind) <= range) {
		printf(_("Warning: block out of range\n"));
		return 1;
	}
	result = check_zone_nr(dind + range, &blk_chg);
	if (blk_chg)
		write_block(block, (char *)dind);
	block = result;
	read_block(block, (char *)ind);
	blk_chg = 0;
	result = check_zone_nr(ind + (blknr % 512), &blk_chg);
	if (blk_chg)
		write_block(block, (char *)ind);
	return result;
}

static int
map_block2(struct minix2_inode *inode, unsigned int blknr) {
	unsigned int ind[MINIX_BLOCK_SIZE >> 2];
	unsigned int dind[MINIX_BLOCK_SIZE >> 2];
	unsigned int tind[MINIX_BLOCK_SIZE >> 2];
	int blk_chg, block, result;

	if (blknr < 7)
		return check_zone_nr2(inode->i_zone + blknr, &changed);
	blknr -= 7;
	if (blknr < 256) {
		block = check_zone_nr2(inode->i_zone + 7, &changed);
		read_block(block, (char *)ind);
		blk_chg = 0;
		result = check_zone_nr2(blknr + ind, &blk_chg);
		if (blk_chg)
			write_block(block, (char *)ind);
		return result;
	}
	blknr -= 256;
	if (blknr < 256 * 256) {
		block = check_zone_nr2(inode->i_zone + 8, &changed);
		read_block(block, (char *)dind);
		blk_chg = 0;
		result = check_zone_nr2(dind + blknr / 256, &blk_chg);
		if (blk_chg)
			write_block(block, (char *)dind);
		block = result;
		read_block(block, (char *)ind);
		blk_chg = 0;
		result = check_zone_nr2(ind + blknr % 256, &blk_chg);
		if (blk_chg)
			write_block(block, (char *)ind);
		return result;
	}
	blknr -= 256 * 256;
	block = check_zone_nr2(inode->i_zone + 9, &changed);
	read_block(block, (char *)tind);
	blk_chg = 0;
	result = check_zone_nr2(tind + blknr / (256 * 256), &blk_chg);
	if (blk_chg)
		write_block(block, (char *)tind);
	block = result;
	read_block(block, (char *)dind);
	blk_chg = 0;
	result = check_zone_nr2(dind + (blknr / 256) % 256, &blk_chg);
	if (blk_chg)
		write_block(block, (char *)dind);
	block = result;
	read_block(block, (char *)ind);
	blk_chg = 0;
	result = check_zone_nr2(ind + blknr % 256, &blk_chg);
	if (blk_chg)
		write_block(block, (char *)ind);
	return result;
}

static void
write_super_block(void) {
	/* v3 super block does not track state */
	if (fs_version == 3)
		return;
	/* Set the state of the filesystem based on whether or not there are
	 * uncorrected errors.  The filesystem valid flag is unconditionally
	 * set if we get this far.  */
	Super.s_state |= MINIX_VALID_FS;
	if (errors_uncorrected)
		Super.s_state |= MINIX_ERROR_FS;
	else
		Super.s_state &= ~MINIX_ERROR_FS;

	if (MINIX_BLOCK_SIZE != lseek(device_fd, MINIX_BLOCK_SIZE, SEEK_SET))
		die(_("seek failed in write_super_block"));
	if (MINIX_BLOCK_SIZE != write(device_fd, super_block_buffer, MINIX_BLOCK_SIZE))
		die(_("unable to write super-block"));
}

static void
write_tables(void) {
	unsigned long buffsz = get_inode_buffer_size();
	unsigned long imaps = get_nimaps();
	unsigned long zmaps = get_nzmaps();

	write_super_block();

	if (write_all(device_fd, inode_map, imaps * MINIX_BLOCK_SIZE))
		die(_("Unable to write inode map"));

	if (write_all(device_fd, zone_map, zmaps * MINIX_BLOCK_SIZE))
		die(_("Unable to write zone map"));

	if (write_all(device_fd, inode_buffer, buffsz))
		die(_("Unable to write inodes"));
}

static void
get_dirsize(void) {
	int block;
	char blk[MINIX_BLOCK_SIZE];
	size_t size;

	if (fs_version == 2 || fs_version == 3)
		block = Inode2[ROOT_INO].i_zone[0];
	else
		block = Inode[ROOT_INO].i_zone[0];
	read_block(block, blk);

	for (size = 16; size + 2 < MINIX_BLOCK_SIZE; size <<= 1) {
		if (strcmp(blk + size + 2, "..") == 0) {
			dirsize = size;
			namelen = size - 2;
			return;
		}
	}
	/* use defaults */
}

static void
read_superblock(void) {
	if (MINIX_BLOCK_SIZE != lseek(device_fd, MINIX_BLOCK_SIZE, SEEK_SET))
		die(_("seek failed"));

	super_block_buffer = calloc(1, MINIX_BLOCK_SIZE);
	if (!super_block_buffer)
		die(_("unable to alloc buffer for superblock"));

	if (MINIX_BLOCK_SIZE != read(device_fd, super_block_buffer, MINIX_BLOCK_SIZE))
		die(_("unable to read super block"));
	if (Super.s_magic == MINIX_SUPER_MAGIC) {
		namelen = 14;
		dirsize = 16;
		fs_version = 1;
	} else if (Super.s_magic == MINIX_SUPER_MAGIC2) {
		namelen = 30;
		dirsize = 32;
		fs_version = 1;
	} else if (Super.s_magic == MINIX2_SUPER_MAGIC) {
		namelen = 14;
		dirsize = 16;
		fs_version = 2;
	} else if (Super.s_magic == MINIX2_SUPER_MAGIC2) {
		namelen = 30;
		dirsize = 32;
		fs_version = 2;
	} else if (Super3.s_magic == MINIX3_SUPER_MAGIC) {
		namelen = 60;
		dirsize = 64;
		fs_version = 3;
	} else
		die(_("bad magic number in super-block"));
	if (get_zone_size() != 0 || MINIX_BLOCK_SIZE != 1024)
		die(_("Only 1k blocks/zones supported"));
	if (get_ninodes() == 0 || get_ninodes() == UINT32_MAX)
		die(_("bad s_ninodes field in super-block"));
	if (get_nimaps() * MINIX_BLOCK_SIZE * 8 < get_ninodes() + 1)
		die(_("bad s_imap_blocks field in super-block"));
	if (get_first_zone() > (off_t) get_nzones())
		die(_("bad s_firstdatazone field in super-block"));
	if (get_nzmaps() * MINIX_BLOCK_SIZE * 8 <
	    get_nzones() - get_first_zone() + 1)
		die(_("bad s_zmap_blocks field in super-block"));
}

static void
read_tables(void) {
	unsigned long inodes = get_ninodes();
	size_t buffsz = get_inode_buffer_size();
	off_t norm_first_zone = first_zone_data();
	off_t first_zone = get_first_zone();
	unsigned long zones = get_nzones();
	unsigned long imaps = get_nimaps();
	unsigned long zmaps = get_nzmaps();
	ssize_t rc;

	inode_map = malloc(imaps * MINIX_BLOCK_SIZE);
	if (!inode_map)
		die(_("Unable to allocate buffer for inode map"));
	zone_map = malloc(zmaps * MINIX_BLOCK_SIZE);
	if (!zone_map)
		die(_("Unable to allocate buffer for zone map"));
	inode_buffer = malloc(buffsz);
	if (!inode_buffer)
		die(_("Unable to allocate buffer for inodes"));
	inode_count = calloc(1, inodes + 1);
	if (!inode_count)
		die(_("Unable to allocate buffer for inode count"));
	zone_count = calloc(1, zones);
	if (!zone_count)
		die(_("Unable to allocate buffer for zone count"));

	rc = read(device_fd, inode_map, imaps * MINIX_BLOCK_SIZE);
	if (rc < 0 || imaps * MINIX_BLOCK_SIZE != (size_t) rc)
		die(_("Unable to read inode map"));

	rc = read(device_fd, zone_map, zmaps * MINIX_BLOCK_SIZE);
	if (rc < 0 || zmaps * MINIX_BLOCK_SIZE != (size_t) rc)
		die(_("Unable to read zone map"));

	rc = read(device_fd, inode_buffer, buffsz);
	if (rc < 0 || buffsz != (size_t) rc)
		die(_("Unable to read inodes"));
	if (norm_first_zone != first_zone) {
		printf(_("Warning: Firstzone != Norm_firstzone\n"));
		errors_uncorrected = 1;
	}
	get_dirsize();
	if (show) {
		printf(_("%ld inodes\n"), inodes);
		printf(_("%ld blocks\n"), zones);
		printf(_("Firstdatazone=%jd (%jd)\n"),
			(intmax_t)first_zone, (intmax_t)norm_first_zone);
		printf(_("Zonesize=%d\n"), MINIX_BLOCK_SIZE << get_zone_size());
		printf(_("Maxsize=%zu\n"), get_max_size());
		if (fs_version < 3)
			printf(_("Filesystem state=%d\n"), Super.s_state);
		printf(_("namelen=%zd\n\n"), namelen);
	}
}

static struct minix_inode *
get_inode(unsigned int nr) {
	struct minix_inode *inode;

	if (!nr || nr > get_ninodes())
		return NULL;
	total++;
	inode = Inode + nr;
	if (!inode_count[nr]) {
		if (!inode_in_use(nr)) {
			get_current_name();
			printf(_("Inode %d marked unused, "
				 "but used for file '%s'\n"), nr, current_name);
			if (repair) {
				if (ask(_("Mark in use"), 1))
					mark_inode(nr);
			} else {
				errors_uncorrected = 1;
			}
		}
		if (S_ISDIR(inode->i_mode))
			directory++;
		else if (S_ISREG(inode->i_mode))
			regular++;
		else if (S_ISCHR(inode->i_mode))
			chardev++;
		else if (S_ISBLK(inode->i_mode))
			blockdev++;
		else if (S_ISLNK(inode->i_mode))
			symlinks++;
		else if (S_ISSOCK(inode->i_mode))
			;
		else if (S_ISFIFO(inode->i_mode))
			;
		else {
			get_current_name();
			printf(_("The file `%s' has mode %05o\n"),
			       current_name, inode->i_mode);
		}

	} else
		links++;
	if (!++inode_count[nr]) {
		printf(_("Warning: inode count too big.\n"));
		inode_count[nr]--;
		errors_uncorrected = 1;
	}
	return inode;
}

static struct minix2_inode *
get_inode2(unsigned int nr) {
	struct minix2_inode *inode;

	if (!nr || nr > get_ninodes())
		return NULL;
	total++;
	inode = Inode2 + nr;
	if (!inode_count[nr]) {
		if (!inode_in_use(nr)) {
			get_current_name();
			printf(_("Inode %d marked unused, "
				 "but used for file '%s'\n"), nr, current_name);
			if (repair) {
				if (ask(_("Mark in use"), 1))
					mark_inode(nr);
				else
					errors_uncorrected = 1;
			}
		}
		if (S_ISDIR(inode->i_mode))
			directory++;
		else if (S_ISREG(inode->i_mode))
			regular++;
		else if (S_ISCHR(inode->i_mode))
			chardev++;
		else if (S_ISBLK(inode->i_mode))
			blockdev++;
		else if (S_ISLNK(inode->i_mode))
			symlinks++;
		else if (S_ISSOCK(inode->i_mode)) ;
		else if (S_ISFIFO(inode->i_mode)) ;
		else {
			get_current_name();
			printf(_("The file `%s' has mode %05o\n"),
			       current_name, inode->i_mode);
		}
	} else
		links++;
	if (!++inode_count[nr]) {
		printf(_("Warning: inode count too big.\n"));
		inode_count[nr]--;
		errors_uncorrected = 1;
	}
	return inode;
}

static void
check_root(void) {
	struct minix_inode *inode = Inode + ROOT_INO;

	if (!inode || !S_ISDIR(inode->i_mode))
		die(_("root inode isn't a directory"));
}

static void
check_root2(void) {
	struct minix2_inode *inode = Inode2 + ROOT_INO;

	if (!inode || !S_ISDIR(inode->i_mode))
		die(_("root inode isn't a directory"));
}

static int
add_zone(unsigned short *znr, int *corrected) {
	int block;

	block = check_zone_nr(znr, corrected);
	if (!block)
		return 0;
	if (zone_count[block]) {
		get_current_name();
		printf(_("Block has been used before. Now in file `%s'."),
		       current_name);
		if (ask(_("Clear"), 1)) {
			*znr = 0;
			block = 0;
			*corrected = 1;
		}
	}
	if (!block)
		return 0;
	if (!zone_in_use(block)) {
		get_current_name();
		printf(_("Block %d in file `%s' is marked not in use."),
		       block, current_name);
		if (ask(_("Correct"), 1))
			mark_zone(block);
	}
	if (!++zone_count[block])
		zone_count[block]--;
	return block;
}

static int
add_zone2(unsigned int *znr, int *corrected) {
	int block;

	block = check_zone_nr2(znr, corrected);
	if (!block)
		return 0;
	if (zone_count[block]) {
		get_current_name();
		printf(_("Block has been used before. Now in file `%s'."),
		       current_name);
		if (ask(_("Clear"), 1)) {
			*znr = 0;
			block = 0;
			*corrected = 1;
		}
	}
	if (!block)
		return 0;
	if (!zone_in_use(block)) {
		get_current_name();
		printf(_("Block %d in file `%s' is marked not in use."),
		       block, current_name);
		if (ask(_("Correct"), 1))
			mark_zone(block);
	}
	if (!++zone_count[block])
		zone_count[block]--;
	return block;
}

static void
add_zone_ind(unsigned short *znr, int *corrected) {
	static char blk[MINIX_BLOCK_SIZE];
	int i, chg_blk = 0;
	int block;

	block = add_zone(znr, corrected);
	if (!block)
		return;
	read_block(block, blk);
	for (i = 0; i < (MINIX_BLOCK_SIZE >> 1); i++)
		add_zone(i + (unsigned short *)blk, &chg_blk);
	if (chg_blk)
		write_block(block, blk);
}

static void
add_zone_ind2(unsigned int *znr, int *corrected) {
	static char blk[MINIX_BLOCK_SIZE];
	int i, chg_blk = 0;
	int block;

	block = add_zone2(znr, corrected);
	if (!block)
		return;
	read_block(block, blk);
	for (i = 0; i < MINIX_BLOCK_SIZE >> 2; i++)
		add_zone2(i + (unsigned int *)blk, &chg_blk);
	if (chg_blk)
		write_block(block, blk);
}

static void
add_zone_dind(unsigned short *znr, int *corrected) {
	static char blk[MINIX_BLOCK_SIZE];
	int i, blk_chg = 0;
	int block;

	block = add_zone(znr, corrected);
	if (!block)
		return;
	read_block(block, blk);
	for (i = 0; i < (MINIX_BLOCK_SIZE >> 1); i++)
		add_zone_ind(i + (unsigned short *)blk, &blk_chg);
	if (blk_chg)
		write_block(block, blk);
}

static void
add_zone_dind2(unsigned int *znr, int *corrected) {
	static char blk[MINIX_BLOCK_SIZE];
	int i, blk_chg = 0;
	int block;

	block = add_zone2(znr, corrected);
	if (!block)
		return;
	read_block(block, blk);
	for (i = 0; i < MINIX_BLOCK_SIZE >> 2; i++)
		add_zone_ind2(i + (unsigned int *)blk, &blk_chg);
	if (blk_chg)
		write_block(block, blk);
}

static void
add_zone_tind2(unsigned int *znr, int *corrected) {
	static char blk[MINIX_BLOCK_SIZE];
	int i, blk_chg = 0;
	int block;

	block = add_zone2(znr, corrected);
	if (!block)
		return;
	read_block(block, blk);
	for (i = 0; i < MINIX_BLOCK_SIZE >> 2; i++)
		add_zone_dind2(i + (unsigned int *)blk, &blk_chg);
	if (blk_chg)
		write_block(block, blk);
}

static void
check_zones(unsigned int i) {
	struct minix_inode *inode;

	if (!i || i > get_ninodes())
		return;
	if (inode_count[i] > 1)	/* have we counted this file already? */
		return;
	inode = Inode + i;
	if (!S_ISDIR(inode->i_mode) && !S_ISREG(inode->i_mode) &&
	    !S_ISLNK(inode->i_mode))
		return;
	for (i = 0; i < 7; i++)
		add_zone(i + inode->i_zone, &changed);
	add_zone_ind(7 + inode->i_zone, &changed);
	add_zone_dind(8 + inode->i_zone, &changed);
}

static void
check_zones2(unsigned int i) {
	struct minix2_inode *inode;

	if (!i || i > get_ninodes())
		return;
	if (inode_count[i] > 1)	/* have we counted this file already? */
		return;
	inode = Inode2 + i;
	if (!S_ISDIR(inode->i_mode) && !S_ISREG(inode->i_mode)
	    && !S_ISLNK(inode->i_mode))
		return;
	for (i = 0; i < 7; i++)
		add_zone2(i + inode->i_zone, &changed);
	add_zone_ind2(7 + inode->i_zone, &changed);
	add_zone_dind2(8 + inode->i_zone, &changed);
	add_zone_tind2(9 + inode->i_zone, &changed);
}

static void
check_file(struct minix_inode *dir, unsigned int offset) {
	static char blk[MINIX_BLOCK_SIZE + 2];
	struct minix_inode *inode;
	unsigned int ino;
	char *name;
	int block;

	block = map_block(dir, offset / MINIX_BLOCK_SIZE);
	read_block(block, blk);
	name = blk + (offset % MINIX_BLOCK_SIZE) + 2;
	ino = *(unsigned short *)(name - 2);
	if (ino > get_ninodes()) {
		get_current_name();
		printf(_("The directory '%s' contains a bad inode number "
			 "for file '%.*s'."), current_name, (int)namelen, name);
		if (ask(_(" Remove"), 1)) {
			*(unsigned short *)(name - 2) = 0;
			write_block(block, blk);
		}
		ino = 0;
	}
	if (name_depth < MAX_DEPTH)
		xstrncpy(name_list[name_depth], name, namelen);
	else
		return;
	name_depth++;
	inode = get_inode(ino);
	name_depth--;
	if (!offset) {
		if (!inode || strcmp(".", name) != 0) {
			get_current_name();
			printf(_("%s: bad directory: '.' isn't first\n"),
			       current_name);
			errors_uncorrected = 1;
		} else
			return;
	}
	if (offset == dirsize) {
		if (!inode || strcmp("..", name) != 0) {
			get_current_name();
			printf(_("%s: bad directory: '..' isn't second\n"),
			       current_name);
			errors_uncorrected = 1;
		} else
			return;
	}
	if (!inode)
		return;
	if (name_depth < MAX_DEPTH)
		xstrncpy(name_list[name_depth], name, namelen);
	else
		return;
	name_depth++;
	if (list) {
		if (verbose)
			printf("%6d %07o %3d ", ino,
			       inode->i_mode, inode->i_nlinks);
		get_current_name();
		printf("%s", current_name);
		if (S_ISDIR(inode->i_mode))
			printf(":\n");
		else
			printf("\n");
	}
	check_zones(ino);
	if (inode && S_ISDIR(inode->i_mode))
		recursive_check(ino);
	name_depth--;
}

static void
check_file2(struct minix2_inode *dir, unsigned int offset) {
	static char blk[MINIX_BLOCK_SIZE + 4];
	struct minix2_inode *inode;
	ino_t ino;
	char *name;
	int block;
	const int version_offset = fs_version == 3 ? 4 : 2;

	block = map_block2(dir, offset / MINIX_BLOCK_SIZE);
	read_block(block, blk);
	name = blk + (offset % MINIX_BLOCK_SIZE) + version_offset;
	ino = version_offset == 4 ? *(uint32_t *)(name - version_offset)
	                          : *(uint16_t *)(name - version_offset);
	if (ino > get_ninodes()) {
		get_current_name();
		printf(_("The directory '%s' contains a bad inode number "
			 "for file '%.*s'."), current_name, (int)namelen, name);
		if (ask(_(" Remove"), 1)) {
			memset(name - version_offset, 0, version_offset);
			write_block(block, blk);
		}
		ino = 0;
	}
	if (name_depth < MAX_DEPTH)
		xstrncpy(name_list[name_depth], name, namelen);
	else
		return;
	name_depth++;
	inode = get_inode2(ino);
	name_depth--;
	if (!offset) {
		if (!inode || strcmp(".", name) != 0) {
			get_current_name();
			printf(_("%s: bad directory: '.' isn't first\n"),
			       current_name);
			errors_uncorrected = 1;
		} else
			return;
	}
	if (offset == dirsize) {
		if (!inode || strcmp("..", name) != 0) {
			get_current_name();
			printf(_("%s: bad directory: '..' isn't second\n"),
			       current_name);
			errors_uncorrected = 1;
		} else
			return;
	}
	if (!inode)
		return;
	name_depth++;
	if (list) {
		if (verbose)
			printf("%6ju %07o %3d ", (uintmax_t)ino, inode->i_mode,
			       inode->i_nlinks);
		get_current_name();
		printf("%s", current_name);
		if (S_ISDIR(inode->i_mode))
			printf(":\n");
		else
			printf("\n");
	}
	check_zones2(ino);
	if (inode && S_ISDIR(inode->i_mode))
		recursive_check2(ino);
	name_depth--;
}

static void
recursive_check(unsigned int ino) {
	struct minix_inode *dir;
	off_t offset;

	dir = Inode + ino;
	if (!S_ISDIR(dir->i_mode))
		die(_("internal error"));
	if (dir->i_size < 2 * dirsize) {
		get_current_name();
		printf(_("%s: bad directory: size < 32"), current_name);
		errors_uncorrected = 1;
	}

	if ((!repair || automatic) && !is_valid_zone_nr(*dir->i_zone)) {
		get_current_name();
		printf(_("%s: bad directory: invalid i_zone, use --repair to fix\n"), current_name);
		return;
	}
	for (offset = 0; offset < dir->i_size; offset += dirsize)
		check_file(dir, offset);
}

static void
recursive_check2(unsigned int ino) {
	struct minix2_inode *dir;
	off_t offset;

	dir = Inode2 + ino;
	if (!S_ISDIR(dir->i_mode))
		die(_("internal error"));
	if (dir->i_size < 2 * dirsize) {
		get_current_name();
		printf(_("%s: bad directory: size < 32"), current_name);
		errors_uncorrected = 1;
	}
	for (offset = 0; offset < dir->i_size; offset += dirsize)
		check_file2(dir, offset);
}

static int
bad_zone(int i) {
	char buffer[1024];

	if (MINIX_BLOCK_SIZE * i != lseek(device_fd, MINIX_BLOCK_SIZE * i, SEEK_SET))
		die(_("seek failed in bad_zone"));
	return (MINIX_BLOCK_SIZE != read(device_fd, buffer, MINIX_BLOCK_SIZE));
}

static void
check_counts(void) {
	unsigned long i;

	for (i = 1; i <= get_ninodes(); i++) {
		if (!inode_in_use(i) && Inode[i].i_mode && warn_mode) {
			printf(_("Inode %lu mode not cleared."), i);
			if (ask(_("Clear"), 1)) {
				Inode[i].i_mode = 0;
				changed = 1;
			}
		}
		if (!inode_count[i]) {
			if (!inode_in_use(i))
				continue;
			printf(_("Inode %lu not used, marked used in the bitmap."), i);
			if (ask(_("Clear"), 1))
				unmark_inode(i);
			continue;
		}
		if (!inode_in_use(i)) {
			printf(_("Inode %lu used, marked unused in the bitmap."), i);
			if (ask(_("Set"), 1))
				mark_inode(i);
		}
		if (Inode[i].i_nlinks != inode_count[i]) {
			printf(_("Inode %lu (mode = %07o), i_nlinks=%d, counted=%d."),
			       i, Inode[i].i_mode, Inode[i].i_nlinks,
			       inode_count[i]);
			if (ask(_("Set i_nlinks to count"), 1)) {
				Inode[i].i_nlinks = inode_count[i];
				changed = 1;
			}
		}
	}
	for (i = get_first_zone(); i < get_nzones(); i++) {
		if (zone_in_use(i) == zone_count[i])
			continue;
		if (!zone_count[i]) {
			if (bad_zone(i))
				continue;
			printf(_("Zone %lu: marked in use, no file uses it."),
			       i);
			if (ask(_("Unmark"), 1))
				unmark_zone(i);
			continue;
		}
		if (zone_in_use(i))
			printf(_("Zone %lu: in use, counted=%d\n"),
			       i, zone_count[i]);
		else
			printf(_("Zone %lu: not in use, counted=%d\n"),
			       i, zone_count[i]);
	}
}

static void
check_counts2(void) {
	unsigned long i;

	for (i = 1; i <= get_ninodes(); i++) {
		if (!inode_in_use(i) && Inode2[i].i_mode && warn_mode) {
			printf(_("Inode %lu mode not cleared."), i);
			if (ask(_("Clear"), 1)) {
				Inode2[i].i_mode = 0;
				changed = 1;
			}
		}
		if (!inode_count[i]) {
			if (!inode_in_use(i))
				continue;
			printf(_("Inode %lu not used, marked used in the bitmap."), i);
			if (ask(_("Clear"), 1))
				unmark_inode(i);
			continue;
		}
		if (!inode_in_use(i)) {
			printf(_("Inode %lu used, marked unused in the bitmap."), i);
			if (ask(_("Set"), 1))
				mark_inode(i);
		}
		if (Inode2[i].i_nlinks != inode_count[i]) {
			printf(_("Inode %lu (mode = %07o), i_nlinks=%d, counted=%d."),
			       i, Inode2[i].i_mode, Inode2[i].i_nlinks,
			       inode_count[i]);
			if (ask(_("Set i_nlinks to count"), 1)) {
				Inode2[i].i_nlinks = inode_count[i];
				changed = 1;
			}
		}
	}
	for (i = get_first_zone(); i < get_nzones(); i++) {
		if (zone_in_use(i) == zone_count[i])
			continue;
		if (!zone_count[i]) {
			if (bad_zone(i))
				continue;
			printf(_("Zone %lu: marked in use, no file uses it."),
			       i);
			if (ask(_("Unmark"), 1))
				unmark_zone(i);
			continue;
		}
		if (zone_in_use(i))
			printf(_("Zone %lu: in use, counted=%d\n"),
			       i, zone_count[i]);
		else
			printf(_("Zone %lu: not in use, counted=%d\n"),
			       i, zone_count[i]);
	}
}

static void
check(void) {
	memset(inode_count, 0, (get_ninodes() + 1) * sizeof(*inode_count));
	memset(zone_count, 0, get_nzones() * sizeof(*zone_count));
	check_zones(ROOT_INO);
	recursive_check(ROOT_INO);
	check_counts();
}

static void
check2(void) {
	memset(inode_count, 0, (get_ninodes() + 1) * sizeof(*inode_count));
	memset(zone_count, 0, get_nzones() * sizeof(*zone_count));
	check_zones2(ROOT_INO);
	recursive_check2(ROOT_INO);
	check_counts2();
}

int
main(int argc, char **argv) {
	struct termios tmp;
	int count;
	int retcode = FSCK_EX_OK;
	int i;
	static const struct option longopts[] = {
		{"list", no_argument, NULL, 'l'},
		{"auto", no_argument, NULL, 'a'},
		{"repair", no_argument, NULL, 'r'},
		{"verbose", no_argument, NULL, 'v'},
		{"super", no_argument, NULL, 's'},
		{"uncleared", no_argument, NULL, 'm'},
		{"force", no_argument, NULL, 'f'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(FSCK_EX_USAGE);

	if (INODE_SIZE * MINIX_INODES_PER_BLOCK != MINIX_BLOCK_SIZE)
		die(_("bad inode size"));
	if (INODE2_SIZE * MINIX2_INODES_PER_BLOCK != MINIX_BLOCK_SIZE)
		die(_("bad v2 inode size"));

	while ((i = getopt_long(argc, argv, "larvsmfVh", longopts, NULL)) != -1)
		switch (i) {
		case 'l':
			list = 1;
			break;
		case 'a':
			automatic = 1;
			repair = 1;
			break;
		case 'r':
			automatic = 0;
			repair = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 's':
			show = 1;
			break;
		case 'm':
			warn_mode = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'V':
			print_version(FSCK_EX_OK);
		case 'h':
			usage();
		default:
			errtryhelp(FSCK_EX_USAGE);
		}
	argc -= optind;
	argv += optind;
	if (0 < argc) {
		device_name = argv[0];
	} else {
		warnx(_("no device specified"));
		errtryhelp(FSCK_EX_USAGE);
	}
	check_mount();		/* trying to check a mounted filesystem? */
	if (repair && !automatic && (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)))
		die(_("need terminal for interactive repairs"));

	device_fd = open(device_name, repair ? O_RDWR : O_RDONLY);
	if (device_fd < 0)
		die(_("cannot open %s: %s"), device_name, strerror(errno));
	for (count = 0; count < 3; count++)
		sync();
	read_superblock();

	/* Determine whether or not we should continue with the checking.  This
	 * is based on the status of the filesystem valid and error flags and
	 * whether or not the -f switch was specified on the command line.  */
	if (fs_version < 3 && !(Super.s_state & MINIX_ERROR_FS) &&
	    (Super.s_state & MINIX_VALID_FS) && !force) {
		if (repair)
			printf(_("%s is clean, no check.\n"), device_name);
		return retcode;
	}

	if (force)
		printf(_("Forcing filesystem check on %s.\n"), device_name);
	else if (repair)
		printf(_("Filesystem on %s is dirty, needs checking.\n"),
		       device_name);

	read_tables();

	/* Restore the terminal state on fatal signals.  We don't do this for
	 * SIGALRM, SIGUSR1 or SIGUSR2.  */
	signal(SIGINT, fatalsig);
	signal(SIGQUIT, fatalsig);
	signal(SIGTERM, fatalsig);

	if (repair && !automatic) {
		tcgetattr(STDIN_FILENO, &termios);
		tmp = termios;
		tmp.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &tmp);
		termios_set = 1;
	}

	if (fs_version == 2 || fs_version == 3) {
		check_root2();
		check2();
	} else {
		check_root();
		check();
	}
	if (verbose) {
		unsigned long inode, free;

		for (inode = 1, free = 0; inode <= get_ninodes(); inode++)
			if (!inode_in_use(inode))
				free++;
		printf(_("\n%6ld inodes used (%ld%%)\n"),
		       (get_ninodes() - free),
		       100 * (get_ninodes() - free) / get_ninodes());
		for (inode = get_first_zone(), free = 0; inode < get_nzones(); inode++)
			if (!zone_in_use(inode))
				free++;
		printf(_("%6ld zones used (%ld%%)\n"), (get_nzones() - free),
		       100 * (get_nzones() - free) / get_nzones());
		printf(_("\n%6d regular files\n"
			 "%6d directories\n"
			 "%6d character device files\n"
			 "%6d block device files\n"
			 "%6d links\n"
			 "%6d symbolic links\n"
			 "------\n"
			 "%6d files\n"),
		       regular, directory, chardev, blockdev,
		       links - 2 * directory + 1, symlinks,
		       total - 2 * directory + 1);
	}
	if (changed) {
		write_tables();
		printf(_("----------------------------\n"
			 "FILE SYSTEM HAS BEEN CHANGED\n"
			 "----------------------------\n"));
		for (count = 0; count < 3; count++)
			sync();
	} else if (repair)
		write_super_block();

	if (repair && !automatic)
		tcsetattr(STDIN_FILENO, TCSANOW, &termios);

	if (close_fd(device_fd) != 0)
		err(FSCK_EX_ERROR, _("write failed"));
	if (changed)
		retcode += 3;
	if (errors_uncorrected)
		retcode += 4;
	return retcode;
}
