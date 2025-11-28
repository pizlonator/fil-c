/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
plane_init_base (mutest_spec_t *spec)
{
  graphene_plane_t *p;
  graphene_vec3_t n;

  p = graphene_plane_init (graphene_plane_alloc (), graphene_vec3_one (), 1.f);
  graphene_plane_get_normal (p, &n);
  mutest_expect ("init to set normal field",
                 mutest_bool_value (graphene_vec3_equal (&n, graphene_vec3_one ())),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("init to set constant field",
                 mutest_float_value (graphene_plane_get_constant (p)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);

  graphene_plane_init (p, NULL, 0.f);
  graphene_plane_get_normal (p, &n);
  mutest_expect ("init with null normal defaults to X axis",
                 mutest_bool_value (graphene_vec3_equal (&n, graphene_vec3_x_axis ())),
                 mutest_to_be_true,
                 NULL);

  graphene_plane_free (p);
}

static void
plane_init_normal_point (mutest_spec_t *spec)
{
  graphene_vec3_t normal, check;
  graphene_plane_t *p;
  graphene_point3d_t zero = GRAPHENE_POINT3D_INIT_ZERO;

  graphene_vec3_normalize (graphene_vec3_one (), &normal);

  p = graphene_plane_init_from_point (graphene_plane_alloc (), &normal, &zero);
  graphene_plane_get_normal (p, &check);

  mutest_expect ("plane normal is set",
                 mutest_bool_value (graphene_vec3_equal (&check, &normal)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("plane constant is set",
                 mutest_float_value (graphene_plane_get_constant (p)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  graphene_plane_free (p);
}

static void
plane_normalize (mutest_spec_t *spec)
{
  graphene_plane_t p;
  graphene_vec3_t tmp;

  graphene_plane_init (&p, graphene_vec3_init (&tmp, 2.f, 0.f, 0.f), 2.f);
  graphene_plane_normalize (&p, &p);
  graphene_plane_get_normal (&p, &tmp);

  mutest_expect ("plane normal to be the X axis",
                 mutest_bool_value (graphene_vec3_near (&tmp, graphene_vec3_x_axis (), 0.0001f)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("length of plane normal to be 1",
                 mutest_float_value (graphene_vec3_length (&tmp)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
  mutest_expect ("plane constant to be 1",
                 mutest_float_value (graphene_plane_get_constant (&p)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);
}

static void
plane_distance_positive (mutest_spec_t *spec)
{
  graphene_plane_t p;
  graphene_vec3_t tmp;
  graphene_point3d_t point;

  graphene_plane_init (&p, graphene_vec3_init (&tmp, 2.f, 0.f, 0.f), -2.f);
  graphene_plane_normalize (&p, &p);

  graphene_point3d_init (&point, 4.f, 0.f, 0.f);
  mutest_expect ("distance(4, 0, 0) to be 3",
                 mutest_float_value (graphene_plane_distance (&p, &point)),
                 mutest_to_be_close_to, 3.0, 0.0001,
                 NULL);

  graphene_point3d_init (&point, 1.f, 0.f, 0.f);
  mutest_expect ("distance(1, 0, 0) to be 0",
                 mutest_float_value (graphene_plane_distance (&p, &point)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  graphene_plane_negate (&p, &p);

  graphene_point3d_init (&point, 4.f, 0.f, 0.f);
  mutest_expect ("distance(4, 0, 0) to be -3 after negating",
                 mutest_float_value (graphene_plane_distance (&p, &point)),
                 mutest_to_be_close_to, -3.0, 0.0001,
                 NULL);

  graphene_point3d_init (&point, 1.f, 0.f, 0.f);
  mutest_expect ("distance(1, 0, 0) to be 0 after negating",
                 mutest_float_value (graphene_plane_distance (&p, &point)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
}

static void
plane_distance_negative (mutest_spec_t *spec)
{
  graphene_plane_t p;
  graphene_vec3_t tmp;
  graphene_point3d_t point;

  graphene_plane_init (&p, graphene_vec3_init (&tmp, 2.f, 0.f, 0.f), 2.f);
  graphene_plane_normalize (&p, &p);

  graphene_point3d_init (&point, 4.f, 0.f, 0.f);
  mutest_expect ("distance(4, 0, 0) to be 5",
                 mutest_float_value (graphene_plane_distance (&p, &point)),
                 mutest_to_be_close_to, 5.0, 0.0001,
                 NULL);
}

static void
plane_suite (mutest_suite_t *suite)
{
  mutest_it ("initializes with a normal and a constant", plane_init_base);
  mutest_it ("initializes with a normal and a point", plane_init_normal_point);
  mutest_it ("normalizes", plane_normalize);
  mutest_it ("handles positive distances", plane_distance_positive);
  mutest_it ("handles negative distances", plane_distance_negative);
}

MUTEST_MAIN (
  mutest_describe ("graphene_plane_t", plane_suite);
)
