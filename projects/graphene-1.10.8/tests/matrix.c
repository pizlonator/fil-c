/* SPDX-License-Identifier: MIT */

#include <math.h>
#include <string.h>
#include <graphene.h>
#include <mutest.h>

/* Custom matcher for near matrices */
static bool
graphene_test_matrix_near (mutest_expect_t *e,
                           mutest_expect_res_t *res)
{
  const graphene_matrix_t *m = mutest_get_pointer (mutest_expect_value (e));
  const graphene_matrix_t *check = mutest_get_pointer (res);

  for (unsigned i = 0; i < 4; i++)
    {
      graphene_vec4_t m_row, check_row;

      graphene_matrix_get_row (m, i, &m_row);
      graphene_matrix_get_row (check, i, &check_row);

      if (!graphene_vec4_near (&m_row, &check_row, 0.1f))
        return false;
    }

  return true;
}

static void
matrix_identity (void)
{
  const float identity[16] = {
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f,
  };
  graphene_matrix_t m;
  float v[16];

  graphene_matrix_init_identity (&m);
  mutest_expect ("init_identity() to set an identity matrix",
                 mutest_bool_value (graphene_matrix_is_identity (&m)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_init_from_float (&m, identity);
  mutest_expect ("init_from_float() with an identity matrix to preserve the identity",
                 mutest_bool_value (graphene_matrix_is_identity (&m)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_init_from_vec4 (&m,
                                  graphene_vec4_x_axis (),
                                  graphene_vec4_y_axis (),
                                  graphene_vec4_z_axis (),
                                  graphene_vec4_w_axis ());
  mutest_expect ("init_from_vec4() with an identity matrix to preserve the identity",
                 mutest_bool_value (graphene_matrix_is_identity (&m)),
                 mutest_to_be_true,
                 NULL);

  mutest_expect ("m.xx to be 1 on an identity matrix",
                 mutest_float_value (graphene_matrix_get_value (&m, 0, 0)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("m.yy to be 1 on an identity matrix",
                 mutest_float_value (graphene_matrix_get_value (&m, 1, 1)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("m.zz to be 1 on an identity matrix",
                 mutest_float_value (graphene_matrix_get_value (&m, 2, 2)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);
  mutest_expect ("m.ww to be 1 on an identity matrix",
                 mutest_float_value (graphene_matrix_get_value (&m, 3, 3)),
                 mutest_to_be_close_to, 1.0, 0.00001,
                 NULL);

  graphene_matrix_to_float (&m, v);
  mutest_expect ("identity matrix to be all zeros except on the diagonal",
                 mutest_bool_value (memcmp (v, identity, sizeof (float) * 16) == 0),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_scale (&m, 2.0f, 2.0f, 2.0f);
  mutest_expect ("transforming an identity matrix stops it from being an identity",
                 mutest_bool_value (graphene_matrix_is_identity (&m)),
                 mutest_to_be_false,
                 NULL);
}

static void
matrix_equal (void)
{
  graphene_matrix_t m1, m2;

  graphene_matrix_init_identity (&m1);
  graphene_matrix_init_identity (&m2);

  mutest_expect ("a matrix to be equal to itself",
                 mutest_bool_value (graphene_matrix_equal (&m1, &m1)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a matrix not to be equal to NULL",
                 mutest_bool_value (graphene_matrix_equal (&m1, NULL)),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("NULL not to be equal to a matrix",
                 mutest_bool_value (graphene_matrix_equal (NULL, &m1)),
                 mutest_to_be_false,
                 NULL);

  mutest_expect ("two identity matrices to be equal (fast version)",
                 mutest_bool_value (graphene_matrix_equal_fast (&m1, &m2)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("two identity matrices to be equal (full version)",
                 mutest_bool_value (graphene_matrix_equal (&m1, &m2)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_scale (&m1, 0.002f, 0.002f, 0.002f);
  graphene_matrix_scale (&m2, 0.001f, 0.001f, 0.001f);
  mutest_expect ("scale(0.001) not to be equal to scale(0.002)",
                 mutest_bool_value (graphene_matrix_equal (&m1, &m2)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("scale(0.001) to be within 0.01 of scale(0.002)",
                 mutest_bool_value (graphene_matrix_near (&m1, &m2, 0.01f)),
                 mutest_to_be_true,
                 NULL);
}

static void
matrix_rotation (void)
{
  graphene_matrix_t m;
  graphene_matrix_t m2;

  graphene_matrix_init_identity (&m);
  graphene_matrix_rotate (&m, 45.0f, graphene_vec3_x_axis ());
  graphene_matrix_init_rotate (&m2, 45.0f, graphene_vec3_x_axis ());
  mutest_expect ("rotating an identity and initializing a rotation to result in the same matrix",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);

  graphene_matrix_rotate (&m, 15.0f, graphene_vec3_x_axis ());
  graphene_matrix_init_rotate (&m2, 60.0f, graphene_vec3_x_axis ());
  mutest_expect ("rotations to be cumulative",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);
}

static void
matrix_rotation_euler_quaternion (void)
{
  graphene_matrix_t m0, m1, m2;
  graphene_quaternion_t q;
  graphene_euler_t e;

  graphene_matrix_init_rotate (&m0, 60.f, graphene_vec3_x_axis ());

  graphene_euler_init (&e, 60.f, 0.f, 0.f);
  graphene_quaternion_init_from_euler (&q, &e);

  graphene_matrix_init_identity (&m1);
  graphene_matrix_rotate_euler (&m1, &e);
  mutest_expect ("rotating with an euler angle to be equal to angle/axis",
                 mutest_pointer (&m0),
                 graphene_test_matrix_near, mutest_pointer (&m1),
                 NULL);

  graphene_matrix_init_identity (&m2);
  graphene_matrix_rotate_quaternion (&m2, &q);
  mutest_expect ("rotating with a quaternion to be equal to angle/axis",
                 mutest_pointer (&m0),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);
}

static void
matrix_translation (void)
{
  graphene_matrix_t m;
  graphene_matrix_t m2;

  graphene_point3d_t trans = GRAPHENE_POINT3D_INIT (1.0f, 2.0f, 3.0f);

  graphene_matrix_init_identity (&m);

  graphene_matrix_translate (&m, &trans);
  graphene_matrix_init_translate (&m2, &trans);

  mutest_expect ("translating an identity and initializing a translation to result in the same matrix",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);
}

static void
matrix_scale (void)
{
  graphene_matrix_t m;
  graphene_matrix_t m2;

  graphene_matrix_init_identity (&m);

  graphene_matrix_scale (&m, 1.0f, 2.0f, 3.0f);
  graphene_matrix_init_scale (&m2, 1.0f, 2.0f, 3.0f);

  mutest_expect ("scaling an identity and initializing a scale to result in the same matrix",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);

  mutest_expect ("scale factor on the X axis to be the same",
                 mutest_float_value (graphene_matrix_get_x_scale (&m)),
                 mutest_to_be_close_to, graphene_matrix_get_x_scale (&m2), 0.001,
                 NULL);
  mutest_expect ("scale factor on the Y axis to be the same",
                 mutest_float_value (graphene_matrix_get_y_scale (&m)),
                 mutest_to_be_close_to, graphene_matrix_get_y_scale (&m2), 0.001,
                 NULL);
  mutest_expect ("scale factor on the Z axis to be the same",
                 mutest_float_value (graphene_matrix_get_z_scale (&m)),
                 mutest_to_be_close_to, graphene_matrix_get_z_scale (&m2), 0.001,
                 NULL);
}

static void
matrix_invert (void)
{
  graphene_matrix_t m;
  graphene_matrix_t identity;
  graphene_matrix_t inv;
  graphene_matrix_t res;
  graphene_point3d_t p;

  graphene_matrix_init_identity (&identity);

  graphene_matrix_init_identity (&m);
  
  graphene_matrix_inverse (&m , &inv);
  graphene_matrix_multiply (&m, &inv, &res);
  mutest_expect ("inverting an identity to return an identity",
                 mutest_pointer (&res),
                 graphene_test_matrix_near, mutest_pointer (&identity),
                 NULL);
  
  graphene_matrix_scale (&m, 1.0f, 2.0f, 3.0f);
  graphene_matrix_inverse (&m , &inv);
  graphene_matrix_multiply (&m, &inv, &res);
  mutest_expect ("inverting a scale to return an identity",
                 mutest_pointer (&res),
                 graphene_test_matrix_near, mutest_pointer (&identity),
                 NULL);

  graphene_matrix_rotate (&m, 44, graphene_vec3_x_axis ());
  graphene_matrix_inverse (&m , &inv);
  graphene_matrix_multiply (&m, &inv, &res);
  mutest_expect ("inverting a rotation to return an identity",
                 mutest_pointer (&res),
                 graphene_test_matrix_near, mutest_pointer (&identity),
                 NULL);

  graphene_matrix_translate (&m, graphene_point3d_init (&p, 1.f, 2.f, 3.f));
  graphene_matrix_inverse (&m , &inv);
  graphene_matrix_multiply (&m, &inv, &res);
  mutest_expect ("inverting a translation to return an identity",
                 mutest_pointer (&res),
                 graphene_test_matrix_near, mutest_pointer (&identity),
                 NULL);
}

static void
matrix_neutral_element (void)
{
  graphene_matrix_t identity;

  graphene_point3d_t null_vector = GRAPHENE_POINT3D_INIT (0, 0, 0);

  graphene_matrix_init_identity (&identity);

  graphene_matrix_scale (&identity, 1.0f, 1.0f, 1.0f);
  mutest_expect ("scale factors of 1 to be neutral",
                 mutest_bool_value (graphene_matrix_is_identity (&identity)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_rotate (&identity, 0.0f, graphene_vec3_z_axis ());
  mutest_expect ("rotation angle of 0 to be neutral",
                 mutest_bool_value (graphene_matrix_is_identity (&identity)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_translate (&identity, &null_vector);
  mutest_expect ("translation by the null vector to be neutral",
                 mutest_bool_value (graphene_matrix_is_identity (&identity)),
                 mutest_to_be_true,
                 NULL);
}

static void
matrix_look_at (void)
{
  graphene_matrix_t m;
  graphene_vec3_t neg_z_axis;
  graphene_vec3_t dir_xz, dir_yz, dir_xyz;
#if 0
  graphene_vec3_t eye, center, up, dist, center_plus_up;
  graphene_vec4_t res;
#endif
  graphene_matrix_t identity, translated, rotated;

  graphene_matrix_init_identity (&identity);
  graphene_vec3_init (&neg_z_axis, 0, 0, -1);
  graphene_vec3_init (&dir_xz, 1, 0, -1);
  graphene_vec3_init (&dir_yz, 0, 1, -1);
  graphene_vec3_init (&dir_xyz, 1, 1, -1);

  graphene_matrix_init_look_at (&m,
                                graphene_vec3_zero (),
                                &neg_z_axis,
                                graphene_vec3_y_axis());
  mutest_expect ("eye: (0, 0, 0), center: (0, 0, -1), up: (0, 1, 0) to be an identity",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&identity),
                 NULL);

  graphene_matrix_init_look_at (&m,
                                graphene_vec3_z_axis(),
                                graphene_vec3_zero (),
                                graphene_vec3_y_axis());

  translated = identity;
  graphene_matrix_translate (&translated, &GRAPHENE_POINT3D_INIT (0, 0, -1));
  mutest_expect ("eye: (0, 0, 1), center: (0, 0, 0), up: (0, 1, 0) to be a translation",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&translated),
                 NULL);

  graphene_matrix_init_look_at (&m,
                                graphene_vec3_zero (),
                                &dir_xz,
                                graphene_vec3_y_axis());
  graphene_matrix_init_rotate (&rotated, -45, graphene_vec3_y_axis());
  mutest_expect ("eye: (0, 0, 0), center: (1, 0, -1), up: (0, 1, 0) to be a rotation",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&rotated),
                 NULL);

  graphene_matrix_init_look_at (&m,
                                graphene_vec3_zero (),
                                &dir_yz,
                                graphene_vec3_y_axis());
  graphene_matrix_init_rotate (&rotated, 45, graphene_vec3_x_axis());
  mutest_expect ("eye: (0, 0, 0), center: (0, 1, -1), up: (0, 1, 0) to be a rotation",
                 mutest_pointer (&m),
                 graphene_test_matrix_near, mutest_pointer (&rotated),
                 NULL);

#if 0
  graphene_vec3_init (&eye,
                      g_random_double_range (-10000, 10000),
                      g_random_double_range (-10000, 10000),
                      g_random_double_range (-10000, 10000));

  graphene_vec3_init (&center,
                      g_random_double_range (-10000, 10000),
                      g_random_double_range (-10000, 10000),
                      g_random_double_range (-10000, 10000));

  graphene_vec3_init (&up,
                      g_random_double_range (-1, 1),
                      g_random_double_range (-1, 1),
                      g_random_double_range (-1, 1));
  graphene_vec3_normalize (&up, &up);

  if (g_test_verbose ())
    g_print ("look at: eye: (%+5.3f, %+5.3f, %+5.3f), "
                   "center: (+%5.3f, %+5.3f, %+5.3f), "
                       "up: (%+.3f, %+.3f, %+.3f)\n",
             graphene_vec3_get_x (&eye), graphene_vec3_get_y (&eye), graphene_vec3_get_z (&eye),
             graphene_vec3_get_x (&center), graphene_vec3_get_y (&center), graphene_vec3_get_z (&center),
             graphene_vec3_get_x (&up), graphene_vec3_get_y (&up), graphene_vec3_get_z (&up));
  graphene_matrix_init_look_at (&m, &eye, &center, &up);
  graphene_vec4_init_from_vec3 (&res, &center, 1);
  graphene_matrix_transform_vec4 (&m, &res, &res);

  graphene_vec3_subtract (&center, &eye, &dist);

  graphene_assert_fuzzy_equals (graphene_vec4_get_x (&res), 0, 0.01f);
  graphene_assert_fuzzy_equals (graphene_vec4_get_y (&res), 0, 0.01f);
  graphene_assert_fuzzy_equals (graphene_vec4_get_z (&res), -graphene_vec3_length (&dist), 0.01f);
  graphene_assert_fuzzy_equals (graphene_vec4_get_w (&res), 1, 0.01f);

  graphene_vec3_add (&center, &up, &center_plus_up);
  graphene_vec3_subtract (&center_plus_up, &eye, &dist);
  graphene_vec4_init_from_vec3 (&res, &center_plus_up, 1);
  graphene_matrix_transform_vec4 (&m, &res, &res);

  graphene_assert_fuzzy_equals (graphene_vec4_get_x (&res), 0, 0.01f);
  g_assert_cmpfloat (graphene_vec4_get_y (&res), <=, 1.f);
  g_assert_cmpfloat (graphene_vec4_get_y (&res), >=, 0.f);
  graphene_assert_fuzzy_equals (graphene_vec4_get_z (&res), -graphene_vec3_length (&dist), 0.01f);
  graphene_assert_fuzzy_equals (graphene_vec4_get_w (&res), 1, 0.01f);
#endif
}

static void
matrix_2d_identity (void)
{
  graphene_matrix_t m1;

  graphene_matrix_init_from_2d (&m1,
                                1, 0,
                                0, 1,
                                0, 0);
  mutest_expect ("a 2D identity matrix to be a 2D matrix",
                 mutest_bool_value (graphene_matrix_is_2d (&m1)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a 2D identity matrix to be an identity matrix",
                 mutest_bool_value (graphene_matrix_is_identity (&m1)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_init_from_2d (&m1,
                                1, 1,
                                1, 1,
                                0, 0);
  mutest_expect ("A matrix initialized by init_from_2d() to be a 2D matrix",
                 mutest_bool_value (graphene_matrix_is_2d (&m1)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("Not all 2D matrices to be identity matrices",
                 mutest_bool_value (graphene_matrix_is_identity (&m1)),
                 mutest_to_be_false,
                 NULL);
}

static void
matrix_2d_transforms (void)
{
  graphene_matrix_t m1, m2;
  double xx, xy, yx, yy, x_0, y_0;

  graphene_matrix_init_identity (&m1);
  graphene_matrix_scale (&m1, 2.0, 2.0, 1.0);
  graphene_matrix_translate (&m1, &GRAPHENE_POINT3D_INIT (0.5, 0.5, 0.0));
  mutest_expect ("scale(2, 2) translate(0.5, 0.5) to be a 2D transformation",
                 mutest_bool_value (graphene_matrix_is_2d (&m1)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_init_from_2d (&m2,
                                2.0, 0.0,
                                0.0, 2.0,
                                0.5, 0.5);
  mutest_expect ("scale(2, 2) translate(0.5, 0.5) to be [ 2 0 | 0 2 | .5 .5 ]",
                 mutest_pointer (&m1),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);

  graphene_matrix_to_2d (&m1, &xx, &yx, &xy, &yy, &x_0, &y_0);
  mutest_expect ("xx to be scale.x",
                 mutest_float_value (xx),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);
  mutest_expect ("yy to be scale.y",
                 mutest_float_value (yy),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);
  mutest_expect ("x0 to be translate.x",
                 mutest_float_value (x_0),
                 mutest_to_be_close_to, 0.5, 0.0001,
                 NULL);
  mutest_expect ("y0 to be translate.y",
                 mutest_float_value (y_0),
                 mutest_to_be_close_to, 0.5, 0.0001,
                 NULL);

  graphene_matrix_init_identity (&m1);
  graphene_matrix_translate (&m1, &GRAPHENE_POINT3D_INIT (50, 50, 0));
  graphene_matrix_rotate_z (&m1, 45.0);
  graphene_matrix_translate (&m1, &GRAPHENE_POINT3D_INIT (-50, -50, 0));
  mutest_expect ("translate(50, 50), rotate(45deg), translate(-50,-50) to be a 2D transformation",
                 mutest_bool_value (graphene_matrix_is_2d (&m1)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_init_identity (&m1);
  graphene_matrix_perspective (&m1, 500, &m1);
  graphene_matrix_rotate_y (&m1, 50.0);
  mutest_expect ("perspective(500), rotateY(50deg) to not be a 2D transformation",
                 mutest_bool_value (graphene_matrix_is_2d (&m1)),
                 mutest_to_be_false,
                 NULL);
}

static void
matrix_2d_round_trip (void)
{
  graphene_matrix_t m1, m2;
  double xx, xy, yx, yy, x_0, y_0;

  graphene_matrix_init_from_2d (&m1,
                                2.0, 0.0,
                                0.0, 2.0,
                                0.5, 0.5);

  graphene_matrix_to_2d (&m1, &xx, &yx, &xy, &yy, &x_0, &y_0);
  graphene_matrix_init_from_2d (&m2, xx, yx, xy, yy, x_0, y_0);
  mutest_expect ("roundtrip between init_from_2d and to_2d to result in the same matrix",
                 mutest_pointer (&m1),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);
}

static void
matrix_interpolate (void)
{
  graphene_matrix_t m1, m2, m3, mr;

  graphene_matrix_init_identity (&m1);
  graphene_matrix_init_identity (&m2);

  graphene_matrix_translate (&m2, &GRAPHENE_POINT3D_INIT (100.f, 100.f, 100.f));
  mutest_expect ("translate(100, 100, 100) is not a 2D transform",
                 mutest_bool_value (graphene_matrix_is_2d (&m2)),
                 mutest_not, mutest_to_be_true,
                 NULL);

  graphene_matrix_interpolate (&m1, &m2, 0.0, &mr);
  mutest_expect ("lerp(identity, translate(100, 100, 100), 0.0) = identity",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m1),
                 NULL);

  graphene_matrix_interpolate (&m1, &m2, 1.0, &mr);
  mutest_expect ("lerp(identity, translate(100, 100, 100), 1.0) = translate(100, 100, 100)",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);

  graphene_matrix_init_translate (&m3, &GRAPHENE_POINT3D_INIT (50.f, 50.f, 50.f));
  graphene_matrix_interpolate (&m1, &m2, 0.5, &mr);
  mutest_expect ("lerp(identity, translate(100, 100, 100), 0.5) = translate(50, 50, 50)",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m3),
                 NULL);
}

static void
matrix_interpolate_perspective (void)
{
  graphene_matrix_t m1, m2, m3, mr;

  graphene_matrix_init_identity (&m1);
  graphene_matrix_perspective (&m1, 200, &m1);
  graphene_matrix_init_identity (&m2);
  graphene_matrix_perspective (&m2, 800, &m2);

  graphene_matrix_interpolate (&m1, &m2, 0.0, &mr);
  mutest_expect ("lerp(perspective(200), perspective(800), 0.0) = perspective(200)",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m1),
                 NULL);

  graphene_matrix_interpolate (&m1, &m2, 1.0, &mr);
  mutest_expect ("lerp(perspective(200), perspective(800), 1.0) = perspective(800)",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);

  graphene_matrix_init_identity (&m3);
  graphene_matrix_perspective (&m3, 400, &m3);
  graphene_matrix_interpolate (&m1, &m2, 0.5, &mr);
  mutest_expect ("lerp(perspective(200), perspective(800), 0.5) = perspective(400)",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m3),
                 NULL);
}

static void
matrix_multiply_self (void)
{
  graphene_matrix_t a, b, res, test;
  float floats[16] = {
    0, 0, 0, 2,
    0, 0, 2, 0,
    0, 2, 0, 0,
    2, 0, 0, 0,
  };

  graphene_matrix_init_from_float (&a, floats);
  graphene_matrix_init_from_float (&b, floats);
  graphene_matrix_multiply (&a, &b, &res);

  graphene_matrix_init_from_float (&test, floats);
  graphene_matrix_multiply (&test, &b, &test);
  mutest_expect ("using the first operand as the result to be valid",
                 mutest_pointer (&test),
                 graphene_test_matrix_near, mutest_pointer (&res),
                 NULL);

  graphene_matrix_init_from_float (&test, floats);
  graphene_matrix_multiply (&a, &test, &test);
  mutest_expect ("using the second operand as the result to be valid",
                 mutest_pointer (&test),
                 graphene_test_matrix_near, mutest_pointer (&res),
                 NULL);

  graphene_matrix_init_from_float (&test, floats);
  graphene_matrix_multiply (&test, &test, &test);
  mutest_expect ("using the same operands as the result to be valid",
                 mutest_pointer (&test),
                 graphene_test_matrix_near, mutest_pointer (&res),
                 NULL);
}

static void
matrix_to_2d (void)
{
  graphene_matrix_t matrix;
  float f[16];
  bool valid[16] = {
    true,  true,  false, false,
    true,  true,  false, false,
    false, false, false, false,
    true,  true,  false, false,
  };
  bool check[16];

  for (unsigned i = 0; i < 16; i++)
    {
      /* Take the identity matrix and change one member to a different value */
      graphene_matrix_init_identity (&matrix);
      graphene_matrix_to_float (&matrix, f);
      f[i] = 0.5f;
      graphene_matrix_init_from_float (&matrix, f);

      check[i] = graphene_matrix_to_2d (&matrix, NULL, NULL, NULL, NULL, NULL, NULL);
    }

  mutest_expect ("2D matrices only change specific fields",
                 mutest_bool_value (memcmp (check, valid, sizeof (bool) * 16) == 0),
                 mutest_to_be_true,
                 NULL);
}

static void
matrix_2d_interpolate (void)
{
  graphene_matrix_t m1, m2, m3, mr;

  graphene_matrix_init_identity (&m1);
  graphene_matrix_init_identity (&m2);

  graphene_matrix_translate (&m2, &GRAPHENE_POINT3D_INIT (100.f, 100.f, 0.f));
  mutest_expect ("translating on the X and Y axis is a 2D transform",
                 mutest_bool_value (graphene_matrix_is_2d (&m2)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_interpolate (&m1, &m2, 0.0, &mr);
  mutest_expect ("lerp(identity, translate(100, 100), 0.0) = identity",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m1),
                 NULL);

  graphene_matrix_interpolate (&m1, &m2, 1.0, &mr);
  mutest_expect ("lerp(identity, translate(100, 100), 1.0) = translate(100, 100)",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m2),
                 NULL);

  graphene_matrix_init_translate (&m3, &GRAPHENE_POINT3D_INIT (50.f, 50.f, 0.f));
  graphene_matrix_interpolate (&m1, &m2, 0.5, &mr);
  mutest_expect ("lerp(identity, translate(100, 100), 0.5) = translate(50, 50)",
                 mutest_pointer (&mr),
                 graphene_test_matrix_near, mutest_pointer (&m3),
                 NULL);
}

static void
matrix_2d_transform_bound (void)
{
  graphene_matrix_t m1, m2;
  graphene_rect_t r, r2, res;

  graphene_matrix_init_identity (&m1);
  graphene_matrix_init_translate (&m2, &GRAPHENE_POINT3D_INIT (100.f, 100.f, 0.f));

  graphene_rect_init (&r, 0.f, 0.f, 50.f, 50.f);
  graphene_rect_init (&r2, 100.f, 100.f, 50.f, 50.f);

  graphene_matrix_transform_bounds (&m1, &r, &res);
  mutest_expect ("identity to not transform the bounds",
                 mutest_bool_value (graphene_rect_equal (&res, &r)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_transform_bounds (&m2, &r, &res);
  mutest_expect ("translate(100, 100, 0) to transform the origin of the bounds",
                 mutest_bool_value (graphene_point_near (&res.origin, &r2.origin, 0.01f)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("translate(100, 100, 0) to not transform the size of the bounds",
                 mutest_bool_value (graphene_size_equal (&res.size, &r2.size)),
                 mutest_to_be_true,
                 NULL);
}

static void
matrix_3d_transform_point (void)
{
  graphene_matrix_t m;
  graphene_point_t p;
  graphene_point3d_t p3;
  graphene_vec3_t v;

  graphene_matrix_init_translate (&m, &GRAPHENE_POINT3D_INIT (50.f, 70.f, 0.f));

  graphene_point_init (&p, 0.f, 0.f);
  graphene_matrix_transform_point (&m, &p, &p);
  mutest_expect ("mat(translation) × point(zero) = point(translation)",
                 mutest_bool_value (graphene_point_near (&p, &GRAPHENE_POINT_INIT (50.f, 70.f), 0.01f)),
                 mutest_to_be_true,
                 NULL);

  graphene_point3d_init (&p3, 0.f, 0.f, 0.f);
  graphene_matrix_transform_point3d (&m, &p3, &p3);
  mutest_expect ("mat(translation) × point3d(zero) = point3d(translation)",
                 mutest_bool_value (graphene_point3d_near (&p3, &GRAPHENE_POINT3D_INIT (50.f, 70.f, 0.0f), 0.01f)),
                 mutest_to_be_true,
                 NULL);

  graphene_vec3_init (&v, 0.f, 0.f, 0.f);
  graphene_matrix_transform_vec3 (&m, &v, &v);
  mutest_expect ("mat(translation) × vec3(zero) = vec3(zero)",
                 mutest_bool_value (graphene_vec3_near (&v, graphene_vec3_zero (), 0.01f)),
                 mutest_to_be_true,
                 NULL);
}

static void
matrix_suite (void)
{
  mutest_it ("can set and check identity", matrix_identity);
  mutest_it ("can check for equality and near equality", matrix_equal);
  mutest_it ("can scale", matrix_scale);
  mutest_it ("can rotate using an axis and an angle", matrix_rotation);
  mutest_it ("can rotate using eulers and quaternions", matrix_rotation_euler_quaternion);
  mutest_it ("can translate", matrix_translation);
  mutest_it ("has a neutral element", matrix_neutral_element);
  mutest_it ("implements look_at", matrix_look_at);
  mutest_it ("can invert", matrix_invert);
  mutest_it ("can interpolate", matrix_interpolate);
  mutest_it ("can interpolate a perspective transformation", matrix_interpolate_perspective);
  mutest_it ("can multiply itself", matrix_multiply_self);
  mutest_it ("can be converted to an affine matrix", matrix_to_2d);
  mutest_it ("can set and check 2D identity", matrix_2d_identity);
  mutest_it ("supports 2D transformations", matrix_2d_transforms);
  mutest_it ("supports round-trips with affine matrices", matrix_2d_round_trip);
  mutest_it ("can interpolate 2D transformations", matrix_2d_interpolate);
  mutest_it ("can transform 2D bounds", matrix_2d_transform_bound);
  mutest_it ("can transform 3D points", matrix_3d_transform_point);
}

MUTEST_MAIN (
  mutest_describe ("graphene_matrix_t", matrix_suite);
)
