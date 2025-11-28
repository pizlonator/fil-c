/* SPDX-License-Identifier: MIT */

#include <math.h>
#include <string.h>
#include <graphene.h>
#include <mutest.h>

static void
vectors_vec4_const (void)
{
  const graphene_vec4_t *vec;

  vec = graphene_vec4_zero ();
  mutest_expect ("zero() vec4 to not be null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("zero() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec4_zero (),
                 NULL);
  mutest_expect ("zero().x to be 0",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);
  mutest_expect ("zero().y to be 0",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);
  mutest_expect ("zero().z to be 0",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);
  mutest_expect ("zero().w to be 0",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);
  mutest_expect ("length(zero()) to be 0",
                 mutest_float_value (graphene_vec4_length (vec)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);

  vec = graphene_vec4_one ();
  mutest_expect ("one() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("one() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec4_one (),
                 NULL);
  mutest_expect ("one().x to be 1",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("one().y to be 1",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("one().z to be 1",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("one().w to be 1",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  vec = graphene_vec4_x_axis ();
  mutest_expect ("x_axis() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("x_axis() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec4_x_axis (),
                 NULL);
  mutest_expect ("x_axis().x to be 1",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  vec = graphene_vec4_y_axis ();
  mutest_expect ("y_axis() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("y_axis() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec4_y_axis (),
                 NULL);
  mutest_expect ("y_axis().y to be 1",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  vec = graphene_vec4_z_axis ();
  mutest_expect ("z_axis() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("z_axis() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec4_z_axis (),
                 NULL);
  mutest_expect ("z_axis().z to be 1",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  vec = graphene_vec4_w_axis ();
  mutest_expect ("w_axis() to not return null",
                 mutest_pointer ((void *) vec),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("w_axis() to return the same pointer",
                 mutest_pointer ((void *) vec),
                 mutest_to_be, graphene_vec4_w_axis (),
                 NULL);
  mutest_expect ("w_axis().w to be 1",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
}

static void
vectors_vec4_init (void)
{
  graphene_vec4_t *vec = graphene_vec4_alloc ();
  const float v[4] = { 1.f, 2.f, 3.f, 4.f };
  float v_out[4];

  graphene_vec4_init (vec, v[0], v[1], v[2], v[3]);
  mutest_expect ("init() to initialize x component",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, v[0], 0.0001,
                 NULL);
  mutest_expect ("init() to initialize y component",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, v[1], 0.0001,
                 NULL);
  mutest_expect ("init() to initialize z component",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, v[2], 0.0001,
                 NULL);
  mutest_expect ("init() to initialize w component",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, v[3], 0.0001,
                 NULL);

  graphene_vec4_init_from_vec4 (vec, graphene_vec4_one ());
  mutest_expect ("init_from_vec4() to initialize x component",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec4() to initialize y component",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec4() to initialize z component",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec4() to initialize w component",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);

  graphene_vec4_init_from_vec3 (vec, graphene_vec3_x_axis (), 4.f);
  mutest_expect ("init_from_vec3() to initialize x component",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec3() to initialize y component",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec3() to initialize z component",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec3() to initialize w component",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, 4.0, 0.0001,
                 NULL);

  graphene_vec4_init_from_vec2 (vec, graphene_vec2_y_axis (), 2.f, 3.f);
  mutest_expect ("init_from_vec2() to initialize x component",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec2() to initialize y component",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec2() to initialize z component",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);
  mutest_expect ("init_from_vec4() to initialize w component",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, 3.0, 0.0001,
                 NULL);

  graphene_vec4_init_from_float (vec, v);
  mutest_expect ("init_from_float() to initialize x component",
                 mutest_float_value (graphene_vec4_get_x (vec)),
                 mutest_to_be_close_to, v[0], 0.0001,
                 NULL);
  mutest_expect ("init_from_float() to initialize y component",
                 mutest_float_value (graphene_vec4_get_y (vec)),
                 mutest_to_be_close_to, v[1], 0.0001,
                 NULL);
  mutest_expect ("init_from_float() to initialize z component",
                 mutest_float_value (graphene_vec4_get_z (vec)),
                 mutest_to_be_close_to, v[2], 0.0001,
                 NULL);
  mutest_expect ("init_from_float() to initialize w component",
                 mutest_float_value (graphene_vec4_get_w (vec)),
                 mutest_to_be_close_to, v[3], 0.0001,
                 NULL);

  graphene_vec4_to_float (vec, v_out);
  mutest_expect ("roundtrip between init_from_float() and to_float()",
                 mutest_bool_value (memcmp (v, v_out, sizeof (float) * 4) == 0),
                 mutest_to_be_true,
                 NULL);

  graphene_vec4_free (vec);
}

static void
vectors_vec4_ops_add (void)
{
  graphene_vec4_t a, b, res, check;

  graphene_vec4_init (&a, 1.f, 2.f, 3.f, 4.f);
  graphene_vec4_init (&b, 3.f, 4.f, 5.f, 6.f);

  graphene_vec4_add (&a, &b, &res);

  graphene_vec4_init (&check, 1.f + 3.f, 2.f + 4.f, 3.f + 5.f, 4.f + 6.f);
  mutest_expect ("add() to subtract each component",
                 mutest_bool_value (graphene_vec4_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_ops_sub (void)
{
  graphene_vec4_t a, b, res, check;

  graphene_vec4_init (&a, 1.f, 2.f, 3.f, 4.f);
  graphene_vec4_init (&b, 3.f, 4.f, 5.f, 6.f);

  graphene_vec4_subtract (&a, &b, &res);

  graphene_vec4_init (&check, 1.f - 3.f, 2.f - 4.f, 3.f - 5.f, 4.f - 6.f);
  mutest_expect ("sub() to subtract each component",
                 mutest_bool_value (graphene_vec4_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_ops_mul (void)
{
  graphene_vec4_t a, b, res, check;

  graphene_vec4_init (&a, 1.f, 2.f, 3.f, 4.f);
  graphene_vec4_init (&b, 3.f, 4.f, 5.f, 6.f);

  graphene_vec4_multiply (&a, &b, &res);

  graphene_vec4_init (&check, 1.f * 3.f, 2.f * 4.f, 3.f * 5.f, 4.f * 6.f);
  mutest_expect ("mul() to multiply each component",
                 mutest_bool_value (graphene_vec4_near (&res, &check, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_ops_div (void)
{
  graphene_vec4_t a, b, res, check;

  graphene_vec4_init (&a, 6.f, 4.f, 2.f, 12.f);
  graphene_vec4_init (&b, 3.f, 2.f, 1.f,  6.f);

  graphene_vec4_divide (&a, &b, &res);

  graphene_vec4_init (&check, 6.f / 3.f, 4.f / 2.f, 2.f / 1.f, 12.f / 6.f);
  mutest_expect ("divide() to divide each component",
                 mutest_bool_value (graphene_vec4_near (&res, &check, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_ops_dot (void)
{
  graphene_vec4_t a, b;
  float dot, check;

  graphene_vec4_init (&a, 1.f, 2.f, 3.f, 4.f);
  graphene_vec4_init (&b, 2.f, 3.f, 4.f, 5.f);

  dot = graphene_vec4_dot (&a, &b);
  check = 1.f * 2.f + 2.f * 3.f + 3.f * 4.f + 4.f * 5.f;

  mutest_expect ("dot() to compute the dot product of all component",
                 mutest_float_value (dot),
                 mutest_to_be_close_to, check, 0.0001,
                 NULL);
}

static void
vectors_vec4_ops_scale (void)
{
  graphene_vec4_t a, res, check;

  graphene_vec4_init (&a, 1.f, 2.f, 3.f, 4.f);
  graphene_vec4_scale (&a, 2.f, &res);

  graphene_vec4_init (&check, 2.f, 4.f, 6.f, 8.f);
  mutest_expect ("scale() to scale every component",
                 mutest_bool_value (graphene_vec4_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_ops_negate (void)
{
  graphene_vec4_t a, res, check;

  graphene_vec4_init (&a, 1.f, -2.f, 3.f, -4.f);
  graphene_vec4_negate (&a, &res);

  graphene_vec4_init (&check, -1.f, 2.f, -3.f, 4.f);
  mutest_expect ("negate() to flip the sign on every component",
                 mutest_bool_value (graphene_vec4_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_length (void)
{
  graphene_vec4_t a;
  float len, check;

  graphene_vec4_init (&a, 1.f, 2.f, 3.f, 4.f);

  len = graphene_vec4_length (&a);
  check = sqrtf ((1.f * 1.f) + (2.f * 2.f) + (3.f * 3.f) + (4.f * 4.f));

  mutest_expect ("length() to be the square distance on all components",
                 mutest_float_value (len),
                 mutest_to_be_close_to, check, 0.0001,
                 NULL);
}

static void
vectors_vec4_normalize (void)
{
  graphene_vec4_t a, b, c;
  float inv_len;

  graphene_vec4_init (&a, 1.f, 2.f, 3.f, 4.f);
  inv_len = 1.f / graphene_vec4_length (&a);

  graphene_vec4_normalize (&a, &b);
  graphene_vec4_scale (&a, inv_len, &c);

  mutest_expect ("normalize() to scale components with the inverse of the length",
                 mutest_bool_value (graphene_vec4_near (&b, &c, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_compare (void)
{
  graphene_vec4_t a, b;
  graphene_vec4_t res, check;

  graphene_vec4_init (&a, 5.f, 10.f, 8.f, -1.f);
  graphene_vec4_init (&b, 2.f, 12.f, 8.f,  1.f);

  graphene_vec4_init (&check, 2.f, 10.f, 8.f, -1.f);
  graphene_vec4_min (&a, &b, &res);
  mutest_expect ("min() to return the minimum value in each channel",
                 mutest_bool_value (graphene_vec4_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec4_init (&check, 5.f, 12.f, 8.f, 1.f);
  graphene_vec4_max (&a, &b, &res);
  mutest_expect ("max() to return the maximum value in each channel",
                 mutest_bool_value (graphene_vec4_equal (&res, &check)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_conversion (void)
{
  graphene_vec4_t v;

  graphene_vec4_init (&v, 1.f, 2.f, 3.f, 4.f);

  graphene_vec2_t xy, check2;

  graphene_vec4_get_xy (&v, &xy);
  graphene_vec2_init (&check2,
                      graphene_vec4_get_x (&v),
                      graphene_vec4_get_y (&v));
  mutest_expect ("get_xy() to return the first two channels of a vec4",
                 mutest_bool_value (graphene_vec2_equal (&xy, &check2)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec3_t xyz, check3;

  graphene_vec4_get_xyz (&v, &xyz);
  graphene_vec3_init (&check3,
                      graphene_vec4_get_x (&v),
                      graphene_vec4_get_y (&v),
                      graphene_vec4_get_z (&v));

  mutest_expect ("get_xyz() to return the first three channels of a vec4",
                 mutest_bool_value (graphene_vec3_equal (&xyz, &check3)),
                 mutest_to_be_true,
                 NULL);
}

static void
vectors_vec4_equal (void)
{
  const graphene_vec4_t *zero = graphene_vec4_zero ();
  const graphene_vec4_t *one = graphene_vec4_one ();

  mutest_expect ("a vector to be equal to itself",
                 mutest_bool_value (graphene_vec4_equal (zero, zero)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a vector not to be equal to null",
                 mutest_bool_value (graphene_vec4_equal (zero, NULL)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("null not to be equal to a vector",
                 mutest_bool_value (graphene_vec4_equal (NULL, zero)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("two different vectors to not be equal",
                 mutest_bool_value (graphene_vec4_equal (zero, one)),
                 mutest_not, mutest_to_be_true,
                 NULL);
}

static void
vec4_suite (void)
{
  mutest_it ("has constant vectors", vectors_vec4_const);
  mutest_it ("initializes channels", vectors_vec4_init);
  mutest_it ("can add", vectors_vec4_ops_add);
  mutest_it ("can subtract", vectors_vec4_ops_sub);
  mutest_it ("can multiply", vectors_vec4_ops_mul);
  mutest_it ("can divide", vectors_vec4_ops_div);
  mutest_it ("can compute the dot product", vectors_vec4_ops_dot);
  mutest_it ("can scale", vectors_vec4_ops_scale);
  mutest_it ("can negate", vectors_vec4_ops_negate);
  mutest_it ("can compute the length", vectors_vec4_length);
  mutest_it ("can normalize", vectors_vec4_normalize);
  mutest_it ("can compare", vectors_vec4_compare);
  mutest_it ("can convert to other vector types", vectors_vec4_conversion);
  mutest_it ("can check for equality", vectors_vec4_equal);
}

MUTEST_MAIN (
  mutest_describe ("graphene_vec4_t", vec4_suite);
)
