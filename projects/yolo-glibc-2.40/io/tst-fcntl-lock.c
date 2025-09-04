/* Test for advisory record locking.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

/* This is essentially the POSIX lockf.  */

static int
fcntl_lockf (int fd, int cmd, off_t len)
{
  struct flock fl = {
    .l_type = F_WRLCK,
    .l_whence = SEEK_CUR,
    .l_len = len
  };

  switch (cmd)
    {
    case F_TEST:
      fl.l_type = F_RDLCK;
      if (fcntl (fd, F_GETLK, &fl) < 0)
	return -1;
      if (fl.l_type == F_UNLCK || fl.l_pid == getpid ())
	return 0;
      errno = EACCES;
      return -1;

    case F_ULOCK:
      fl.l_type = F_UNLCK;
      return fcntl (fd, F_SETLK, &fl);

    case F_LOCK:
      return fcntl (fd, F_SETLKW, &fl);

    case F_TLOCK:
      return fcntl (fd, F_SETLK, &fl);
    }

  errno = EINVAL;
  return -1;
}

static int
fcntl64_lockf (int fd, int cmd, off64_t len64)
  {
  struct flock64 fl64 = {
    .l_type = F_WRLCK,
    .l_whence = SEEK_CUR,
    .l_len = len64
  };

  switch (cmd)
    {
    case F_TEST:
      fl64.l_type = F_RDLCK;
      if (fcntl64 (fd, F_GETLK64, &fl64) < 0)
	return -1;
      if (fl64.l_type == F_UNLCK || fl64.l_pid == getpid ())
	return 0;
      errno = EACCES;
      return -1;

    case F_ULOCK:
      fl64.l_type = F_UNLCK;
      return fcntl64 (fd, F_SETLK64, &fl64);

    case F_LOCK:
      return fcntl64 (fd, F_SETLKW64, &fl64);

    case F_TLOCK:
      return fcntl64 (fd, F_SETLK64, &fl64);
    }

  errno = EINVAL;
  return -1;
}

#define TST_LOCKFD  "tst-fcntl-lock."
#define LOCKF       fcntl_lockf
#define LOCKF64     fcntl64_lockf
#include "tst-lockf.c"
