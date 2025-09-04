/* Set source filter.  Linux version.
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
#include <string.h>
#include <netinet/in.h>
#include <scratch_buffer.h>
#include "getsourcefilter.h"


int
setsourcefilter (int s, uint32_t interface, const struct sockaddr *group,
		 socklen_t grouplen, uint32_t fmode, uint32_t numsrc,
		 const struct sockaddr_storage *slist)
{
  /* We have to create an struct ip_msfilter object which we can pass
     to the kernel.  */
  size_t needed = GROUP_FILTER_SIZE (numsrc);

  struct scratch_buffer buf;
  scratch_buffer_init (&buf);
  if (!scratch_buffer_set_array_size (&buf, 1, needed))
    return -1;
  struct group_filter *gf = buf.data;

  gf->gf_interface = interface;
  memcpy (&gf->gf_group, group, grouplen);
  gf->gf_fmode = fmode;
  gf->gf_numsrc = numsrc;
  memcpy (gf->gf_slist, slist, numsrc * sizeof (struct sockaddr_storage));

  /* We need to provide the appropriate socket level value.  */
  int result;
  int sol = __get_sol (group->sa_family, grouplen);
  if (sol == -1)
    {
      __set_errno (EINVAL);
      result = -1;
    }
  else
    result = __setsockopt (s, sol, MCAST_MSFILTER, gf, needed);

  scratch_buffer_free (&buf);

  return result;
}
