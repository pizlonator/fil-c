/* Copyright (C) 2007-2026 Free Software Foundation, Inc.
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

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <tls-internal.h>
#include <libc-internal.h>

static const char *
translate (const char *str, locale_t loc)
{
  locale_t oldloc = __uselocale (loc);
  const char *res = _(str);
  __uselocale (oldloc);
  return res;
}

static char *
unknown_error (locale_t loc)
{
  return (char *) translate ("Unknown error", loc);
}


/* Return a string describing the errno code in ERRNUM.  */
char *
__strerror_l (int errnum, locale_t loc)
{
  int saved_errno = errno;
  char *err = (char *) __get_errlist (errnum);
  if (__glibc_unlikely (err == NULL))
    {
      if (__libc_initial)
	{
	  struct tls_internal_t *tls_internal = __glibc_tls_internal ();
	  free (tls_internal->strerror_l_buf);
	  if (__asprintf (&tls_internal->strerror_l_buf, "%s%d",
			  translate ("Unknown error ", loc), errnum) > 0)
	    err = tls_internal->strerror_l_buf;
	  else
	    {
	      /* The memory was freed above.  */
	      tls_internal->strerror_l_buf = NULL;
	      /* Provide a fallback translation.  */
	      err = unknown_error (loc);
	    }
	}
      else
	/* Secondary namespaces use a different malloc, so cannot
	   participate in the buffer management.  */
	err = unknown_error (loc);
    }
  else
    err = (char *) translate (err, loc);

  __set_errno (saved_errno);
  return err;
}
weak_alias (__strerror_l, strerror_l)
libc_hidden_def (__strerror_l)
