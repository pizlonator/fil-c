/* Convert a struct ether_addr object to a string.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <support/format_nss.h>

#include <errno.h>
#include <netinet/ether.h>
#include <support/support.h>

char *
support_format_ether_addr (const struct ether_addr *e)
{
  if (e == NULL)
    return xasprintf ("error: (errno %d, %m)\n", errno);

  char buf[sizeof ("xx:xx:xx:xx:xx:xx")];
  return xasprintf ("address: %s\n", ether_ntoa_r (e, buf));
}
