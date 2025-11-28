/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
sphere_init (mutest_spec_t *spec)
{
  graphene_point3d_t check;
  graphene_point3d_t zero = GRAPHENE_POINT3D_INIT_ZERO;
  graphene_point3d_t one = GRAPHENE_POINT3D_INIT (1, 1, 1);
  graphene_sphere_t *s;

  s = graphene_sphere_init (graphene_sphere_alloc (), &one, 1.f);
  mutest_expect ("alloc to not return null",
                 mutest_pointer (s),
                 mutest_not, mutest_to_be_null,
                 NULL);
  mutest_expect ("unit sphere not to be empty",
                 mutest_bool_value (graphene_sphere_is_empty (s)),
                 mutest_not, mutest_to_be_true,
                 NULL);

  graphene_sphere_get_center (s, &check);
  mutest_expect ("init to set the center",
                 mutest_bool_value (graphene_point3d_equal (&one, &check)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("init to set the radius",
                 mutest_float_value (graphene_sphere_get_radius (s)),
                 mutest_to_be_close_to, 1.0, 0.0001,
                 NULL);

  graphene_sphere_init (s, NULL, 1.f);
  graphene_sphere_get_center (s, &check);
  mutest_expect ("null center to be the origin",
                 mutest_bool_value (graphene_point3d_equal (&zero, &check)),
                 mutest_to_be_true,
                 NULL);

  graphene_sphere_free (s);
}

static void
sphere_contains_point (mutest_spec_t *spec)
{
  graphene_point3d_t check, zero = GRAPHENE_POINT3D_INIT_ZERO;
  graphene_sphere_t *s;

  s = graphene_sphere_init (graphene_sphere_alloc (), &zero, 1.f);

  mutest_expect ("unit sphere to contain origin",
                 mutest_bool_value (graphene_sphere_contains_point (s, &zero)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&check, 1.f, 0.f, 0.f);
  mutest_expect ("unit sphere to contain x=1",
                 mutest_bool_value (graphene_sphere_contains_point (s, &check)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&check, 0.f, -1.f, 0.f);
  mutest_expect ("unit sphere to contain y=-1",
                 mutest_bool_value (graphene_sphere_contains_point (s, &check)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&check, 0.f, 0.f, 1.f);
  mutest_expect ("unit sphere to contain z=1",
                 mutest_bool_value (graphene_sphere_contains_point (s, &check)),
                 mutest_to_be_true,
                 NULL);

  mutest_expect ("unit sphere to not contain (1, 1, 1)",
                 mutest_bool_value (graphene_sphere_contains_point (s, &GRAPHENE_POINT3D_INIT (1, 1, 1))),
                 mutest_not, mutest_to_be_true,
                 NULL);

  graphene_sphere_free (s);
}

static void
sphere_bounding_box (mutest_spec_t *spec)
{
  graphene_sphere_t s;
  graphene_box_t b, check;

  graphene_box_init (&check,
                     &GRAPHENE_POINT3D_INIT (0.f, 0.f, 0.f),
                     &GRAPHENE_POINT3D_INIT (2.f, 2.f, 2.f));

  graphene_sphere_init (&s, &GRAPHENE_POINT3D_INIT (1.f, 1.f, 1.f), 1.f);
  graphene_sphere_get_bounding_box (&s, &b);
  mutest_expect ("bounding box for a unit sphere centered in (1, 1, 1) is (0, 2)",
                 mutest_bool_value (graphene_box_equal (&b, &check)),
                 mutest_to_be_true,
                 NULL);

  graphene_sphere_init (&s, &GRAPHENE_POINT3D_INIT (0.f, 0.f, 0.f), 0.f);
  graphene_sphere_get_bounding_box (&s, &b);
  mutest_expect ("bounding box for degenerate sphere is degenerate box",
                 mutest_bool_value (graphene_box_equal (&b, graphene_box_zero ())),
                 mutest_to_be_true,
                 NULL);
}

static void
sphere_distance (mutest_spec_t *spec)
{
  graphene_sphere_t s;

  graphene_sphere_init (&s, &GRAPHENE_POINT3D_INIT (1.f, 1.f, 1.f), 1.f);
  mutest_expect ("distance of unit sphere from (0, 0, 0) is 0.7320",
                 mutest_float_value (graphene_sphere_distance (&s, &GRAPHENE_POINT3D_INIT (0, 0, 0))),
                 mutest_to_be_close_to, 0.7320, 0.001,
                 NULL);
  mutest_expect ("distance of unit sphere from (1, 1, 1) is -1",
                 mutest_float_value (graphene_sphere_distance (&s, &GRAPHENE_POINT3D_INIT (1, 1, 1))),
                 mutest_to_be_close_to, -1.0, 0.001,
                 NULL);
}

static void
sphere_translate (mutest_spec_t *spec)
{
  graphene_sphere_t s;
  graphene_point3d_t p;

  graphene_sphere_init (&s, &GRAPHENE_POINT3D_INIT (1.f, 1.f, 1.f), 1.f);
  graphene_point3d_init (&p, -1.f, -1.f, -1.f);
  graphene_sphere_translate (&s, &p, &s);
  graphene_sphere_get_center (&s, &p);
  mutest_expect ("translating a unit sphere from (1, 1, 1) by -1 puts it at (0, 0, 0)",
                 mutest_bool_value (graphene_point3d_equal (&p, &GRAPHENE_POINT3D_INIT (0.f, 0.f, 0.f))),
                 mutest_to_be_true,
                 NULL);
}

static void
sphere_empty (mutest_spec_t *spec)
{
  graphene_sphere_t s;

  graphene_sphere_init (&s, NULL, 0.f);
  mutest_expect ("sphere with a radius of zero is empty",
                 mutest_bool_value (graphene_sphere_is_empty (&s)),
                 mutest_to_be_true,
                 NULL);

  graphene_sphere_init (&s, &GRAPHENE_POINT3D_INIT (1.f, 1.f, 1.f), 1.f);
  mutest_expect ("unit sphere is not empty",
                 mutest_bool_value (graphene_sphere_is_empty (&s)),
                 mutest_to_be_false,
                 NULL);
}

static void
sphere_suite (mutest_suite_t *suite)
{
  mutest_it ("initialize", sphere_init);
  mutest_it ("contains point", sphere_contains_point);
  mutest_it ("bounding box", sphere_bounding_box);
  mutest_it ("distance", sphere_distance);
  mutest_it ("translate", sphere_translate);
  mutest_it ("empty", sphere_empty);
}

MUTEST_MAIN (
  mutest_describe ("graphene_sphere_t", sphere_suite);
)
