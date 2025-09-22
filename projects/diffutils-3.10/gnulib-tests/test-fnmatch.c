/* Test of fnmatch string matching function.
   Copyright (C) 2009-2023 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Simon Josefsson <simon@josefsson.org>, 2009.  */

#include <config.h>

#include <fnmatch.h>

#include "signature.h"
SIGNATURE_CHECK (fnmatch, int, (char const *, char const *, int));

#include "macros.h"

int
main ()
{
  ASSERT (fnmatch ("", "", 0) == 0);

  ASSERT (fnmatch ("*", "", 0) == 0);
  ASSERT (fnmatch ("*", "foo", 0) == 0);
  ASSERT (fnmatch ("*", "bar", 0) == 0);
  ASSERT (fnmatch ("*", "*", 0) == 0);
  ASSERT (fnmatch ("**", "f", 0) == 0);
  ASSERT (fnmatch ("**", "foo.txt", 0) == 0);
  ASSERT (fnmatch ("*.*", "foo.txt", 0) == 0);

  ASSERT (fnmatch ("foo*.txt", "foobar.txt", 0) == 0);

  ASSERT (fnmatch ("foo.txt", "foo.txt", 0) == 0);
  ASSERT (fnmatch ("foo\\.txt", "foo.txt", 0) == 0);
  ASSERT (fnmatch ("foo\\.txt", "foo.txt", FNM_NOESCAPE) == FNM_NOMATCH);

  /* Verify that an unmatched [ is treated as a literal, as POSIX
     requires.  This test ensures that glibc Bugzilla bug #12378 stays
     fixed.
   */
  ASSERT (fnmatch ("[/b", "[/b", 0) == 0);

  ASSERT (fnmatch ("[[:alpha:]'[:alpha:]\0]", "a", 0) == FNM_NOMATCH);
  ASSERT (fnmatch ("[a[.\0.]]", "a", 0) == FNM_NOMATCH);
#ifdef FNM_EXTMATCH
  ASSERT (fnmatch ("**(!()", "**(!()", FNM_EXTMATCH) == 0);
#endif
#ifdef FNM_LEADING_DIR
  ASSERT (fnmatch ("x?y", "x/y/z", FNM_PATHNAME | FNM_LEADING_DIR)
          == FNM_NOMATCH);
#endif

  return 0;
}
