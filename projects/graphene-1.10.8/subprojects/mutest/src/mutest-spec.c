/* mutest-spec.c: Specifications
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void
mutest_it_full (const char *file,
                int line,
                const char *func_name,
                const char *description,
                mutest_spec_func_t func)
{
  if (description == NULL)
    mutest_assert_if_reached ("missing spec description");

  if (func == NULL)
    mutest_assert_if_reached ("missing spec functions");

  if (mutest_get_current_suite () == NULL)
    mutest_assert_if_reached ("missing suite");

  mutest_spec_t spec = {
    .description = description,
    .file = file,
    .line = line,
    .func_name = func_name,
    .skip_all = false,
    .n_expects = 0,
    .pass = 0,
    .fail = 0,
    .skip = 0,
  };

  mutest_format_spec_preamble (&spec);

  mutest_set_current_spec (&spec);

  mutest_suite_t *suite = mutest_get_current_suite ();

  if (suite->before_each_hook != NULL)
    suite->before_each_hook ();

  /* If mutest_spec_skip() was called inside the before_each() hook,
   * then we don't call func(), and mark the whole spec as skipped
   */
  if (spec.skip_all)
    {
      spec.n_expects = 1;
      spec.skip = 1;
    }
  else
    {
      spec.start_time = mutest_get_current_time ();
      func (&spec);
      spec.end_time = mutest_get_current_time ();

      /* If mutest_spec_skip() was called in func() then we mark the
       * whole spec as skipped regardless of how many expectations
       * were actually ran
       */
      if (spec.skip_all)
        {
          spec.n_expects = 1;
          spec.skip = 1;
        }
    }

  mutest_suite_add_spec_results (suite, &spec);

  if (suite->after_each_hook != NULL)
    suite->after_each_hook ();

  mutest_set_current_spec (NULL);

  mutest_format_spec_results (&spec);
}

void
mutest_spec_skip (const char *reason)
{
  mutest_spec_t *spec = mutest_get_current_spec ();
  if (spec == NULL)
    mutest_assert_if_reached ("skip called without a spec");

  spec->skip_all = true;
  spec->skip_reason = reason;
}

void
mutest_spec_add_expect_result (mutest_spec_t *spec,
                               mutest_expect_t *expect)
{
  switch (expect->result)
    {
    case MUTEST_RESULT_PASS:
      spec->n_expects += 1;
      spec->pass += 1;
      break;

    case MUTEST_RESULT_FAIL:
      spec->n_expects += 1;
      spec->fail += 1;
      break;

    case MUTEST_RESULT_SKIP:
      spec->n_expects += 1;
      spec->skip += 1;
      break;
    }
}
