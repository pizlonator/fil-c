/* Copyright (C) 1996-2024 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wcsmbsload.h>

#include <pointer_guard.h>

#ifndef EILSEQ
# define EILSEQ EINVAL
#endif


/* This is the private state used if PS is NULL.  */
static mbstate_t state;

size_t
__wcrtomb_internal (char *s, wchar_t wc, mbstate_t *ps, size_t s_size)
{
  char buf[MB_LEN_MAX];
  struct __gconv_step_data data;
  int status;
  size_t result;
  size_t dummy;
  const struct gconv_fcts *fcts;

  /* Set information for this step.  */
  data.__invocation_counter = 0;
  data.__internal_use = 1;
  data.__flags = __GCONV_IS_LAST;
  data.__statep = ps ?: &state;

  /* A first special case is if S is NULL.  This means put PS in the
     initial state.  */
  if (s == NULL)
    wc = L'\0';

  /* Tell where we want to have the result.  */
  data.__outbuf = (unsigned char *) buf;
  data.__outbufend = (unsigned char *) buf + sizeof buf;

  /* Get the conversion functions.  */
  fcts = get_gconv_fcts (_NL_CURRENT_DATA (LC_CTYPE));
  __gconv_fct fct = fcts->tomb->__fct;
  if (fcts->tomb->__shlib_handle != NULL)
    PTR_DEMANGLE (fct);

  /* If WC is the NUL character we write into the output buffer the byte
     sequence necessary for PS to get into the initial state, followed
     by a NUL byte.  */
  if (wc == L'\0')
    {
      status = DL_CALL_FCT (fct, (fcts->tomb, &data, NULL, NULL,
				  NULL, &dummy, 1, 1));

      if (status == __GCONV_OK || status == __GCONV_EMPTY_INPUT)
	*data.__outbuf++ = '\0';
    }
  else
    {
      /* Do a normal conversion.  */
      const unsigned char *inbuf = (const unsigned char *) &wc;

      status = DL_CALL_FCT (fct,
			    (fcts->tomb, &data, &inbuf,
			     inbuf + sizeof (wchar_t), NULL, &dummy, 0, 1));
    }

  /* There must not be any problems with the conversion but illegal input
     characters.  The output buffer must be large enough, otherwise the
     definition of MB_CUR_MAX is not correct.  All the other possible
     errors also must not happen.  */
  assert (status == __GCONV_OK || status == __GCONV_EMPTY_INPUT
	  || status == __GCONV_ILLEGAL_INPUT
	  || status == __GCONV_INCOMPLETE_INPUT
	  || status == __GCONV_FULL_OUTPUT);

  if (status == __GCONV_OK || status == __GCONV_EMPTY_INPUT
      || status == __GCONV_FULL_OUTPUT)
    {
      result = data.__outbuf - (unsigned char *) buf;

      if (s != NULL)
	{
	  if (result > s_size)
	    __chk_fail ();

	  memcpy (s, buf, result);
	}
    }
  else
    {
      result = (size_t) -1;
      __set_errno (EILSEQ);
    }

  return result;
}

size_t
__wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
{
  return __wcrtomb_internal (s, wc, ps, (size_t) -1);
}
weak_alias (__wcrtomb, wcrtomb)
libc_hidden_weak (wcrtomb)
