/*
 * lsfd-sock.c - handle associations opening socket objects
 *
 * Copyright (C) 2021 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
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

#include <sys/types.h>
#include <sys/xattr.h>

#include "lsfd.h"
#include "lsfd-sock.h"

static void attach_sock_xinfo(struct file *file)
{
	struct sock *sock = (struct sock *)file;

	sock->xinfo = get_sock_xinfo(file->stat.st_ino);
	if (sock->xinfo) {
		struct ipc *ipc = get_ipc(file);
		if (ipc)
			add_endpoint(&sock->endpoint, ipc);
	}
}

static const struct ipc_class *sock_get_ipc_class(struct file *file)
{
	struct sock *sock = (struct sock *)file;

	if (sock->xinfo && sock->xinfo->class->get_ipc_class)
		return sock->xinfo->class->get_ipc_class(sock->xinfo, sock);

	return NULL;
}

static bool sock_fill_column(struct proc *proc __attribute__((__unused__)),
			     struct file *file,
			     struct libscols_line *ln,
			     int column_id,
			     size_t column_index)
{
	char *str = NULL;
	struct sock *sock = (struct sock *)file;

	if (sock->xinfo && sock->xinfo->class
	    && sock->xinfo->class->fill_column) {
		if (sock->xinfo->class->fill_column(proc, sock->xinfo, sock, ln,
						    column_id, column_index,
						    &str))
			goto out;
	}

	switch(column_id) {
	case COL_TYPE:
		if (!sock->protoname)
			return false;
		/* FALL THROUGH */
	case COL_SOCK_PROTONAME:
		if (sock->protoname)
			if (scols_line_set_data(ln, column_index, sock->protoname))
				err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_NAME:
		if (sock->xinfo
		    && sock->xinfo->class && sock->xinfo->class->get_name) {
			str = sock->xinfo->class->get_name(sock->xinfo, sock);
			if (str)
				break;
		}
		return false;
	case COL_SOURCE:
		if (major(file->stat.st_dev) == 0
		    && strncmp(file->name, "socket:", 7) == 0) {
			str = xstrdup("sockfs");
			break;
		}
		return false;
	case COL_SOCK_NETNS:
		if (sock->xinfo) {
			xasprintf(&str, "%llu",
				  (unsigned long long)sock->xinfo->netns_inode);
			break;
		}
		return false;
	case COL_SOCK_TYPE:
		if (sock->xinfo
		    && sock->xinfo->class && sock->xinfo->class->get_type) {
			str = sock->xinfo->class->get_type(sock->xinfo, sock);
			if (str)
				break;
		}
		return false;
	case COL_SOCK_STATE:
		if (sock->xinfo
		    && sock->xinfo->class && sock->xinfo->class->get_state) {
			str = sock->xinfo->class->get_state(sock->xinfo, sock);
			if (str)
				break;
		}
		return false;
	case COL_SOCK_LISTENING:
		str = xstrdup((sock->xinfo
			       && sock->xinfo->class
			       && sock->xinfo->class->get_listening
			       && sock->xinfo->class->get_listening(sock->xinfo, sock))
			      ? "1"
			      : "0");
		break;
	case COL_SOCK_SHUTDOWN:
		str = xstrdup("??");
		break;
	default:
		return false;
	}

 out:
	if (!str)
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_refer_data(ln, column_index, str))
		err(EXIT_FAILURE, _("failed to add output data"));
	return true;
}

static void init_sock_content(struct file *file)
{
	int fd;
	struct sock *sock = (struct sock *)file;

	assert(file);

	fd = file->association;

	if (fd >= 0 || fd == -ASSOC_MEM || fd == -ASSOC_SHM) {
		char path[PATH_MAX] = {'\0'};
		char buf[256];
		ssize_t len;

		assert(file->proc);

		if (is_opened_file(file))
			sprintf(path, "/proc/%d/fd/%d", file->proc->pid, fd);
		else
			sprintf(path, "/proc/%d/map_files/%"PRIx64 "-%" PRIx64,
				file->proc->pid,
				file->map_start,
				file->map_end);

		len = getxattr(path, "system.sockprotoname", buf, sizeof(buf) - 1);
		if (len > 0) {
			buf[len] = '\0';
			sock->protoname = xstrdup(buf);
		}
	}

	init_endpoint(&sock->endpoint);
}

static void free_sock_content(struct file *file)
{
	struct sock *sock = (struct sock *)file;
	if (sock->protoname) {
		free(sock->protoname);
		sock->protoname = NULL;
	}
}

static void initialize_sock_class(void)
{
	initialize_sock_xinfos();
}

static void finalize_sock_class(void)
{
	finalize_sock_xinfos();
}

const struct file_class sock_class = {
	.super = &file_class,
	.size = sizeof(struct sock),
	.fill_column = sock_fill_column,
	.attach_xinfo = attach_sock_xinfo,
	.initialize_content = init_sock_content,
	.free_content = free_sock_content,
	.initialize_class = initialize_sock_class,
	.finalize_class = finalize_sock_class,
	.get_ipc_class = sock_get_ipc_class,
};
