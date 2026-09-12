/* Test for gconv module reference counter leak.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <support/check.h>
#include <support/support.h>

/* Internal headers for accessing the gconv structures.  */
#include <locale/localeinfo.h>
#include <iconv/gconv_int.h>
#include <wcsmbs/wcsmbsload.h>

static int
do_test (void)
{
  locale_t loc_obj = newlocale (LC_ALL_MASK, "de_DE.ISO-8859-1", NULL);
  if (loc_obj == NULL)
    FAIL_EXIT1 ("newlocale failed, check if de_DE.ISO-8859-1 is generated");

  uselocale (loc_obj);

  wchar_t buf[32] = L"123";
  int j;

  /* First iteration initializes the gconv functions internally.  */
  if (swscanf (buf, L"%d", &j) < 1)
    FAIL_EXIT1 ("swscanf failed");

  /* Retrieve the current gconv_fcts from the LC_CTYPE locale data.  */
  struct __locale_data *loc = loc_obj->__locales[LC_CTYPE];
  struct lc_ctype_data *ctype = loc->private;
  const struct gconv_fcts *fcts = ctype->fcts;

  TEST_VERIFY_EXIT (fcts != NULL);
  TEST_VERIFY_EXIT (fcts->towc != NULL);

  /* Capture the reference counter.  */
  int initial_counter = fcts->towc->__counter;

  if (fcts->towc->__shlib_handle == NULL)
    FAIL_EXIT1 ("__shlib_handle is NULL!");

  /* Perform a second iteration of swscanf. If the stack-allocated FILE
     leaks the gconv reference, the counter will increment.  */
  if (swscanf (buf, L"%d", &j) < 1)
    FAIL_EXIT1 ("swscanf failed");

  /* The counter should be unchanged, as _IO_wstrfile_fclose_stack should
     have decremented it correctly.  */
  TEST_COMPARE (fcts->towc->__counter, initial_counter);

  return 0;
}

#include <support/test-driver.c>
