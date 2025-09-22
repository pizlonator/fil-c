/*
 * lsfd-pidfd.c - handle pidfd (from anon_inode or pidfs)
 *
 * Copyright (C) 2024 Xi Ruoyao <xry111@xry111.site>
 *
 * Refactored and moved out from lsfd-unkn.c (originally authored by
 * Masatake YAMATO <yamato@redhat.com>).
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

#include <string.h>

#include "strutils.h"
#include "xalloc.h"

#include "lsfd.h"
#include "lsfd-pidfd.h"

int pidfd_handle_fdinfo(struct pidfd_data *data, const char *key,
			const char *value)
{
	if (strcmp(key, "Pid") == 0) {
		uint64_t pid;
		int rc = ul_strtou64(value, &pid, 10);

		if (rc < 0)
			return 0; /* ignore -- parse failed */

		data->pid = (pid_t)pid;
		return 1;
	} else if (strcmp(key, "NSpid") == 0) {
		data->nspid = xstrdup(value);
		return 1;
	}

	return 0;
}

char *pidfd_get_name(struct pidfd_data *data)
{
	char *str = NULL;
	char *comm = NULL;
	struct proc *proc = get_proc(data->pid);

	if (proc)
		comm = proc->command;

	xasprintf(&str, "pid=%d comm=%s nspid=%s",
			  data->pid,
			  comm ? comm : "",
			  data->nspid ? data->nspid : "");
	return str;
}

bool pidfd_fill_column(struct pidfd_data *data, int column_id, char **str)
{
	switch(column_id) {
	case COL_PIDFD_COMM: {
		struct proc *pidfd_proc = get_proc(data->pid);
		char *pidfd_comm = NULL;

		if (pidfd_proc)
			pidfd_comm = pidfd_proc->command;
		if (pidfd_comm) {
			*str = xstrdup(pidfd_comm);
			return true;
		}
		break;
	}
	case COL_PIDFD_NSPID:
		if (data->nspid) {
			*str = xstrdup(data->nspid);
			return true;
		}
		break;
	case COL_PIDFD_PID:
		xasprintf(str, "%d", (int)data->pid);
		return true;
	}

	return false;
}
