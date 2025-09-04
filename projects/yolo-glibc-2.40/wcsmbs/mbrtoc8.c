/* Multibyte to UTF-8 conversion.
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

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <gconv.h>
#include <uchar.h>
#include <wcsmbsload.h>

#ifndef EILSEQ
# define EILSEQ EINVAL
#endif


/* This is the private state used if PS is NULL.  */
static mbstate_t state;

size_t
mbrtoc8 (char8_t *pc8, const char *s, size_t n, mbstate_t *ps)
{
  /* This implementation depends on the converter invoked by mbrtowc not
     needing to retain state in either the top most bit of ps->__count or
     in ps->__value between invocations.  This implementation uses the
     top most bit of ps->__count to indicate that trailing code units are
     yet to be written and uses ps->__value to store those code units.  */

  if (ps == NULL)
    ps = &state;

  /* If state indicates that trailing code units are yet to be written, write
     those first regardless of whether 's' is a null pointer.  */
  if (ps->__count & 0x80000000)
    {
      /* ps->__value.__wchb[3] stores the index of the next code unit to
         write.  Code units are stored in reverse order.  */
      size_t i = ps->__value.__wchb[3];
      if (pc8 != NULL)
	{
	  *pc8 = ps->__value.__wchb[i];
	}
      if (i == 0)
	{
	  ps->__count &= 0x7fffffff;
	  ps->__value.__wch = 0;
	}
      else
	--ps->__value.__wchb[3];
      return -3;
    }

  if (s == NULL)
    {
      /* if 's' is a null pointer, behave as if a null pointer was passed for
         'pc8', an empty string was passed for 's', and 1 passed for 'n'.  */
      pc8 = NULL;
      s = "";
      n = 1;
    }

  wchar_t wc;
  size_t result;

  result = mbrtowc (&wc, s, n, ps);
  if (result <= n)
    {
      if (wc <= 0x7F)
	{
	  if (pc8 != NULL)
	    *pc8 = wc;
	}
      else if (wc <= 0x7FF)
	{
	  if (pc8 != NULL)
	    *pc8 = 0xC0 + ((wc >> 6) & 0x1F);
	  ps->__value.__wchb[0] = 0x80 + (wc & 0x3F);
	  ps->__value.__wchb[3] = 0;
	  ps->__count |= 0x80000000;
	}
      else if (wc <= 0xFFFF)
	{
	  if (pc8 != NULL)
	    *pc8 = 0xE0 + ((wc >> 12) & 0x0F);
	  ps->__value.__wchb[1] = 0x80 + ((wc >> 6) & 0x3F);
	  ps->__value.__wchb[0] = 0x80 + (wc & 0x3F);
	  ps->__value.__wchb[3] = 1;
	  ps->__count |= 0x80000000;
	}
      else if (wc <= 0x10FFFF)
	{
	  if (pc8 != NULL)
	    *pc8 = 0xF0 + ((wc >> 18) & 0x07);
	  ps->__value.__wchb[2] = 0x80 + ((wc >> 12) & 0x3F);
	  ps->__value.__wchb[1] = 0x80 + ((wc >> 6) & 0x3F);
	  ps->__value.__wchb[0] = 0x80 + (wc & 0x3F);
	  ps->__value.__wchb[3] = 2;
	  ps->__count |= 0x80000000;
	}
    }
  if (result == 0 && wc != 0)
    {
      /* mbrtowc() never returns -3.  When a MB sequence converts to multiple
         WCs, no input is consumed when writing the subsequent WCs resulting
         in a result of 0 even if a null character wasn't written.  */
      result = -3;
    }

  return result;
}
