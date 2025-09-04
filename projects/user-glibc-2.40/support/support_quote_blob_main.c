/* Quote a blob so that it can be used in C literals.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
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

#include <support/support.h>
#include <support/xmemstream.h>

char *
SUPPORT_QUOTE_BLOB (const void *blob, size_t length)
{
  struct xmemstream out;
  xopen_memstream (&out);

  const CHAR *p = blob;
  for (size_t i = 0; i < length; ++i)
    {
      CHAR ch = p[i];

      /* Use C backslash escapes for those control characters for
	 which they are defined.  */
      switch (ch)
	{
	case L_('\a'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked ('a', out.out);
	  break;
	case L_('\b'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked ('b', out.out);
	  break;
	case L_('\f'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked ('f', out.out);
	  break;
	case L_('\n'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked ('n', out.out);
	  break;
	case L_('\r'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked ('r', out.out);
	  break;
	case L_('\t'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked ('t', out.out);
	  break;
	case L_('\v'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked ('v', out.out);
	  break;
	case L_('\\'):
	case L_('\''):
	case L_('\"'):
	  putc_unlocked ('\\', out.out);
	  putc_unlocked (ch, out.out);
	  break;
	default:
	  if (ch < L_(' ') || ch > L_('~'))
	    /* For narrow characters, use octal sequences because they
	       are fixed width, unlike hexadecimal sequences.  For
	       wide characters, use N2785 delimited escape
	       sequences.  */
	    if (WIDE)
	      fprintf (out.out, "\\x{%x}", (unsigned int) ch);
	    else
	      fprintf (out.out, "\\%03o", (unsigned int) ch);
	  else
	    putc_unlocked (ch, out.out);
	}
    }

  xfclose_memstream (&out);
  return out.buffer;
}
