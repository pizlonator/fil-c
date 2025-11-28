/* mutest-suite.c: Suites
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

#include <string.h>
#include <stdlib.h>

// mutest_describe:
// @description: The human readable description of the test suite
//   used when writing the report
// @func: The test suite function
//
// Describes a new test suite, represented by the given @func.
//
// The #mutest_describe_func_t function is responsible for adding
// specifications that need to be satisfied, e.g.:
//
// ```cpp
// static void
// string_tests (mutest_suite_t *suite)
// {
//   /* string_start_end() is defined elsewhere */
//   mutest_it ("must check start and end", string_start_end);
//
//   /* string_contains() is defined elsewhere */
//   mutest_it ("must containt substring", string_contains);
// }
//
// int
// main (int argc, char *argv[])
// {
//   mutest_describe ("string tests", string_tests);
//
//   return mutest_run (argc, argv);
// }
// ```
//
// It's not possible to call mutest_describe() from within a
// another test suite.
void
mutest_describe_full (const char *file,
                      int line,
                      const char *func_name,
                      const char *description,
                      mutest_describe_func_t func)
{
  if (description == NULL)
    mutest_assert_if_reached ("missing spec description");

  if (func == NULL)
    mutest_assert_if_reached ("missing spec functions");

  if (mutest_get_current_suite () != NULL)
    mutest_assert_if_reached ("nested suite");

  mutest_init ();

  mutest_suite_t suite = {
    .file = file,
    .line = line,
    .func_name = func_name,
    .description = description,
    .skip_all = false,
  };

  mutest_set_current_suite (&suite);

  mutest_state_t *state = mutest_get_global_state ();

  if (state->before_hook != NULL)
    state->before_hook ();

  mutest_format_suite_preamble (&suite);

  if (suite.skip_all)
    {
      state->total_skip += 1;
    }
  else
    {
      suite.start_time = mutest_get_current_time ();
      func (&suite);
      suite.end_time = mutest_get_current_time ();

      if (suite.skip_all)
        state->total_skip += 1;
    }

  mutest_add_suite_results (&suite);

  if (state->after_hook != NULL)
    state->after_hook ();

  mutest_format_suite_results (&suite);

  mutest_set_current_suite (NULL);
}

void
mutest_suite_add_spec_results (mutest_suite_t *suite,
                               mutest_spec_t *spec)
{
  suite->n_specs += 1;

  suite->pass += spec->pass;
  suite->fail += spec->fail;
  suite->skip += spec->skip;
}

void
mutest_suite_skip (const char *reason)
{
  mutest_suite_t *suite = mutest_get_current_suite ();
  if (suite == NULL)
    mutest_assert_if_reached ("skip called without a suite");

  suite->skip_all = true;
  suite->skip_reason = reason;
}

void
mutest_before_each (mutest_hook_func_t hook)
{
  mutest_suite_t *suite = mutest_get_current_suite ();

  if (suite == NULL)
    mutest_assert_if_reached ("no suite defined");

  suite->before_each_hook = hook;
}

void
mutest_after_each (mutest_hook_func_t hook)
{
  mutest_suite_t *suite = mutest_get_current_suite ();

  if (suite == NULL)
    mutest_assert_if_reached ("no suite defined");

  suite->after_each_hook = hook;
}
