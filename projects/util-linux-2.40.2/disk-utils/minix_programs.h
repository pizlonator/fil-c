/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2011 Sami Kerola <kerolasa@iki.fi>
 */
#ifndef UTIL_LINUX_MINIX_PROGRAMS_H
#define UTIL_LINUX_MINIX_PROGRAMS_H

#include "minix.h"

/*
 * Global variables.
 */
extern int fs_version;
extern char *super_block_buffer;

#define Super (*(struct minix_super_block *) super_block_buffer)
#define Super3 (*(struct minix3_super_block *) super_block_buffer)

#define INODE_SIZE (sizeof(struct minix_inode))
#define INODE2_SIZE (sizeof(struct minix2_inode))

#define BITS_PER_BLOCK (MINIX_BLOCK_SIZE << 3)

#define UPPER(size,n) ((size+((n)-1))/(n))

/*
 * Inline functions.
 */
static inline unsigned long get_ninodes(void)
{
	switch (fs_version) {
	case 3:
		return Super3.s_ninodes;
	default:
		return Super.s_ninodes;
	}
}

static inline unsigned long get_nzones(void)
{
	switch (fs_version) {
	case 3:
		return Super3.s_zones;
	case 2:
		return Super.s_zones;
	default:
		return Super.s_nzones;
	}
}

static inline unsigned long get_nimaps(void)
{
	switch (fs_version) {
	case 3:
		return Super3.s_imap_blocks;
	default:
		return Super.s_imap_blocks;
	}
}

static inline unsigned long get_nzmaps(void)
{
	switch (fs_version) {
	case 3:
		return Super3.s_zmap_blocks;
	default:
		return Super.s_zmap_blocks;
	}
}

static inline off_t get_first_zone(void)
{
	switch (fs_version) {
	case 3:
		return Super3.s_firstdatazone;
	default:
		return Super.s_firstdatazone;
	}
}

static inline size_t get_zone_size(void)
{
	switch (fs_version) {
	case 3:
		return Super3.s_log_zone_size;
	default:
		return Super.s_log_zone_size;
	}
}

static inline size_t get_max_size(void)
{
	switch (fs_version) {
	case 3:
		return Super3.s_max_size;
	default:
		return Super.s_max_size;
	}
}

static inline unsigned long inode_blocks(void)
{
	switch (fs_version) {
	case 3:
	case 2:
		return UPPER(get_ninodes(), MINIX2_INODES_PER_BLOCK);
	default:
		return UPPER(get_ninodes(), MINIX_INODES_PER_BLOCK);
	}
}

static inline off_t first_zone_data(void)
{
	return 2 + get_nimaps() + get_nzmaps() + inode_blocks();
}

static inline size_t get_inode_buffer_size(void)
{
	return inode_blocks() * MINIX_BLOCK_SIZE;
}

#endif				/* UTIL_LINUX_MINIX_PROGRAMS_H */
