/* stdopen.c - ensure that the three standard file descriptors are in use

   Copyright (C) 2005-2006, 2019-2023 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert and Jim Meyering.  */

#include <config.h>

#include "stdopen.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* Try to ensure that all of the standard file numbers (0, 1, 2)
   are in use.  Without this, each application would have to guard
   every call to open, dup, fopen, etc. with tests to ensure they
   don't use one of the special file numbers when opening a file.
   Return zero if successful, an errno value if at least one of
   the file descriptors is initially closed and could not be opened.  */

int
stdopen (void)
{
  int fd;
  for (fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++)
    {
      if (fcntl (fd, F_GETFD) < 0)
        {
          /* Open /dev/null with the contrary mode so that the typical
             read (stdin) or write (stdout, stderr) operation will fail.
             With descriptor 0, we can do even better on systems that
             have /dev/full, by opening that write-only instead of
             /dev/null.  The only drawback is that a write-provoked
             failure comes with a misleading errno value, ENOSPC.  */
          int mode = fd == STDIN_FILENO ? O_WRONLY : O_RDONLY;
          int full_fd = fd == STDIN_FILENO ? open ("/dev/full", mode) : -1;
          int new_fd = full_fd < 0 ? open ("/dev/null", mode) : full_fd;
          if (new_fd < 0)
            return errno;
          if (STDERR_FILENO < new_fd)
            {
              /* 0, 1, and 2 are already open somehow.
                 Our is not to reason why.  */
              close (new_fd);
              return 0;
            }
        }
    }

  return 0;
}
