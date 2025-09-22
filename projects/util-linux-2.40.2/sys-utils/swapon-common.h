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
#ifndef UTIL_LINUX_SWAPON_COMMON_H
#define UTIL_LINUX_SWAPON_COMMON_H

#include <libmount.h>

extern struct libmnt_cache *mntcache;

extern struct libmnt_table *get_fstab(const char *filename);
extern struct libmnt_table *get_swaps(void);
extern void free_tables(void);

extern int match_swap(struct libmnt_fs *fs, void *data);
extern int is_active_swap(const char *filename);

extern int cannot_find(const char *special);

extern void add_label(const char *label);
extern const char *get_label(size_t i);
extern size_t numof_labels(void);

extern void add_uuid(const char *uuid);
extern const char *get_uuid(size_t i);
extern size_t numof_uuids(void);

#endif /* UTIL_LINUX_SWAPON_COMMON_H */
