/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2016 David Sterba <dsterba@suse.cz>
 * Copyright (C) 2016 Stanislav Brabec <sbrabec@suse.cz>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Based on kernel ctree.h, rbtree.h and btrfs-progs.
 */
#include <dirent.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdint.h>
#include <linux/btrfs.h>

#include "mountP.h"
#include "bitops.h"


/* linux/btrfs.h lacks large parts of stuff needed for getting default
 * sub-volume. Suppose that if BTRFS_DIR_ITEM_KEY is not defined, all
 * declarations are still missing.
 */
#ifndef BTRFS_DIR_ITEM_KEY

/*
 * dir items are the name -> inode pointers in a directory.  There is one
 * for every name in a directory.
 */
#define BTRFS_DIR_ITEM_KEY	84

/* holds pointers to all of the tree roots */
#define BTRFS_ROOT_TREE_OBJECTID 1ULL

/* directory objectid inside the root tree */
#define BTRFS_ROOT_TREE_DIR_OBJECTID 6ULL

/*
 * the key defines the order in the tree, and so it also defines (optimal)
 * block layout.  objectid corresponds with the inode number.  The flags
 * tells us things about the object, and is a kind of stream selector.
 * so for a given inode, keys with flags of 1 might refer to the inode
 * data, flags of 2 may point to file data in the btree and flags == 3
 * may point to extents.
 *
 * offset is the starting byte offset for this key in the stream.
 *
 * btrfs_disk_key is in disk byte order.  struct btrfs_key is always
 * in cpu native order.  Otherwise they are identical and their sizes
 * should be the same (ie both packed)
 */
struct btrfs_disk_key {
	uint64_t objectid; /* little endian */
	uint8_t type;
	uint64_t offset; /* little endian */
} __attribute__ ((__packed__));

struct btrfs_dir_item {
	struct btrfs_disk_key location;
	uint64_t transid; /* little endian */
	uint16_t data_len; /* little endian */
	uint16_t name_len; /* little endian */
	uint8_t type;
} __attribute__ ((__packed__));

#define BTRFS_SETGET_STACK_FUNCS(name, type, member, bits)		\
static inline uint##bits##_t btrfs_##name(const type *s)		\
{									\
	return le##bits##_to_cpu(s->member);				\
}

/* struct btrfs_disk_key */
BTRFS_SETGET_STACK_FUNCS(disk_key_objectid, struct btrfs_disk_key,
			 objectid, 64)

BTRFS_SETGET_STACK_FUNCS(stack_dir_name_len, struct btrfs_dir_item, name_len, 16)

/*
  Red Black Trees
*/
struct rb_node {
	unsigned long  __rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
    /* The alignment might seem pointless, but allegedly CRIS needs it */

#endif /* BTRFS_DIR_ITEM_KEY */

/*
 * btrfs_get_default_subvol_id:
 * @path: Path to mounted btrfs volume
 *
 * Searches for the btrfs default subvolume id.
 *
 * Returns: default subvolume id or UINT64_MAX (-1) in case of no
 * default subvolume or error. In case of error, errno is set
 * properly.
 */
uint64_t btrfs_get_default_subvol_id(const char *path)
{
	int iocret;
	int fd;
	DIR *dirstream;
	struct btrfs_ioctl_search_args args;
	struct btrfs_ioctl_search_key *sk = &args.key;
	struct btrfs_ioctl_search_header *sh;
	uint64_t found = UINT64_MAX;

	dirstream = opendir(path);
	if (!dirstream) {
		DBG(BTRFS, ul_debug("opendir() failed for \"%s\" [errno=%d %m]", path, errno));
		return UINT64_MAX;
	}
	fd = dirfd(dirstream);
	if (fd < 0) {
		DBG(BTRFS, ul_debug("dirfd(opendir()) failed for \"%s\" [errno=%d %m]", path, errno));
		goto out;
	}

	memset(&args, 0, sizeof(args));
	sk->tree_id = BTRFS_ROOT_TREE_OBJECTID;
	sk->min_objectid = BTRFS_ROOT_TREE_DIR_OBJECTID;
	sk->max_objectid = BTRFS_ROOT_TREE_DIR_OBJECTID;
	sk->min_type = BTRFS_DIR_ITEM_KEY;
	sk->max_type = BTRFS_DIR_ITEM_KEY;
	sk->max_offset = UINT64_MAX;
	sk->max_transid = UINT64_MAX;
	sk->nr_items = 1;

	iocret = ioctl(fd, BTRFS_IOC_TREE_SEARCH, &args);
	if (iocret < 0) {
		DBG(BTRFS, ul_debug("ioctl() failed for \"%s\" [errno=%d %m]", path, errno));
		goto out;
	}

	/* the ioctl returns the number of items it found in nr_items */
	if (sk->nr_items == 0) {
		DBG(BTRFS, ul_debug("root tree dir object id not found"));
		goto out;
	}
	DBG(BTRFS, ul_debug("found %d root tree dir object id items", sk->nr_items));

	sh = (struct btrfs_ioctl_search_header *)args.buf;

	if (sh->type == BTRFS_DIR_ITEM_KEY) {
		struct btrfs_dir_item *di;
		int name_len;
		char *name;

		di = (struct btrfs_dir_item *)(sh + 1);
		name_len = btrfs_stack_dir_name_len(di);
		name = (char *)(di + 1);

		if (!strncmp("default", name, name_len)) {
			found = btrfs_disk_key_objectid(&di->location);
			DBG(BTRFS, ul_debug("\"default\" id is %llu", (unsigned long long)found));
		} else {
			DBG(BTRFS, ul_debug("\"default\" id not found in tree root"));
			goto out;
		}
	} else {
		DBG(BTRFS, ul_debug("unexpected type found: %d", (int)sh->type));
		goto out;
	}

out:
	closedir(dirstream);
	return found;
}
