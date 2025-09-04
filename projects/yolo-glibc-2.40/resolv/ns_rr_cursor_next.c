/* Simple DNS record parser without textual name decoding.
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

#include <arpa/nameser.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

bool
__ns_rr_cursor_next (struct ns_rr_cursor *c, struct ns_rr_wire *rr)
{
  rr->rdata = NULL;

  /* Extract the record owner name.  */
  int consumed = __ns_name_unpack (c->begin, c->end, c->current,
                                   rr->rname, sizeof (rr->rname));
  if (consumed < 0)
    {
      memset (rr, 0, sizeof (*rr));
      __set_errno (EMSGSIZE);
      return false;
    }
  c->current += consumed;

  /* Extract the metadata.  */
  struct
  {
    uint16_t rtype;
    uint16_t rclass;
    uint32_t ttl;
    uint16_t rdlength;
  } __attribute__ ((packed)) metadata;
  _Static_assert (sizeof (metadata) == 10, "sizeof metadata");
  if (c->end - c->current < sizeof (metadata))
    {
      memset (rr, 0, sizeof (*rr));
      __set_errno (EMSGSIZE);
      return false;
    }
  memcpy (&metadata, c->current, sizeof (metadata));
  c->current += sizeof (metadata);
  /* Endianness conversion.  */
  rr->rtype = ntohs (metadata.rtype);
  rr->rclass = ntohs (metadata.rclass);
  rr->ttl = ntohl (metadata.ttl);
  rr->rdlength = ntohs (metadata.rdlength);

  /* Extract record data.  */
  if (c->end - c->current < rr->rdlength)
    {
      memset (rr, 0, sizeof (*rr));
      __set_errno (EMSGSIZE);
      return false;
    }
  rr->rdata = c->current;
  c->current += rr->rdlength;

  return true;
}
