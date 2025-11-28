/* mutest-main.c: Global state
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static mutest_state_t global_state = {
  .initialized = false,

  .term_width = 80,
  .term_height = 25,
  .is_tty = false,
  .use_colors = false,

  .current_suite = NULL,
  .current_spec = NULL,

  .n_suites = 0,
  .total_pass = 0,
  .total_fail = 0,
  .total_skip = 0,

  .start_time = 0,
  .end_time = 0,
};

mutest_state_t *
mutest_get_global_state (void)
{
  return &global_state;
}

int
mutest_get_term_width (void)
{
  return global_state.term_width;
}

int
mutest_get_term_height (void)
{
  return global_state.term_height;
}

bool
mutest_is_term_tty (void)
{
  return global_state.is_tty;
}

bool
mutest_use_colors (void)
{
  return global_state.use_colors;
}

mutest_suite_t *
mutest_get_current_suite (void)
{
  return global_state.current_suite;
}

mutest_spec_t *
mutest_get_current_spec (void)
{
  return global_state.current_spec;
}

void
mutest_set_current_suite (mutest_suite_t *suite)
{
  if (suite != NULL && global_state.current_suite != NULL)
    mutest_assert_if_reached ("overriding the current suite");

  global_state.current_suite = suite;
}

void
mutest_set_current_spec (mutest_spec_t *spec)
{
  if (spec != NULL && global_state.current_spec != NULL)
    mutest_assert_if_reached ("overriding the current spec");

  global_state.current_spec = spec;
}

void
mutest_add_suite_results (mutest_suite_t *suite)
{
  global_state.n_suites += 1;

  global_state.total_pass += suite->pass;
  global_state.total_fail += suite->fail;
  global_state.total_skip += suite->skip;
}

int
mutest_get_results (int *total_pass,
                    int *total_fail,
                    int *total_skip)
{
  int total = global_state.total_pass
            + global_state.total_fail
            + global_state.total_skip;

  if (total_pass != NULL)
    *total_pass = global_state.total_pass;
  if (total_fail != NULL)
    *total_fail = global_state.total_fail;
  if (total_skip != NULL)
    *total_skip = global_state.total_skip;

  return total;
}

static void
update_term_caps (void)
{
#ifdef HAVE_ISATTY
  global_state.is_tty = isatty (STDOUT_FILENO);
#endif

  global_state.use_colors = false;

  if (!global_state.is_tty)
    return;

  char *env = mutest_getenv ("MUTEST_NO_COLOR");
  if (env != NULL && env[0] != '\0')
    goto out;

  env = mutest_getenv ("FORCE_COLOR");
  if (env != NULL && env[0] != '\0')
    {
      global_state.use_colors = true;
      goto out;
    }

  env = mutest_getenv ("TERM");
  if (env != NULL)
    {
      if (strncmp (env, "xterm", 5) == 0 ||
          strncmp (env, "rxvt", 4) == 0 ||
          strncmp (env, "vt100", 5) == 0 ||
          strncmp (env, "vt220", 5) == 0 ||
          strncmp (env, "linux", 5) == 0 ||
          strncmp (env, "screen", 6) == 0)
        {
          global_state.use_colors = true;
        }

      goto out;
    }

  env = mutest_getenv ("COLORTERM");
  if (env != NULL)
    {
      global_state.use_colors = true;
      goto out;
    }

out:
  free (env);
}

static void
update_term_size (void)
{
  if (!global_state.is_tty)
    return;

#ifdef HAVE_SYS_IOCTL_H
  struct winsize ws;

  if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
    perror ("ioctl");
  else
    {
      global_state.term_width = ws.ws_col;
      global_state.term_height = ws.ws_row;
      return;
    }
#endif

  char *env = mutest_getenv ("COLUMNS");

  if (env == NULL || *env == '\0')
    {
      free (env);
      return;
    }

  int saved_errno = errno;
  int columns = strtol (env, NULL, 10);
  if (errno == ERANGE)
    {
      free (env);
      return;
    }

  global_state.term_width = columns;
  free (env);

  errno = saved_errno;
}

static void
update_output_format (void)
{
  static const struct {
    const char *name;
    mutest_output_format_t format;
  } available_formats[] = {
    { NULL, MUTEST_OUTPUT_MOCHA },
    { "tap", MUTEST_OUTPUT_TAP },
    { "mocha", MUTEST_OUTPUT_MOCHA },
    { "default", MUTEST_OUTPUT_MOCHA },
  };

  const size_t n_available_formats =
    sizeof (available_formats) / sizeof (available_formats[0]);

  char *env = mutest_getenv ("MUTEST_OUTPUT");

  if (env == NULL || *env == '\0')
    {
      // Default
      global_state.output_format = available_formats[0].format;
      free (env);
      return;
    }

  for (size_t i = 1; i < n_available_formats; i++)
    {
      if (strcmp (env, available_formats[i].name) == 0)
        {
          global_state.output_format = available_formats[i].format;
          break;
        }
    }

  free (env);
}

void
mutest_before (mutest_hook_func_t hook)
{
  global_state.before_hook = hook;
}

void
mutest_after (mutest_hook_func_t hook)
{
  global_state.after_hook = hook;
}

void
mutest_init (void)
{
  if (mutest_likely (global_state.initialized))
    return;

  update_term_caps ();
  update_term_size ();
  update_output_format ();

  global_state.start_time = mutest_get_current_time ();

  global_state.initialized = true;
}

mutest_output_format_t
mutest_get_output_format (void)
{
  return global_state.output_format;
}

int
mutest_report (void)
{
  global_state.end_time = mutest_get_current_time ();

  mutest_format_total_results (&global_state);

  int n_tests, n_skipped, n_failed;

  n_tests = mutest_get_results (NULL, &n_failed, &n_skipped);

  if (global_state.output_format != MUTEST_OUTPUT_TAP)
    {
      /* The Autotools test harness uses the 77 exit code to signify
       * that all tests have been skipped
       */
      if (n_tests == n_skipped)
        return 77;
    }

  return n_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
