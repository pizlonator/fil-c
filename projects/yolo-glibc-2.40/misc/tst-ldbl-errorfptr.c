/* Test for the long double redirections in error* functions
   when they are returned as function pointer BZ #29033.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <err.h>
#include <errno.h>
#include <error.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/cdefs.h>

#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/xdlfcn.h>

#if __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 1
# define LDBL_NAME(alias) "__" #alias "ieee128"
#elif defined __LONG_DOUBLE_MATH_OPTIONAL && defined __NO_LONG_DOUBLE_MATH
# define LDBL_NAME(alias) "__nldbl_" #alias
#else
# define LDBL_NAME(alias) #alias
#endif

typedef void (*error_func_t) (int, int, const char*, ...);
typedef void (*error_at_line_func_t) (int, int, const char*,
	      unsigned int, const char*, ...);

error_func_t
__attribute__ ((noinline))
get_error_func (void) {
  return &error;
}

error_at_line_func_t
__attribute__ ((noinline))
get_error_at_line_func (void) {
  return &error_at_line;
}

static int
do_test (void)
{
  /* Prepare the symbol names as per long double standards */
  char *error_sym = NULL;
  char *error_sym_at_line = NULL;
  error_sym = (char *) LDBL_NAME(error);
  error_sym_at_line = (char *) LDBL_NAME(error_at_line);
  TEST_VERIFY (error_sym != NULL && error_sym_at_line != NULL);
  /* Map the function pointers to appropriate redirected error symbols */
  error_func_t fp;
  fp = get_error_func ();
  if (fp != xdlsym (RTLD_DEFAULT, error_sym))
    {
      printf ("FAIL: fp=%p error_sym=%p\n", fp, error_sym);
      return 1;
    }

  error_at_line_func_t fpat;
  fpat = get_error_at_line_func ();
  if (fpat != xdlsym (RTLD_DEFAULT, error_sym_at_line))
    {
      printf ("FAIL: fpat=%p error_sym_at_line=%p\n",
	      fpat, error_sym_at_line);
      return 1;
    }

  return 0;
}

#include <support/test-driver.c>
