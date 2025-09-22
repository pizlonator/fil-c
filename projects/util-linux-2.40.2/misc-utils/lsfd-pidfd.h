/*
 * lsfd-pidfd.h - handle pidfd (from anon_inode or pidfs)
 *
 * Copyright (C) 2024 Xi Ruoyao <xry111@xry111.site>
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

#include <stdbool.h>
#include <sys/types.h>

struct pidfd_data {
	pid_t pid;
	char *nspid;
};

int pidfd_handle_fdinfo(struct pidfd_data *, const char *, const char *);
char *pidfd_get_name(struct pidfd_data *);
bool pidfd_fill_column(struct pidfd_data *, int, char **);

static inline void __attribute__((nonnull(1)))
pidfd_free(struct pidfd_data *data)
{
	free(data->nspid);
}
