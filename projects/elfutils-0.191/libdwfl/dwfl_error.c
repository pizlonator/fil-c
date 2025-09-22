/* Error handling in libdwfl.
   Copyright (C) 2005-2015 Red Hat, Inc.
   This file is part of elfutils.

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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "libdwflP.h"


/* The error number.  */
static __thread int global_error;


int
dwfl_errno (void)
{
  int result = global_error;
  global_error = DWFL_E_NOERROR;
  return result;
}
INTDEF (dwfl_errno)


struct msgtable
{
#define DWFL_ERROR(name, text) char msg_##name[sizeof text];
  DWFL_ERRORS
#undef	DWFL_ERROR
};

static const union
{
  struct msgtable table;
  char strings[
#define DWFL_ERROR(name, text)	+ sizeof text
	       DWFL_ERRORS
#undef	DWFL_ERROR
	       ];
} msgtable =
  {
    .table =
    {
#define DWFL_ERROR(name, text) text,
      DWFL_ERRORS
#undef	DWFL_ERROR
    }
  };
#define msgstr (msgtable.strings)

static const uint_fast16_t msgidx[] =
{
#define DWFL_ERROR(name, text) \
  [DWFL_E_##name] = offsetof (struct msgtable, msg_##name),
  DWFL_ERRORS
#undef	DWFL_ERROR
};
#define nmsgidx (sizeof msgidx / sizeof msgidx[0])


static inline int
canonicalize (Dwfl_Error error)
{
  unsigned int value;

  switch (error)
    {
    default:
      value = error;
      if ((value &~ 0xffff) != 0)
	break;
      assert (value < nmsgidx);
      break;
    case DWFL_E_ERRNO:
      value = DWFL_E (ERRNO, errno);
      break;
    case DWFL_E_LIBELF:
      value = DWFL_E (LIBELF, elf_errno ());
      break;
    case DWFL_E_LIBDW:
      value = DWFL_E (LIBDW, INTUSE(dwarf_errno) ());
      break;
#if 0
    DWFL_E_LIBEBL:
      value = DWFL_E (LIBEBL, ebl_errno ());
      break;
#endif
    }

  return value;
}

int
internal_function
__libdwfl_canon_error (Dwfl_Error error)
{
  return canonicalize (error);
}

void
internal_function
__libdwfl_seterrno (Dwfl_Error error)
{
  global_error = canonicalize (error);
}


static const char *
errnomsg(int error)
{
  /* Won't be changed by strerror_r, but not const so compiler doesn't throw warning */
  static char unknown[] = "unknown error";

#ifdef STRERROR_R_CHAR_P
  return strerror_r (error, unknown, 0);
#else
  /* To store the error message from strerror_r in a thread-safe manner */
  static __thread char msg[128];
  return strerror_r (error, msg, sizeof (msg)) ? unknown : msg;
#endif
}

const char *
dwfl_errmsg (int error)
{
  if (error == 0 || error == -1)
    {
      int last_error = global_error;

      if (error == 0 && last_error == 0)
	return NULL;

      error = last_error;
      global_error = DWFL_E_NOERROR;
    }

  switch (error &~ 0xffff)
    {
    case OTHER_ERROR (ERRNO):
      return errnomsg (error & 0xffff);
    case OTHER_ERROR (LIBELF):
      return elf_errmsg (error & 0xffff);
    case OTHER_ERROR (LIBDW):
      return INTUSE(dwarf_errmsg) (error & 0xffff);
#if 0
    case OTHER_ERROR (LIBEBL):
      return ebl_errmsg (error & 0xffff);
#endif
    }

  return _(&msgstr[msgidx[(unsigned int) error < nmsgidx
			  ? error : DWFL_E_UNKNOWN_ERROR]]);
}
INTDEF (dwfl_errmsg)
