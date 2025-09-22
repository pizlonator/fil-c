/* Copyright (C) 2005, 2008 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2007.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <system.h>
#include "libasmP.h"


struct buffer
{
  char *buf;
  size_t len;
};


static int
buffer_cb (char *str, size_t len, void *arg)
{
  struct buffer *buffer = (struct buffer *) arg;

  if (len > buffer->len)
    /* Return additional needed space.  */
    return len - buffer->len;

  buffer->buf = mempcpy (buffer->buf, str, len);
  buffer->len = len;

  return 0;
}


int
disasm_str (DisasmCtx_t *ctx, const uint8_t **startp, const uint8_t *end,
	    GElf_Addr addr, const char *fmt, char **bufp, size_t len,
	    void *symcbarg)
{
  struct buffer buffer = { .buf = *bufp, .len = len };

  int res = INTUSE(disasm_cb) (ctx, startp, end, addr, fmt, buffer_cb, &buffer,
			       symcbarg);
  *bufp = buffer.buf;
  return res;
}
