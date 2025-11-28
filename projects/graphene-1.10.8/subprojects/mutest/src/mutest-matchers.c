/* mutest-matchers.c: Matching functions
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

bool
mutest_not (mutest_expect_t *e MUTEST_UNUSED,
            mutest_expect_res_t *check MUTEST_UNUSED)
{
  return true;
}

bool
mutest_skip (mutest_expect_t *e MUTEST_UNUSED,
             mutest_expect_res_t *check MUTEST_UNUSED)
{
  return true;
}

static bool
mutest_to_be_boolean (mutest_expect_t *e,
                      mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_BOOLEAN &&
      check->expect_type == MUTEST_EXPECT_BOOLEAN)
    return value->expect.v_bool == check->expect.v_bool;

  return false;
}

static inline bool
int_near_equal (int a,
                int b,
                int epsilon)
{
  return (a > b ? (a - b) : (b - a)) <= epsilon;
}

static bool
mutest_to_be_int_value (mutest_expect_t *e,
                        mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_INT &&
      check->expect_type == MUTEST_EXPECT_INT)
    {
      return int_near_equal (value->expect.v_int.value,
                             check->expect.v_int.value,
                             check->expect.v_int.tolerance);
    }

  return false;
}

static bool
mutest_to_be_in_int_range (mutest_expect_t *e,
                           mutest_expect_res_t *check)
{
  if (e->value->expect_type == MUTEST_EXPECT_INT &&
      check->expect_type == MUTEST_EXPECT_INT_RANGE)
    {
      if (e->value->expect.v_int.value >= check->expect.v_irange.min &&
          e->value->expect.v_int.value <= check->expect.v_irange.max)
        return true;
    }

  return false;
}

static inline bool
float_near_equal (double a,
                  double b,
                  double epsilon)
{
  return (a > b ? (a - b) : (b - a)) <= epsilon;
}

static bool
mutest_to_be_float_value (mutest_expect_t *e,
                          mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT &&
      check->expect_type == MUTEST_EXPECT_FLOAT)
    {
      return float_near_equal (value->expect.v_float.value,
                               check->expect.v_float.value,
                               check->expect.v_float.tolerance);
    }

  return false;
}

static bool
mutest_to_be_in_float_range (mutest_expect_t *e,
                             mutest_expect_res_t *check)
{
  if (e->value->expect_type == MUTEST_EXPECT_FLOAT &&
      check->expect_type == MUTEST_EXPECT_FLOAT_RANGE)
    {
      if (e->value->expect.v_float.value >= check->expect.v_frange.min &&
          e->value->expect.v_float.value <= check->expect.v_frange.max)
        return true;
    }

  return false;
}

static bool
mutest_to_be_pointer (mutest_expect_t *e,
                      mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_POINTER &&
      check->expect_type == MUTEST_EXPECT_POINTER)
    return value->expect.v_pointer == check->expect.v_pointer;

  return false;
}

static bool
mutest_to_be_string (mutest_expect_t *e,
                     mutest_expect_res_t *check)
{
  if (e->value->expect_type == MUTEST_EXPECT_STR &&
      check->expect_type == MUTEST_EXPECT_STR)
    {
      if (e->value->expect.v_str.len != check->expect.v_str.len)
        return false;

      if (e->value->expect.v_str.str == NULL && check->expect.v_str.str == NULL)
        return true;

      if (e->value->expect.v_str.str != NULL &&
          check->expect.v_str.str != NULL &&
          strcmp (e->value->expect.v_str.str, check->expect.v_str.str) == 0)
        return true;
    }

  return false;
}

bool
mutest_to_be (mutest_expect_t *e,
              mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type != check->expect_type)
    return false;

  switch (value->expect_type)
    {
    case MUTEST_EXPECT_BOOLEAN:
      return mutest_to_be_boolean (e, check);

    case MUTEST_EXPECT_INT:
      return mutest_to_be_int_value (e, check);

    case MUTEST_EXPECT_INT_RANGE:
      return mutest_to_be_in_int_range (e, check);

    case MUTEST_EXPECT_FLOAT:
      return mutest_to_be_float_value (e, check);

    case MUTEST_EXPECT_FLOAT_RANGE:
      return mutest_to_be_in_float_range (e, check);

    case MUTEST_EXPECT_POINTER:
      return mutest_to_be_pointer (e, check);

    case MUTEST_EXPECT_STR:
      return mutest_to_be_string (e, check);

    case MUTEST_EXPECT_INVALID:
      mutest_assert_if_reached ("invalid expect value");
      break;
    }

  return false;
}

bool
mutest_to_be_close_to (mutest_expect_t *e,
                       mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT)
    return mutest_to_be_float_value (e, check);

  if (value->expect_type == MUTEST_EXPECT_INT)
    return mutest_to_be_int_value (e, check);

  return false;
}

bool
mutest_to_be_nan (mutest_expect_t *e,
                  mutest_expect_res_t *check MUTEST_UNUSED)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT)
    return isnan (value->expect.v_float.value) != 0;

  return false;
}

bool
mutest_to_be_positive_infinity (mutest_expect_t *e,
                                mutest_expect_res_t *check MUTEST_UNUSED)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT)
    return isinf (value->expect.v_float.value) != 0 && signbit (value->expect.v_float.value) == 0;

  return false;
}

bool
mutest_to_be_negative_infinity (mutest_expect_t *e,
                                mutest_expect_res_t *check MUTEST_UNUSED)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT)
    return isinf (value->expect.v_float.value) != 0 && signbit (value->expect.v_float.value) != 0;

  return false;
}

bool
mutest_to_be_true (mutest_expect_t *e,
                   mutest_expect_res_t *check MUTEST_UNUSED)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_BOOLEAN)
    return value->expect.v_bool;

  if (value->expect_type == MUTEST_EXPECT_INT)
    return value->expect.v_int.value != 0;

  return false;
}

bool
mutest_to_be_false (mutest_expect_t *e,
                    mutest_expect_res_t *check MUTEST_UNUSED)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_BOOLEAN)
    return !value->expect.v_bool;

  if (value->expect_type == MUTEST_EXPECT_INT)
    return value->expect.v_int.value == 0;

  return false;
}

bool
mutest_to_be_null (mutest_expect_t *e,
                   mutest_expect_res_t *check MUTEST_UNUSED)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_POINTER)
    return value->expect.v_pointer == NULL;

  return false;
}

bool
mutest_to_be_greater_than (mutest_expect_t *e,
                           mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_INT && check->expect_type == MUTEST_EXPECT_INT)
    return value->expect.v_int.value > check->expect.v_int.value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT && check->expect_type == MUTEST_EXPECT_FLOAT)
    return value->expect.v_float.value > check->expect.v_float.value;

  return false;
}

bool
mutest_to_be_greater_than_or_equal (mutest_expect_t *e,
                                    mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_INT && check->expect_type == MUTEST_EXPECT_INT)
    return value->expect.v_int.value >= check->expect.v_int.value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT && check->expect_type == MUTEST_EXPECT_FLOAT)
    return value->expect.v_float.value >= check->expect.v_float.value;

  return false;
}

bool
mutest_to_be_less_than_or_equal (mutest_expect_t *e,
                                 mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_INT && check->expect_type == MUTEST_EXPECT_INT)
    return value->expect.v_int.value <= check->expect.v_int.value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT && check->expect_type == MUTEST_EXPECT_FLOAT)
    return value->expect.v_float.value <= check->expect.v_float.value;

  return false;
}

bool
mutest_to_be_less_than (mutest_expect_t *e,
                        mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_INT && check->expect_type == MUTEST_EXPECT_INT)
    return value->expect.v_int.value < check->expect.v_int.value;

  if (value->expect_type == MUTEST_EXPECT_FLOAT && check->expect_type == MUTEST_EXPECT_FLOAT)
    return value->expect.v_float.value < check->expect.v_float.value;

  return false;
}

bool
mutest_to_be_in_range (mutest_expect_t *e,
                       mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_INT)
    return mutest_to_be_in_int_range (e, check);

  if (value->expect_type == MUTEST_EXPECT_FLOAT)
    return mutest_to_be_in_float_range (e, check);

  return false;
}

bool
mutest_to_contain (mutest_expect_t *e,
                   mutest_expect_res_t *check)
{
  mutest_expect_res_t *value = e->value;

  if (value->expect_type == MUTEST_EXPECT_INT_RANGE &&
      check->expect_type == MUTEST_EXPECT_INT)
    {
      if (check->expect.v_int.value >= value->expect.v_irange.min &&
          check->expect.v_int.value <= value->expect.v_irange.max)
        return true;

      return false;
    }

  if (value->expect_type == MUTEST_EXPECT_FLOAT_RANGE &&
      check->expect_type == MUTEST_EXPECT_FLOAT)
    {
      if (check->expect.v_float.value >= value->expect.v_frange.min &&
          check->expect.v_float.value <= value->expect.v_frange.max)
        return true;

      return false;
    }

  if (value->expect_type == MUTEST_EXPECT_STR &&
      check->expect_type == MUTEST_EXPECT_STR)
    {
      if (value->expect.v_str.len < check->expect.v_str.len)
        return false;

      if (strstr (value->expect.v_str.str, check->expect.v_str.str) != NULL)
        return true;
    }

  return false;
}

bool
mutest_to_contain_string (mutest_expect_t *e,
                          mutest_expect_res_t *check)
{
  if (e->value->expect_type == MUTEST_EXPECT_STR &&
      check->expect_type == MUTEST_EXPECT_STR)
    {
      if (e->value->expect.v_str.len < check->expect.v_str.len)
        return false;

      if (strstr (e->value->expect.v_str.str, check->expect.v_str.str) != NULL)
        return true;
    }

  return false;
}

bool
mutest_to_start_with_string (mutest_expect_t *e,
                             mutest_expect_res_t *check)
{
  if (e->value->expect_type == MUTEST_EXPECT_STR &&
      check->expect_type == MUTEST_EXPECT_STR)
    {
      if (e->value->expect.v_str.len < check->expect.v_str.len)
        return false;

      const char *str = e->value->expect.v_str.str;
      const char *prefix = check->expect.v_str.str;

      size_t prefix_len = check->expect.v_str.len;

      return strncmp (str, prefix, prefix_len) == 0;
    }

  return false;
}

bool
mutest_to_end_with_string (mutest_expect_t *e,
                           mutest_expect_res_t *check)
{
  if (e->value->expect_type == MUTEST_EXPECT_STR &&
      check->expect_type == MUTEST_EXPECT_STR)
    {
      if (e->value->expect.v_str.len < check->expect.v_str.len)
        return false;

      const char *str = e->value->expect.v_str.str;
      const char *suffix = check->expect.v_str.str;

      size_t str_len = e->value->expect.v_str.len;
      size_t suffix_len = check->expect.v_str.len;

      return strcmp (str + str_len - suffix_len, suffix) == 0;
    }

  return false;
}
