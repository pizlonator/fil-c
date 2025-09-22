/*
 * lsfd(1) - list file descriptors
 *
 * Copyright (C) 2022 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * Very generally based on lsof(8) by Victor A. Abell <abe@purdue.edu>
 * It supports multiple OSes. lsfd specializes to Linux.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef UTIL_LINUX_LSFD_SOCK_H
#define UTIL_LINUX_LSFD_SOCK_H

#include <stdbool.h>
#include <sys/stat.h>

#include "libsmartcols.h"

/*
 * xinfo: eXtra inforation about sockets
 */
struct sock_xinfo {
	ino_t inode;		/* inode in sockfs */
	ino_t netns_inode;	/* inode of netns where
				   the socket belongs to */
	const struct sock_xinfo_class *class;
};

struct sock {
	struct file file;
	char *protoname;
	struct sock_xinfo *xinfo;
	struct ipc_endpoint endpoint;
};

struct sock_xinfo_class {
	/* Methods for filling socket related columns */
	char * (*get_name)(struct sock_xinfo *, struct sock *);
	char * (*get_type)(struct sock_xinfo *, struct sock *);
	char * (*get_state)(struct sock_xinfo *, struct sock *);
	bool (*get_listening)(struct sock_xinfo *, struct sock *);
	/* Method for class specific columns.
	 * Return true when the method fills the column. */
	bool (*fill_column)(struct proc *,
			    struct sock_xinfo *,
			    struct sock *,
			    struct libscols_line *,
			    int,
			    size_t,
			    char **str);
	struct ipc_class *(*get_ipc_class)(struct sock_xinfo *, struct sock *);

	void (*free)(struct sock_xinfo *);
};

void initialize_sock_xinfos(void);
void finalize_sock_xinfos(void);

struct sock_xinfo *get_sock_xinfo(ino_t inode);

#endif /* UTIL_LINUX_LSFD_SOCK_H */
