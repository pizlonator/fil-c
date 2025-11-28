/* SPDX-License-Identifier: MIT */

#include <string.h>
#include <math.h>
#include <graphene.h>
#include <mutest.h>

static void
simd_dup_4f (void)
{
  graphene_simd4f_t s;
  float v[4];
  struct { float x, y, z, w; } p;
  float check[4] = { 2.f, 3.f, 4.f, 5.f };

  s = graphene_simd4f_init (2.f, 3.f, 4.f, 5.f);

  graphene_simd4f_dup_4f (s, v);
  mutest_expect ("dup_4f() to work with an array",
                 mutest_bool_value (memcmp (v, check, sizeof (float) * 4) == 0),
                 mutest_to_be_true,
                 NULL);

  graphene_simd4f_dup_4f (s, &(p.x));
  mutest_expect ("dup_4f() to work with a structure",
                 mutest_bool_value (memcmp (&p, check, sizeof (float) * 4) == 0),
                 mutest_to_be_true,
                 NULL);
}

static void
simd_dup_3f (void)
{
  graphene_simd4f_t s;
  float v[3];
  struct { float x, y, z; } p;
  float check[3] = { 2.f, 3.f, 4.f };

  s = graphene_simd4f_init (2.f, 3.f, 4.f, 5.f);

  graphene_simd4f_dup_3f (s, v);
  mutest_expect ("dup_3f() to work with an array",
                 mutest_bool_value (memcmp (v, check, sizeof (float) * 3) == 0),
                 mutest_to_be_true,
                 NULL);

  graphene_simd4f_dup_3f (s, &(p.x));
  mutest_expect ("dup_3f() to work with a structure",
                 mutest_bool_value (memcmp (&p, check, sizeof (float) * 3) == 0),
                 mutest_to_be_true,
                 NULL);
}

static void
simd_dup_2f (void)
{
  graphene_simd4f_t s;
  float v[2];
  struct { float x, y; } p;
  float check[2] = { 2.f, 3.f };

  s = graphene_simd4f_init (2.f, 3.f, 4.f, 5.f);

  graphene_simd4f_dup_2f (s, v);
  mutest_expect ("dup_2f() to work with an array",
                 mutest_bool_value (memcmp (v, check, sizeof (float) * 2) == 0),
                 mutest_to_be_true,
                 NULL);

  graphene_simd4f_dup_2f (s, &(p.x));
  mutest_expect ("dup_2f() to work with a structure",
                 mutest_bool_value (memcmp (&p, check, sizeof (float) * 2) == 0),
                 mutest_to_be_true,
                 NULL);
}

static void
simd_compare_eq (void)
{
  graphene_simd4f_t a, b, c;

  a = graphene_simd4f_init (1.f, 0.f, 1.f, 0.f);
  mutest_expect ("to be equal to itself",
                 mutest_bool_value (graphene_simd4f_cmp_eq (a, a)),
                 mutest_to_be_true,
                 NULL);

  b = graphene_simd4f_init (1.f, 0.f, 1.f, 0.f);
  mutest_expect ("two vectors with the same values to be equal",
                 mutest_bool_value (graphene_simd4f_cmp_eq (a, b) && graphene_simd4f_cmp_eq (b, a)),
                 mutest_to_be_true,
                 NULL);

  c = graphene_simd4f_splat (1.f);
  mutest_expect ("two different vectors to be different",
                 mutest_bool_value (graphene_simd4f_cmp_eq (a, c)),
                 mutest_to_be_false,
                 NULL);

  mutest_expect ("two different vectors to not be equal",
                 mutest_bool_value (graphene_simd4f_cmp_neq (b, c)),
                 mutest_to_be_true,
                 NULL);
}

static void
simd_compare_lt (void)
{
  graphene_simd4f_t a, b;

  a = graphene_simd4f_init (0.f, 0.f, 0.f, 0.f);
  b = graphene_simd4f_init (1.f, 1.f, 1.f, 1.f);
  mutest_expect ("a < b to be true if all components satisfy the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_lt (a, b)),
                 mutest_to_be_true,
                 NULL);

  b = graphene_simd4f_init (0.f, 1.f, 1.f, 1.f);
  mutest_expect ("a < b to be false if any component does not satify the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_lt (a, b)),
                 mutest_to_be_false,
                 NULL);
}

static void
simd_compare_le (void)
{
  graphene_simd4f_t a, b;

  a = graphene_simd4f_init (0.f, 0.f, 0.f, 0.f);
  b = graphene_simd4f_init (1.f, 0.f, 1.f, 0.f);
  mutest_expect ("a ≤ b to be true if all components satisfy the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_le (a, b)),
                 mutest_to_be_true,
                 NULL);

  a = graphene_simd4f_init (2.f, 0.f, 1.f, 0.f);
  mutest_expect ("a ≤ b to be false if any component does not satify the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_le (a, b)),
                 mutest_to_be_false,
                 NULL);
}

static void
simd_compare_ge (void)
{
  graphene_simd4f_t a, b;

  a = graphene_simd4f_init (0.f, 0.f, 0.f, 0.f);
  b = graphene_simd4f_init (1.f, 0.f, 1.f, 0.f);
  mutest_expect ("a ≤ b to be true if all components satisfy the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_ge (b, a)),
                 mutest_to_be_true,
                 NULL);

  a = graphene_simd4f_init (2.f, 0.f, 1.f, 0.f);
  mutest_expect ("a ≤ b to be false if any component does not satify the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_ge (b, a)),
                 mutest_to_be_false,
                 NULL);
}

static void
simd_compare_gt (void)
{
  graphene_simd4f_t a, b;

  a = graphene_simd4f_init (0.f, 0.f, 0.f, 0.f);
  b = graphene_simd4f_init (1.f, 1.f, 1.f, 1.f);
  mutest_expect ("a ≤ b to be true if all components satisfy the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_gt (b, a)),
                 mutest_to_be_true,
                 NULL);

  a = graphene_simd4f_init (2.f, 0.f, 0.f, 0.f);
  mutest_expect ("a ≤ b to be false if any component does not satify the order relation",
                 mutest_bool_value (graphene_simd4f_cmp_gt (b, a)),
                 mutest_to_be_false,
                 NULL);
}

static void
simd_operators_dot (void)
{
  graphene_simd4f_t a, b, c;
  float dot;

  a = graphene_simd4f_init (1.f, 2.f, 0.f, 0.f);
  b = graphene_simd4f_init (3.f, 4.f, 0.f, 0.f);
  c = graphene_simd4f_dot2 (a, b);

  dot = 3.f * 1.f + 2.f * 4.f;
  mutest_expect ("dot2() to compute the dot product of the first 2 components",
                 mutest_float_value (graphene_simd4f_get_x (c)),
                 mutest_to_be_close_to, dot, 0.0001,
                 NULL);

  a = graphene_simd4f_init (1.f, 2.f, 3.f, 0.f);
  b = graphene_simd4f_init (4.f, 5.f, 6.f, 0.f);
  c = graphene_simd4f_dot3 (a, b);

  dot = 1.f * 4.f + 2.f * 5.f + 3.f * 6.f;
  mutest_expect ("dot3() to compute the dot product of the first 3 components",
                 mutest_float_value (graphene_simd4f_get_x (c)),
                 mutest_to_be_close_to, dot, 0.0001,
                 NULL);

  a = graphene_simd4f_init (1.f, 2.f, 3.f, 4.f);
  b = graphene_simd4f_init (5.f, 6.f, 7.f, 8.f);
  c = graphene_simd4f_dot4 (a, b);

  dot = 1.f * 5.f + 2.f * 6.f + 3.f * 7.f + 4.f * 8.f;
  mutest_expect ("dot4() to compute the dot product of all four components",
                 mutest_float_value (graphene_simd4f_get_x (c)),
                 mutest_to_be_close_to, dot, 0.0001,
                 NULL);
}

static void
simd_operators_min (void)
{
  graphene_simd4f_t a, b, c, check;

  a = graphene_simd4f_init (1.f, 6.f, 3.f, 8.f);
  b = graphene_simd4f_init (5.f, 2.f, 7.f, 4.f);

  c = graphene_simd4f_min (a, b);
  check = graphene_simd4f_init (1.f, 2.f, 3.f, 4.f);
  mutest_expect ("min() to return the minimum of each component",
                 mutest_bool_value (graphene_simd4f_cmp_eq (c, check)),
                 mutest_to_be_true,
                 NULL);

  a = graphene_simd4f_init (1.f, 2.f, 3.f, 4.f);
  check = graphene_simd4f_min_val (a);
  mutest_expect ("min_val() to put the minimum value in the X component",
                 mutest_float_value (graphene_simd4f_get_x (check)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
}

static void
simd_operators_max (void)
{
  graphene_simd4f_t a, b, c, check;

  a = graphene_simd4f_init (1.f, 6.f, 3.f, 8.f);
  b = graphene_simd4f_init (5.f, 2.f, 7.f, 4.f);

  c = graphene_simd4f_max (a, b);
  check = graphene_simd4f_init (5.f, 6.f, 7.f, 8.f);
  mutest_expect ("max() to return the maximum of each component",
                 mutest_bool_value (graphene_simd4f_cmp_eq (c, check)),
                 mutest_to_be_true,
                 NULL);

  a = graphene_simd4f_init (1.f, 2.f, 3.f, 4.f);
  check = graphene_simd4f_max_val (a);
  mutest_expect ("max_val() to put the maximum value in the X component",
                 mutest_float_value (graphene_simd4f_get_x (check)),
                 mutest_to_be_close_to, 4.0, 0.0001,
                 NULL);
}

static void
simd_operators_reciprocal (void)
{
  graphene_simd4f_t a, b;

  a = graphene_simd4f_init (1.f, -1.f, -8.f, 0.5f);

  b = graphene_simd4f_reciprocal (a);
  mutest_expect ("reciprocal() to return the reciprocal of the X component",
                 mutest_float_value (graphene_simd4f_get_x (b)),
                 mutest_to_be_close_to, 1.f, FLT_EPSILON,
                 NULL);
  mutest_expect ("reciprocal() to return the reciprocal of the Y component",
                 mutest_float_value (graphene_simd4f_get_y (b)),
                 mutest_to_be_close_to, -1.f, FLT_EPSILON,
                 NULL);
  mutest_expect ("reciprocal() to return the reciprocal of the Z component",
                 mutest_float_value (graphene_simd4f_get_z (b)),
                 mutest_to_be_close_to, -0.125f, FLT_EPSILON,
                 NULL);
  mutest_expect ("reciprocal() to return the reciprocal of the W component",
                 mutest_float_value (graphene_simd4f_get_w (b)),
                 mutest_to_be_close_to, 2.f, FLT_EPSILON,
                 NULL);

  a = graphene_simd4f_init (.1234f, -1234.f, -0.4321f, 4321.f);

  b = graphene_simd4f_reciprocal (a);
  mutest_expect ("reciprocal() to return the approximate reciprocal of the X component",
                 mutest_float_value (graphene_simd4f_get_x (b)),
                 mutest_to_be_close_to, 8.103727714749, 0.000001,
                 NULL);
  mutest_expect ("reciprocal() to return the approximate reciprocal of the Y component",
                 mutest_float_value (graphene_simd4f_get_y (b)),
                 mutest_to_be_close_to, -0.000810372771, 0.000001,
                 NULL);
  mutest_expect ("reciprocal() to return the approximate reciprocal of the Z component",
                 mutest_float_value (graphene_simd4f_get_z (b)),
                 mutest_to_be_close_to, -2.31427910206, 0.000001,
                 NULL);
  mutest_expect ("reciprocal() to return the approximate reciprocal of the W component",
                 mutest_float_value (graphene_simd4f_get_w (b)),
                 mutest_to_be_close_to, 0.00023142791, 0.000001,
                 NULL);

  a = graphene_simd4f_init (0.f, -0.f, 5.f, -10.f);

  b = graphene_simd4f_reciprocal (a);
  mutest_expect ("reciprocal() to return positive infinity in the X component",
                 mutest_float_value (graphene_simd4f_get_x (b)),
                 mutest_to_be_positive_infinity,
                 NULL);
  mutest_expect ("reciprocal() to return negative infinity in the Y component",
                 mutest_float_value (graphene_simd4f_get_y (b)),
                 mutest_to_be_negative_infinity,
                 NULL);
}

static void
simd_suite (void)
{
  mutest_it ("can copy 4 components", simd_dup_4f);
  mutest_it ("can copy 3 components", simd_dup_3f);
  mutest_it ("can copy 2 components", simd_dup_2f);

  mutest_it ("can compare for equality", simd_compare_eq);
  mutest_it ("has less than ordering", simd_compare_lt);
  mutest_it ("has less than or equal ordering", simd_compare_le);
  mutest_it ("has greater than or equal ordering", simd_compare_ge);
  mutest_it ("has greater than ordering", simd_compare_gt);

  mutest_it ("can compute the dot product", simd_operators_dot);

  mutest_it ("can compute the minimum vector and scalar", simd_operators_min);
  mutest_it ("can compute the maximum vector and scalar", simd_operators_max);

  mutest_it ("can compute the reciprocal of vector", simd_operators_reciprocal);
}

MUTEST_MAIN (
  mutest_describe ("graphene_simd4f_t", simd_suite);
)
