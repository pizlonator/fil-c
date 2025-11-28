/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <graphene.h>
#include <mutest.h>

static void
triangle_init_from_point3d (mutest_spec_t *spec)
{
  graphene_point3d_t a = GRAPHENE_POINT3D_INIT ( 0.f,  1.f, 0.f);
  graphene_point3d_t b = GRAPHENE_POINT3D_INIT ( 1.f, -1.f, 0.f);
  graphene_point3d_t c = GRAPHENE_POINT3D_INIT (-1.f, -1.f, 0.f);
  graphene_point3d_t zero = GRAPHENE_POINT3D_INIT_ZERO;
  graphene_point3d_t check_a, check_b, check_c;
  graphene_triangle_t *t;

  t = graphene_triangle_init_from_point3d (graphene_triangle_alloc (), &a, &b, &c);
  graphene_triangle_get_points (t, &check_a, &check_b, &check_c);
  mutest_expect ("triangle.a to match first point",
                 mutest_bool_value (graphene_point3d_equal (&check_a, &a)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.b to match second point",
                 mutest_bool_value (graphene_point3d_equal (&check_b, &b)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.c to match third point",
                 mutest_bool_value (graphene_point3d_equal (&check_c, &c)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a unit triangle to have an area of 2",
                 mutest_float_value (graphene_triangle_get_area (t)),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);

  graphene_triangle_init_from_point3d (t, NULL, NULL, NULL);
  graphene_triangle_get_points (t, &check_a, &check_b, &check_c);
  mutest_expect ("triangle.a to be 0 when initialized to NULL",
                 mutest_bool_value (graphene_point3d_equal (&check_a, &zero)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.b to be 0 when initialized to NULL",
                 mutest_bool_value (graphene_point3d_equal (&check_b, &zero)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.c to be 0 when initialized to NULL",
                 mutest_bool_value (graphene_point3d_equal (&check_c, &zero)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("degenerate triangle to have an area of zero",
                 mutest_float_value (graphene_triangle_get_area (t)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);

  graphene_triangle_free (t);
}

static void
triangle_init_from_vec3 (mutest_spec_t *spec)
{
  graphene_vec3_t a, b, c;
  graphene_vec3_t check_a, check_b, check_c;
  graphene_triangle_t *t;

  graphene_vec3_init (&a, 0, 1, 0);
  graphene_vec3_init (&b, 1, -1, 0);
  graphene_vec3_init (&c, -1, -1, 0);

  t = graphene_triangle_init_from_vec3 (graphene_triangle_alloc (), &a, &b, &c);
  graphene_triangle_get_vertices (t, &check_a, &check_b, &check_c);
  mutest_expect ("triangle.a to match first vector",
                 mutest_bool_value (graphene_vec3_equal (&check_a, &a)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.b to match second vector",
                 mutest_bool_value (graphene_vec3_equal (&check_b, &b)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.c to match third vector",
                 mutest_bool_value (graphene_vec3_equal (&check_c, &c)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a unit triangle to have an area of 2",
                 mutest_float_value (graphene_triangle_get_area (t)),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);

  graphene_triangle_init_from_vec3 (t, NULL, NULL, NULL);
  graphene_triangle_get_vertices (t, &check_a, &check_b, &check_c);
  mutest_expect ("triangle.a to be 0 when initialized to NULL",
                 mutest_bool_value (graphene_vec3_equal (&check_a, graphene_vec3_zero ())),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.b to be 0 when initialized to NULL",
                 mutest_bool_value (graphene_vec3_equal (&check_b, graphene_vec3_zero ())),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.c to be 0 when initialized to NULL",
                 mutest_bool_value (graphene_vec3_equal (&check_c, graphene_vec3_zero ())),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("degenerate triangle to have an area of zero",
                 mutest_float_value (graphene_triangle_get_area (t)),
                 mutest_to_be_close_to, 0.0, 0.00001,
                 NULL);

  graphene_triangle_free (t);
}

static void
triangle_init_from_float (mutest_spec_t *spec)
{
  graphene_vec3_t a, b, c;
  graphene_vec3_t check_a, check_b, check_c;
  graphene_triangle_t *t;
  float v[9] = { 0.f, 1.f, 0.f, 1.f, -1.f, 0.f, -1.f, -1.f, 0.f };

  graphene_vec3_init_from_float (&a, v);
  graphene_vec3_init_from_float (&b, v + 3);
  graphene_vec3_init_from_float (&c, v + 6);

  t = graphene_triangle_init_from_float (graphene_triangle_alloc (), v, v + 3, v + 6);
  graphene_triangle_get_vertices (t, &check_a, &check_b, &check_c);
  mutest_expect ("triangle.a to match first vector",
                 mutest_bool_value (graphene_vec3_equal (&check_a, &a)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.b to match second vector",
                 mutest_bool_value (graphene_vec3_equal (&check_b, &b)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle.c to match third vector",
                 mutest_bool_value (graphene_vec3_equal (&check_c, &c)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a unit triangle to have an area of 2",
                 mutest_float_value (graphene_triangle_get_area (t)),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);

  graphene_triangle_free (t);
}

static void
triangle_contains_point (mutest_spec_t *spec)
{
  graphene_point3d_t a = GRAPHENE_POINT3D_INIT ( 0.f,  1.f, 0.f);
  graphene_point3d_t b = GRAPHENE_POINT3D_INIT ( 1.f, -1.f, 0.f);
  graphene_point3d_t c = GRAPHENE_POINT3D_INIT (-1.f, -1.f, 0.f);
  graphene_triangle_t *t;
  graphene_point3d_t zero = GRAPHENE_POINT3D_INIT_ZERO;
  graphene_point3d_t one = GRAPHENE_POINT3D_INIT (1.f, 1.f, 1.f);
  graphene_point3d_t midpoint;

  t = graphene_triangle_init_from_point3d (graphene_triangle_alloc (), &a, &b, &c);
  graphene_triangle_get_midpoint (t, &midpoint);

  mutest_expect ("triangle contains (0, 0, 0)",
                 mutest_bool_value (graphene_triangle_contains_point (t, &zero)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("triangle does not contain (1, 1, 1)",
                 mutest_bool_value (graphene_triangle_contains_point (t, &one)),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("triangle contains its midpoint",
                 mutest_bool_value (graphene_triangle_contains_point (t, &midpoint)),
                 mutest_to_be_true,
                 NULL);

  graphene_triangle_free (t);
}

static void
triangle_plane (mutest_spec_t *spec)
{
  graphene_triangle_t t;
  graphene_plane_t p;
  graphene_point3d_t a, b, c;
  graphene_vec3_t t_norm, p_norm;

  graphene_triangle_init_from_point3d (&t, NULL, NULL, NULL);
  graphene_triangle_get_plane (&t, &p);
  graphene_triangle_get_points (&t, &a, &b, &c);

  mutest_expect ("degenerate triangle point A sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &a)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("degenerate triangle point B sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &b)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("degenerate triangle point C sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &c)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  graphene_triangle_get_normal (&t, &t_norm);
  graphene_plane_get_normal (&p, &p_norm);

  mutest_expect ("degenerate triangle normal is zero vector",
                 mutest_bool_value (graphene_vec3_equal (&t_norm, graphene_vec3_zero ())),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("degenerate triangle normal is equal to the plane normal",
                 mutest_bool_value (graphene_vec3_equal (&t_norm, &p_norm)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&a, 0.f, 0.f, 0.f);
  graphene_point3d_init (&b, 1.f, 0.f, 0.f);
  graphene_point3d_init (&c, 0.f, 1.f, 0.f);
  graphene_triangle_init_from_point3d (&t, &a, &b, &c);
  graphene_triangle_get_plane (&t, &p);

  mutest_expect ("unit triangle point A sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &a)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("unit triangle point B sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &b)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("unit triangle point C sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &c)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  graphene_triangle_get_normal (&t, &t_norm);
  graphene_plane_get_normal (&p, &p_norm);
  mutest_expect ("unit triangle normal is equal to the plane normal",
                 mutest_bool_value (graphene_vec3_near (&t_norm, &p_norm, 0.0001f)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&a, 2.f, 0.f, 0.f);
  graphene_point3d_init (&b, 0.f, 0.f, 0.f);
  graphene_point3d_init (&c, 0.f, 0.f, 2.f);
  graphene_triangle_init_from_point3d (&t, &a, &b, &c);
  graphene_triangle_get_plane (&t, &p);

  mutest_expect ("triangle point A sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &a)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("triangle point B sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &b)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("triangle point C sits on the triangle's plane",
                 mutest_float_value (graphene_plane_distance (&p, &c)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  graphene_triangle_get_normal (&t, &t_norm);
  graphene_vec3_normalize (&t_norm, &t_norm);
  graphene_plane_get_normal (&p, &p_norm);
  mutest_expect ("triangle normal is equal to the plane normal",
                 mutest_bool_value (graphene_vec3_near (&t_norm, &p_norm, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
triangle_barycoords (mutest_spec_t *spec)
{
  graphene_triangle_t t;
  graphene_point3d_t a, b, c, p;
  graphene_vec2_t barycoords;
  graphene_vec2_t check;

  graphene_point3d_init (&a, 0.f, 0.f, 0.f);
  graphene_point3d_init (&b, 1.f, 0.f, 0.f);
  graphene_point3d_init (&c, 1.f, 1.f, 0.f);
  graphene_triangle_init_from_point3d (&t, &a, &b, &c);

  struct {
    const char *description;
    graphene_point3d_t point;
    float uv[2];
  } border_points[] = {
    { "unit triangle point A",
      GRAPHENE_POINT3D_INIT (0.f, 0.f, 0.f),
      { 0.f, 0.f },
    },
    { "unit triangle point B",
      GRAPHENE_POINT3D_INIT (1.f, 0.f, 0.f),
      { 0.f, 1.f },
    },
    { "unit triangle point C",
      GRAPHENE_POINT3D_INIT (1.f, 1.f, 0.f),
      { 1.f, 0.f },
    },
  };

  /* Border */
  for (int i = 0; i < (sizeof (border_points) / sizeof (border_points[0])); i++)
    {
      char desc[128];

      snprintf (desc, 128, "barycoords for %s to exist", border_points[i].description);

      mutest_expect (desc,
                     mutest_bool_value (graphene_triangle_get_barycoords (&t, &(border_points[i].point), &barycoords)),
                     mutest_to_be_true,
                     NULL);

      snprintf (desc, 128, "barycoords for %s to be (%g, %g)",
                border_points[i].description,
                border_points[i].uv[0],
                border_points[i].uv[1]);
      graphene_vec2_init_from_float (&check, border_points[i].uv);
      mutest_expect (desc,
                     mutest_bool_value (graphene_vec2_near (&barycoords, &check, 0.0001f)),
                     mutest_to_be_true,
                     NULL);
    }

  /* Inside */
  struct {
    graphene_point3d_t point;
    float uv[2];
  } inside_points[] = {
    { GRAPHENE_POINT3D_INIT (0.5f, 0.0f, 0.0f), { 0.0f, 0.5f } },
    { GRAPHENE_POINT3D_INIT (0.5f, 0.5f, 0.0f), { 0.5f, 0.0f } },
    { GRAPHENE_POINT3D_INIT (1.0f, 0.5f, 0.0f), { 0.5f, 0.5f } },
  };

  for (int i = 0; i < (sizeof (inside_points) / sizeof (inside_points[0])); i++)
    {
      char desc[128];

      snprintf (desc, 128, "barycoords for inside point (%g, %g, %g) to exists",
                inside_points[i].point.x,
                inside_points[i].point.y,
                inside_points[i].point.z);
      mutest_expect (desc,
                     mutest_bool_value (graphene_triangle_get_barycoords (&t, &(inside_points[i].point), &barycoords)),
                     mutest_to_be_true,
                     NULL);

      snprintf (desc, 128, "barycoords for inside point (%g, %g, %g) to be (%g, %g)",
                inside_points[i].point.x,
                inside_points[i].point.y,
                inside_points[i].point.z,
                inside_points[i].uv[0],
                inside_points[i].uv[1]);
      graphene_vec2_init_from_float (&check, inside_points[i].uv);
      mutest_expect (desc,
                     mutest_bool_value (graphene_vec2_near (&barycoords, &check, 0.0001f)),
                     mutest_to_be_true,
                     NULL);
    }

  /* Outside the triangle, same plane */
  struct {
    graphene_point3d_t point;
    float uv[2];
  } outside_points[] = {
    { GRAPHENE_POINT3D_INIT (2.0f, 1.0f, 0.0f), { 1.0f, 1.0f } },
    { GRAPHENE_POINT3D_INIT (-1.0f, 0.0f, 0.0f), { 0.0f, -1.0f } },
  };

  for (int i = 0; i < (sizeof (outside_points) / sizeof (outside_points[0])); i++)
    {
      char desc[128];

      snprintf (desc, 128, "barycoords for outside point (%g, %g, %g) to exists",
                outside_points[i].point.x,
                outside_points[i].point.y,
                outside_points[i].point.z);
      mutest_expect (desc,
                     mutest_bool_value (graphene_triangle_get_barycoords (&t, &(outside_points[i].point), &barycoords)),
                     mutest_to_be_true,
                     NULL);

      snprintf (desc, 128, "barycoords for outside point (%g, %g, %g) to be (%g, %g)",
                outside_points[i].point.x,
                outside_points[i].point.y,
                outside_points[i].point.z,
                outside_points[i].uv[0],
                outside_points[i].uv[1]);
      graphene_vec2_init_from_float (&check, outside_points[i].uv);
      mutest_expect (desc,
                     mutest_bool_value (graphene_vec2_near (&barycoords, &check, 0.0001f)),
                     mutest_to_be_true,
                     NULL);
    }

  /* Outside the triangle plane */
  /* FIXME is that normal? */
  graphene_point3d_init (&p, 0.f, 0.f, 1.f);
  mutest_expect ("barycoords for point outside triangle plane to exist",
                 mutest_bool_value (graphene_triangle_get_barycoords (&t, &p, &barycoords)),
                 mutest_to_be_true,
                 NULL);
  graphene_vec2_init (&check, 0.f, 0.f);
  mutest_expect ("barycoords for point outside triangle plane to be (0, 0)",
                 mutest_bool_value (graphene_vec2_near (&barycoords, &check, 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
triangle_area (mutest_spec_t *spec)
{
  graphene_triangle_t t;
  graphene_point3d_t a, b, c;

  /* Counterclockwise */
  graphene_point3d_init (&a, 0.f, 0.f, 0.f);
  graphene_point3d_init (&b, 1.f, 0.f, 0.f);
  graphene_point3d_init (&c, 1.f, 1.f, 0.f);

  graphene_triangle_init_from_point3d (&t, &a, &b, &c);
  mutest_expect ("area of unit triangle constructed counterclockwise to be 0.5",
                 mutest_float_value (graphene_triangle_get_area (&t)),
                 mutest_to_be_close_to, 0.5, 0.0001,
                 NULL);

  /* Clockwise (positive too) */
  graphene_triangle_init_from_point3d (&t, &a, &c, &b);
  mutest_expect ("area of unit triangle constructed clockwise to be 0.5",
                 mutest_float_value (graphene_triangle_get_area (&t)),
                 mutest_to_be_close_to, 0.5, 0.0001,
                 NULL);
}

static void
triangle_suite (mutest_suite_t *suite)
{
  mutest_it ("initializes from points", triangle_init_from_point3d);
  mutest_it ("initializes from vectors", triangle_init_from_vec3);
  mutest_it ("initialized from float arrays", triangle_init_from_float);
  mutest_it ("contains points", triangle_contains_point);
  mutest_it ("defines planes", triangle_plane);
  mutest_it ("defines barycoords", triangle_barycoords);
  mutest_it ("defines areas", triangle_area);
}

MUTEST_MAIN (
  mutest_describe ("graphene_triangle_t", triangle_suite);
)
