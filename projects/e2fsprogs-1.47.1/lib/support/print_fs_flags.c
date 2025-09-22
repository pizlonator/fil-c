/*
 * print_flags.c	- Print ext2_filsys flags
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>

#include "print_fs_flags.h"
#include "ext2fs/ext2fs.h"

struct flags_name {
	unsigned long	flag;
	const char	*name;
};

static const struct flags_name flags_array[] = {
	{ EXT2_FLAG_RW, "EXT2_FLAG_RW" },
	{ EXT2_FLAG_CHANGED, "EXT2_FLAG_CHANGED" },
	{ EXT2_FLAG_DIRTY, "EXT2_FLAG_DIRTY" },
	{ EXT2_FLAG_VALID, "EXT2_FLAG_VALID" },
	{ EXT2_FLAG_IB_DIRTY, "EXT2_FLAG_IB_DIRTY" },
	{ EXT2_FLAG_BB_DIRTY, "EXT2_FLAG_BB_DIRTY" },
	{ EXT2_FLAG_SWAP_BYTES, "EXT2_FLAG_SWAP_BYTES" },
	{ EXT2_FLAG_SWAP_BYTES_READ, "EXT2_FLAG_SWAP_BYTES_READ" },
	{ EXT2_FLAG_SWAP_BYTES_WRITE, "EXT2_FLAG_SWAP_BYTES_WRITE" },
	{ EXT2_FLAG_MASTER_SB_ONLY, "EXT2_FLAG_MASTER_SB_ONLY" },
	{ EXT2_FLAG_FORCE, "EXT2_FLAG_FORCE" },
	{ EXT2_FLAG_SUPER_ONLY, "EXT2_FLAG_SUPER_ONLY" },
	{ EXT2_FLAG_JOURNAL_DEV_OK, "EXT2_FLAG_JOURNAL_DEV_OK" },
	{ EXT2_FLAG_IMAGE_FILE, "EXT2_FLAG_IMAGE_FILE" },
	{ EXT2_FLAG_EXCLUSIVE, "EXT2_FLAG_EXCLUSIVE" },
	{ EXT2_FLAG_SOFTSUPP_FEATURES, "EXT2_FLAG_SOFTSUPP_FEATURES" },
	{ EXT2_FLAG_NOFREE_ON_ERROR, "EXT2_FLAG_NOFREE_ON_ERROR" },
	{ EXT2_FLAG_64BITS, "EXT2_FLAG_64BITS" },
	{ EXT2_FLAG_PRINT_PROGRESS, "EXT2_FLAG_PRINT_PROGRESS" },
	{ EXT2_FLAG_DIRECT_IO, "EXT2_FLAG_DIRECT_IO" },
	{ EXT2_FLAG_SKIP_MMP, "EXT2_FLAG_SKIP_MMP" },
	{ EXT2_FLAG_IGNORE_CSUM_ERRORS, "EXT2_FLAG_IGNORE_CSUM_ERRORS" },
	{ EXT2_FLAG_SHARE_DUP, "EXT2_FLAG_SHARE_DUP" },
	{ EXT2_FLAG_IGNORE_SB_ERRORS, "EXT2_FLAG_IGNORE_SB_ERRORS" },
	{ EXT2_FLAG_BBITMAP_TAIL_PROBLEM, "EXT2_FLAG_BBITMAP_TAIL_PROBLEM" },
	{ EXT2_FLAG_IBITMAP_TAIL_PROBLEM, "EXT2_FLAG_IBITMAP_TAIL_PROBLEM" },
	{ EXT2_FLAG_THREADS, "EXT2_FLAG_THREADS" },
	{ 0, NULL },
};

void print_fs_flags(FILE * f, unsigned long flags)
{
	const struct flags_name *fp;
	int	first = 1, pos = 16;

	for (fp = flags_array; fp->flag != 0; fp++) {
		if ((flags & fp->flag) == 0)
			continue;
		pos += strlen(fp->name) + 1;
		if (pos > 72) {
			fputs("\n\t", f);
			pos = 9 + strlen(fp->name);
		}
		if (first)
			first = 0;
		else
			fputc(' ', f);
		fputs(fp->name, f);
	}
	fputc('\n', f);
}
