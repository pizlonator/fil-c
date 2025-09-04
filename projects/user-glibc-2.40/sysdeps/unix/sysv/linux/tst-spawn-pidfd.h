/* Tests for spawn pidfd extension.
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

#include <errno.h>
#include <spawn.h>
#include <support/check.h>

#define PID_T_TYPE int

/* Call posix_spawn with POSIX_SPAWN_PIDFD set.  */
static inline int
pidfd_spawn_check (int *pidfd, const char *path,
		   const posix_spawn_file_actions_t *fa,
		   const posix_spawnattr_t *attr, char *const argv[],
		   char *const envp[])
{
  int r = pidfd_spawn (pidfd, path, fa, attr, argv, envp);
  if (r == ENOSYS)
    FAIL_UNSUPPORTED ("kernel does not support CLONE_PIDFD clone flag");
  return r;
}

#define POSIX_SPAWN(__pidfd, __path, __actions, __attr, __argv, __envp)	     \
  pidfd_spawn_check (__pidfd, __path, __actions, __attr, __argv, __envp)

static inline int
pidfd_spawnp_check (int *pidfd, const char *file,
		    const posix_spawn_file_actions_t *fa,
		    const posix_spawnattr_t *attr,
		    char *const argv[], char *const envp[])
{
  int r = pidfd_spawnp (pidfd, file, fa, attr, argv, envp);
  if (r == ENOSYS)
    FAIL_UNSUPPORTED ("kernel does not support CLONE_PIDFD clone flag");
  return r;
}

#define POSIX_SPAWNP(__child, __path, __actions, __attr, __argv, __envp) \
  pidfd_spawnp_check (__child, __path, __actions, __attr, __argv, __envp)

#define WAITID(__idtype, __id, __info, __opts)				     \
  ({									     \
     __typeof (__idtype) __new_idtype = __idtype == P_PID		     \
					? P_PIDFD : __idtype;		     \
     waitid (__new_idtype, __id, __info, __opts);			     \
  })

#define TST_SPAWN_PIDFD 1
