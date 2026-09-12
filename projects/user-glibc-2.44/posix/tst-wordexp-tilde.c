/* Test wordexp tilde expansion with large usernames (BZ 34091).
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

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include <stdlib.h>
#include <sys/resource.h>

#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>
#include <support/namespace.h>

typedef void (*func_callback_t)(void);

static void
subprocess_small_stack (void *closure)
{
  struct rlimit rl;
  TEST_COMPARE (getrlimit (RLIMIT_STACK, &rl), 0);
  rl.rlim_cur = 512 * 1024;
  TEST_COMPARE (setrlimit (RLIMIT_STACK, &rl), 0);

  func_callback_t func_test = closure;
  func_test ();
}

/* Build a string "~<padding>/tail" where <padding> is LEN bytes of the
   character CH.  The caller must free the result.  */
static char *
make_tilde_input (char ch, size_t len, const char *tail)
{
  /* ~  +  len  +  /  +  tail  +  \0 */
  size_t taillen = tail != NULL ? strlen (tail) : 0;
  size_t total = 1 + len + 1 + taillen + 1;
  char *buf = xmalloc (total);
  buf[0] = '~';
  memset (buf + 1, ch, len);
  buf[1 + len] = '/';
  if (tail != NULL)
    memcpy (buf + 1 + len + 1, tail, taillen);
  buf[total - 1] = '\0';
  return buf;
}

/* Test 1: A very long username must not crash.  The username will not match
   any real user, so wordexp returns ~<long>/rest.  */
static void
test_long_username (void)
{
  printf ("info: test_long_username_no_crash\n");

  static const char REST[] = "rest";

  /* 1 MiB username — well beyond any reasonable stack frame.  */
  const size_t long_len = 1024 * 1024;
  char *input = make_tilde_input ('A', long_len, REST);

  wordexp_t we = { 0 };
  int ret = wordexp (input, &we, 0);
  /* The (non-existent) username is invalid, so wordexp falls back to
     literal output: ~AAA…/rest.  */
  TEST_COMPARE (ret, 0);
  TEST_COMPARE (we.we_wordc, 1);

  /* Verify prefix: '~' followed by long_len 'A's.  */
  const char *result = we.we_wordv[0];
  TEST_COMPARE (result[0], '~');
  TEST_COMPARE (strlen (result),
	        1 /* ~ */ + long_len + sizeof (REST));
  for (size_t j = 1; j <= long_len; j++)
    if (result[j] != 'A')
      {
	printf ("  mismatch at position %zu: expected 'A', got '%c'\n",
		j, result[j]);
	support_record_failure ();
	break;
      }
  /* Verify the tail after the username.  */
  TEST_COMPARE_STRING (result + 1 + long_len, "/rest");

  wordfree (&we);
  free (input);
}

/* Test 2: A username that just exceeds the default scratch_buffer inline
   size (1024 bytes) exercises the scratch_buffer_set_array_size growth path
   without being excessively large.  */
static void
test_scratch_buffer_growth (void)
{
  printf ("info: test_scratch_buffer_growth\n");

  const size_t len = 2048;
  char *input = make_tilde_input ('x', len, NULL);

  wordexp_t we = { 0 };
  int ret = wordexp (input, &we, 0);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE (we.we_wordc, 1);

  /* ~xxx…/ — the trailing slash makes a separate empty component, but
     wordexp merges it into the single token ~xxx…/.  */
  const char *result = we.we_wordv[0];
  TEST_COMPARE (result[0], '~');
  for (size_t j = 1; j <= len; j++)
    if (result[j] != 'x')
      {
	printf ("  mismatch at position %zu\n", j);
	support_record_failure ();
	break;
      }
  TEST_COMPARE (result[1 + len], '/');

  wordfree (&we);
  free (input);
}

/* Test 3: ~root still resolves to the correct home directory through the
   __getpwnam_r path.  */
static void
test_known_user (void)
{
  printf ("info: test_known_user\n");

  /* Look up root's home directory for comparison.  */
  struct passwd *pw = getpwnam ("root");
  if (pw == NULL || pw->pw_dir == NULL)
    {
      printf ("  SKIP: cannot look up root\n");
      return;
    }

  char *expected = xasprintf ("%s/file", pw->pw_dir);

  wordexp_t we = { 0 };
  TEST_COMPARE (wordexp ("~root/file", &we, 0), 0);
  TEST_COMPARE (we.we_wordc, 1);
  TEST_COMPARE_STRING (we.we_wordv[0], expected);

  wordfree (&we);
  free (expected);
}

/* Test 4: Bare tilde expands to $HOME.  */
static void
test_bare_tilde (void)
{
  printf ("info: test_bare_tilde\n");

  const char *home = getenv ("HOME");
  if (home == NULL)
    {
      printf ("  SKIP: HOME is not set\n");
      return;
    }

  wordexp_t we = { 0 };
  TEST_COMPARE (wordexp ("~", &we, 0), 0);
  TEST_COMPARE (we.we_wordc, 1);
  TEST_COMPARE_STRING (we.we_wordv[0], home);

  wordfree (&we);
}

/* Test 5: Short non-existent username falls back to literal ~username output,
   exercising the invalid-login-name path.  */
static void
test_unknown_user (void)
{
  printf ("info: test_unknown_user\n");

  /* Pick a username that is extremely unlikely to exist.  */
  wordexp_t we = { 0 };
  TEST_COMPARE (wordexp ("~no_such_user_xyzzy42", &we, 0), 0);
  TEST_COMPARE (we.we_wordc, 1);
  TEST_COMPARE_STRING (we.we_wordv[0], "~no_such_user_xyzzy42");

  wordfree (&we);
}

/* Test 6: Tilde with username and WRDE_APPEND — exercises parse_tilde's
   interaction with the WRDE_APPEND word list.  */
static void
test_tilde_with_append (void)
{
  printf ("info: test_tilde_with_append\n");

  const char *home = getenv ("HOME");
  if (home == NULL)
    {
      printf ("  SKIP: HOME is not set\n");
      return;
    }

  wordexp_t we = { 0 };
  TEST_COMPARE (wordexp ("first", &we, 0), 0);

  TEST_COMPARE (wordexp ("~/path", &we, WRDE_APPEND), 0);
  TEST_COMPARE (we.we_wordc, 2);
  TEST_COMPARE_STRING (we.we_wordv[0], "first");

  char *expected = xasprintf ("%s/path", home);
  TEST_COMPARE_STRING (we.we_wordv[1], expected);

  wordfree (&we);
  free (expected);
}

static int
do_test (void)
{
  test_known_user ();
  test_bare_tilde ();
  test_unknown_user ();
  test_tilde_with_append ();

  support_isolate_in_subprocess (subprocess_small_stack,
				 test_long_username);

  support_isolate_in_subprocess (subprocess_small_stack,
				 test_scratch_buffer_growth);

  return 0;
}

#include <support/test-driver.c>
