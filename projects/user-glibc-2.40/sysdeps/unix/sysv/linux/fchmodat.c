/* Change the protections of file relative to open directory.  Linux version.
   Copyright (C) 2006-2024 Free Software Foundation, Inc.
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
#include <fcntl.h>
#include <fd_to_filename.h>
#include <not-cancel.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysdep.h>
#include <unistd.h>
#include <pizlonated_syscalls.h>

int
fchmodat (int fd, const char *file, mode_t mode, int flag)
{
  return zsys_fchmodat (fd, file, mode, flag);
}
libc_hidden_def (fchmodat)
