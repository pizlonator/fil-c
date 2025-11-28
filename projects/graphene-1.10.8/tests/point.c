/* SPDX-License-Identifier: MIT */

#include <math.h>
#include <graphene.h>
#include <mutest.h>

static void
point_zero (mutest_spec_t *spec)
{
  graphene_point_t zero = GRAPHENE_POINT_INIT_ZERO;
  const graphene_point_t *p;

  p = graphene_point_zero ();
  mutest_expect ("graphene_point_zero() to not return null",
                 mutest_pointer ((void *) p),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("graphene_point_zero() to return the same pointer",
                 mutest_pointer ((void *) graphene_point_zero ()),
                 mutest_to_be, p,
                 NULL);
  mutest_expect ("graphene_point_zero() to return (0, 0)",
                 mutest_bool_value (graphene_point_equal (p, &zero)),
                 mutest_to_be_true,
                 NULL);
}

static void
point_alloc_and_init (mutest_spec_t *spec)
{
  graphene_point_t *a, b;
  graphene_point_t p, r;
  graphene_vec2_t vec;

  a = graphene_point_alloc ();
  mutest_expect ("allocation to not fail",
                 mutest_pointer (a),
                 mutest_not, mutest_to_be_null,
                 NULL);

  graphene_point_init (a, 1.f, 2.f);
  graphene_point_init_from_point (&b, a);
  mutest_expect ("init_from_point to copy the source",
                 mutest_bool_value (graphene_point_equal (&b, a)),
                 mutest_to_be_true,
                 NULL);

  graphene_point_free (a);

  graphene_point_init (&p, 1.f, 2.f);
  mutest_expect ("init to set the X coordinate",
                 mutest_float_value (p.x),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init to set the Y coordinate",
                 mutest_float_value (p.y),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);

  graphene_vec2_init (&vec, 2.0, 4.0);
  graphene_point_init_from_vec2 (&r, &vec);
  mutest_expect ("init from vec2 to set the X coordinate",
                 mutest_float_value (graphene_vec2_get_x (&vec)),
                 mutest_to_be_close_to, r.x, 0.0001,
                 NULL);
  mutest_expect ("init from vec2 to set the Y coordinate",
                 mutest_float_value (graphene_vec2_get_y (&vec)),
                 mutest_to_be_close_to, r.y, 0.0001,
                 NULL);
}

static void
point_equal (mutest_spec_t *spec)
{
  graphene_point_t p, q;

  graphene_point_init (&p, 0.f, 0.f);

  mutest_expect ("a point to be equal to itself",
                 mutest_bool_value (graphene_point_equal (&p, &p)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a point to not be equal to null",
                 mutest_bool_value (graphene_point_equal (&p, NULL)),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("null to not be equal to a point",
                 mutest_bool_value (graphene_point_equal (NULL, &p)),
                 mutest_to_be_false,
                 NULL);

  graphene_point_init (&q, 0.f, 0.f);
  mutest_expect ("two points initialized to the same coordinates to be equal",
                 mutest_bool_value (graphene_point_equal (&p, &q)),
                 mutest_to_be_true,
                 NULL);

  graphene_point_init (&q, 1.f, 1.f);
  mutest_expect ("two points initialized to different coordinates not to be equal",
                 mutest_bool_value (graphene_point_equal (&p, &q)),
                 mutest_to_be_false,
                 NULL);
}

static void
point_near (mutest_spec_t *spec)
{
  graphene_point_t p, q;

  graphene_point_init (&p, 0.f, 0.f);
  mutest_expect ("a point to be near itself",
                 mutest_bool_value (graphene_point_near (&p, &p, 0.f)),
                 mutest_to_be_true,
                 NULL);

  graphene_point_init (&q, 0.1f, 0.1f);
  mutest_expect ("(0.1, 0.1) to be within 0.2 units from (0, 0)",
                 mutest_bool_value (graphene_point_near (&p, &q, 0.2f)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(0.1, 0.1) to not be within 0.001 from (0, 0)",
                 mutest_bool_value (graphene_point_near (&p, &q, 0.001f)),
                 mutest_to_be_false,
                 NULL);
}

static void
point_distance (mutest_spec_t *spec)
{
  graphene_point_t p, q;
  float d, x_d, y_d;

  graphene_point_init (&p, 0.f, 0.f);
  mutest_expect ("the distance of a point from itself to be zero",
                 mutest_float_value (graphene_point_distance (&p, &p, NULL, NULL)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);

  graphene_point_init (&q, 1.f, 1.f);

  x_d = y_d = 0.f;
  d = graphene_point_distance (&p, &q, &x_d, &y_d);
  mutest_expect ("the distance between (0, 0) and (1, 1) to be sqrt(2)",
                 mutest_float_value (d),
                 mutest_to_be_close_to, sqrtf (2.f), 0.0001,
                 NULL);
  mutest_expect ("the distance between (0, 0) and (1, 1) to be 1 on the X axis",
                 mutest_float_value (x_d),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("the distance between (0, 0) and (1, 1) to be 1 on the Y axis",
                 mutest_float_value (y_d),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
}

static void
point_interpolate (mutest_spec_t *spec)
{
  graphene_point_t zero = GRAPHENE_POINT_INIT (0.f, 0.f);
  graphene_point_t half = GRAPHENE_POINT_INIT (.5f, .5f);
  graphene_point_t one = GRAPHENE_POINT_INIT (1.f, 1.f);
  graphene_point_t res;

  graphene_point_interpolate (&zero, &one, 0.0, &res);
  mutest_expect ("lerp(0, 1, 0.0) = 0",
                 mutest_bool_value (graphene_point_equal (&res, &zero)),
                 mutest_to_be_true,
                 NULL);

  graphene_point_interpolate (&zero, &one, 0.5, &res);
  mutest_expect ("lerp(0, 1, 0.5) = 0.5",
                 mutest_bool_value (graphene_point_equal (&res, &half)),
                 mutest_to_be_true,
                 NULL);

  graphene_point_interpolate (&zero, &one, 1.0, &res);
  mutest_expect ("lerp(0, 1, 1.0) = 1",
                 mutest_bool_value (graphene_point_equal (&res, &one)),
                 mutest_to_be_true,
                 NULL);
}

static void
point_suite (mutest_suite_t *suite)
{
  mutest_it ("allocates and initializes data", point_alloc_and_init);
  mutest_it ("provides a zero point", point_zero);
  mutest_it ("has exact equality", point_equal);
  mutest_it ("has near equality", point_near);
  mutest_it ("computes distances", point_distance);
  mutest_it ("interpolates between values", point_interpolate);
}

MUTEST_MAIN (
  mutest_describe ("graphene_point_t", point_suite);
)
