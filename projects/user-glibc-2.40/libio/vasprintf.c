/* Copyright (C) 1995-2024 Free Software Foundation, Inc.
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
   <https://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  */

#include <array_length.h>
#include <errno.h>
#include <limits.h>
#include <math_ldbl_opt.h>
#include <printf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <printf_buffer.h>

struct __printf_buffer_asprintf
{
  /* base.write_base points either to a heap-allocated buffer, or to
     the direct array below.  */
  struct __printf_buffer base;

  /* Initial allocation.  200 should be large enough to copy almost
     all asprintf usages with just a single (final, correctly sized)
     heap allocation.  */
  char direct[PRINTF_BUFFER_SIZE_ASPRINTF];
};

void
__printf_buffer_flush_asprintf (struct __printf_buffer_asprintf *buf)
{
  size_t current_pos = buf->base.write_ptr - buf->base.write_base;
  if (current_pos >= INT_MAX)
    {
      /* The result is not representable.  No need to continue.  */
      __set_errno (EOVERFLOW);
      __printf_buffer_mark_failed (&buf->base);
      return;
    }

  size_t current_size = buf->base.write_end - buf->base.write_base;
  /* Implement an exponentiation sizing policy.  Keep the size
     congruent 8 (mod 16), to account for the footer in glibc
     malloc.  */
  size_t new_size = ALIGN_UP (current_size + current_size / 2, 16) | 8;
  char *new_buffer;
  if (buf->base.write_base == buf->direct)
    {
      new_buffer = malloc (new_size);
      if (new_buffer == NULL)
	{
	  __printf_buffer_mark_failed (&buf->base);
	  return;
	}
      memcpy (new_buffer, buf->direct, current_pos);
    }
  else
    {
      new_buffer = realloc (buf->base.write_base, new_size);
      if (new_buffer == NULL)
	{
	  __printf_buffer_mark_failed (&buf->base);
	  return;
	}
    }

  /* Set up the new write area.  */
  buf->base.write_base = new_buffer;
  buf->base.write_ptr = new_buffer + current_pos;
  buf->base.write_end = new_buffer + new_size;
}


int
__vasprintf_internal (char **result_ptr, const char *format, va_list args,
		      unsigned int mode_flags)
{
  struct __printf_buffer_asprintf buf;
  __printf_buffer_init (&buf.base, buf.direct, array_length (buf.direct),
			__printf_buffer_mode_asprintf);

  __printf_buffer (&buf.base, format, args, mode_flags);
  int done = __printf_buffer_done (&buf.base);
  if (done < 0)
    {
      if (buf.base.write_base != buf.direct)
	free (buf.base.write_base);
      return done;
    }

  /* Transfer to the final buffer.  */
  char *result;
  size_t size = buf.base.write_ptr - buf.base.write_base;
  if (buf.base.write_base == buf.direct)
    {
      result = malloc (size + 1);
      if (result == NULL)
	return -1;
      memcpy (result, buf.direct, size);
    }
  else
    {
      result = realloc (buf.base.write_base, size + 1);
      if (result == NULL)
	{
	  free (buf.base.write_base);
	  return -1;
	}
    }

  /* Add NUL termination.  */
  result[size] = '\0';
  *result_ptr = result;

  return done;
}

int
__vasprintf (char **result_ptr, const char *format, va_list args)
{
  return __vasprintf_internal (result_ptr, format, args, 0);
}
ldbl_weak_alias (__vasprintf, vasprintf)
