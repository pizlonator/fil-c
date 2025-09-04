/* Set the sun_path member of struct sockaddr_un.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

int
__sockaddr_un_set (struct sockaddr_un *addr, const char *pathname)
{
  size_t name_length = strlen (pathname);

  /* The kernel supports names of exactly sizeof (addr->sun_path)
     bytes, without a null terminator, but userspace does not; see the
     SUN_LEN macro.  */
  if (name_length >= sizeof (addr->sun_path))
    {
      __set_errno (EINVAL);     /* Error code used by the kernel.  */
      return -1;
    }

  addr->sun_family = AF_UNIX;
  memcpy (addr->sun_path, pathname, name_length + 1);
  return 0;
}
