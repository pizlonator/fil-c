/* Initialize a simple DNS packet parser.
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
__ns_rr_cursor_init (struct ns_rr_cursor *c,
                     const unsigned char *buf, size_t len)
{
  c->begin = buf;
  c->end = buf + len;

  /* Check for header size and 16-bit question count value (it must be 1).  */
  if (len < 12 || buf[4] != 0 || buf[5] != 1)
    {
      __set_errno (EMSGSIZE);
      c->current = c->end;
      return false;
    }
  c->current = buf + 12;

  int consumed = __ns_name_length_uncompressed (c->current, c->end);
  if (consumed < 0)
    {
      __set_errno (EMSGSIZE);
      c->current = c->end;
      c->first_rr = NULL;
      return false;
    }
  c->current += consumed;

  /* Ensure there is room for question type and class.  */
  if (c->end - c->current < 4)
    {
      __set_errno (EMSGSIZE);
      c->current = c->end;
      c->first_rr = NULL;
      return false;
    }
  c->current += 4;
  c->first_rr = c->current;

  return true;
}
