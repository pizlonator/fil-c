/* mutest-format-tap.c: TAP format output
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

static int tap_counter;

static void
tap_expect_result (mutest_expect_t *expect)
{
  char buf[64];

  snprintf (buf, 64, "%d", ++tap_counter);

  switch (expect->result)
    {
    case MUTEST_RESULT_PASS:
      mutest_print (stdout,
                    "ok ", buf, " - ", expect->description,
                    NULL);
      break;

    case MUTEST_RESULT_FAIL:
      mutest_print (stdout,
                    "not ok ", buf, " - ", expect->description,
                    NULL);
      break;

    case MUTEST_RESULT_SKIP:
      mutest_print (stdout,
                    "ok ", buf, " - ", expect->description,
                    " # SKIP: ", expect->skip_reason != NULL ? expect->skip_reason : "",
                    NULL);
      break;
    }
}

static void
tap_expect_fail (mutest_expect_t *expect,
                 bool negate,
                 mutest_expect_res_t *check,
                 const char *check_repr)
{
  char *diagnostic = NULL;
  char *location = NULL;
  mutest_expect_diagnostic (expect, negate, check, check_repr,
                           &diagnostic,
                           &location);

  mutest_print (stdout,
                "# ",
                location,
                ": ",
                diagnostic,
                NULL);

  free (diagnostic);
  free (location);
}

static void
tap_spec_preamble (mutest_spec_t *spec)
{
  mutest_print (stdout, "# ", spec->description, NULL);
}

static void
tap_suite_preamble (mutest_suite_t *suite)
{
  mutest_print (stdout, "# ", suite->description, NULL);
}

static void
tap_total_results (mutest_state_t *state MUTEST_UNUSED)
{
  int n_tests, n_skipped;

  n_tests = mutest_get_results (NULL, NULL, &n_skipped);

  if (n_tests == n_skipped)
    mutest_print (stdout, "1..0 # skip", NULL);
  else
    {
      char plan[128];

      snprintf (plan, 128, "1..%d", n_tests);

      mutest_print (stdout, plan, NULL);
    }
}

const mutest_formatter_t *
mutest_get_tap_formatter (void)
{
  static mutest_formatter_t tap = {
    .suite_preamble = tap_suite_preamble,
    .spec_preamble = tap_spec_preamble,
    .expect_result = tap_expect_result,
    .expect_fail = tap_expect_fail,
    .spec_results = NULL,
    .suite_results = NULL,
    .total_results = tap_total_results,
  };

  return &tap;
}
