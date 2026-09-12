/* Test for wordexp with WRDE_APPEND flag.
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <support/check.h>
#include <support/support.h>

static unsigned int relocating_reallocs;

/* w_addword grows we_wordv with realloc, make every call guaranteed to
   relocate the block.  This makes BZ 34090 regression more deterministic.  */
void *
realloc (void *ptr, size_t size)
{
  if (ptr == NULL)
    return malloc (size);
  if (size == 0)
    {
      free (ptr);
      return NULL;
    }

  void *new = malloc (size);
  if (new == NULL)
    return NULL;

  /* Copy only what is valid in the old block to avoid reading past it.  */
  size_t old = malloc_usable_size (ptr);
  memcpy (new, ptr, old < size ? old : size);
  /* Clobber the old block so that a stale we_wordv pointer restored on the
     error path reads garbage instead of the old contents, which might
     otherwise survive intact and mask the bug.  */
  memset (ptr, 0x5a, old);
  free (ptr);
  relocating_reallocs++;
  return new;
}

/* Verify that all words in we match the expected NULL-terminated
   array.  */
static void
check_words (const wordexp_t *we, const char *const *expected)
{
  size_t i;
  for (i = 0; expected[i] != NULL; i++)
    {
      TEST_VERIFY (i < we->we_wordc);
      TEST_COMPARE_STRING (we->we_wordv[we->we_offs + i], expected[i]);
    }
  TEST_COMPARE (we->we_wordc, i);
}

#define CHECK_WORDS(we, ...) \
  do {								\
    const char *const expected_[] = { __VA_ARGS__, NULL };	\
    check_words (we, expected_);				\
  } while (0)

/* Test 1: WRDE_APPEND + WRDE_BADCHAR preserves we_wordc.  */
static void
test_append_badchar_preserves_count (void)
{
  printf ("info: test_append_badchar_preserves_count\n");
  wordexp_t we = { 0 };

  TEST_COMPARE (wordexp ("one two three", &we, 0), 0);
  TEST_COMPARE (we.we_wordc, 3);

  size_t saved_count = we.we_wordc;

  /* ')' triggers WRDE_BADCHAR and  "extra" would be a new word if the
     expansion succeeded, exercising the w_addword path before the error
     is detected.  */
  TEST_COMPARE (wordexp ("extra )", &we, WRDE_APPEND), WRDE_BADCHAR);
  TEST_COMPARE (we.we_wordc, saved_count);

  wordfree (&we);
}

/* Test 2: WRDE_APPEND + WRDE_BADCHAR preserves the we_wordv pointer even
   when internal realloc would move the buffer.  */
static void
test_append_badchar_preserves_pointer (void)
{
  printf ("info: test_append_badchar_preserves_pointer\n");
  wordexp_t we = { 0 };

  /* Use many words so that the initial we_wordv allocation is
     non-trivial and a later realloc is more likely to move it.  */
  TEST_COMPARE (wordexp ("a b c d e f g h", &we, 0), 0);
  TEST_COMPARE (we.we_wordc, 8);

  char **saved_wordv = we.we_wordv;
  size_t saved_count = we.we_wordc;
  unsigned int saved_reallocs = relocating_reallocs;

  /* The interposed realloc guarantees the internal we_wordv buffer moves
     during parsing, so the pointer-stability check below is meaningful.  */
  TEST_COMPARE (wordexp ("append )", &we, WRDE_APPEND), WRDE_BADCHAR);
  /* Verify that a relocating realloc actually happened during the failed
     call, otherwise the pointer-stability check is vacuous.  */
  TEST_VERIFY (relocating_reallocs > saved_reallocs);
  TEST_COMPARE (we.we_wordc, saved_count);
  TEST_VERIFY (we.we_wordv == saved_wordv);

  wordfree (&we);
}

/* Test 3: After a failed WRDE_APPEND the original words are still accessible
   and correct.  */
static void
test_append_badchar_words_intact (void)
{
  printf ("info: test_append_badchar_words_intact\n");
  wordexp_t we = { 0 };

  TEST_COMPARE (wordexp ("alpha beta gamma", &we, 0), 0);
  CHECK_WORDS (&we, "alpha", "beta", "gamma");

  TEST_COMPARE (wordexp ("delta )", &we, WRDE_APPEND), WRDE_BADCHAR);

  /* Words must still be intact.  */
  CHECK_WORDS (&we, "alpha", "beta", "gamma");
  /* The NULL terminator must still be present.  */
  TEST_VERIFY (we.we_wordv[we.we_offs + we.we_wordc] == NULL);

  wordfree (&we);
}

/* Test 4: Successful WRDE_APPEND still works (regression test).  */
static void
test_append_success (void)
{
  printf ("info: test_append_success\n");
  wordexp_t we = { 0 };

  TEST_COMPARE (wordexp ("hello", &we, 0), 0);
  TEST_COMPARE (we.we_wordc, 1);

  char **saved_wordv = we.we_wordv;

  TEST_COMPARE (wordexp ("world", &we, WRDE_APPEND), 0);
  TEST_COMPARE (we.we_wordc, 2);
  /* A successful append works on a fresh copy of the array, so the
     caller-visible pointer must have changed.  */
  TEST_VERIFY (we.we_wordv != saved_wordv);
  CHECK_WORDS (&we, "hello", "world");

  wordfree (&we);
}

/* Test 5: Successful append after a failed append — the implementation must
   recover and allow further use of the wordexp_t.  */
static void
test_append_success_after_failure (void)
{
  printf ("info: test_append_success_after_failure\n");
  wordexp_t we = { 0 };

  TEST_COMPARE (wordexp ("first", &we, 0), 0);
  CHECK_WORDS (&we, "first");

  TEST_COMPARE (wordexp ("bad |", &we, WRDE_APPEND), WRDE_BADCHAR);

  /* State must be exactly as before the failed call.  */
  CHECK_WORDS (&we, "first");

  /* A subsequent successful append must work.  */
  TEST_COMPARE (wordexp ("second third", &we, WRDE_APPEND), 0);
  CHECK_WORDS (&we, "first", "second", "third");

  wordfree (&we);
}

/* Test 6: Multiple consecutive failed appends do not corrupt state.  */
static void
test_append_multiple_failures (void)
{
  printf ("info: test_append_multiple_failures\n");
  wordexp_t we = { 0 };

  TEST_COMPARE (wordexp ("keep this", &we, 0), 0);
  CHECK_WORDS (&we, "keep", "this");

  size_t saved_count = we.we_wordc;
  char **saved_wordv = we.we_wordv;

  /* Each of these bad characters must leave the state unchanged.  */
  TEST_COMPARE (wordexp ("x )", &we, WRDE_APPEND), WRDE_BADCHAR);
  TEST_COMPARE (wordexp ("x |", &we, WRDE_APPEND), WRDE_BADCHAR);
  TEST_COMPARE (wordexp ("x ;", &we, WRDE_APPEND), WRDE_BADCHAR);
  TEST_COMPARE (wordexp ("x &", &we, WRDE_APPEND), WRDE_BADCHAR);
  TEST_COMPARE (wordexp ("x <", &we, WRDE_APPEND), WRDE_BADCHAR);
  TEST_COMPARE (wordexp ("x >", &we, WRDE_APPEND), WRDE_BADCHAR);

  TEST_COMPARE (we.we_wordc, saved_count);
  TEST_VERIFY (we.we_wordv == saved_wordv);
  CHECK_WORDS (&we, "keep", "this");

  wordfree (&we);
}

/* Test 7: WRDE_APPEND with WRDE_SYNTAX error (unterminated quote) also
   preserves state.  */
static void
test_append_syntax_error (void)
{
  printf ("info: test_append_syntax_error\n");
  wordexp_t we = { 0 };

  TEST_COMPARE (wordexp ("original", &we, 0), 0);
  CHECK_WORDS (&we, "original");

  char **saved_wordv = we.we_wordv;
  size_t saved_count = we.we_wordc;

  /* Unterminated double quote triggers WRDE_SYNTAX.  */
  TEST_COMPARE (wordexp ("\"unterminated", &we, WRDE_APPEND), WRDE_SYNTAX);

  TEST_COMPARE (we.we_wordc, saved_count);
  TEST_VERIFY (we.we_wordv == saved_wordv);
  CHECK_WORDS (&we, "original");

  wordfree (&we);
}

/* Test 8: Error without WRDE_APPEND still works (regression test for the
   non-APPEND code path in do_error).  */
static void
test_no_append_error (void)
{
  printf ("info: test_no_append_error\n");
  wordexp_t we = { 0 };

  /* Simple failure without WRDE_APPEND.  */
  TEST_COMPARE (wordexp ("bad |", &we, 0), WRDE_BADCHAR);

  /* After failure without WRDE_APPEND the struct should be safe to
     reuse — start fresh.  */
  TEST_COMPARE (wordexp ("ok", &we, 0), 0);
  CHECK_WORDS (&we, "ok");

  wordfree (&we);
}

/* Test 9: WRDE_BADCHAR on the very first character (no partial words added
   before the error).  */
static void
test_append_badchar_immediate (void)
{
  printf ("info: test_append_badchar_immediate\n");
  wordexp_t we = { 0 };

  TEST_COMPARE (wordexp ("hello world", &we, 0), 0);
  CHECK_WORDS (&we, "hello", "world");

  char **saved_wordv = we.we_wordv;
  size_t saved_count = we.we_wordc;

  /* The bad character is the very first byte — no w_addword call happens
     before the error.  */
  TEST_COMPARE (wordexp ("|", &we, WRDE_APPEND), WRDE_BADCHAR);
  TEST_COMPARE (we.we_wordc, saved_count);
  TEST_VERIFY (we.we_wordv == saved_wordv);

  wordfree (&we);
}

/* Test 10: WRDE_APPEND into an empty wordexp_t (initial call uses WRDE_APPEND
   with a zeroed struct — unusual but allowed).  */
static void
test_append_into_empty (void)
{
  printf ("info: test_append_into_empty\n");
  wordexp_t we = { 0 };

  /* First call with WRDE_APPEND on a zeroed struct.  The implementation
     must handle we_wordv == NULL gracefully.  */
  TEST_COMPARE (wordexp ("solo", &we, WRDE_APPEND), 0);
  TEST_COMPARE (we.we_wordc, 1);
  CHECK_WORDS (&we, "solo");

  wordfree (&we);
}

/* Verify that the leading we_offs slots are all NULL.  */
static void
check_offs_null (const wordexp_t *we)
{
  for (size_t i = 0; i < we->we_offs; i++)
    TEST_VERIFY (we->we_wordv[i] == NULL);
}

/* Test 11: successful WRDE_APPEND with WRDE_DOOFFS and a non-zero we_offs.
   The leading offset slots must stay NULL and words must land at
   we_wordv[we_offs + i] across both the initial and the appended call.  */
static void
test_dooffs_append_success (void)
{
  printf ("info: test_dooffs_append_success\n");
  wordexp_t we = { 0 };
  we.we_offs = 2;

  TEST_COMPARE (wordexp ("one two", &we, WRDE_DOOFFS), 0);
  TEST_COMPARE (we.we_offs, 2);
  check_offs_null (&we);
  CHECK_WORDS (&we, "one", "two");

  TEST_COMPARE (wordexp ("three", &we, WRDE_APPEND | WRDE_DOOFFS), 0);
  TEST_COMPARE (we.we_offs, 2);
  check_offs_null (&we);
  CHECK_WORDS (&we, "one", "two", "three");
  /* The NULL terminator must sit right after the last word.  */
  TEST_VERIFY (we.we_wordv[we.we_offs + we.we_wordc] == NULL);

  wordfree (&we);
}

/* Test 12: failed WRDE_APPEND with WRDE_DOOFFS preserves we_wordc, the
   we_wordv pointer, the words and the leading NULL offset slots.  This
   exercises the we_offs arithmetic in the array duplication and in the
   error-path cleanup (we_wordv[we_offs + --we_wordc]).  */
static void
test_dooffs_append_error_preserves_state (void)
{
  printf ("info: test_dooffs_append_error_preserves_state\n");
  wordexp_t we = { 0 };
  we.we_offs = 3;

  TEST_COMPARE (wordexp ("alpha beta", &we, WRDE_DOOFFS), 0);
  check_offs_null (&we);
  CHECK_WORDS (&we, "alpha", "beta");

  char **saved_wordv = we.we_wordv;
  size_t saved_count = we.we_wordc;
  unsigned int saved_reallocs = relocating_reallocs;

  /* "gamma" is a partial word added via w_addword (forcing a relocating
     realloc of we_wordv) before ')' triggers WRDE_BADCHAR.  */
  TEST_COMPARE (wordexp ("gamma )", &we, WRDE_APPEND | WRDE_DOOFFS),
		WRDE_BADCHAR);
  TEST_VERIFY (relocating_reallocs > saved_reallocs);

  TEST_COMPARE (we.we_offs, 3);
  TEST_COMPARE (we.we_wordc, saved_count);
  TEST_VERIFY (we.we_wordv == saved_wordv);
  check_offs_null (&we);
  CHECK_WORDS (&we, "alpha", "beta");
  TEST_VERIFY (we.we_wordv[we.we_offs + we.we_wordc] == NULL);

  wordfree (&we);
}

static int
do_test (void)
{
  test_append_badchar_preserves_count ();
  test_append_badchar_preserves_pointer ();
  test_append_badchar_words_intact ();
  test_append_success ();
  test_append_success_after_failure ();
  test_append_multiple_failures ();
  test_append_syntax_error ();
  test_no_append_error ();
  test_append_badchar_immediate ();
  test_append_into_empty ();
  test_dooffs_append_success ();
  test_dooffs_append_error_preserves_state ();

  return 0;
}

#include <support/test-driver.c>
