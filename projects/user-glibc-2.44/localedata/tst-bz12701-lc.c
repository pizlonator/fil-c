/* Verify scanf field width handling with the 'lc' conversion (BZ #12701).
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <libc-diag.h>
#include <support/check.h>
#include <support/next_to_fault.h>
#include <support/xstdio.h>

/* Compare character-wise the initial part of the wide character object
   pointed to by WS corresponding to wide characters obtained by the
   conversion of first N bytes of the multibyte character object pointed
   to by S.  */

static int
tst_bz12701_lc_memcmp (const wchar_t *ds, const char *s, size_t n)
{
  size_t nc = mbsnrtowcs (NULL, &s, n, 0, NULL);

  struct support_next_to_fault ntf;
  ntf = support_next_to_fault_allocate (nc * sizeof (wchar_t));
  wchar_t *ss = (wchar_t *) ntf.buffer;

  mbsnrtowcs (ss, &s, n, nc, NULL);
  int r = wmemcmp (ds, ss, nc);

  support_next_to_fault_free (&ntf);

  return r;
}

/* Verify various aspects of field width handling, including the data
   obtained, the number of bytes consumed, and the stream position.  */

static int
do_test (void)
{
  if (setlocale (LC_ALL, "pl_PL.UTF-8") == NULL)
    FAIL_EXIT1 ("setlocale (LC_ALL, \"pl_PL.UTF-8\")");

  /* Part of a tongue-twister in Polish, which says:
     "On a rainy morning cuckoos and warblers, rather than starting
     on earthworms, stuffed themselves fasted with the flesh of cress."  */
  static const char s[126] = "Dżdżystym rankiem gżegżółki i piegże, "
			     "zamiast wziąć się za dżdżownice, "
			     "nażarły się na czczo miąższu rzeżuchy";

  const char *sp = s;
  size_t nc;
  TEST_VERIFY_EXIT ((nc = mbsnrtowcs (NULL, &sp, sizeof (s), 0, NULL)) == 108);

  struct support_next_to_fault ntfo, ntfi;
  ntfo = support_next_to_fault_allocate (nc * sizeof (wchar_t));
  ntfi = support_next_to_fault_allocate (sizeof (s));
  wchar_t *e = (wchar_t *) ntfo.buffer + nc;
  char *b = ntfi.buffer;

  wchar_t *c;
  FILE *f;
  int ic;
  int n;
  int i;

  memcpy (ntfi.buffer, s, sizeof (s));

  ic = i = 0;
  f = xfmemopen (b, sizeof (s), "r");

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  /* Avoid: "warning: zero width in gnu_scanf format [-Werror=format=]".  */
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wformat");
  TEST_VERIFY_EXIT (fscanf (f, "%0lc%n", c, &n) == 1);
  DIAG_POP_NEEDS_COMMENT;
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 1;
  i += n;

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 2);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 1;
  i += n;

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%1lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 1;
  i += n;

  c = e - 2;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 3);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 2;
  i += n;

  c = e - 4;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%4lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 4);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 4;
  i += n;

  c = e - 8;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%8lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 8);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 8;
  i += n;

  c = e - 16;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%16lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 20);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 16;
  i += n;

  c = e - 32;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%32lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 38);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 32;
  i += n;

  c = e - (nc - ic);
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%64lc%n", c, &n) == EOF);
  TEST_VERIFY_EXIT (n == 38);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, sizeof (s) - i) == 0);

  TEST_VERIFY_EXIT (ftell (f) == sizeof (s));
  TEST_VERIFY_EXIT (feof (f) != 0);

  xfclose (f);

  ic = i = 0;
  f = xfmemopen (b, 3, "r");

  c = e - 2;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 3);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 2;
  i += n;

  c = e - (nc - ic);
  TEST_VERIFY_EXIT (feof (f) == 0);
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2lc%n", c, &n) == EOF);
  TEST_VERIFY_EXIT (n == 3);

  TEST_VERIFY_EXIT (ftell (f) == 3);
  TEST_VERIFY_EXIT (feof (f) != 0);

  xfclose (f);

  ic = i = 0;
  f = xfmemopen (b, 3, "r");

  c = e - 1;
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%lc%n", c, &n) == 1);
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, n) == 0);
  ic += 1;
  i += n;

  c = e - (nc - ic);
  TEST_VERIFY_EXIT (ftell (f) == i);
  TEST_VERIFY_EXIT (fscanf (f, "%2lc%n", c, &n) == EOF);
  TEST_VERIFY_EXIT (n == 1);
  TEST_VERIFY_EXIT (tst_bz12701_lc_memcmp (c, s + i, 3 - i) == 0);

  TEST_VERIFY_EXIT (ftell (f) == 3);
  TEST_VERIFY_EXIT (feof (f) != 0);

  xfclose (f);

  support_next_to_fault_free (&ntfi);
  support_next_to_fault_free (&ntfo);

  return 0;
}

#include <support/test-driver.c>
