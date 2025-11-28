/* mutest-format-mocha.c: Mocha format output
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

#include <string.h>

static void
mocha_suite_preamble (mutest_suite_t *suite)
{
  if (mutest_use_colors ())
    mutest_print (stdout,
                  "\n",
                  "  ",
                  MUTEST_BOLD_DEFAULT, suite->description, MUTEST_COLOR_NONE,
                  NULL);
  else
    mutest_print (stdout,
                  "\n",
                  "  ", suite->description,
                  NULL);
}

static const char *
indent_expect (void)
{
  return "      ";
}

static void
mocha_expect_result (mutest_expect_t *expect)
{
  // indentation of every line after the first is the indentation
  // of the first line plus the result glyph
  size_t indent_len = strlen (indent_expect ()) + 3;
  char *description =
    mutest_format_string_for_display (expect->description,
                                      ' ',
                                      indent_len);

  switch (expect->result)
    {
    case MUTEST_RESULT_PASS:
      if (mutest_use_colors ())
        mutest_print (stdout,
                      indent_expect (),
                      MUTEST_COLOR_GREEN, "✓ ", MUTEST_DIM_DEFAULT, description,
                      MUTEST_COLOR_NONE,
                      NULL);
      else
        mutest_print (stdout,
                      indent_expect (),
                      "✓ ", description,
                      NULL);
      break;

    case MUTEST_RESULT_FAIL:
      if (mutest_use_colors ())
        mutest_print (stdout,
                      indent_expect (),
                      MUTEST_COLOR_RED, "✗ ", description, MUTEST_COLOR_NONE,
                      NULL);
      else
        mutest_print (stdout,
                      indent_expect (),
                      "✗ ", description,
                      NULL);
      break;

    case MUTEST_RESULT_SKIP:
      if (mutest_use_colors ())
        mutest_print (stdout,
                      indent_expect (),
                      MUTEST_COLOR_YELLOW, "Θ ", description, MUTEST_COLOR_NONE,
                      NULL);
      else
        mutest_print (stdout,
                      indent_expect (),
                      "Θ ", description,
                      NULL);
      break;
    }

  free (description);
}

static const char *
indent_spec (void)
{
  return "    ";
}

static void
mocha_spec_preamble (mutest_spec_t *spec)
{
  char *description =
    mutest_format_string_for_display (spec->description, ' ', strlen (indent_spec ()));

  mutest_print (stdout,
                indent_spec (),
                description,
                NULL);

  free (description);
}

static void
mocha_spec_results (mutest_spec_t *spec)
{
  if (spec->skip_all)
    {
      char *reason =
        mutest_format_string_for_display (spec->skip_reason != NULL
                                            ? spec->skip_reason
                                            : "unknown reason",
                                          ' ',
                                          strlen (indent_spec ()) + strlen ("skipped: "));

      if (mutest_use_colors ())
        {
          mutest_print (stdout,
                        indent_spec (),
                        MUTEST_COLOR_YELLOW, "skipped: ",
                        MUTEST_COLOR_DARK_GREY,
                        reason,
                        MUTEST_COLOR_NONE,
                        NULL);
        }
      else
        {
          mutest_print (stdout,
                        indent_spec (),
                        "skipped: ",
                        reason,
                        NULL);
        }

      free (reason);

      return;
    }

  char passing_s[128], failing_s[128], skipped_s[128];

  snprintf (passing_s, 128, "%d passing", spec->pass);
  snprintf (failing_s, 128, "%d failing", spec->fail);
  snprintf (skipped_s, 128, "%d skipped", spec->skip);

  const char *delta_u;
  double delta_t;
  char delta_s[128];

  delta_t = mutest_format_time (spec->end_time - spec->start_time, &delta_u);
  snprintf (delta_s, 128, "(%.2f %s)", delta_t, delta_u);

  if (mutest_use_colors ())
    {
      mutest_print (stdout,
                    "\n",
                    indent_expect (),
                    MUTEST_COLOR_GREEN, passing_s, MUTEST_COLOR_NONE, " ",
                    MUTEST_COLOR_DARK_GREY, delta_s, MUTEST_COLOR_NONE,
                    NULL);

      if (spec->skip != 0)
        mutest_print (stdout,
                      indent_expect (),
                      MUTEST_COLOR_YELLOW, skipped_s, MUTEST_COLOR_NONE,
                      NULL);

      if (spec->fail != 0)
        mutest_print (stdout,
                      indent_expect (),
                      MUTEST_COLOR_RED, failing_s, MUTEST_COLOR_NONE,
                      NULL);

      // Emit a newline
      mutest_print (stdout, "", NULL);
    }
  else
    mutest_print (stdout,
                  "\n",
                  indent_expect (), passing_s, " ", delta_s, "\n",
                  indent_expect (), skipped_s, "\n",
                  indent_expect (), failing_s, "\n",
                  NULL);
}

static void
mocha_suite_results (mutest_suite_t *suite)
{
  if (suite->skip_all)
    {
      char *reason =
        mutest_format_string_for_display (suite->skip_reason != NULL
                                            ? suite->skip_reason
                                            : "unknown reason",
                                          ' ',
                                          2 + strlen ("skipped: "));
      if (mutest_use_colors ())
        {
          mutest_print (stdout,
                        "\n",
                        "  ",
                        MUTEST_COLOR_YELLOW, "skipped: ",
                        MUTEST_COLOR_DARK_GREY,
                        reason,
                        MUTEST_COLOR_NONE,
                        NULL);
        }
      else
        {
          mutest_print (stdout,
                        "\n",
                        "  ",
                        "skipped: ",
                        reason,
                        NULL);
        }

      free (reason);
    }
}

static void
mocha_total_results (mutest_state_t *state)
{
  int total_pass, total_fail, total_skip;

  mutest_get_results (&total_pass, &total_fail, &total_skip);

  char passing_s[128], failing_s[128], skipped_s[128];

  snprintf (passing_s, 128, "%d passing", total_pass);
  snprintf (failing_s, 128, "%d failing", total_fail);
  snprintf (skipped_s, 128, "%d skipped", total_skip);

  const char *delta_u;
  double delta_t;
  char delta_s[128];

  delta_t = mutest_format_time (state->end_time - state->start_time, &delta_u);
  snprintf (delta_s, 128, "(%.2f %s)", delta_t, delta_u);

  if (mutest_use_colors ())
    {
      mutest_print (stdout,
                    "\n",
                    MUTEST_UNDERLINE_DEFAULT, "Total", MUTEST_COLOR_NONE, "\n",
                    MUTEST_COLOR_GREEN, passing_s, MUTEST_COLOR_NONE, " ",
                    MUTEST_COLOR_DARK_GREY, delta_s, MUTEST_COLOR_NONE,
                    NULL);

      if (total_skip != 0)
        mutest_print (stdout,
                      MUTEST_COLOR_YELLOW, skipped_s, MUTEST_COLOR_NONE,
                      NULL);

      if (total_fail != 0)
        mutest_print (stdout,
                      MUTEST_COLOR_RED, failing_s, MUTEST_COLOR_NONE,
                      NULL);

      mutest_print (stdout, "", NULL);
    }
  else
    mutest_print (stdout,
                  "\n",
                  "Total\n",
                  passing_s, " ", delta_s, "\n",
                  skipped_s, "\n",
                  failing_s, "\n",
                  NULL);
}

static void
mocha_expect_fail (mutest_expect_t *expect,
                   bool negate,
                   mutest_expect_res_t *check,
                   const char *check_repr)
{
  char *diagnostic = NULL;
  char *location = NULL;
  mutest_expect_diagnostic (expect, negate, check, check_repr,
                            &diagnostic,
                            &location);

  if (mutest_use_colors ())
    {
      mutest_print (stdout,
                    indent_expect (),
                    MUTEST_COLOR_RED,
                    "Assertion failure: ", diagnostic,
                    " at ", location,
                    MUTEST_COLOR_NONE,
                    NULL);
    }
  else
    {
      mutest_print (stdout,
                    indent_expect (),
                    "Assertion failure: ", diagnostic,
                    " at ", location,
                    NULL);
    }

  free (diagnostic);
  free (location);
}

const mutest_formatter_t *
mutest_get_mocha_formatter (void)
{
  static mutest_formatter_t mocha = {
    .suite_preamble = mocha_suite_preamble,
    .spec_preamble = mocha_spec_preamble,
    .expect_result = mocha_expect_result,
    .expect_fail = mocha_expect_fail,
    .spec_results = mocha_spec_results,
    .suite_results = mocha_suite_results,
    .total_results = mocha_total_results,
  };

  return &mocha;
}
