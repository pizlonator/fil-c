/* Set IPv4 source filter.  Linux version.
   Copyright (C) 2004-2024 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <scratch_buffer.h>
#include <sys/socket.h>


int
setipv4sourcefilter (int s, struct in_addr interface, struct in_addr group,
		     uint32_t fmode, uint32_t numsrc,
		     const struct in_addr *slist)
{
  /* We have to create an struct ip_msfilter object which we can pass
     to the kernel.  */
  size_t needed = IP_MSFILTER_SIZE (numsrc);

  struct scratch_buffer buf;
  scratch_buffer_init (&buf);
  if (!scratch_buffer_set_array_size (&buf, 1, needed))
    return -1;
  struct ip_msfilter *imsf = buf.data;

  imsf->imsf_multiaddr = group;
  imsf->imsf_interface = interface;
  imsf->imsf_fmode = fmode;
  imsf->imsf_numsrc = numsrc;
  memcpy (imsf->imsf_slist, slist, numsrc * sizeof (struct in_addr));

  int result = __setsockopt (s, SOL_IP, IP_MSFILTER, imsf, needed);

  scratch_buffer_free (&buf);

  return result;
}
