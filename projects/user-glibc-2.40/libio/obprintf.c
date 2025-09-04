/* Print output of stream to given obstack.
   Copyright (C) 1996-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <math_ldbl_opt.h>
#include <obstack.h>
#include <printf.h>
#include <stdarg.h>
#include <printf_buffer.h>

struct __printf_buffer_obstack
{
  struct __printf_buffer base;
  struct obstack *obstack;

  /* obstack_1grow is called for compatibility reasons.  This needs
     one extra character, and this is the backing store for it.  */
  char ch;
};

void
__printf_buffer_flush_obstack (struct __printf_buffer_obstack *buf)
{
  /* About to switch buffers, so record the bytes written so far.  */
  buf->base.written += buf->base.write_ptr - buf->base.write_base;

  if (buf->base.write_ptr == &buf->ch + 1)
    {
      /* Errors are reported via a callback mechanism (presumably for
	 process termination).  */
      obstack_1grow (buf->obstack, buf->ch);
      buf->base.write_base = obstack_next_free (buf->obstack);
      buf->base.write_ptr = buf->base.write_base;
      size_t size = obstack_room (buf->obstack);
      buf->base.write_end = buf->base.write_ptr + size;
      /* Reserve the space on the obstack size.  */
      obstack_blank_fast (buf->obstack, size);
    }
  else
    {
      /* Obtain the extra character.  */
      buf->base.write_base = &buf->ch;
      buf->base.write_ptr = &buf->ch;
      buf->base.write_end = &buf->ch + 1;
    }
}

int
__obstack_vprintf_internal (struct obstack *obstack, const char *format,
			    va_list args, unsigned int mode_flags)
{
  /* Legacy setup code for compatibility.  */
  size_t room = obstack_room (obstack);
  size_t size = obstack_object_size (obstack) + room;
  if (size == 0)
    {
      /* Get more memory.  */
      obstack_make_room (obstack, 64);

      /* Recompute how much room we have.  */
      room = obstack_room (obstack);
      size = room;

      assert (size != 0);
    }

  struct __printf_buffer_obstack buf;
  {
    /* The obstack write location might be in the middle of an object.  */
    char *ptr = obstack_next_free (obstack);
    char *end = obstack_base (obstack) + size;
    __printf_buffer_init (&buf.base, ptr, end - ptr,
			  __printf_buffer_mode_obstack);
  }
  buf.obstack = obstack;

  /* Now allocate the rest of the current chunk.  */
  obstack_blank_fast (obstack, room);

  __printf_buffer (&buf.base, format, args, mode_flags);

  if (buf.base.write_ptr == &buf.ch + 1)
    /* buf.ch is in use.  Put it into the obstack.  */
    obstack_1grow (buf.obstack, buf.ch);
  else if (buf.base.write_ptr != &buf.ch)
    /* Shrink the buffer to the space we really currently need.  */
    obstack_blank_fast (buf.obstack, buf.base.write_ptr - buf.base.write_end);

  return __printf_buffer_done (&buf.base);
}

int
__obstack_vprintf (struct obstack *obstack, const char *format, va_list ap)
{
  return __obstack_vprintf_internal (obstack, format, ap, 0);
}
ldbl_weak_alias (__obstack_vprintf, obstack_vprintf)

int
__obstack_printf (struct obstack *obstack, const char *format, ...)
{
  int result;
  va_list ap;
  va_start (ap, format);
  result = __obstack_vprintf_internal (obstack, format, ap, 0);
  va_end (ap);
  return result;
}
ldbl_weak_alias (__obstack_printf, obstack_printf)
