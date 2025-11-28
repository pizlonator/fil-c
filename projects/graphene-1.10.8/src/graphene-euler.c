/* graphene-euler.c: Euler angles
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2014  Emmanuele Bassi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * SECTION:graphene-euler
 * @Title: Euler
 * @Short_description: Euler angles
 *
 * The #graphene_euler_t structure defines a rotation along three axes using
 * three angles. It also optionally can describe the order of the rotations.
 *
 * [Euler's rotation theorem](https://en.wikipedia.org/wiki/Euler%27s_rotation_theorem)
 * states that, in three-dimensional space, any displacement of a rigid body
 * such that a point on the rigid body remains fixed, is equivalent to a single
 * rotation about some axis that runs through the fixed point. The angles on
 * each axis can be placed in a vector of three components—α, β, and γ—called
 * the *Euler angle vector*. Each rotation described by these components results
 * in a rotation matrix:
 *
 * |[
 *   rot(α) = A
 *   rot(β) = B
 *   rot(γ) = G
 * ]|
 *
 * The resulting rotation matrix expressed by the Euler angle vector is
 * given by the product of each rotation matrix:
 *
 * |[
 *   G × B × A = R
 * ]|
 *
 * In order to specify the meaning of an Euler angle vector, we need to
 * assign each axis of rotation to the corresponding α, β, and γ components,
 * for instance X, Y, and Z.
 *
 * Additionally, we need to specify whether the rotations move the axes
 * as they are applied, also known as intrinsic, or relative rotations;
 * or if the axes stay fixed and the vectors move within the axis frame,
 * also known as extrinsic, or static rotations. For instance, a static
 * rotation alongside the ZYX axes will be interpreted as relative to
 * extrinsic coordinate axes, and be performed, in order, about the Z,
 * Y, and finally X axis. A relative rotation alongside the ZXZ axes will
 * be interpreted as relative to intrinsic coordinate axes, and be
 * performed, in order, about the Z axis, about the rotated X axis, and
 * finally about the rotated Z axis.
 *
 * Finally, we need to define the direction of the rotation, or the handedness
 * of the coordinate system. In the case of Graphene, the direction is given
 * by the right-hand rule, which means all rotations are counterclockwise.
 *
 * Rotations described Euler angles are typically immediately understandable,
 * compared to rotations expressed using quaternions, but they are susceptible
 * of ["Gimbal lock"](http://en.wikipedia.org/wiki/Gimbal_lock) — the loss of
 * one degree of freedom caused by two axis on the same plane. You typically
 * should use #graphene_euler_t to expose rotation angles in your API, or to
 * store them, but use #graphene_quaternion_t to apply rotations to modelview
 * matrices, or interpolate between initial and final rotation transformations.
 *
 * For more information, see:
 *
 *   - http://en.wikipedia.org/wiki/Rotation_matrix
 *   - http://en.wikipedia.org/wiki/Euler_angles
 *   - http://mathworld.wolfram.com/EulerAngles.html
 *   - "Representing Attitude with Euler Angles and Quaternions: A
 *     Reference" by James Diebel, 2006
 *   - "Graphics Gems IV", edited by Paul Heckbert, Academic Press, 1994.
 *
 * See also: #graphene_quaternion_t.
 */

#include "graphene-private.h"

#include "graphene-euler.h"

#include "graphene-alloc-private.h"
#include "graphene-matrix.h"
#include "graphene-quaternion.h"
#include "graphene-vectors-private.h"

#define EULER_DEFAULT_ORDER GRAPHENE_EULER_ORDER_SXYZ

#define LAST_DEPRECATED GRAPHENE_EULER_ORDER_ZYX

#define ORDER_OFFSET(o) ((o) - (LAST_DEPRECATED + 1))

/* The orders of rotation we support, minus the deprecated aliases */
enum {
  SXYZ,
  SXYX,
  SXZY,

  SXZX,
  SYZX,
  SYZY,

  SYXZ,
  SYXY,
  SZXY,

  SZXZ,
  SZYX,
  SZYZ,

  RZYX,
  RXYX,
  RYZX,

  RXZX,
  RXZY,
  RYZY,

  RZXY,
  RYXY,
  RYXZ,

  RZXZ,
  RXYZ,
  RZYZ
};

struct axis_param
{
  /* The initial axis in the permutation */
  int first_axis;
  /* Parity of the axis permutation (false for even, true for odd) */
  bool parity;
  /* Repetition of first_axis as the last */
  bool repetition;
  /* Frame from which the axes are taken (false for static, true for relative) */
  bool frame;
};

/* Map each order to the corresponding parameters of the rotation */
static const struct axis_param order_parameters[] = {
  [SXYZ] = { 0, false, false, false },
  [SXYX] = { 0, false, true,  false },
  [SXZY] = { 0, true,  false, false },

  [SXZX] = { 0, true,  true,  false },
  [SYZX] = { 1, false, false, false },
  [SYZY] = { 1, false, true,  false },

  [SYXZ] = { 1, true,  false, false },
  [SYXY] = { 1, true,  true,  false },
  [SZXY] = { 2, false, false, false },

  [SZXZ] = { 2, false, true,  false },
  [SZYX] = { 2, true,  false, false },
  [SZYZ] = { 2, true,  true,  false },

  [RZYX] = { 0, false, false, true  },
  [RXYX] = { 0, false, true,  true  },
  [RYZX] = { 0, true,  false, true  },

  [RXZX] = { 0, true,  true,  true  },
  [RXZY] = { 1, false, false, true  },
  [RYZY] = { 1, false, true,  true  },

  [RZXY] = { 1, true,  false, true  },
  [RYXY] = { 1, true,  true,  true  },
  [RYXZ] = { 2, false, false, true  },

  [RZXZ] = { 2, false, true,  true  },
  [RXYZ] = { 2, true,  false, true  },
  [RZYZ] = { 2, true,  true,  true  },
};

/* Axis sequences for Euler angles */
static const int next_axis[4] = { 1, 2, 0, 1 };

/* Original code to convert Euler angles to and from a 4x4 matrix is taken
 * from "Graphics Gems IV", edited by Paul Heckbert, Academic Press, 1994.
 *
 * Original author: Ken Shoemake, 1993
 *
 * https://github.com/erich666/GraphicsGems/blob/master/gemsiv/euler_angle/EulerAngles.c
 */
static inline void
euler_to_matrix (float                    ai,
                 float                    aj,
                 float                    ak,
                 const struct axis_param *params,
                 graphene_matrix_t       *matrix)
{
  int i = params->first_axis;
  int j = next_axis[i + (params->parity ? 1 : 0)];
  int k = next_axis[i - (params->parity ? 1 : 0) + 1];

  if (params->frame)
    {
      float tmp = ai;
      ai = ak;
      ak = tmp;
    }

  if (params->parity)
    {
      ai *= -1.f;
      aj *= -1.f;
      ak *= -1.f;
    }

  float si, sj, sk;
  float ci, cj, ck;

  graphene_sincos (ai, &si, &ci);
  graphene_sincos (aj, &sj, &cj);
  graphene_sincos (ak, &sk, &ck);

  float cc = ci * ck;
  float cs = ci * sk;
  float sc = si * ck;
  float ss = si * sk;

  float m[16];

/* Our matrices are row major, however the code below is based on code
   that assumes matrixes apply from the left, and we apply on the
   right, so need to flip row/column. */
#define M(m, r, c) (m)[((c) << 2) + (r)]

  /* We need to construct the matrix from float values instead
   * of SIMD vectors because the access is parametrised on the
   * axes of the transformation, and it would lead to a
   * combinatorial explosion of branches
   */
  if (params->repetition)
    {
      M (m, i, i) = cj;
      M (m, i, j) = sj * si;
      M (m, i, k) = sj * ci;
      M (m, j, i) = sj * sk;
      M (m, j, j) = -cj * ss + cc;
      M (m, j, k) = -cj * cs - sc;
      M (m, k, i) = -sj * ck;
      M (m, k, j) = cj * sc + cs;
      M (m, k, k) = cj * cc - ss;
    }
  else
    {
      M (m, i, i) = cj * ck;
      M (m, i, j) = sj * sc - cs;
      M (m, i, k) = sj * cc + ss;
      M (m, j, i) = cj * sk;
      M (m, j, j) = sj * ss + cc;
      M (m, j, k) = sj * cs - sc;
      M (m, k, i) = -sj;
      M (m, k, j) = cj * si;
      M (m, k, k) = cj * ci;
    }

  M (m, 3, 0) = M (m, 3, 1) = M (m, 3, 2) = 0.f;
  M (m, 0, 3) = M (m, 1, 3) = M (m, 2, 3) = 0.f;
  M (m, 3, 3) = 1.f;

#undef M

  graphene_matrix_init_from_float (matrix, m);
}

static inline void
matrix_to_euler (const graphene_matrix_t *matrix,
                 const struct axis_param *params,
                 float                   *ai,
                 float                   *aj,
                 float                   *ak)
{
  int i = params->first_axis;
  int j = next_axis[i + (params->parity ? 1 : 0)];
  int k = next_axis[i - (params->parity ? 1 : 0) + 1];

  /* The cell access to the matrix is parametrised on the axes
   * of the transformation, so we cannot use SIMD vectors to
   * avoid a combinatorial explosion of branches, or slow single
   * lane access.
   */
  float m[16];

  graphene_matrix_to_float (matrix, m);

/* Our matrices are row major, however the code below is based on code
   that assumes matrixes apply from the left, and we apply on the
   right, so need to flip row/column. */
#define M(m, r, c) (m)[((c) << 2) + (r)]

  float ax, ay, az;
  if (params->repetition)
    {
      float sy = sqrtf (M (m, i, j) * M (m, i, j) + M (m, i, k) * M (m, i, j));

      if (sy >= 16 * FLT_EPSILON)
        {
          ax = atan2f (M (m, i, j), M (m, i, k));
          ay = atan2f (sy, M (m, i, i));
          az = atan2f (M (m, j, i), M (m, k, i) * -1.f);
        }
      else
        {
          ax = atan2f (M (m, j, k) * -1.f, M (m, j, j));
          ay = atan2f (sy, M (m, i, i));
          az = 0.f;
        }
    }
  else
    {
      float cy = sqrtf (M (m, i, i) * M (m, i, i) + M (m, j, i) * M (m, j, i));

      if (cy >= 16 * FLT_EPSILON)
        {
          ax = atan2f (M (m, k, j), M (m, k, k));
          ay = atan2f (M (m, k, i) * -1.f, cy);
          az = atan2f (M (m, j, i), M (m, i, i));
        }
      else
        {
          ax = atan2f (M (m, j, k) * -1.f, M (m, j, j));
          ay = atan2f (M (m, k, i) * -1.f, cy);
          az = 0.f;
        }
    }

#undef M

  if (params->parity)
    {
      ax *= -1.f;
      ay *= -1.f;
      az *= -1.f;
    }

  if (params->frame)
    {
      float tmp = ax;

      ax = az;
      az = tmp;
    }

  if (ai != NULL)
    *ai = ax;
  if (aj != NULL)
    *aj = ay;
  if (ak != NULL)
    *ak = az;
}

/**
 * graphene_euler_alloc: (constructor)
 *
 * Allocates a new #graphene_euler_t.
 *
 * The contents of the returned structure are undefined.
 *
 * Returns: (transfer full): the newly allocated #graphene_euler_t
 *
 * Since: 1.2
 */
graphene_euler_t *
graphene_euler_alloc (void)
{
  return graphene_aligned_alloc0 (sizeof (graphene_euler_t), 1, 16);
}

/**
 * graphene_euler_free:
 * @e: a #graphene_euler_t
 *
 * Frees the resources allocated by graphene_euler_alloc().
 *
 * Since: 1.2
 */
void
graphene_euler_free (graphene_euler_t *e)
{
  graphene_aligned_free (e);
}

/*< private >
 * graphene_euler_get_real_order:
 * @order: a #graphene_euler_order_t
 *
 * Normalizes the given enumeration value to remove aliases and
 * deprecated values.
 *
 * Returns: the real order
 */
static graphene_euler_order_t
graphene_euler_get_real_order (graphene_euler_order_t order)
{
  switch (order)
    {
    case GRAPHENE_EULER_ORDER_XYZ:
      return GRAPHENE_EULER_ORDER_SXYZ;

    case GRAPHENE_EULER_ORDER_YXZ:
      return GRAPHENE_EULER_ORDER_SYXZ;

    case GRAPHENE_EULER_ORDER_ZXY:
      return GRAPHENE_EULER_ORDER_SZXY;

    case GRAPHENE_EULER_ORDER_ZYX:
      return GRAPHENE_EULER_ORDER_SZYX;

    case GRAPHENE_EULER_ORDER_YZX:
      return GRAPHENE_EULER_ORDER_SYZX;

    case GRAPHENE_EULER_ORDER_XZY:
      return GRAPHENE_EULER_ORDER_SXZY;

    case GRAPHENE_EULER_ORDER_DEFAULT:
      return GRAPHENE_EULER_ORDER_SXYZ;

    default:
      break;
    }

  return order;
}

/*< private >
 * graphene_euler_init_internal:
 * @e: the #graphene_euler_t to initialize
 * @x: rotation angle on the X axis, in radians
 * @y: rotation angle on the Y axis, in radians
 * @z: rotation angle on the Z axis, in radians
 * @order: order of rotations
 *
 * Initializes a #graphene_euler_t using the given angles
 * and order of rotation.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 */
static graphene_euler_t *
graphene_euler_init_internal (graphene_euler_t       *e,
                              float                   rad_x,
                              float                   rad_y,
                              float                   rad_z,
                              graphene_euler_order_t  order)
{
  graphene_vec3_init (&e->angles, rad_x, rad_y, rad_z);
  e->order = graphene_euler_get_real_order (order);

  return e;
}

/**
 * graphene_euler_init:
 * @e: the #graphene_euler_t to initialize
 * @x: rotation angle on the X axis, in degrees
 * @y: rotation angle on the Y axis, in degrees
 * @z: rotation angle on the Z axis, in degrees
 *
 * Initializes a #graphene_euler_t using the given angles.
 *
 * The order of the rotations is %GRAPHENE_EULER_ORDER_DEFAULT.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 *
 * Since: 1.2
 */
graphene_euler_t *
graphene_euler_init (graphene_euler_t *e,
                     float             x,
                     float             y,
                     float             z)
{
  return graphene_euler_init_internal (e,
                                       GRAPHENE_DEG_TO_RAD (x),
                                       GRAPHENE_DEG_TO_RAD (y),
                                       GRAPHENE_DEG_TO_RAD (z),
                                       GRAPHENE_EULER_ORDER_DEFAULT);
}

/**
 * graphene_euler_init_with_order:
 * @e: the #graphene_euler_t to initialize
 * @x: rotation angle on the X axis, in degrees
 * @y: rotation angle on the Y axis, in degrees
 * @z: rotation angle on the Z axis, in degrees
 * @order: the order used to apply the rotations
 *
 * Initializes a #graphene_euler_t with the given angles and @order.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 *
 * Since: 1.2
 */
graphene_euler_t *
graphene_euler_init_with_order (graphene_euler_t       *e,
                                float                   x,
                                float                   y,
                                float                   z,
                                graphene_euler_order_t  order)
{
  return graphene_euler_init_internal (e,
                                       GRAPHENE_DEG_TO_RAD (x),
                                       GRAPHENE_DEG_TO_RAD (y),
                                       GRAPHENE_DEG_TO_RAD (z),
                                       order);
}

/**
 * graphene_euler_init_from_matrix:
 * @e: the #graphene_euler_t to initialize
 * @m: (nullable): a rotation matrix
 * @order: the order used to apply the rotations
 *
 * Initializes a #graphene_euler_t using the given rotation matrix.
 *
 * If the #graphene_matrix_t @m is %NULL, the #graphene_euler_t will
 * be initialized with all angles set to 0.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 *
 * Since: 1.2
 */
graphene_euler_t *
graphene_euler_init_from_matrix (graphene_euler_t        *e,
                                 const graphene_matrix_t *m,
                                 graphene_euler_order_t   order)
{
  float x, y, z;

  if (m == NULL || graphene_matrix_is_identity (m))
    return graphene_euler_init_with_order (e, 0.f, 0.f, 0.f, order);

  order = graphene_euler_get_real_order (order);
  matrix_to_euler (m, &order_parameters[ORDER_OFFSET (order)], &x, &y, &z);
  graphene_euler_init_internal (e, x, y, z, order);

  return e;
}

/**
 * graphene_euler_init_from_quaternion:
 * @e: a #graphene_euler_t
 * @q: (nullable): a normalized #graphene_quaternion_t
 * @order: the order used to apply the rotations
 *
 * Initializes a #graphene_euler_t using the given normalized quaternion.
 *
 * If the #graphene_quaternion_t @q is %NULL, the #graphene_euler_t will
 * be initialized with all angles set to 0.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 *
 * Since: 1.2
 */
graphene_euler_t *
graphene_euler_init_from_quaternion (graphene_euler_t            *e,
                                     const graphene_quaternion_t *q,
                                     graphene_euler_order_t       order)
{
  if (q == NULL)
    return graphene_euler_init_with_order (e, 0.f, 0.f, 0.f, order);

  graphene_matrix_t m;

  graphene_quaternion_to_matrix (q, &m);

  return graphene_euler_init_from_matrix (e, &m, graphene_euler_get_real_order (order));
}

/**
 * graphene_euler_init_from_vec3:
 * @e: the #graphene_euler_t to initialize
 * @v: (nullable): a #graphene_vec3_t containing the rotation
 *   angles in degrees
 * @order: the order used to apply the rotations
 *
 * Initializes a #graphene_euler_t using the angles contained in a
 * #graphene_vec3_t.
 *
 * If the #graphene_vec3_t @v is %NULL, the #graphene_euler_t will be
 * initialized with all angles set to 0.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 *
 * Since: 1.2
 */
graphene_euler_t *
graphene_euler_init_from_vec3 (graphene_euler_t       *e,
                               const graphene_vec3_t  *v,
                               graphene_euler_order_t  order)
{
  if (v != NULL)
    graphene_vec3_scale (v, (GRAPHENE_PI / 180.f), &e->angles);
  else
    graphene_vec3_init_from_vec3 (&e->angles, graphene_vec3_zero ());

  e->order = graphene_euler_get_real_order (order);

  return e;
}

/**
 * graphene_euler_init_from_euler:
 * @e: the #graphene_euler_t to initialize
 * @src: (nullable): a #graphene_euler_t
 *
 * Initializes a #graphene_euler_t using the angles and order of
 * another #graphene_euler_t.
 *
 * If the #graphene_euler_t @src is %NULL, this function is equivalent
 * to calling graphene_euler_init() with all angles set to 0.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 *
 * Since: 1.2
 */
graphene_euler_t *
graphene_euler_init_from_euler (graphene_euler_t       *e,
                                const graphene_euler_t *src)
{
  if (src == NULL)
    return graphene_euler_init (e, 0.f, 0.f, 0.f);

  *e = *src;

  return e;
}

/**
 * graphene_euler_init_from_radians:
 * @e: the #graphene_euler_t to initialize
 * @x: rotation angle on the X axis, in radians
 * @y: rotation angle on the Y axis, in radians
 * @z: rotation angle on the Z axis, in radians
 * @order: order of rotations
 *
 * Initializes a #graphene_euler_t using the given angles
 * and order of rotation.
 *
 * Returns: (transfer none): the initialized #graphene_euler_t
 *
 * Since: 1.10
 */
graphene_euler_t *
graphene_euler_init_from_radians (graphene_euler_t       *e,
                                  float                   x,
                                  float                   y,
                                  float                   z,
                                  graphene_euler_order_t  order)
{
  return graphene_euler_init_internal (e, x, y, z, order);
}

static bool
euler_equal (const void *p1,
             const void *p2)
{
  const graphene_euler_t *a = p1;
  const graphene_euler_t *b = p2;

  return graphene_vec3_equal (&a->angles, &b->angles) && a->order == b->order;
}

/**
 * graphene_euler_equal:
 * @a: a #graphene_euler_t
 * @b: a #graphene_euler_t
 *
 * Checks if two #graphene_euler_t are equal.
 *
 * Returns: `true` if the two #graphene_euler_t are equal
 *
 * Since: 1.2
 */
bool
graphene_euler_equal (const graphene_euler_t *a,
                      const graphene_euler_t *b)
{
  return graphene_pointer_equal (a, b, euler_equal);
}

/**
 * graphene_euler_get_x:
 * @e: a #graphene_euler_t
 *
 * Retrieves the rotation angle on the X axis, in degrees.
 *
 * Returns: the rotation angle
 *
 * Since: 1.2
 */
float
graphene_euler_get_x (const graphene_euler_t *e)
{
  return GRAPHENE_RAD_TO_DEG (graphene_vec3_get_x (&e->angles));
}

/**
 * graphene_euler_get_y:
 * @e: a #graphene_euler_t
 *
 * Retrieves the rotation angle on the Y axis, in degrees.
 *
 * Returns: the rotation angle
 *
 * Since: 1.2
 */
float
graphene_euler_get_y (const graphene_euler_t *e)
{
  return GRAPHENE_RAD_TO_DEG (graphene_vec3_get_y (&e->angles));
}

/**
 * graphene_euler_get_z:
 * @e: a #graphene_euler_t
 *
 * Retrieves the rotation angle on the Z axis, in degrees.
 *
 * Returns: the rotation angle
 *
 * Since: 1.2
 */
float
graphene_euler_get_z (const graphene_euler_t *e)
{
  return GRAPHENE_RAD_TO_DEG (graphene_vec3_get_z (&e->angles));
}

/**
 * graphene_euler_get_order:
 * @e: a #graphene_euler_t
 *
 * Retrieves the order used to apply the rotations described in the
 * #graphene_euler_t structure, when converting to and from other
 * structures, like #graphene_quaternion_t and #graphene_matrix_t.
 *
 * This function does not return the %GRAPHENE_EULER_ORDER_DEFAULT
 * enumeration value; it will return the effective order of rotation
 * instead.
 *
 * Returns: the order used to apply the rotations
 *
 * Since: 1.2
 */
graphene_euler_order_t
graphene_euler_get_order (const graphene_euler_t *e)
{
  if (e->order == GRAPHENE_EULER_ORDER_DEFAULT)
    return EULER_DEFAULT_ORDER;

  return e->order;
}

/**
 * graphene_euler_to_vec3:
 * @e: a #graphene_euler_t
 * @res: (out caller-allocates): return location for a #graphene_vec3_t
 *
 * Retrieves the angles of a #graphene_euler_t and initializes a
 * #graphene_vec3_t with them.
 *
 * Since: 1.2
 */
void
graphene_euler_to_vec3 (const graphene_euler_t *e,
                        graphene_vec3_t        *res)
{
  graphene_vec3_init_from_vec3 (res, &e->angles);
  graphene_vec3_scale (res, (180.f / GRAPHENE_PI), res);
}

/**
 * graphene_euler_get_alpha:
 * @e: a #graphene_euler_t
 *
 * Retrieves the first component of the Euler angle vector,
 * depending on the order of rotation.
 *
 * See also: graphene_euler_get_x()
 *
 * Returns: the first component of the Euler angle vector, in radians
 *
 * Since: 1.10
 */
float
graphene_euler_get_alpha (const graphene_euler_t *e)
{
  graphene_euler_order_t order = graphene_euler_get_real_order (e->order);

  switch (order)
    {
    case GRAPHENE_EULER_ORDER_SXYZ:
    case GRAPHENE_EULER_ORDER_SXYX:
    case GRAPHENE_EULER_ORDER_SXZY:
    case GRAPHENE_EULER_ORDER_SXZX:
    case GRAPHENE_EULER_ORDER_RXYX:
    case GRAPHENE_EULER_ORDER_RXZX:
    case GRAPHENE_EULER_ORDER_RXZY:
    case GRAPHENE_EULER_ORDER_RXYZ:
      return graphene_vec3_get_x (&e->angles);

    case GRAPHENE_EULER_ORDER_SYZX:
    case GRAPHENE_EULER_ORDER_SYZY:
    case GRAPHENE_EULER_ORDER_SYXZ:
    case GRAPHENE_EULER_ORDER_SYXY:
    case GRAPHENE_EULER_ORDER_RYZX:
    case GRAPHENE_EULER_ORDER_RYZY:
    case GRAPHENE_EULER_ORDER_RYXY:
    case GRAPHENE_EULER_ORDER_RYXZ:
      return graphene_vec3_get_y (&e->angles);

    case GRAPHENE_EULER_ORDER_SZXY:
    case GRAPHENE_EULER_ORDER_SZXZ:
    case GRAPHENE_EULER_ORDER_SZYX:
    case GRAPHENE_EULER_ORDER_SZYZ:
    case GRAPHENE_EULER_ORDER_RZYX:
    case GRAPHENE_EULER_ORDER_RZXY:
    case GRAPHENE_EULER_ORDER_RZXZ:
    case GRAPHENE_EULER_ORDER_RZYZ:
      return graphene_vec3_get_z (&e->angles);

    default:
      break;
    }

  return 0.f;
}

/**
 * graphene_euler_get_beta:
 * @e: a #graphene_euler_t
 *
 * Retrieves the second component of the Euler angle vector,
 * depending on the order of rotation.
 *
 * See also: graphene_euler_get_y()
 *
 * Returns: the second component of the Euler angle vector, in radians
 *
 * Since: 1.10
 */
float
graphene_euler_get_beta (const graphene_euler_t *e)
{
  graphene_euler_order_t order = graphene_euler_get_real_order (e->order);

  switch (order)
    {
    case GRAPHENE_EULER_ORDER_SYXZ:
    case GRAPHENE_EULER_ORDER_SYXY:
    case GRAPHENE_EULER_ORDER_SZXY:
    case GRAPHENE_EULER_ORDER_SZXZ:
    case GRAPHENE_EULER_ORDER_RZXY:
    case GRAPHENE_EULER_ORDER_RYXY:
    case GRAPHENE_EULER_ORDER_RYXZ:
    case GRAPHENE_EULER_ORDER_RZXZ:
      return graphene_vec3_get_x (&e->angles);

    case GRAPHENE_EULER_ORDER_SXYZ:
    case GRAPHENE_EULER_ORDER_SXYX:
    case GRAPHENE_EULER_ORDER_SZYX:
    case GRAPHENE_EULER_ORDER_SZYZ:
    case GRAPHENE_EULER_ORDER_RZYX:
    case GRAPHENE_EULER_ORDER_RXYX:
    case GRAPHENE_EULER_ORDER_RXYZ:
    case GRAPHENE_EULER_ORDER_RZYZ:
      return graphene_vec3_get_y (&e->angles);

    case GRAPHENE_EULER_ORDER_SYZX:
    case GRAPHENE_EULER_ORDER_SYZY:
    case GRAPHENE_EULER_ORDER_SXZY:
    case GRAPHENE_EULER_ORDER_SXZX:
    case GRAPHENE_EULER_ORDER_RYZX:
    case GRAPHENE_EULER_ORDER_RXZX:
    case GRAPHENE_EULER_ORDER_RXZY:
    case GRAPHENE_EULER_ORDER_RYZY:
      return graphene_vec3_get_z (&e->angles);

    default:
      break;
    }

  return 0.f;
}

/**
 * graphene_euler_get_gamma:
 * @e: a #graphene_euler_t
 *
 * Retrieves the third component of the Euler angle vector,
 * depending on the order of rotation.
 *
 * See also: graphene_euler_get_z()
 *
 * Returns: the third component of the Euler angle vector, in radians
 *
 * Since: 1.10
 */
float
graphene_euler_get_gamma (const graphene_euler_t *e)
{
  graphene_euler_order_t order = graphene_euler_get_real_order (e->order);

  switch (order)
    {
    case GRAPHENE_EULER_ORDER_SXYX:
    case GRAPHENE_EULER_ORDER_SZYX:
    case GRAPHENE_EULER_ORDER_SYZX:
    case GRAPHENE_EULER_ORDER_SXZX:
    case GRAPHENE_EULER_ORDER_RZYX:
    case GRAPHENE_EULER_ORDER_RXYX:
    case GRAPHENE_EULER_ORDER_RYZX:
    case GRAPHENE_EULER_ORDER_RXZX:
      return graphene_vec3_get_x (&e->angles);

    case GRAPHENE_EULER_ORDER_SYZY:
    case GRAPHENE_EULER_ORDER_SXZY:
    case GRAPHENE_EULER_ORDER_SYXY:
    case GRAPHENE_EULER_ORDER_SZXY:
    case GRAPHENE_EULER_ORDER_RXZY:
    case GRAPHENE_EULER_ORDER_RYZY:
    case GRAPHENE_EULER_ORDER_RZXY:
    case GRAPHENE_EULER_ORDER_RYXY:
      return graphene_vec3_get_y (&e->angles);

    case GRAPHENE_EULER_ORDER_SZYZ:
    case GRAPHENE_EULER_ORDER_SYXZ:
    case GRAPHENE_EULER_ORDER_SXYZ:
    case GRAPHENE_EULER_ORDER_SZXZ:
    case GRAPHENE_EULER_ORDER_RYXZ:
    case GRAPHENE_EULER_ORDER_RZXZ:
    case GRAPHENE_EULER_ORDER_RXYZ:
    case GRAPHENE_EULER_ORDER_RZYZ:
      return graphene_vec3_get_z (&e->angles);

    default:
      break;
    }

  return 0.f;
}

/**
 * graphene_euler_to_matrix:
 * @e: a #graphene_euler_t
 * @res: (out caller-allocates): return location for a #graphene_matrix_t
 *
 * Converts a #graphene_euler_t into a transformation matrix expressing
 * the extrinsic composition of rotations described by the Euler angles.
 *
 * The rotations are applied over the reference frame axes in the order
 * associated with the #graphene_euler_t; for instance, if the order
 * used to initialize @e is %GRAPHENE_EULER_ORDER_XYZ:
 *
 *  * the first rotation moves the body around the X axis with
 *    an angle φ
 *  * the second rotation moves the body around the Y axis with
 *    an angle of ϑ
 *  * the third rotation moves the body around the Z axis with
 *    an angle of ψ
 *
 * The rotation sign convention is right-handed, to preserve compatibility
 * between Euler-based, quaternion-based, and angle-axis-based rotations.
 *
 * Since: 1.2
 */
void
graphene_euler_to_matrix (const graphene_euler_t *e,
                          graphene_matrix_t      *res)
{
  graphene_euler_order_t order = graphene_euler_get_real_order (e->order);

  /* We need to use the alpha/beta/gamma accessor to account for
   * rotations that replicate the first axis on the last
   */
  float ai = graphene_euler_get_alpha (e);
  float aj = graphene_euler_get_beta (e);
  float ak = graphene_euler_get_gamma (e);

  euler_to_matrix (ai, aj, ak, &order_parameters[ORDER_OFFSET (order)], res);
}

/**
 * graphene_euler_to_quaternion:
 * @e: a #graphene_euler_t
 * @res: (out caller-allocates): return location for a #graphene_quaternion_t
 *
 * Converts a #graphene_euler_t into a #graphene_quaternion_t.
 *
 * Since: 1.10
 */
void
graphene_euler_to_quaternion (const graphene_euler_t *e,
                              graphene_quaternion_t  *res)
{
  float ti = graphene_vec3_get_x (&e->angles) * 0.5f;
  float tj = graphene_vec3_get_y (&e->angles) * 0.5f;
  float tk = graphene_vec3_get_z (&e->angles) * 0.5f;

  float ci, cj, ck;
  float si, sj, sk;

  graphene_sincos (ti, &si, &ci);
  graphene_sincos (tj, &sj, &cj);
  graphene_sincos (tk, &sk, &ck);

  float cc = ci * ck;
  float cs = ci * sk;
  float sc = si * ck;
  float ss = si * sk;

  graphene_euler_order_t order = graphene_euler_get_real_order (e->order);
  const struct axis_param *params = &order_parameters[ORDER_OFFSET (order)];

  if (params->repetition)
    {
      res->x = cj * (cs + cc);
      res->y = sj * (cc + ss);
      res->z = sj * (cs - sc);
      res->w = cj * (cc - ss);
    }
  else
    {
      res->x = cj * sc - sj * cs;
      res->y = cj * ss + sj * cc;
      res->z = cj * cs - sj * sc;
      res->w = cj * cc + sj * ss;
    }

  if (params->parity)
    res->y *= -1.f;
}

/**
 * graphene_euler_reorder:
 * @e: a #graphene_euler_t
 * @order: the new order
 * @res: (out caller-allocates): return location for the reordered
 *   #graphene_euler_t
 *
 * Reorders a #graphene_euler_t using @order.
 *
 * This function is equivalent to creating a #graphene_quaternion_t from the
 * given #graphene_euler_t, and then converting the quaternion into another
 * #graphene_euler_t.
 *
 * Since: 1.2
 */
void
graphene_euler_reorder (const graphene_euler_t *e,
                        graphene_euler_order_t  order,
                        graphene_euler_t       *res)
{
  graphene_quaternion_t q;

  graphene_quaternion_init_from_euler (&q, e);
  graphene_euler_init_from_quaternion (res, &q, graphene_euler_get_real_order (order));
}
