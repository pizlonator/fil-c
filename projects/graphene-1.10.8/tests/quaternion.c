/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
quaternion_init (mutest_spec_t *spec)
{
  graphene_quaternion_t q1, q2;

  graphene_quaternion_init (&q1, 0.f, 0.f, 0.f, 1.f);
  graphene_quaternion_init_identity (&q2);
  mutest_expect ("identity to set (0, 0, 0, 1)",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q2)),
                 mutest_to_be_true,
                 NULL);

  graphene_quaternion_init (&q1, 1.f, 1.f, 1.f, 1.f);
  mutest_expect ("initialization sets all fields",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q2)),
                 mutest_not, mutest_to_be_true,
                 NULL);

  graphene_quaternion_init_from_quaternion (&q2, &q1);
  mutest_expect ("initialization from quaternion makes a copy",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q2)),
                 mutest_to_be_true,
                 NULL);
}

static void
quaternion_operators_invert (mutest_spec_t *spec)
{
  graphene_quaternion_t q1, q2, tmp;

  graphene_quaternion_init_identity (&q1);
  graphene_quaternion_invert (&q1, &q2);
  mutest_expect ("inverting identity gives back an identity",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q2)),
                 mutest_to_be_true,
                 NULL);

  graphene_quaternion_init (&q1, 1.f, 1.f, 1.f, 1.f);
  graphene_quaternion_invert (&q1, &q2);
  mutest_expect ("inverting a quaternion flips the sign of the first three fields",
                 mutest_bool_value (graphene_quaternion_equal (&q2, graphene_quaternion_init (&tmp, -1.f, -1.f, -1.f, 1.f))),
                 mutest_to_be_true,
                 NULL);
}

static void
quaternion_operators_dot (mutest_spec_t *spec)
{
  graphene_quaternion_t q1, q2;

  graphene_quaternion_init (&q1, 1.f, 1.f, 1.f, 1.f);
  graphene_quaternion_invert (&q1, &q2);

  mutest_expect ("dot(q, inv(q)) with q = (1, 1, 1, 1) to be -2",
                 mutest_float_value (graphene_quaternion_dot (&q1, &q2)),
                 mutest_to_be_close_to, -2.0, 0.0001,
                 NULL);
}

static void
quaternion_operators_equal (mutest_spec_t *spec)
{
  graphene_quaternion_t q1, q2, q3;

  graphene_quaternion_init (&q1, 1.f, 1.f, 1.f, 1.f);
  graphene_quaternion_init (&q2, 1.f, 2.f, 3.f, 1.f);
  graphene_quaternion_invert (&q1, &q3);

  mutest_expect ("a quaternion to be equal to itself",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q1)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a quaternion not to be equal to null",
                 mutest_bool_value (graphene_quaternion_equal (&q1, NULL)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("null not to be equal to a quaternion",
                 mutest_bool_value (graphene_quaternion_equal (NULL, &q1)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("two different quaternions not to be equal",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q2)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("a quaternion to be equal to its invert",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q3)),
                 mutest_to_be_true,
                 NULL);
}

static void
quaternion_operators_normalize (mutest_spec_t *spec)
{
  graphene_quaternion_t q1, q2;
  graphene_vec4_t v1, v2;

  graphene_quaternion_init (&q1, 1.f, 2.f, 3.f, 4.f);
  graphene_quaternion_normalize (&q1, &q2);

  graphene_vec4_init (&v1, 1.f, 2.f, 3.f, 4.f);
  graphene_vec4_normalize (&v1, &v1);

  graphene_quaternion_to_vec4 (&q2, &v2);

  mutest_expect ("normalizing a quaternion is the same as normalizing the equivalent vec4",
                 mutest_bool_value (graphene_vec4_near (&v1, &v2, 0.00001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
quaternion_angles_to_from (mutest_spec_t *spec)
{
  graphene_quaternion_t *q;
  float a, b, c;

  q = graphene_quaternion_init_from_angles (graphene_quaternion_alloc (), 90.f, 45.f, -105.f);
  graphene_quaternion_to_angles (q, &a, &b, &c);

  mutest_expect ("roundtrip between init_from_angles and to_angles to yield the same yaw",
                 mutest_float_value (a),
                 mutest_to_be_close_to, 90.0, 0.0001,
                 NULL);
  mutest_expect ("roundtrip between init_from_angles and to_angles to yield the same pitch",
                 mutest_float_value (b),
                 mutest_to_be_close_to, 45.0, 0.0001,
                 NULL);
  mutest_expect ("roundtrip between init_from_angles and to_angles to yield the same roll",
                 mutest_float_value (c),
                 mutest_to_be_close_to, -105.0, 0.0001,
                 NULL);

  graphene_quaternion_free (q);
}

static void
quaternion_vec4_to_from (mutest_spec_t *spec)
{
  graphene_quaternion_t *q;
  graphene_vec4_t v, check;

  graphene_vec4_init (&v, 1.f, 2.f, 3.f, 4.f);

  q = graphene_quaternion_init_from_vec4 (graphene_quaternion_alloc (), &v);
  graphene_quaternion_to_vec4 (q, &check);

  mutest_expect ("roundtrip between init_from_vec4 and to_vec4 to yield the same vector",
                 mutest_bool_value (graphene_vec4_near (&v, &check, 0.0001f)),
                 mutest_to_be_true,
                 NULL);

  graphene_quaternion_free (q);
}

static void
quaternion_matrix_to_from (mutest_spec_t *spec)
{
  graphene_quaternion_t *q1, *q2;
  graphene_matrix_t *m;

  m = graphene_matrix_init_identity (graphene_matrix_alloc ());

  q1 = graphene_quaternion_init_from_matrix (graphene_quaternion_alloc (), m);
  q2 = graphene_quaternion_init_identity (graphene_quaternion_alloc ());
  mutest_expect ("initializing from an identity matrix yields an identity quaternion",
                 mutest_bool_value (graphene_quaternion_equal (q1, q2)),
                 mutest_to_be_true,
                 NULL);

  graphene_matrix_rotate_x (m, 30.f);
  graphene_matrix_rotate_y (m, 45.f);
  graphene_matrix_rotate_z (m, -135.f);
  graphene_quaternion_init_from_matrix (q1, m);
  mutest_expect ("initializing from a rotation matrix does not yield an identity quaternion",
                 mutest_bool_value (graphene_quaternion_equal (q1, q2)),
                 mutest_to_be_false,
                 NULL);

  graphene_matrix_init_identity (m);
  graphene_matrix_rotate_quaternion (m, q1);
  graphene_quaternion_init_from_matrix (q2, m);
  mutest_expect ("rotating a matrix with a quaternion yields the same quaternion",
                 mutest_bool_value (graphene_quaternion_equal (q1, q2)),
                 mutest_to_be_true,
                 NULL);

  graphene_quaternion_free (q2);
  graphene_quaternion_free (q1);

  graphene_matrix_free (m);
}

static void
quaternion_angle_vec3_to_from (mutest_spec_t *spec)
{
  graphene_quaternion_t q1, q2;
  graphene_vec3_t axis;
  float angle;

  graphene_quaternion_init_from_angle_vec3 (&q1, 45.f, graphene_vec3_y_axis ());
  graphene_quaternion_init_from_quaternion (&q2, &q1);
  graphene_quaternion_to_angle_vec3 (&q2, &angle, &axis);

  mutest_expect ("roundtrip init_from_angle_vec3 and to_angle_vec3 yields the same angle",
                 mutest_float_value (angle),
                 mutest_to_be_close_to, 45.0, 0.0001,
                 NULL);
  mutest_expect ("roundtrip init_from_angle_vec3 and to_angle_vec3 yields the same axis",
                 mutest_bool_value (graphene_vec3_near (&axis, graphene_vec3_y_axis (), 0.0001f)),
                 mutest_to_be_true,
                 NULL);
}

static void
quaternion_slerp (mutest_spec_t *spec)
{
  graphene_quaternion_t q1, q2, q3;

  graphene_quaternion_init (&q1, 0.0f, 0.0f, 0.0f, 1.0f);
  graphene_quaternion_init (&q2, 0.2f, 0.3f, 0.4f, 0.5f);
  mutest_expect ("initial and final states are different",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q2)),
                 mutest_to_be_false,
                 NULL);

  graphene_quaternion_slerp (&q1, &q2, 0.33f, &q3);
  mutest_expect ("interpolated result is different from the initial state",
                 mutest_bool_value (graphene_quaternion_equal (&q1, &q3)),
                 mutest_to_be_false,
                 NULL);

  graphene_quaternion_slerp (&q1, &q2, 0.66f, &q3);
  mutest_expect ("interpolated result is different from the final state",
                 mutest_bool_value (graphene_quaternion_equal (&q2, &q3)),
                 mutest_to_be_false,
                 NULL);
}

static void
quaternion_suite (mutest_suite_t *suite)
{
  mutest_it ("initialize", quaternion_init);
  mutest_it ("invert", quaternion_operators_invert);
  mutest_it ("dot", quaternion_operators_dot);
  mutest_it ("equal", quaternion_operators_equal);
  mutest_it ("normalize", quaternion_operators_normalize);
  mutest_it ("converts to and from angles", quaternion_angles_to_from);
  mutest_it ("converts to and from vec4", quaternion_vec4_to_from);
  mutest_it ("converts to and from matrix", quaternion_matrix_to_from);
  mutest_it ("converts to and from angle/axis", quaternion_angle_vec3_to_from);
  mutest_it ("slerp", quaternion_slerp);
}

MUTEST_MAIN (
  mutest_describe ("graphene_quaternion_t", quaternion_suite);
)
