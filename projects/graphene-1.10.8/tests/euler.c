/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
euler_init (mutest_spec_t *spec)
{
  graphene_euler_t *e;
  graphene_vec3_t v;

  e = graphene_euler_init (graphene_euler_alloc (), 0.f, 0.f, 0.f);
  mutest_expect ("graphene_euler_init() to set the x component",
                 mutest_float_value (graphene_euler_get_x (e)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("graphene_euler_init() to set the y component",
                 mutest_float_value (graphene_euler_get_y (e)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("graphene_euler_init() to set the z component",
                 mutest_float_value (graphene_euler_get_z (e)),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  graphene_euler_to_vec3 (e, &v);
  mutest_expect ("graphene_euler_to_vec3() to return a zero vector",
                 mutest_bool_value (graphene_vec3_equal (&v, graphene_vec3_zero ())),
                 mutest_to_be_true,
                 NULL);

  graphene_euler_free (e);
}

static void
euler_quaternion_roundtrip (mutest_spec_t *spec)
{
  graphene_euler_t values[3];
  unsigned int i;

  graphene_euler_init_with_order (&values[0], 0.f, 0.f, 0.f, GRAPHENE_EULER_ORDER_XYZ);
  graphene_euler_init_with_order (&values[1], 1.f, 0.f, 0.f, GRAPHENE_EULER_ORDER_XYZ);
  graphene_euler_init_with_order (&values[2], 0.f, 1.f, 0.f, GRAPHENE_EULER_ORDER_ZYX);

  for (i = 0; i < 3; i++)
    {
      graphene_quaternion_t q, check;
      graphene_euler_t e;

      graphene_quaternion_init_from_euler (&q, &values[i]);

      graphene_euler_init_from_quaternion (&e, &q, graphene_euler_get_order (&values[i]));
      graphene_quaternion_init_from_euler (&check, &e);

      mutest_expect ("roundtrip: quaternion → euler → quaternion",
                     mutest_bool_value (graphene_quaternion_equal (&q, &check)),
                     mutest_to_be_true,
                     NULL);
    }
}

static void
euler_matrix_roundtrip (mutest_spec_t *spec)
{
  graphene_euler_t values[3];
  unsigned int i;

  graphene_euler_init_with_order (&values[0], 0.f, 0.f, 0.f, GRAPHENE_EULER_ORDER_XYZ);
  graphene_euler_init_with_order (&values[1], 1.f, 0.f, 0.f, GRAPHENE_EULER_ORDER_XYZ);
  graphene_euler_init_with_order (&values[2], 0.f, 1.f, 0.f, GRAPHENE_EULER_ORDER_ZYX);

  for (i = 0; i < 3; i++)
    {
      graphene_matrix_t m, check;
      graphene_euler_t e;

      graphene_euler_to_matrix (&values[i], &m);
      graphene_euler_init_from_matrix (&e, &m, graphene_euler_get_order (&values[i]));

      graphene_euler_to_matrix (&e, &check);

      mutest_expect ("roundtrip: matrix → euler → matrix",
                     mutest_bool_value (graphene_matrix_near (&m, &check, 0.01f)),
                     mutest_to_be_true,
                     NULL);
    }
}

static void
euler_suite (mutest_suite_t *suite)
{
  mutest_it ("initializes all components", euler_init);
  mutest_it ("roundtrips between euler angles and quaternions", euler_quaternion_roundtrip);
  mutest_it ("roundtrips between euler angles and matrices", euler_matrix_roundtrip);
}

MUTEST_MAIN (
  mutest_describe ("graphene_euler_t", euler_suite);
)
