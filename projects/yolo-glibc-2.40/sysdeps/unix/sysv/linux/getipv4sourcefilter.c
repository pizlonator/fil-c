/* Get IPv4 source filter.  Linux version.
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
#include <scratch_buffer.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/socket.h>


int
getipv4sourcefilter (int s, struct in_addr interface, struct in_addr group,
		     uint32_t *fmode, uint32_t *numsrc, struct in_addr *slist)
{
  /* We have to create an struct ip_msfilter object which we can pass
     to the kernel.  */
  socklen_t needed = IP_MSFILTER_SIZE (*numsrc);

  struct scratch_buffer buf;
  scratch_buffer_init (&buf);
  if (!scratch_buffer_set_array_size (&buf, 1, needed))
    return -1;
  struct ip_msfilter *imsf = buf.data;

  imsf->imsf_multiaddr = group;
  imsf->imsf_interface = interface;
  imsf->imsf_numsrc = *numsrc;

  int result = __getsockopt (s, SOL_IP, IP_MSFILTER, imsf, &needed);

  /* If successful, copy the results to the places the caller wants
     them in.  */
  if (result == 0)
    {
      *fmode = imsf->imsf_fmode;
      memcpy (slist, imsf->imsf_slist,
	      MIN (*numsrc, imsf->imsf_numsrc) * sizeof (struct in_addr));
      *numsrc = imsf->imsf_numsrc;
    }

  scratch_buffer_free (&buf);

  return result;
}
