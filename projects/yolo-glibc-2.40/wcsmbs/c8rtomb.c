/* UTF-8 to multibyte conversion.
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

#include <errno.h>
#include <uchar.h>
#include <wchar.h>


/* This is the private state used if PS is NULL.  */
static mbstate_t state;

size_t
c8rtomb (char *s, char8_t c8, mbstate_t *ps)
{
  /* This implementation depends on the converter invoked by wcrtomb not
     needing to retain state in either the top most bit of ps->__count or
     in ps->__value between invocations.  This implementation uses the
     top most bit of ps->__count to indicate that trailing code units are
     expected and uses ps->__value to store previously seen code units.  */

  wchar_t wc;

  if (ps == NULL)
    ps = &state;

  if (s == NULL)
    {
      /* if 's' is a null pointer, behave as if u8'\0' was passed as 'c8'.  If
         this occurs for an incomplete code unit sequence, then an error will
         be reported below.  */
      c8 = u8""[0];
    }

  if (! (ps->__count & 0x80000000))
    {
      /* Initial state.  */
      if ((c8 >= 0x80 && c8 <= 0xC1) || c8 >= 0xF5)
	{
	  /* An invalid lead code unit.  */
	  __set_errno (EILSEQ);
	  return -1;
	}
      if (c8 >= 0xC2)
	{
	  /* A valid lead code unit.  */
	  ps->__count |= 0x80000000;
	  ps->__value.__wchb[0] = c8;
	  ps->__value.__wchb[3] = 1;
	  return 0;
	}
      /* A single byte (ASCII) code unit.  */
      wc = c8;
    }
  else
    {
      char8_t cu1 = ps->__value.__wchb[0];
      if (ps->__value.__wchb[3] == 1)
	{
	  /* A single lead code unit was previously seen.  */
	  if ((c8 < 0x80 || c8 > 0xBF)
              || (cu1 == 0xE0 && c8 < 0xA0)
              || (cu1 == 0xED && c8 > 0x9F)
              || (cu1 == 0xF0 && c8 < 0x90)
              || (cu1 == 0xF4 && c8 > 0x8F))
	    {
	      /* An invalid second code unit.  */
	      __set_errno (EILSEQ);
	      return -1;
	    }
	  if (cu1 >= 0xE0)
	    {
	      /* A three or four code unit sequence.  */
	      ps->__value.__wchb[1] = c8;
	      ++ps->__value.__wchb[3];
	      return 0;
	    }
	  wc = ((cu1 & 0x1F) << 6)
	       + (c8 & 0x3F);
	}
      else
	{
	  char8_t cu2 = ps->__value.__wchb[1];
	  /* A three or four byte code unit sequence.  */
	  if (c8 < 0x80 || c8 > 0xBF)
	    {
	      /* An invalid third or fourth code unit.  */
	      __set_errno (EILSEQ);
	      return -1;
	    }
	  if (ps->__value.__wchb[3] == 2 && cu1 >= 0xF0)
	    {
	      /* A four code unit sequence.  */
	      ps->__value.__wchb[2] = c8;
	      ++ps->__value.__wchb[3];
	      return 0;
	    }
	  if (cu1 < 0xF0)
	    {
	      wc = ((cu1 & 0x0F) << 12)
		   + ((cu2 & 0x3F) << 6)
		   + (c8 & 0x3F);
	    }
	  else
	    {
	      char8_t cu3 = ps->__value.__wchb[2];
	      wc = ((cu1 & 0x07) << 18)
		   + ((cu2 & 0x3F) << 12)
		   + ((cu3 & 0x3F) << 6)
		   + (c8 & 0x3F);
	    }
	}
      ps->__count &= 0x7fffffff;
      ps->__value.__wch = 0;
    }

  return wcrtomb (s, wc, ps);
}
