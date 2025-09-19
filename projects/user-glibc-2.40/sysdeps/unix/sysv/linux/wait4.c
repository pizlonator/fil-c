/* Wait for process to change state.  Linux version.
   Copyright (C) 2019-2024 Free Software Foundation, Inc.
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

#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sysdep-cancel.h>
#include <tv32-compat.h>
#include <pizlonated_syscalls.h>

pid_t
__wait4_time64 (pid_t pid, int *stat_loc, int options, struct __rusage64 *usage)
{
  return zsys_wait4 (pid, stat_loc, options, usage);
}

#if __TIMESIZE != 64
libc_hidden_def (__wait4_time64)

pid_t
__wait4 (pid_t pid, int *stat_loc, int options, struct rusage *usage)
{
  pid_t ret;
  struct __rusage64 usage64;

  ret = __wait4_time64 (pid, stat_loc, options,
                        usage != NULL ? &usage64 : NULL);

  if (ret > 0 && usage != 0)
    rusage64_to_rusage (&usage64, usage);

  return ret;
}
#endif
libc_hidden_def (__wait4);
weak_alias (__wait4, wait4)
