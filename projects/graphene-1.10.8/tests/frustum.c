/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
frustum_init (mutest_spec_t *spec)
{
  graphene_plane_t p0, p1, p2, p3, p4, p5;
  graphene_plane_t planes[6];
  graphene_frustum_t *t;

  graphene_plane_init (&p0, graphene_vec3_x_axis (), -1.f);
  graphene_plane_init (&p1, graphene_vec3_x_axis (),  1.f);
  graphene_plane_init (&p2, graphene_vec3_x_axis (),  2.f);
  graphene_plane_init (&p3, graphene_vec3_x_axis (),  3.f);
  graphene_plane_init (&p4, graphene_vec3_x_axis (),  4.f);
  graphene_plane_init (&p5, graphene_vec3_x_axis (),  5.f);

  t = graphene_frustum_init (graphene_frustum_alloc (), &p0, &p1, &p2, &p3, &p4, &p5);

  graphene_frustum_get_planes (t, planes);

  bool all_planes_equal =
    graphene_plane_equal (&planes[0], &p0) &&
    graphene_plane_equal (&planes[1], &p1) &&
    graphene_plane_equal (&planes[2], &p2) &&
    graphene_plane_equal (&planes[3], &p3) &&
    graphene_plane_equal (&planes[4], &p4) &&
    graphene_plane_equal (&planes[5], &p5);

  mutest_expect ("init to use the given planes",
                 mutest_bool_value (all_planes_equal),
                 mutest_to_be_true,
                 NULL);

  graphene_frustum_free (t);
}

static void
frustum_ortho_contains_point (mutest_spec_t *spec)
{
  graphene_matrix_t m;
  graphene_frustum_t f;
  graphene_point3d_t p;

  graphene_matrix_init_ortho (&m, -1.f, 1.f, -1.f, 1.f, 1.f, 100.f);
  graphene_frustum_init_from_matrix (&f, &m);

  mutest_expect ("(0, 0, 0) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, 0.f))),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("(0, 0, -50) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, -50.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(0, 0, -1.001) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, -1.001f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(-1, -1, -1.001) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, -1.f, -1.f, -1.001f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(-1.1, -1.1, -1.001) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, -1.1f, -1.1f, -1.001f))),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("(1, 1, -1.001) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 1.f, 1.f, -1.001f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(1.1, 1.1, -1.001) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 1.1f, 1.1f, -1.001f))),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("(0, 0, -100) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, -100.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(-1, -1, -100) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, -1.f, -1.f, -100.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(-1.1, -1.1, -100) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, -1.1f, -1.1f, -100.1f))),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("(1, 1, -100) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 1.f, 1.f, -100.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(1.1, 1.1, -100) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 1.1f, 1.1f, -100.1f))),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("(0, 0, -101) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, -101.f))),
                 mutest_to_be_false,
                 NULL);
}

static void
frustum_matrix_contains_point (mutest_spec_t *spec)
{
#if !defined(GRAPHENE_USE_GCC)
  graphene_matrix_t m;
  graphene_frustum_t f;
  graphene_point3d_t p;

  graphene_matrix_init_frustum (&m, -1.f, 1.f, -1.f, 1.f, 1.f, 100.f);
  graphene_frustum_init_from_matrix (&f, &m);

  mutest_expect ("(0, 0, 0) is to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, 0.f))),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("(0, 0, -50) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, -50.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(0, 0, -1.001) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 0.f, 0.f, -1.001f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(-1, -1, -1.001) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, -1.f, -1.f, -1.001f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(-1.1, -1.1, -1.001) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, -1.1f, -1.1f, -1.001f))),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("(1, 1, -1.001) to be inside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 1.f, 1.f, -1.001f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("(1.1, 1.1, -1.001) to be outside the frustum",
                 mutest_bool_value (graphene_frustum_contains_point (&f, graphene_point3d_init (&p, 1.1f, 1.1f, -1.001f))),
                 mutest_to_be_false,
                 NULL);
#endif
}

static void
frustum_suite (mutest_suite_t *suite)
{
  mutest_it ("initializes frustums from planes", frustum_init);
  mutest_it ("contains points in an orthographic frustum", frustum_ortho_contains_point);
  mutest_it ("contains points in a frustum matrix", frustum_matrix_contains_point);
}

MUTEST_MAIN (
  mutest_describe ("graphene_frustum_t", frustum_suite);
)
