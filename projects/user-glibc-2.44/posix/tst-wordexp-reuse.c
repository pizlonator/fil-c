/* Test for wordexp with WRDE_REUSE flag.
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

#include <wordexp.h>
#include <mcheck.h>

#include <support/check.h>

static int
do_test (void)
{
  mtrace ();

  {
    wordexp_t p = { 0 };
    TEST_COMPARE (wordexp ("one", &p, 0), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[0], "one");
    TEST_COMPARE (wordexp ("two", &p, WRDE_REUSE), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[0], "two");
    wordfree (&p);
  }

  {
    wordexp_t p = { .we_offs = 2 };
    TEST_COMPARE (wordexp ("one", &p, 0), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[0], "one");
    TEST_COMPARE (wordexp ("two", &p, WRDE_REUSE | WRDE_DOOFFS), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[p.we_offs + 0], "two");
    wordfree (&p);
  }

  {
    wordexp_t p = { 0 };
    TEST_COMPARE (wordexp ("one", &p, 0), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[0], "one");
    TEST_COMPARE (wordexp ("two", &p, WRDE_REUSE | WRDE_APPEND), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[0], "two");
    wordfree (&p);
  }

  {
    wordexp_t p = { .we_offs = 2 };
    TEST_COMPARE (wordexp ("one", &p, WRDE_DOOFFS), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[p.we_offs + 0], "one");
    TEST_COMPARE (wordexp ("two", &p, WRDE_REUSE
				      | WRDE_DOOFFS), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[p.we_offs + 0], "two");
    wordfree (&p);
  }

  {
    wordexp_t p = { .we_offs = 2 };
    TEST_COMPARE (wordexp ("one", &p, WRDE_DOOFFS), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[p.we_offs + 0], "one");
    TEST_COMPARE (wordexp ("two", &p, WRDE_REUSE
				      | WRDE_DOOFFS | WRDE_APPEND), 0);
    TEST_COMPARE (p.we_wordc, 1);
    TEST_COMPARE_STRING (p.we_wordv[p.we_offs + 0], "two");
    wordfree (&p);
  }

  return 0;
}

#include <support/test-driver.c>
