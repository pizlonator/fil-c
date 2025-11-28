/* SPDX-License-Identifier: MIT */

#include <math.h>
#include <graphene.h>
#include <mutest.h>

static void
point_zero (mutest_spec_t *spec)
{
  graphene_point3d_t zero = GRAPHENE_POINT3D_INIT_ZERO;
  const graphene_point3d_t *p;

  p = graphene_point3d_zero ();
  mutest_expect ("graphene_point3d_zero() to not return null",
                 mutest_pointer ((void *) p),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("graphene_point3d_zero() to return the same pointer",
                 mutest_pointer ((void *) graphene_point3d_zero ()),
                 mutest_to_be, p,
                 NULL);
  mutest_expect ("graphene_point3d_zero() to return (0, 0, 0)",
                 mutest_bool_value (graphene_point3d_equal (p, &zero)),
                 mutest_to_be_true,
                 NULL);
}

static void
point_alloc_and_init (mutest_spec_t *spec)
{
  graphene_point3d_t *a, b;
  graphene_point3d_t p, r;
  graphene_vec3_t vec3;

  a = graphene_point3d_alloc ();
  mutest_expect ("allocation to not fail",
                 mutest_pointer (a),
                 mutest_not, mutest_to_be_null,
                 NULL);

  graphene_point3d_init (a, 1.f, 2.f, 3.f);
  graphene_point3d_init_from_point (&b, a);
  mutest_expect ("init_from_point to copy the source",
                 mutest_bool_value (graphene_point3d_equal (&b, a)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_free (a);

  graphene_point3d_init (&p, 1.f, 2.f, 3.f);
  mutest_expect ("init to set the X coordinate",
                 mutest_float_value (p.x),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("init to set the Y coordinate",
                 mutest_float_value (p.y),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);
  mutest_expect ("init to set the Z coordinate",
                 mutest_float_value (p.z),
                 mutest_to_be_close_to, 3.0, 0.0001,
                 NULL);

  graphene_vec3_init (&vec3, 1.0, 2.0, 4.0);
  graphene_point3d_init_from_vec3 (&r, &vec3);
  mutest_expect ("init from vec3 to set the X coordinate",
                 mutest_float_value (graphene_vec3_get_x (&vec3)),
                 mutest_to_be_close_to, r.x, 0.0001,
                 NULL);
  mutest_expect ("init from vec3 to set the Y coordinate",
                 mutest_float_value (graphene_vec3_get_y (&vec3)),
                 mutest_to_be_close_to, r.y, 0.0001,
                 NULL);
  mutest_expect ("init from vec3 to set the Z coordinate",
                 mutest_float_value (graphene_vec3_get_z (&vec3)),
                 mutest_to_be_close_to, r.z, 0.0001,
                 NULL);
}

static void
point_equal (mutest_spec_t *spec)
{
  graphene_point3d_t p, q;

  graphene_point3d_init (&p, 0.f, 0.f, 0.f);

  mutest_expect ("a point to be equal to itself",
                 mutest_bool_value (graphene_point3d_equal (&p, &p)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a point to not be equal to null",
                 mutest_bool_value (graphene_point3d_equal (&p, NULL)),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("null to not be equal to a point",
                 mutest_bool_value (graphene_point3d_equal (NULL, &p)),
                 mutest_to_be_false,
                 NULL);

  graphene_point3d_init (&q, 0.f, 0.f, 0.f);
  mutest_expect ("two points initialized to the same coordinates to be equal",
                 mutest_bool_value (graphene_point3d_equal (&p, &q)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&q, 1.f, 1.f, 1.f);
  mutest_expect ("two points initialized to different coordinates not to be equal",
                 mutest_bool_value (graphene_point3d_equal (&p, &q)),
                 mutest_to_be_false,
                 NULL);
}

static void
point_near (mutest_spec_t *spec)
{
  graphene_point3d_t p, q;

  graphene_point3d_init (&p, 0.f, 0.f, 0.f);
  mutest_expect ("a point to be near itself",
                 mutest_bool_value (graphene_point3d_near (&p, &p, 0.f)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&q, 0.1f, 0.1f, 0.1f);
  mutest_expect ("(0.1, 0.1, 0.1) to be within 0.2 units from (0, 0, 0)",
                 mutest_bool_value (graphene_point3d_near (&p, &q, 0.2f)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(0.1, 0.1, 0.1) to not be within 0.001 from (0, 0, 0)",
                 mutest_bool_value (graphene_point3d_near (&p, &q, 0.001f)),
                 mutest_to_be_false,
                 NULL);
}

static void
point_interpolate (mutest_spec_t *spec)
{
  graphene_point3d_t zero = GRAPHENE_POINT3D_INIT (0.f, 0.f, 0.f);
  graphene_point3d_t half = GRAPHENE_POINT3D_INIT (.5f, .5f, .5f);
  graphene_point3d_t one = GRAPHENE_POINT3D_INIT (1.f, 1.f, 1.f);
  graphene_point3d_t res;

  graphene_point3d_interpolate (&zero, &one, 0.0, &res);
  mutest_expect ("lerp(0, 1, 0.0) = 0",
                 mutest_bool_value (graphene_point3d_equal (&res, &zero)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_interpolate (&zero, &one, 0.5, &res);
  mutest_expect ("lerp(0, 1, 0.5) = 0.5",
                 mutest_bool_value (graphene_point3d_equal (&res, &half)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_interpolate (&zero, &one, 1.0, &res);
  mutest_expect ("lerp(0, 1, 1.0) = 1",
                 mutest_bool_value (graphene_point3d_equal (&res, &one)),
                 mutest_to_be_true,
                 NULL);
}

static void
point_scale (mutest_spec_t *spec)
{
  float x = 1.f, y = 2.f, z = 3.f;
  graphene_point3d_t p;

  graphene_point3d_init (&p, x, y, z);
  graphene_point3d_scale (&p, 2.f, &p);
  mutest_expect ("scale(2) scales the X coordinate",
                 mutest_float_value (p.x),
                 mutest_to_be_close_to, x * 2.0, 0.0001,
                 NULL);
  mutest_expect ("scale(2) scales the Y coordinate",
                 mutest_float_value (p.y),
                 mutest_to_be_close_to, y * 2.0, 0.0001,
                 NULL);
  mutest_expect ("scale(2) scales the Z coordinate",
                 mutest_float_value (p.z),
                 mutest_to_be_close_to, z * 2.0, 0.0001,
                 NULL);

  graphene_point3d_init (&p, x, y, z);
  graphene_point3d_scale (&p, .5f, &p);
  mutest_expect ("scale(0.5) scales the X coordinate",
                 mutest_float_value (p.x),
                 mutest_to_be_close_to, x * 0.5, 0.0001,
                 NULL);
  mutest_expect ("scale(0.5) scales the Y coordinate",
                 mutest_float_value (p.y),
                 mutest_to_be_close_to, y * 0.5, 0.0001,
                 NULL);
  mutest_expect ("scale(0.5) scales the Z coordinate",
                 mutest_float_value (p.z),
                 mutest_to_be_close_to, z * 0.5, 0.0001,
                 NULL);
}

static void
point_cross (mutest_spec_t *spec)
{
  graphene_point3d_t a, b, res;
  graphene_vec3_t v_a, v_b, v_res, check;

  graphene_point3d_init (&a, 1.f, 2.f, 3.f);
  graphene_point3d_to_vec3 (&a, &v_a);

  graphene_point3d_init (&b, 4.f, 5.f, 6.f);
  graphene_point3d_to_vec3 (&b, &v_b);

  graphene_point3d_cross (&a, &b, &res);
  graphene_point3d_to_vec3 (&res, &check);

  graphene_vec3_cross (&v_a, &v_b, &v_res);

  mutest_expect ("cross(a, b) matches both point3d and vec3",
                 mutest_bool_value (graphene_vec3_equal (&check, &v_res)),
                 mutest_to_be_true,
                 NULL);
}

static void
point_dot (mutest_spec_t *spec)
{
  graphene_point3d_t a, b;
  graphene_vec3_t v_a, v_b;

  graphene_point3d_init (&a, 1.f, 2.f, 3.f);
  graphene_point3d_to_vec3 (&a, &v_a);
  graphene_point3d_init (&b, 4.f, 5.f, 6.f);
  graphene_point3d_to_vec3 (&b, &v_b);

  mutest_expect ("dot(a, b) matches both point3d and vec3",
                 mutest_float_value (graphene_point3d_dot (&a, &b)),
                 mutest_to_be_close_to, graphene_vec3_dot (&v_a, &v_b), 0.0001,
                 NULL);
}

static void
point_length (mutest_spec_t *spec)
{
  graphene_point3d_t p;
  float res;

  graphene_point3d_init (&p, 1.f, 3.f, 5.f);
  res = sqrtf (1.f + (3.f * 3.f) + (5.f * 5.f));

  mutest_expect ("length is the square root of the sum components squared",
                 mutest_float_value (graphene_point3d_length (&p)),
                 mutest_to_be_close_to, res, 0.0001f,
                 NULL);
}

static void
point_normalize (mutest_spec_t *spec)
{
  graphene_point3d_t p, q;

  graphene_point3d_init (&p, 4.f, 8.f, 2.f);
  graphene_point3d_normalize (&p, &q);

  mutest_expect ("normalization to change the point",
                 mutest_bool_value (graphene_point3d_equal (&p, &q)),
                 mutest_to_be_false,
                 NULL);
}

static void
point_normalize_viewport (mutest_spec_t *spec)
{
  graphene_point3d_t p, q;
  graphene_rect_t v;

  graphene_point3d_init (&p, 150.f, 20.f, 0.f);
  graphene_rect_init (&v, 0.f, 0.f, 640.f, 480.f);
  graphene_point3d_normalize_viewport (&p, &v, 1.f, 100.f, &q);

  mutest_expect ("original point and normalized point not to match",
                 mutest_bool_value (graphene_point3d_equal (&p, &q)),
                 mutest_to_be_false,
                 NULL);

  mutest_expect ("normalized X coordinate to be [-1, 1)",
                 mutest_float_value (q.x),
                 mutest_to_be_in_range, -1.0, 1.01,
                 NULL);
  mutest_expect ("normalized Y coordinate to be [-1, 1)",
                 mutest_float_value (q.y),
                 mutest_to_be_in_range, -1.0, 1.01,
                 NULL);
  mutest_expect ("normalized Z coordinate to be [-1, 1)",
                 mutest_float_value (q.z),
                 mutest_to_be_in_range, -1.0, 1.01,
                 NULL);
}

static void
point3d_suite (mutest_suite_t *suite)
{
  mutest_it ("allocates and initializes data", point_alloc_and_init);
  mutest_it ("provides a zero point", point_zero);
  mutest_it ("has exact equality", point_equal);
  mutest_it ("has near equality", point_near);
  mutest_it ("interpolates between values", point_interpolate);
  mutest_it ("scales coordinates", point_scale);
  mutest_it ("implements cross product", point_cross);
  mutest_it ("implements dot product", point_dot);
  mutest_it ("implements length", point_length);
  mutest_it ("implements normalization", point_normalize);
  mutest_it ("allows normalizing within a viewport", point_normalize_viewport);
}

MUTEST_MAIN (
  mutest_describe ("graphene_point3d_t", point3d_suite);
)
