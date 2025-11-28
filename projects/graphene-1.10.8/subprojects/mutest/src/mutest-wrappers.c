/* mutest-wrappers.c: Value wrappers
 *
 * µTest - Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#include "mutest-private.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

void
mutest_expect_res_free (mutest_expect_res_t *res)
{
  if (res == NULL)
    return;

  switch (res->expect_type)
    {
    case MUTEST_EXPECT_INVALID:
      mutest_assert_if_reached ("invalid value");
      break;

    case MUTEST_EXPECT_BOOLEAN:
    case MUTEST_EXPECT_INT:
    case MUTEST_EXPECT_INT_RANGE:
    case MUTEST_EXPECT_FLOAT:
    case MUTEST_EXPECT_FLOAT_RANGE:
    case MUTEST_EXPECT_POINTER:
      break;

    case MUTEST_EXPECT_STR:
      free (res->expect.v_str.str);
      break;
    }

  free (res);
}

void
mutest_expect_res_to_string (mutest_expect_res_t *res,
                             char *buf,
                             size_t len)
{
  if (res == NULL)
    return;

  switch (res->expect_type)
    {
    case MUTEST_EXPECT_INVALID:
      snprintf (buf, len, "invalid");
      break;

    case MUTEST_EXPECT_BOOLEAN:
      snprintf (buf, len, "%s", res->expect.v_bool ? "true" : "false");
      break;

    case MUTEST_EXPECT_INT:
      if (res->expect.v_int.tolerance == 0)
        snprintf (buf, len, "%d", res->expect.v_int.value);
      else
        snprintf (buf, len, "%d (± %d)",
                  res->expect.v_int.value,
                  res->expect.v_int.tolerance);
      break;

    case MUTEST_EXPECT_INT_RANGE:
      snprintf (buf, len, "[ %d, %d ]",
                res->expect.v_irange.min,
                res->expect.v_irange.max);
      break;

    case MUTEST_EXPECT_FLOAT:
      if (isnan (res->expect.v_float.value))
        {
          snprintf (buf, len, "%s", "NaN");
        }
      else if (isinf (res->expect.v_float.value) && signbit (res->expect.v_float.value) == 0)
        {
          snprintf (buf, len, "%s", "+∞");
        }
      else if (isinf (res->expect.v_float.value) && signbit (res->expect.v_float.value) != 0)
        {
          snprintf (buf, len, "%s", "-∞");
        }
      else
        {
          snprintf (buf, len, "%g (± %g)",
                    res->expect.v_float.value,
                    res->expect.v_float.tolerance);
        }
      break;

    case MUTEST_EXPECT_FLOAT_RANGE:
      snprintf (buf, len, "[ %g, %g ]",
                res->expect.v_frange.min,
                res->expect.v_frange.max);
      break;

    case MUTEST_EXPECT_POINTER:
      if (res->expect.v_pointer != NULL)
        snprintf (buf, len, "%p", res->expect.v_pointer);
      else
        snprintf (buf, len, "%s", "null");
      break;

    case MUTEST_EXPECT_STR:
      snprintf (buf, len, "%s", res->expect.v_str.str);
      break;
    }
}

mutest_expect_res_t *
mutest_expect_res_alloc (mutest_expect_type_t type)
{
  mutest_expect_res_t *retval = calloc (1, sizeof (mutest_expect_res_t));
  if (retval == NULL)
    mutest_oom_abort ();

  retval->expect_type = type;

  return retval;
}

mutest_expect_res_t *
mutest_bool_value (bool value)
{
  mutest_expect_res_t *res = mutest_expect_res_alloc (MUTEST_EXPECT_BOOLEAN);
  if (res == NULL)
    mutest_oom_abort ();

  res->expect.v_bool = value;

  return res;
}

bool
mutest_get_bool_value (const mutest_expect_res_t *res)
{
  if (res->expect_type != MUTEST_EXPECT_BOOLEAN)
    mutest_assert_if_reached ("invalid boolean value");

  return res->expect.v_bool;
}

mutest_expect_res_t *
mutest_int_value (int value)
{
  mutest_expect_res_t *res = mutest_expect_res_alloc (MUTEST_EXPECT_INT);

  res->expect.v_int.value = value;
  res->expect.v_int.tolerance = 0;

  return res;
}

int
mutest_get_int_value (const mutest_expect_res_t *res)
{
  if (res->expect_type != MUTEST_EXPECT_INT)
    mutest_assert_if_reached ("invalid integer value");

  return res->expect.v_int.value;
}

mutest_expect_res_t *
mutest_string_value (const char *value)
{
  mutest_expect_res_t *res = mutest_expect_res_alloc (MUTEST_EXPECT_STR);

  res->expect.v_str.str = mutest_strdup_and_len (value, &(res->expect.v_str.len));

  return res;
}

const char *
mutest_get_string_value (const mutest_expect_res_t *res)
{
  if (res->expect_type != MUTEST_EXPECT_STR)
    mutest_assert_if_reached ("invalid string value");

  return res->expect.v_str.str;
}

mutest_expect_res_t *
mutest_float_value (double value)
{
  mutest_expect_res_t *res = mutest_expect_res_alloc (MUTEST_EXPECT_FLOAT);

  res->expect.v_float.value = value;
  res->expect.v_float.tolerance = DBL_EPSILON;

  return res;
}

double
mutest_get_float_value (const mutest_expect_res_t *res)
{
  if (res->expect_type != MUTEST_EXPECT_FLOAT)
    mutest_assert_if_reached ("invalid floating point value");

  return res->expect.v_float.value;
}

mutest_expect_res_t *
mutest_int_range (int min,
                  int max)
{
  mutest_expect_res_t *res = mutest_expect_res_alloc (MUTEST_EXPECT_INT_RANGE);

  res->expect.v_irange.min = min;
  res->expect.v_irange.max = max;

  return res;
}

void
mutest_get_int_range (const mutest_expect_res_t *res,
                      int *min,
                      int *max)
{
  if (res->expect_type != MUTEST_EXPECT_INT_RANGE)
    mutest_assert_if_reached ("invalid integer range");

  if (min != NULL)
    *min = res->expect.v_irange.min;
  if (max != NULL)
    *max = res->expect.v_irange.max;
}

mutest_expect_res_t *
mutest_float_range (double min,
                    double max)
{
  mutest_expect_res_t *res = mutest_expect_res_alloc (MUTEST_EXPECT_FLOAT_RANGE);

  res->expect.v_frange.min = min;
  res->expect.v_frange.max = max;

  return res;
}

void
mutest_get_float_range (const mutest_expect_res_t *res,
                        double *min,
                        double *max)
{
  if (res->expect_type != MUTEST_EXPECT_FLOAT_RANGE)
    mutest_assert_if_reached ("invalid floating point range");

  if (min != NULL)
    *min = res->expect.v_frange.min;
  if (max != NULL)
    *max = res->expect.v_frange.max;
}

mutest_expect_res_t *
mutest_pointer (const void *pointer)
{
  mutest_expect_res_t *res = mutest_expect_res_alloc (MUTEST_EXPECT_POINTER);

  res->expect.v_pointer = (void *) pointer;

  return res;
}

const void *
mutest_get_pointer (const mutest_expect_res_t *res)
{
  if (res->expect_type != MUTEST_EXPECT_POINTER)
    mutest_assert_if_reached ("invalid pointer");

  return res->expect.v_pointer;
}

#if 0
mutest_expect_res_t *
mutest_byte_array (void *data,
                   size_t element_size,
                   size_t length)
{
  mutest_expect_res_t *res = malloc (sizeof (mutest_expect_res_t));
  if (res == NULL)
    mutest_oom_abort ();

  res->expect_type = MUTEST_EXPECT_BYTE_ARRAY;
  res->expect.v_bytearray.data = NULL;
  res->expect.v_bytearray.element_size = element_size;
  res->expect.v_bytearray.length = length;

  if (element_size == 0 || length == 0)
    return res;

  size_t max_size = (size_t) -1;

  if (element_size > 0 && length > max_size / element_size)
    mutest_assert_if_reached ("overflow");

  size_t total_size = element_size * length;

  res->expect.v_bytearray.data = malloc (total_size);
  if (res->expect.v_bytearray.data == NULL)
    mutest_oom_abort ();

  memcpy (res->expect.v_bytearray.data, data, total_size);

  return res;
}

mutest_expect_res_t *
mutest_closure (mutest_expect_closure_func_t func,
                void *data)
{
  mutest_expect_res_t *res = malloc (sizeof (mutest_expect_res_t));
  if (res == NULL)
    mutest_oom_abort ();

  res->expect_type = MUTEST_EXPECT_CLOSURE;
  res->expect.v_closure.func = func;
  res->expect.v_closure.data = data;

  return res;
}
#endif
