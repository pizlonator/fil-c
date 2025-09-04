/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <unistd.h>
#include <hurd.h>
#include <hurd/paths.h>
#include <hurd/fd.h>
#include <fcntl.h>
#include <string.h>

/* Read the contents of the symbolic link FILE_NAME relative to FD into no more
   than LEN bytes of BUF.  The contents are not null-terminated.
   Returns the number of characters read, or -1 for errors.  */
ssize_t
__readlinkat (int fd, const char *file_name, char *buf, size_t len)
{
  error_t err;
  file_t file_stat;
  struct stat64 st;
  enum retry_type doretry;
  char retryname[1024];
  file_t file;
  char *rbuf = buf;
  mach_msg_type_number_t nread = len;

  file_stat = __file_name_lookup_at (fd, 0, file_name, O_NOLINK, 0);
  if (file_stat == MACH_PORT_NULL)
    return -1;

  err = __io_stat (file_stat, &st);
  if (err)
    goto out;
  if (!S_ISLNK (st.st_mode))
    {
      err = EINVAL;
      goto out;
    }

  err = __dir_lookup (file_stat, "", O_READ | O_NOLINK,
                      0, &doretry, retryname, &file);
  if (err)
    goto out;
  if (doretry != FS_RETRY_NORMAL || retryname[0] != '\0')
    {
      err = EGRATUITOUS;
      goto out;
    }

  err = __io_read (file, &rbuf, &nread, 0, len);
  __mach_port_deallocate (__mach_task_self (), file);
  if (err)
    goto out;

  len = nread;
  if (rbuf != buf)
    {
      memcpy (buf, rbuf, len);
      __vm_deallocate (__mach_task_self (), (vm_address_t) rbuf, nread);
    }


 out:
  __mach_port_deallocate (__mach_task_self (), file_stat);

  return err ? __hurd_fail (err) : len;
}
weak_alias (__readlinkat, readlinkat)
libc_hidden_def (readlinkat)
