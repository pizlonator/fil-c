/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2014-2023 Karel Zak <kzak@redhat.com>
 */
#ifndef UTIL_LINUX_FDISK_H
#define UTIL_LINUX_FDISK_H
/*
 *   fdisk.h
 */

#include "c.h"
#include <assert.h>
#include <libfdisk.h>

/* Let's temporary include private libfdisk header file. The final libfdisk.h
 * maybe included when fdisk.c and libfdisk code will be completely spit.
 */
#include "blkdev.h"
#include "colors.h"
#include "debug.h"
#include "nls.h"

#include "fdisk-list.h"

#define FDISKPROG_DEBUG_INIT	(1 << 1)
#define FDISKPROG_DEBUG_MENU	(1 << 3)
#define FDISKPROG_DEBUG_MISC	(1 << 4)
#define FDISKPROG_DEBUG_ASK	(1 << 5)
#define FDISKPROG_DEBUG_ALL	0xFFFF

extern int pwipemode;
extern struct fdisk_table *original_layout;
extern int device_is_used;
extern int is_interactive;

UL_DEBUG_DECLARE_MASK(fdisk);
#define DBG(m, x)       __UL_DBG(fdisk, FDISKPROG_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(fdisk, FDISKPROG_DEBUG_, m, x)

extern int get_user_reply(const char *prompt,
			  char *buf, size_t bufsz);
extern int process_fdisk_menu(struct fdisk_context **cxt);

extern int ask_callback(struct fdisk_context *cxt, struct fdisk_ask *ask,
		    void *data __attribute__((__unused__)));

extern int print_partition_info(struct fdisk_context *cxt);

/* prototypes for fdisk.c */
extern void dump_firstsector(struct fdisk_context *cxt);
extern void dump_disklabel(struct fdisk_context *cxt);

extern void list_partition_types(struct fdisk_context *cxt);
extern void change_partition_type(struct fdisk_context *cxt);

extern void toggle_dos_compatibility_flag(struct fdisk_context *cxt);

extern void follow_wipe_mode(struct fdisk_context *cxt);

extern void resize_partition(struct fdisk_context *cxt);

#endif /* UTIL_LINUX_FDISK_H */
