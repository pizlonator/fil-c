#include "four_point_transform.h"
#include "singular_value_decomposition.h"

/* Make a 4x4 matrix that maps
 * e1        -> p1
 * e2        -> p3
 * e3        -> p3
 * (1,1,1,0) -> p4
 */
static void
unit_to (graphene_point3d_t *p1,
         graphene_point3d_t *p2,
         graphene_point3d_t *p3,
         graphene_point3d_t *p4,
         graphene_matrix_t  *m)
{
  graphene_vec3_t v1, v2, v3, v4;
  graphene_vec4_t vv1, vv2, vv3, vv4, p;
  graphene_matrix_t u, s;
  float v[16] = { 0., };
  double A[16];
  double U[16];
  double S[4];
  double V[16];
  double B[4];
  double x[4];
  int i, j;

  graphene_point3d_to_vec3 (p1, &v1);
  graphene_point3d_to_vec3 (p2, &v2);
  graphene_point3d_to_vec3 (p3, &v3);
  graphene_point3d_to_vec3 (p4, &v4);

  graphene_vec4_init_from_vec3 (&vv1, &v1, 1.);
  graphene_vec4_init_from_vec3 (&vv2, &v2, 1.);
  graphene_vec4_init_from_vec3 (&vv3, &v3, 1.);
  graphene_vec4_init_from_vec3 (&vv4, &v4, 1.);

  graphene_vec4_init (&p, 0., 0., 0., 1.);

  graphene_matrix_init_from_vec4 (&u, &vv1, &vv2, &vv3, &p);

  /* solve x * u = vv4 */

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      A[j * 4 + i] = graphene_matrix_get_value (&u, i, j);

  B[0] = graphene_vec4_get_x (&vv4);
  B[1] = graphene_vec4_get_y (&vv4);
  B[2] = graphene_vec4_get_z (&vv4);
  B[3] = graphene_vec4_get_w (&vv4);

  singular_value_decomposition (A, 4, 4, U, S, V);
  singular_value_decomposition_solve (U, S, V, 4, 4, B, x);

  v[ 0] = x[0];
  v[ 5] = x[1];
  v[10] = x[2];
  v[15] = 1;

  graphene_matrix_init_from_float (&s, (const float *)&v);
  graphene_matrix_multiply (&s, &u, m);
}

/* Compute a 4x4 matrix m that maps
 * p1 -> q1
 * p2 -> q2
 * p3 -> q3
 * p4 -> q4
 *
 * This is not in general possible, because projective
 * transforms preserve coplanarity. But in the cases we
 * care about here, both sets of points are always coplanar.
 */
void
perspective_3d (graphene_point3d_t *p1,
                graphene_point3d_t *p2,
                graphene_point3d_t *p3,
                graphene_point3d_t *p4,
                graphene_point3d_t *q1,
                graphene_point3d_t *q2,
                graphene_point3d_t *q3,
                graphene_point3d_t *q4,
                graphene_matrix_t  *m)
{
  graphene_matrix_t a, a_inv, b;

  unit_to (p1, p2, p3, p4, &a);
  unit_to (q1, q2, q3, q4, &b);

  graphene_matrix_inverse (&a, &a_inv);
  graphene_matrix_multiply (&a_inv, &b, m);
}
