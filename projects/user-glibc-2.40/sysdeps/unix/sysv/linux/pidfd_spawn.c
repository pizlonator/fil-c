/* pidfd_spawn - Spawn a process and return a PID file descriptor.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <spawn.h>
#include "spawn_int.h"

int
pidfd_spawn (int *pidfd, const char *path,
	     const posix_spawn_file_actions_t *file_actions,
	     const posix_spawnattr_t *attrp, char *const argv[],
	     char *const envp[])
{
  return __spawni (pidfd, path, file_actions, attrp, argv, envp,
		   SPAWN_XFLAGS_RET_PIDFD);
}
