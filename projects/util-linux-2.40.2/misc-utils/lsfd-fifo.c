/*
 * lsfd-fifo.c - handle associations opening fifo objects
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

#include "lsfd.h"

struct fifo {
	struct file file;
	struct ipc_endpoint endpoint;
};

struct fifo_ipc {
	struct ipc ipc;
	ino_t ino;
};

static inline char *fifo_xstrendpoint(struct file *file)
{
	char *str = NULL;
	xasprintf(&str, "%d,%s,%d%c%c",
		  file->proc->pid, file->proc->command, file->association,
		  (file->mode & S_IRUSR)? 'r': '-',
		  (file->mode & S_IWUSR)? 'w': '-');
	return str;
}

static bool fifo_fill_column(struct proc *proc __attribute__((__unused__)),
			     struct file *file,
			     struct libscols_line *ln,
			     int column_id,
			     size_t column_index)
{
	char *str = NULL;

	switch(column_id) {
	case COL_TYPE:
		if (scols_line_set_data(ln, column_index, "FIFO"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_SOURCE:
		if (major(file->stat.st_dev) == 0
		    && strncmp(file->name, "pipe:", 5) == 0) {
			str = xstrdup("pipefs");
			break;
		}
		return false;
	case COL_ENDPOINTS: {
		struct fifo *this = (struct fifo *)file;
		struct list_head *e;
		foreach_endpoint(e, this->endpoint) {
			char *estr;
			struct fifo *other = list_entry(e, struct fifo, endpoint.endpoints);
			if (this == other)
				continue;
			if (str)
				xstrputc(&str, '\n');
			estr = fifo_xstrendpoint(&other->file);
			xstrappend(&str, estr);
			free(estr);
		}
		if (!str)
			return false;
		break;
	}
	default:
		return false;
	}

	if (!str)
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_refer_data(ln, column_index, str))
		err(EXIT_FAILURE, _("failed to add output data"));
	return true;
}

static unsigned int fifo_get_hash(struct file *file)
{
	return (unsigned int)(file->stat.st_ino % UINT_MAX);
}

static bool fifo_is_suitable_ipc(struct ipc *ipc, struct file *file)
{
	return ((struct fifo_ipc *)ipc)->ino == file->stat.st_ino;
}

static const struct ipc_class *fifo_get_ipc_class(struct file *file __attribute__((__unused__)))
{
	static const struct ipc_class fifo_ipc_class = {
		.size = sizeof(struct fifo_ipc),
		.get_hash = fifo_get_hash,
		.is_suitable_ipc = fifo_is_suitable_ipc,
		.free = NULL,
	};
	return &fifo_ipc_class;
}

static void fifo_initialize_content(struct file *file)
{
	struct fifo *fifo = (struct fifo *)file;
	struct ipc *ipc;
	unsigned int hash;

	init_endpoint(&fifo->endpoint);
	ipc = get_ipc(file);
	if (ipc)
		goto link;

	ipc = new_ipc(fifo_get_ipc_class(file));
	((struct fifo_ipc *)ipc)->ino = file->stat.st_ino;

	hash = fifo_get_hash(file);
	add_ipc(ipc, hash);
 link:
	add_endpoint(&fifo->endpoint, ipc);
}

const struct file_class fifo_class = {
	.super = &file_class,
	.size = sizeof(struct fifo),
	.fill_column = fifo_fill_column,
	.initialize_content = fifo_initialize_content,
	.free_content = NULL,
	.get_ipc_class = fifo_get_ipc_class,
};
