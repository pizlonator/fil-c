/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
quad_bounds (mutest_spec_t *spec)
{
  graphene_quad_t p;
  graphene_rect_t r, s;
  graphene_point_t p0, p1, p2, p3;

  graphene_rect_init (&r, 0.f, 0.f, 10.f, 10.f);
  graphene_rect_get_top_left (&r, &p0);
  graphene_rect_get_top_right (&r, &p1);
  graphene_rect_get_bottom_right (&r, &p2);
  graphene_rect_get_bottom_left (&r, &p3);

  graphene_quad_init (&p, &p0, &p1, &p2, &p3);
  graphene_quad_bounds (&p, &s);
  mutest_expect ("converting rect corners into a quad yields the same bounds",
                 mutest_bool_value (graphene_rect_contains_rect (&s, &r)),
                 mutest_to_be_true,
                 NULL);

  graphene_quad_init_from_rect (&p, &r);
  graphene_quad_bounds (&p, &s);
  mutest_expect ("converting a rect into a quad yields the same bounds",
                 mutest_bool_value (graphene_rect_contains_rect (&s, &r)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_get_top_left (&r, &p0);
  mutest_expect ("point[0] to be top left",
                  mutest_bool_value (graphene_point_equal (graphene_quad_get_point (&p, 0), &p0)),
                  mutest_to_be_true,
                  NULL);

  graphene_rect_get_top_right (&r, &p1);
  mutest_expect ("point[1] to be top right",
                  mutest_bool_value (graphene_point_equal (graphene_quad_get_point (&p, 1), &p1)),
                  mutest_to_be_true,
                  NULL);

  graphene_rect_get_bottom_right (&r, &p2);
  mutest_expect ("point[2] to be bottom right",
                  mutest_bool_value (graphene_point_equal (graphene_quad_get_point (&p, 2), &p2)),
                  mutest_to_be_true,
                  NULL);

  graphene_rect_get_bottom_left (&r, &p3);
  mutest_expect ("point[3] to be bottom left",
                  mutest_bool_value (graphene_point_equal (graphene_quad_get_point (&p, 3), &p3)),
                  mutest_to_be_true,
                  NULL);
}

static void
quad_contains (mutest_spec_t *spec)
{
  graphene_point_t p[4] = {
    GRAPHENE_POINT_INIT ( 0.f,  0.f),
    GRAPHENE_POINT_INIT (10.f,  1.f),
    GRAPHENE_POINT_INIT (10.f,  9.f),
    GRAPHENE_POINT_INIT ( 0.f, 10.f),
  };
  graphene_quad_t *q;

  q = graphene_quad_init_from_points (graphene_quad_alloc (), p);

  mutest_expect ("quad to contain p[0]",
                 mutest_bool_value (graphene_quad_contains (q, &p[0])),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("quad to contain p[1]",
                 mutest_bool_value (graphene_quad_contains (q, &p[1])),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("quad to contain p[2]",
                 mutest_bool_value (graphene_quad_contains (q, &p[2])),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("quad to contain p[3]",
                 mutest_bool_value (graphene_quad_contains (q, &p[3])),
                 mutest_to_be_true,
                 NULL);

  mutest_expect ("quad to contain (0.5, 0.5)",
                 mutest_bool_value (graphene_quad_contains (q, &GRAPHENE_POINT_INIT (0.5f, 0.5f))),
                 mutest_to_be_true,
                 NULL);

  mutest_expect ("quad to not contain (10, 10)",
                 mutest_bool_value (graphene_quad_contains (q, &GRAPHENE_POINT_INIT (10.f, 10.f))),
                 mutest_to_be_false,
                 NULL);

  graphene_quad_free (q);
}

static void
quad_suite (mutest_suite_t *suite)
{
  mutest_it ("has bounds", quad_bounds);
  mutest_it ("can contain points", quad_contains);
}

MUTEST_MAIN (
  mutest_describe ("graphene_quad_t", quad_suite);
)
