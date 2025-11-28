/* SPDX-License-Identifier: MIT */

#include <math.h>
#include <string.h>
#include <graphene.h>
#include <mutest.h>

static void
vectors_vec3_const (void)
{
  const graphene_vec3_t *vec;

  vec = graphene_vec3_zero ();
  mutest_expect ("zero() vec3 to not be null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("zero() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec3_zero (),
                 NULL);
  mutest_expect ("zero().x to be 0",
                 mutest_float_value (graphene_vec3_get_x (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);
  mutest_expect ("zero().y to be 0",
                 mutest_float_value (graphene_vec3_get_y (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);
  mutest_expect ("zero().z to be 0",
                 mutest_float_value (graphene_vec3_get_z (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);
  mutest_expect ("length(zero()) to be 0",
                 mutest_float_value (graphene_vec3_length (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);

  vec = graphene_vec3_one ();
  mutest_expect ("one() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("one() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec3_one (),
                 NULL);
  mutest_expect ("one().x to be 1",
                 mutest_float_value (graphene_vec3_get_x (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("one().y to be 1",
                 mutest_float_value (graphene_vec3_get_y (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("one().z to be 1",
                 mutest_float_value (graphene_vec3_get_z (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  vec = graphene_vec3_x_axis ();
  mutest_expect ("x_axis() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("x_axis() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec3_x_axis (),
                 NULL);
  mutest_expect ("x_axis().x to be 1",
                 mutest_float_value (graphene_vec3_get_x (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  vec = graphene_vec3_y_axis ();
  mutest_expect ("y_axis() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("y_axis() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec3_y_axis (),
                 NULL);
  mutest_expect ("y_axis().y to be 1",
                 mutest_float_value (graphene_vec3_get_y (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  vec = graphene_vec3_z_axis ();
  mutest_expect ("z_axis() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("z_axis() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec3_z_axis (),
                 NULL);
  mutest_expect ("z_axis().z to be 1",
                 mutest_float_value (graphene_vec3_get_z (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
}

static void
vectors_vec3_init (void)
{
  graphene_vec3_t *vec = graphene_vec3_alloc ();
  const float v[3] = { 1.f, 2.f, 3.f };
  float v_out[3];

  graphene_vec3_init (vec, v[0], v[1], v[2]);
  mutest_expect ("init() to initialize x component",
                 mutest_float_value (graphene_vec3_get_x (vec)),
                 mutest_to_be_close_to, v[0], 0.0001,
                 NULL);
  mutest_expect ("init() to initialize y component",
                 mutest_float_value (graphene_vec3_get_y (vec)),
                 mutest_to_be_close_to, v[1], 0.0001,
                 NULL);
  mutest_expect ("init() to initialize z component",
                 mutest_float_value (graphene_vec3_get_z (vec)),
                 mutest_to_be_close_to, v[2], 0.0001,
                 NULL);

  graphene_vec3_init_from_vec3 (vec, graphene_vec3_x_axis ());
  mutest_expect ("init_from_vec3() to initialize x component",
                 mutest_float_value (graphene_vec3_get_x (vec)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec3() to initialize y component",
                 mutest_float_value (graphene_vec3_get_y (vec)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec3() to initialize z component",
                 mutest_float_value (graphene_vec3_get_z (vec)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  graphene_vec3_init_from_float (vec, v);
  mutest_expect ("init_from_float() to initialize x component",
                 mutest_float_value (graphene_vec3_get_x (vec)),
                 mutest_to_be_close_to, v[0], 0.0001,
                 NULL);
  mutest_expect ("init_from_float() to initialize y component",
                 mutest_float_value (graphene_vec3_get_y (vec)),
                 mutest_to_be_close_to, v[1], 0.0001,
                 NULL);
  mutest_expect ("init_from_float() to initialize z component",
                 mutest_float_value (graphene_vec3_get_z (vec)),
                 mutest_to_be_close_to, v[2], 0.0001,
                 NULL);

  graphene_vec3_to_float (vec, v_out);
  mutest_expect ("roundtrip between init_from_float() and to_float()",
                 mutest_bool_value (memcmp (v, v_out, sizeof (float) * 3) == 0),
                 mutest_to_be_true,
                 NULL);

  graphene_vec3_free (vec);
}

static void
vectors_vec3_ops_add (void)
{
  graphene_vec3_t a, b, res, check;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);
  graphene_vec3_init (&b, 3.f, 4.f, 5.f);

  graphene_vec3_add (&a, &b, &res);

  graphene_vec3_init (&check, 1.f + 3.f, 2.f + 4.f, 3.f + 5.f);
  mutest_expect ("add() to subtract each component",
                 mutest_bool_value (graphene_vec3_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_ops_sub (void)
{
  graphene_vec3_t a, b, res, check;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);
  graphene_vec3_init (&b, 3.f, 4.f, 5.f);

  graphene_vec3_subtract (&a, &b, &res);

  graphene_vec3_init (&check, 1.f - 3.f, 2.f - 4.f, 3.f - 5.f);
  mutest_expect ("sub() to subtract each component",
                 mutest_bool_value (graphene_vec3_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_ops_mul (void)
{
  graphene_vec3_t a, b, res, check;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);
  graphene_vec3_init (&b, 3.f, 4.f, 5.f);

  graphene_vec3_multiply (&a, &b, &res);

  graphene_vec3_init (&check, 1.f * 3.f, 2.f * 4.f, 3.f * 5.f);
  mutest_expect ("mul() to multiply each component",
                 mutest_bool_value (graphene_vec3_near (&res, &check, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_ops_div (void)
{
  graphene_vec3_t a, b, res, check;

  graphene_vec3_init (&a, 6.f, 4.f, 2.f);
  graphene_vec3_init (&b, 3.f, 2.f, 1.f);

  graphene_vec3_divide (&a, &b, &res);

  graphene_vec3_init (&check, 6.f / 3.f, 4.f / 2.f, 2.f / 1.f);
  mutest_expect ("divide() to divide each component",
                 mutest_bool_value (graphene_vec3_near (&res, &check, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_ops_dot (void)
{
  graphene_vec3_t a, b;
  float dot, check;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);
  graphene_vec3_init (&b, 2.f, 3.f, 4.f);

  dot = graphene_vec3_dot (&a, &b);
  check = 1.f * 2.f + 2.f * 3.f + 3.f * 4.f;

  mutest_expect ("dot() to compute the dot product of all component",
                 mutest_float_value (dot),
                 mutest_to_be_close_to, check, 0.0001,
                 NULL);
}

static void
vectors_vec3_ops_cross (void)
{
  graphene_vec3_t a, b, res, cross;
  float cross_x, cross_y, cross_z;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);
  graphene_vec3_init (&b, 2.f, 3.f, 4.f);

  graphene_vec3_cross (&a, &b, &res);

  cross_x = 2.f * 4.f - 3.f * 3.f;
  cross_y = 3.f * 2.f - 1.f * 4.f;
  cross_z = 1.f * 3.f - 2.f * 2.f;
  graphene_vec3_init (&cross, cross_x, cross_y, cross_z);

  mutest_expect ("cross() to compute the cross product of all component",
                 mutest_bool_value (graphene_vec3_near (&res, &cross, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_ops_scale (void)
{
  graphene_vec3_t a, res, check;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);
  graphene_vec3_scale (&a, 2.f, &res);

  graphene_vec3_init (&check, 2.f, 4.f, 6.f);
  mutest_expect ("scale() to scale every component",
                 mutest_bool_value (graphene_vec3_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_ops_negate (void)
{
  graphene_vec3_t a, res, check;

  graphene_vec3_init (&a, 1.f, -2.f, 3.f);
  graphene_vec3_negate (&a, &res);

  graphene_vec3_init (&check, -1.f, 2.f, -3.f);
  mutest_expect ("negate() to flip the sign on every component",
                 mutest_bool_value (graphene_vec3_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_length (void)
{
  graphene_vec3_t a;
  float len, check;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);

  len = graphene_vec3_length (&a);
  check = sqrtf ((1.f * 1.f) + (2.f * 2.f) + (3.f * 3.f));

  mutest_expect ("length() to be the square distance on all components",
                 mutest_float_value (len),
                 mutest_to_be_close_to, check, 0.0001,
                 NULL);
}

static void
vectors_vec3_normalize (void)
{
  graphene_vec3_t a, b, c;
  float inv_len;

  graphene_vec3_init (&a, 1.f, 2.f, 3.f);
  inv_len = 1.f / graphene_vec3_length (&a);

  graphene_vec3_normalize (&a, &b);
  graphene_vec3_scale (&a, inv_len, &c);

  mutest_expect ("normalize() to scale components with the inverse of the length",
                 mutest_bool_value (graphene_vec3_near (&b, &c, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_compare (void)
{
  graphene_vec3_t a, b;
  graphene_vec3_t res, check;

  graphene_vec3_init (&a, 5.f, 10.f, 8.f);
  graphene_vec3_init (&b, 2.f, 12.f, 8.f);

  graphene_vec3_init (&check, 2.f, 10.f, 8.f);
  graphene_vec3_min (&a, &b, &res);
  mutest_expect ("min() to return the minimum value in each channel",
                 mutest_bool_value (graphene_vec3_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec3_init (&check, 5.f, 12.f, 8.f);
  graphene_vec3_max (&a, &b, &res);
  mutest_expect ("max() to return the maximum value in each channel",
                 mutest_bool_value (graphene_vec3_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_conversion (void)
{
  graphene_vec3_t v;

  graphene_vec3_init (&v, 1.f, 2.f, 3.f);

  graphene_vec2_t xy, check2;

  graphene_vec2_init (&check2, graphene_vec3_get_x (&v), graphene_vec3_get_y (&v));
  graphene_vec3_get_xy (&v, &xy);
  mutest_expect ("get_xy() to use the first two components",
                 mutest_bool_value (graphene_vec2_equal (&xy, &check2)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec3_t xy0, check3;

  graphene_vec3_init (&check3, graphene_vec3_get_x (&v), graphene_vec3_get_y (&v), 0.f);
  graphene_vec3_get_xy0 (&v, &xy0);
  mutest_expect ("get_xy0() to use the first two components, and set the third to 0",
                 mutest_bool_value (graphene_vec3_equal (&xy0, &check3)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec4_t xyz0, xyz1, xyzw, check4;

  graphene_vec4_init (&check4,
                      graphene_vec3_get_x (&v),
                      graphene_vec3_get_y (&v),
                      graphene_vec3_get_z (&v),
                      0.f);
  graphene_vec3_get_xyz0 (&v, &xyz0);
  mutest_expect ("get_xyz0() to use all three components, and set the fourth to 0",
                 mutest_bool_value (graphene_vec4_equal (&xyz0, &check4)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec4_init (&check4,
                      graphene_vec3_get_x (&v),
                      graphene_vec3_get_y (&v),
                      graphene_vec3_get_z (&v),
                      1.f);
  graphene_vec3_get_xyz1 (&v, &xyz1);
  mutest_expect ("get_xyz1() to use all three components, and set the fourth to 1",
                 mutest_bool_value (graphene_vec4_equal (&xyz1, &check4)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec4_init (&check4,
                      graphene_vec3_get_x (&v),
                      graphene_vec3_get_y (&v),
                      graphene_vec3_get_z (&v),
                      4.f);
  graphene_vec3_get_xyzw (&v, 4.f, &xyzw);
  mutest_expect ("get_xyzw() to use all three components, and set the fourth to the w argument",
                 mutest_bool_value (graphene_vec4_equal (&xyzw, &check4)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec3_equal (void)
{
  const graphene_vec3_t *zero = graphene_vec3_zero ();
  const graphene_vec3_t *one = graphene_vec3_one ();

  mutest_expect ("a vector to be equal to itself",
                 mutest_bool_value (graphene_vec3_equal (zero, zero)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a vector not to be equal to null",
                 mutest_bool_value (graphene_vec3_equal (zero, NULL)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("null not to be equal to a vector",
                 mutest_bool_value (graphene_vec3_equal (NULL, zero)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("two different vectors to not be equal",
                 mutest_bool_value (graphene_vec3_equal (zero, one)),
                 mutest_not, mutest_to_be_true,
                 NULL);
}

static void
vec3_suite (void)
{
  mutest_it ("has constant vectors", vectors_vec3_const);
  mutest_it ("initializes channels", vectors_vec3_init);
  mutest_it ("can add", vectors_vec3_ops_add);
  mutest_it ("can subtract", vectors_vec3_ops_sub);
  mutest_it ("can multiply", vectors_vec3_ops_mul);
  mutest_it ("can divide", vectors_vec3_ops_div);
  mutest_it ("can compute the dot product", vectors_vec3_ops_dot);
  mutest_it ("can compute the cross product", vectors_vec3_ops_cross);
  mutest_it ("can scale", vectors_vec3_ops_scale);
  mutest_it ("can negate", vectors_vec3_ops_negate);
  mutest_it ("can compute the length", vectors_vec3_length);
  mutest_it ("can normalize", vectors_vec3_normalize);
  mutest_it ("can compare", vectors_vec3_compare);
  mutest_it ("can convert to other vector types", vectors_vec3_conversion);
  mutest_it ("can check for equality", vectors_vec3_equal);
}

MUTEST_MAIN (
  mutest_describe ("graphene_vec3_t", vec3_suite);
)
