/* Generic definitions for posix_spawn tests.
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

#ifndef PID_T_TYPE
# define PID_T_TYPE pid_t
#endif

#ifndef POSIX_SPAWN
# define POSIX_SPAWN(__child, __path, __actions, __attr, __argv, __envp) \
  posix_spawn (__child, __path, __actions, __attr, __argv, __envp)
#endif

#ifndef POSIX_SPAWNP
# define POSIX_SPAWNP(__child, __path, __actions, __attr, __argv, __envp) \
  posix_spawnp (__child, __path, __actions, __attr, __argv, __envp)
#endif

#ifndef WAITID
# define WAITID(__idtype, __id, __info, __opts) \
  waitid (__idtype, __id, __info, __opts)
#endif
