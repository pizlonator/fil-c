/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>
#include <math.h>

static void
ray_init (void)
{
  graphene_ray_t *r;
  graphene_point3d_t origin, zero = GRAPHENE_POINT3D_INIT_ZERO;
  graphene_vec3_t direction;

  r = graphene_ray_init (graphene_ray_alloc (), NULL, NULL);
  graphene_ray_get_origin (r, &origin);
  graphene_ray_get_direction (r, &direction);

  mutest_expect ("initializing origin to null sets it to (0, 0, 0)",
                 mutest_bool_value (graphene_point3d_equal (&origin, &zero)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("initializing direction to null uses a zero vector",
                 mutest_bool_value (graphene_vec3_equal (&direction, graphene_vec3_zero ())),
                 mutest_to_be_true,
                 NULL);

  graphene_ray_free (r);
}

static void
ray_get_position_at (void)
{
  graphene_point3d_t one = GRAPHENE_POINT3D_INIT (1.f, 1.f, 1.f);
  graphene_point3d_t tmp;
  graphene_ray_t r;

  graphene_ray_init (&r, &one, graphene_vec3_z_axis ());

  graphene_ray_get_position_at (&r, 0.f, &tmp);
  mutest_expect ("querying a distance of 0 to return the origin",
                 mutest_bool_value (graphene_point3d_equal (&tmp, &one)),
                 mutest_to_be_true,
                 NULL);

  graphene_ray_get_position_at (&r, -1.f, &tmp);
  mutest_expect ("querying a negative distance to return a point behind the origin",
                 mutest_bool_value (graphene_point3d_near (&tmp, &GRAPHENE_POINT3D_INIT (1.f, 1.f, 0.f), 0.0001f)),
                 mutest_to_be_true,
                 NULL);

  graphene_ray_get_position_at (&r, 1.f, &tmp);
  mutest_expect ("querying a positive distance to return a point on the ray",
                 mutest_bool_value (graphene_point3d_near (&tmp, &GRAPHENE_POINT3D_INIT (1.f, 1.f, 2.f), 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
ray_get_distance_to_point (void)
{
  graphene_ray_t r;

  graphene_ray_init (&r, &GRAPHENE_POINT3D_INIT (1, 1, 1), graphene_vec3_z_axis ());

  mutest_expect ("closest approach from (0, 0, 0) to be sqrt(3)",
                 mutest_float_value (graphene_ray_get_distance_to_point (&r, &GRAPHENE_POINT3D_INIT (0, 0, 0))),
                 mutest_to_be_close_to, sqrtf (3.f), 0.0001,
                 NULL);

  mutest_expect ("closest approach on the origin of the ray to be zero",
                 mutest_float_value (graphene_ray_get_distance_to_point (&r, &GRAPHENE_POINT3D_INIT (1, 1, 1))),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  mutest_expect ("closest approach from (0, 0, 50) to be sqrt(2)",
                 mutest_float_value (graphene_ray_get_distance_to_point (&r, &GRAPHENE_POINT3D_INIT (0, 0, 50))),
                 mutest_to_be_close_to, sqrtf (2.f), 0.0001,
                 NULL);
}

static void
ray_closest_point_to_point (void)
{
  graphene_point3d_t res;
  graphene_ray_t r;

  graphene_ray_init (&r, &GRAPHENE_POINT3D_INIT (1, 1, 1), graphene_vec3_z_axis ());

  graphene_ray_get_closest_point_to_point (&r, &GRAPHENE_POINT3D_INIT (0, 0, 0), &res);
  mutest_expect ("closest point behind the origin is the origin",
                 mutest_bool_value (graphene_point3d_near (&res, &GRAPHENE_POINT3D_INIT (1, 1, 1), 0.00001f)),
                 mutest_to_be_true,
                 NULL);

  graphene_ray_get_closest_point_to_point (&r, &GRAPHENE_POINT3D_INIT (0, 0, 50), &res);
  mutest_expect ("closest point in front of the ray to be on the ray",
                 mutest_bool_value (graphene_point3d_near (&res, &GRAPHENE_POINT3D_INIT (1, 1, 50), 0.00001f)),
                 mutest_to_be_true,
                 NULL);

  graphene_ray_get_closest_point_to_point (&r, &GRAPHENE_POINT3D_INIT (1, 1, 1), &res);
  mutest_expect ("closest point on the ray to be the same point",
                 mutest_bool_value (graphene_point3d_near (&res, &GRAPHENE_POINT3D_INIT (1, 1, 1), 0.00001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
ray_matrix_transform (void)
{
  graphene_ray_t r, res;
  graphene_matrix_t m;

  graphene_ray_init (&r, &GRAPHENE_POINT3D_INIT (1, 1, 1), graphene_vec3_z_axis ());
  graphene_matrix_init_identity (&m);
  graphene_matrix_transform_ray (&m, &r, &res);
  mutest_expect ("transforming a ray with an identity matrix does not change the ray",
                 mutest_bool_value (graphene_ray_equal (&r, &res)),
                 mutest_to_be_true,
                 NULL);

  graphene_ray_init (&r, &GRAPHENE_POINT3D_INIT (0, 0, 0), graphene_vec3_z_axis ());
  graphene_matrix_init_rotate (&m, 90, graphene_vec3_z_axis ());
  graphene_matrix_transform_ray (&m, &r, &res);
  mutest_expect ("transforming a ray on the Z axis with a rotation on the Z axis does not change the ray",
                 mutest_bool_value (graphene_ray_equal (&r, &res)),
                 mutest_to_be_true,
                 NULL);
}

static void
ray_intersect_triangle (void)
{
  graphene_vec3_t one3;
  graphene_vec3_init (&one3, 1, 1, 1);

  graphene_ray_t r;

  graphene_triangle_t t;
  graphene_triangle_init_from_point3d (&t,
                                       &GRAPHENE_POINT3D_INIT (1, 1, 0),
                                       &GRAPHENE_POINT3D_INIT (0, 1, 1),
                                       &GRAPHENE_POINT3D_INIT (1, 0, 1));

  graphene_ray_init (&r, NULL, graphene_vec3_zero ());
  mutest_expect ("no intersection if ray's direction is zero",
                 mutest_int_value (graphene_ray_intersect_triangle (&r, &t, NULL)),
                 mutest_to_be, GRAPHENE_RAY_INTERSECTION_KIND_NONE,
                 NULL);

  graphene_ray_init (&r, NULL, &one3);
  mutest_expect ("intersection with backside faces",
                 mutest_int_value (graphene_ray_intersect_triangle (&r, &t, NULL)),
                 mutest_to_be, GRAPHENE_RAY_INTERSECTION_KIND_LEAVE,
                 NULL);

  float d;
  graphene_ray_init (&r, NULL, &one3);
  mutest_expect ("intersection without backside faces",
                 mutest_int_value (graphene_ray_intersect_triangle (&r, &t, &d)),
                 mutest_not, mutest_to_be, GRAPHENE_RAY_INTERSECTION_KIND_NONE,
                 NULL);
  mutest_expect ("intersection distance to be valid",
                 mutest_float_value (d),
                 mutest_to_be_close_to, d - 1 / 3, 0.001,
                 NULL);
}

static void
ray_intersects_box (void)
{
  graphene_point3d_t min;
  graphene_point3d_t max;
  graphene_point3d_t origin;
  graphene_vec3_t direction;
  graphene_box_t box;
  graphene_ray_t ray;

  /* Off center box */

  graphene_point3d_init (&min, 41.843132f, 27.356903f, -50.368336f);
  graphene_point3d_init (&max, 51.698078f, 29.080172f, -50.368336f);
  graphene_box_init (&box, &min, &max);

  /* Ray from (0, 0, 0) along an axis *NOT* hitting the above box
   */

  graphene_point3d_init (&origin, 0, 0, 0);
  graphene_vec3_init (&direction, 0, 0.495176f, -0.868793f);
  graphene_ray_init (&ray, &origin, &direction);

  mutest_expect ("intersection kind should be NONE",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &box)),
                 mutest_to_be_false,
                 NULL);

  /* Nudged variant of the above ray */

  graphene_vec3_init (&direction, 0 + 0.0001f, 0.495176f, -0.868793f);
  graphene_ray_init (&ray, &origin, &direction);

  mutest_expect ("intersection kind should still be NONE",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &box)),
                 mutest_to_be_false,
                 NULL);

  /* Centered box */

  graphene_point3d_init (&min, -5.654480f, 27.356903f, -50.368336f);
  graphene_point3d_init (&max, 5.654475f, 29.080172f, -50.368336f);
  graphene_box_init (&box, &min, &max);

  /* Ray from (0, 0, 0) along the axis hitting the above box */

  graphene_point3d_init (&origin, 0, 0, 0);
  graphene_vec3_init (&direction, 0, 0.495176f, -0.868793f);
  graphene_ray_init (&ray, &origin, &direction);

  mutest_expect ("intersection kind should be ENTER",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &box)),
                 mutest_to_be_true,
                 NULL);

  /* Nudged variant of the above ray */

  graphene_vec3_init (&direction, 2 * FLT_EPSILON, 0.495176f, -0.868793f);
  graphene_ray_init (&ray, &origin, &direction);

  mutest_expect ("intersection kind should still be ENTER",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &box)),
                 mutest_to_be_true,
                 NULL);
}

/* Adapted from Mutter/Clutter */
static void
setup_ray_for_coordinates (graphene_matrix_t  *view,
                           float               x,
                           float               y,
                           graphene_ray_t     *ray)
{
  graphene_point3d_t camera_position;
  graphene_point3d_t p;
  graphene_vec3_t direction;
  graphene_vec3_t cv;
  graphene_vec3_t v;

  camera_position = GRAPHENE_POINT3D_INIT_ZERO;
  graphene_vec3_init (&cv,
                      camera_position.x,
                      camera_position.y,
                      camera_position.z);

  p = GRAPHENE_POINT3D_INIT (x, y, 0.f);
  graphene_matrix_transform_point3d (view, &p, &p);

  graphene_vec3_init (&v, p.x, p.y, p.z);
  graphene_vec3_subtract (&v, &cv, &direction);
  graphene_vec3_normalize (&direction, &direction);

  graphene_ray_init (ray, &camera_position, &direction);
}

static void
ray_picking (void)
{
  graphene_matrix_t view;
  graphene_point3d_t box_min;
  graphene_point3d_t box_max;
  graphene_box_t box;
  graphene_box_t transformed_box;
  graphene_ray_t ray;
  float x;
  float y;
  const float view_array[] = { 0.053852f, 0, 0, 0,
                               0, 0.053852f, 0, 0,
                               0, 0.053852f, 0, 0,
                               -51.698082f, 29.080172f, -50.368336f, 1 };

  graphene_matrix_init_from_float (&view, view_array);

  graphene_point3d_init (&box_min, 700, 400, 0);
  graphene_point3d_init (&box_max, 1200, 800, 0);
  graphene_box_init (&box, &box_min, &box_max);
  graphene_matrix_transform_box (&view, &box, &transformed_box);

  /* Left of the box */

  x = box_min.x - 50.0f;
  y = box_min.y + 50.0f;
  setup_ray_for_coordinates (&view, x, y, &ray);
  mutest_expect ("picking left of the box should not be in the box",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &transformed_box)),
                 mutest_to_be_false,
                 NULL);

  /* Right of the box */

  x = box_max.x + 50.0f;
  y = box_min.y + 50.0f;
  setup_ray_for_coordinates (&view, x, y, &ray);
  mutest_expect ("picking right of the box should not be in the box",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &transformed_box)),
                 mutest_to_be_false,
                 NULL);


  /* Above the box */

  x = box_min.x + 50.0f;
  y = box_min.y - 50.0f;
  setup_ray_for_coordinates (&view, x, y, &ray);
  mutest_expect ("picking above the box should not be in the box",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &transformed_box)),
                 mutest_to_be_false,
                 NULL);

  /* Below the box */

  x = box_min.x + 50.0f;
  y = box_max.y + 50.0f;
  setup_ray_for_coordinates (&view, x, y, &ray);
  mutest_expect ("picking below the box should not be in the box",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &transformed_box)),
                 mutest_to_be_false,
                 NULL);

  /* In the box */

  x = box_min.x + 50.0f;
  y = box_min.y + 50.0f;
  setup_ray_for_coordinates (&view, x, y, &ray);
  mutest_expect ("picking in the box should be a hit",
                 mutest_int_value (graphene_ray_intersects_box (&ray, &transformed_box)),
                 mutest_to_be_true,
                 NULL);
}

static void
ray_suite (void)
{
  mutest_it ("can be initialized", ray_init);
  mutest_it ("can compute the position of a point on the ray", ray_get_position_at);
  mutest_it ("can compute the closest approach of a point from the ray", ray_get_distance_to_point);
  mutest_it ("can compute the closest point to a point on the ray", ray_closest_point_to_point);
  mutest_it ("can be transformed", ray_matrix_transform);
  mutest_it ("can intersect triangles", ray_intersect_triangle);
  mutest_it ("can intersect on axis", ray_intersects_box);
  mutest_it ("can be used for picking", ray_picking);
}

MUTEST_MAIN (
  mutest_describe ("graphene_ray_t", ray_suite);
)
