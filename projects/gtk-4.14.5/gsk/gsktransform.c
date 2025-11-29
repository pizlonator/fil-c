/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


/**
 * GskTransform: (ref-func gsk_transform_ref) (unref-func gsk_transform_unref)
 *
 * `GskTransform` is an object to describe transform matrices.
 *
 * Unlike `graphene_matrix_t`, `GskTransform` retains the steps in how
 * a transform was constructed, and allows inspecting them. It is modeled
 * after the way CSS describes transforms.
 *
 * `GskTransform` objects are immutable and cannot be changed after creation.
 * This means code can safely expose them as properties of objects without
 * having to worry about others changing them.
 */

#include "config.h"

#include "gsktransformprivate.h"

/* {{{ Boilerplate */

struct _GskTransformClass
{
  gsize struct_size;
  const char *type_name;

  void                  (* finalize)            (GskTransform           *transform);
  void                  (* to_matrix)           (GskTransform           *transform,
                                                 graphene_matrix_t      *out_matrix);
  void                  (* apply_2d)            (GskTransform           *transform,
                                                 float                  *out_xx,
                                                 float                  *out_yx,
                                                 float                  *out_xy,
                                                 float                  *out_yy,
                                                 float                  *out_dx,
                                                 float                  *out_dy);
  void                  (* apply_affine)        (GskTransform           *transform,
                                                 float                  *out_scale_x,
                                                 float                  *out_scale_y,
                                                 float                  *out_dx,
                                                 float                  *out_dy);
  void                  (* apply_translate)     (GskTransform           *transform,
                                                 float                  *out_dx,
                                                 float                  *out_dy);
  void                  (* print)               (GskTransform           *transform,
                                                 GString                *string);
  GskTransform *        (* apply)               (GskTransform           *transform,
                                                 GskTransform           *apply_to);
  GskTransform *        (* invert)              (GskTransform           *transform,
                                                 GskTransform           *next);
  /* both matrices have the same type */
  gboolean              (* equal)               (GskTransform           *first_transform,
                                                 GskTransform           *second_transform);
};


G_DEFINE_BOXED_TYPE (GskTransform, gsk_transform,
                     gsk_transform_ref,
                     gsk_transform_unref)

static gboolean
gsk_transform_is_identity (GskTransform *self);
static GskTransform *
gsk_transform_matrix_with_category (GskTransform           *next,
                                    const graphene_matrix_t*matrix,
                                    GskTransformCategory    category);

static inline gboolean
gsk_transform_has_class (GskTransform            *self,
                         const GskTransformClass *transform_class)
{
  return self != NULL && self->transform_class == transform_class;
}

/*< private >
 * gsk_transform_alloc:
 * @transform_class: class structure for this self
 * @category: The category of this transform. Will be used to initialize
 *   the result's category together with &next's category
 * @next: (transfer full) (nullable): Next transform to multiply with
 *
 * Returns: (transfer full): the newly created `GskTransform`
 */
static gpointer
gsk_transform_alloc (const GskTransformClass *transform_class,
                     GskTransformCategory     category,
                     GskTransform            *next)
{
  GskTransform *self;

  g_return_val_if_fail (transform_class != NULL, NULL);

  self = g_atomic_rc_box_alloc0 (transform_class->struct_size);

  self->transform_class = transform_class;
  self->category = next ? MIN (category, next->category) : category;
  if (gsk_transform_is_identity (next))
    gsk_transform_unref (next);
  else
    self->next = next;

  return self;
}

static void
gsk_transform_finalize (GskTransform *self)
{
  self->transform_class->finalize (self);

  gsk_transform_unref (self->next);
}

/* }}} */
/* {{{ IDENTITY */

static void
gsk_identity_transform_finalize (GskTransform *transform)
{
}

static void
gsk_identity_transform_to_matrix (GskTransform      *transform,
                                  graphene_matrix_t *out_matrix)
{
  graphene_matrix_init_identity (out_matrix);
}

static void
gsk_identity_transform_apply_2d (GskTransform *transform,
                                 float        *out_xx,
                                 float        *out_yx,
                                 float        *out_xy,
                                 float        *out_yy,
                                 float        *out_dx,
                                 float        *out_dy)
{
}

static void
gsk_identity_transform_apply_affine (GskTransform *transform,
                                     float        *out_scale_x,
                                     float        *out_scale_y,
                                     float        *out_dx,
                                     float        *out_dy)
{
}

static void
gsk_identity_transform_apply_translate (GskTransform *transform,
                                        float        *out_dx,
                                        float        *out_dy)
{
}

static void
gsk_identity_transform_print (GskTransform *transform,
                              GString      *string)
{
  g_string_append (string, "none");
}

static GskTransform *
gsk_identity_transform_apply (GskTransform *transform,
                              GskTransform *apply_to)
{
  /* We do the following to make sure inverting a non-NULL transform
   * will return a non-NULL transform.
   */
  if (apply_to)
    return apply_to;
  else
    return gsk_transform_new ();
}

static GskTransform *
gsk_identity_transform_invert (GskTransform *transform,
                               GskTransform *next)
{
  /* We do the following to make sure inverting a non-NULL transform
   * will return a non-NULL transform.
   */
  if (next)
    return next;
  else
    return gsk_transform_new ();
}

static gboolean
gsk_identity_transform_equal (GskTransform *first_transform,
                              GskTransform *second_transform)
{
  return TRUE;
}

static const GskTransformClass GSK_IDENTITY_TRANSFORM_CLASS =
{
  sizeof (GskTransform),
  "GskIdentityTransform",
  gsk_identity_transform_finalize,
  gsk_identity_transform_to_matrix,
  gsk_identity_transform_apply_2d,
  gsk_identity_transform_apply_affine,
  gsk_identity_transform_apply_translate,
  gsk_identity_transform_print,
  gsk_identity_transform_apply,
  gsk_identity_transform_invert,
  gsk_identity_transform_equal,
};

/*<private>
 * gsk_transform_is_identity:
 * @transform: (nullable): A transform
 *
 * Checks if the transform is a representation of the identity
 * transform.
 *
 * This is different from a transform like `scale(2) scale(0.5)`
 * which just results in an identity transform when simplified.
 *
 * Returns: %TRUE  if this transform is a representation of
 *   the identity transform
 **/
static gboolean
gsk_transform_is_identity (GskTransform *self)
{
  return self == NULL ||
         (self->transform_class == &GSK_IDENTITY_TRANSFORM_CLASS && gsk_transform_is_identity (self->next));
}

/* }}} */
/* {{{ MATRIX */

typedef struct _GskMatrixTransform GskMatrixTransform;

struct _GskMatrixTransform
{
  GskTransform parent;

  graphene_matrix_t matrix;
};

static void
gsk_matrix_transform_finalize (GskTransform *self)
{
}

static void
gsk_matrix_transform_to_matrix (GskTransform      *transform,
                                graphene_matrix_t *out_matrix)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  graphene_matrix_init_from_matrix (out_matrix, &self->matrix);
}

static void 
gsk_matrix_transform_apply_2d (GskTransform *transform,
                               float        *out_xx,
                               float        *out_yx,
                               float        *out_xy,
                               float        *out_yy,
                               float        *out_dx,
                               float        *out_dy)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;
  graphene_matrix_t mat;

  graphene_matrix_init_from_2d (&mat,
                                *out_xx, *out_yx,
                                *out_xy, *out_yy,
                                *out_dx, *out_dy);
  graphene_matrix_multiply (&self->matrix, &mat, &mat);

  /* not using graphene_matrix_to_2d() because it may
   * fail the is_2d() check due to improper rounding */
  *out_xx = graphene_matrix_get_value (&mat, 0, 0);
  *out_yx = graphene_matrix_get_value (&mat, 0, 1);
  *out_xy = graphene_matrix_get_value (&mat, 1, 0);
  *out_yy = graphene_matrix_get_value (&mat, 1, 1);
  *out_dx = graphene_matrix_get_value (&mat, 3, 0);
  *out_dy = graphene_matrix_get_value (&mat, 3, 1);
}

static void
gsk_matrix_transform_apply_affine (GskTransform *transform,
                                   float        *out_scale_x,
                                   float        *out_scale_y,
                                   float        *out_dx,
                                   float        *out_dy)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  switch (transform->category)
  {
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    default:
      g_assert_not_reached ();
      break;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      *out_dx += *out_scale_x * graphene_matrix_get_x_translation (&self->matrix);
      *out_dy += *out_scale_y * graphene_matrix_get_y_translation (&self->matrix);
      *out_scale_x *= graphene_matrix_get_x_scale (&self->matrix);
      *out_scale_y *= graphene_matrix_get_y_scale (&self->matrix);
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      *out_dx += *out_scale_x * graphene_matrix_get_x_translation (&self->matrix);
      *out_dy += *out_scale_y * graphene_matrix_get_y_translation (&self->matrix);
      break;

    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      break;
  }
}

static void
gsk_matrix_transform_apply_translate (GskTransform *transform,
                                      float        *out_dx,
                                      float        *out_dy)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  switch (transform->category)
  {
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
    default:
      g_assert_not_reached ();
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      *out_dx += graphene_matrix_get_x_translation (&self->matrix);
      *out_dy += graphene_matrix_get_y_translation (&self->matrix);
      break;

    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      break;
  }
}

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%g", d);
  g_string_append (string, buf);
}

static void
gsk_matrix_transform_print (GskTransform *transform,
                            GString      *string)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;
  guint i;
  float f[16];

  if (transform->category >= GSK_TRANSFORM_CATEGORY_2D)
    {
      g_string_append (string, "matrix(");
      graphene_matrix_to_float (&self->matrix, f);
      string_append_double (string, f[0]);
      g_string_append (string, ", ");
      string_append_double (string, f[1]);
      g_string_append (string, ", ");
      string_append_double (string, f[4]);
      g_string_append (string, ", ");
      string_append_double (string, f[5]);
      g_string_append (string, ", ");
      string_append_double (string, f[12]);
      g_string_append (string, ", ");
      string_append_double (string, f[13]);
      g_string_append (string, ")");
    }
  else
    {
      g_string_append (string, "matrix3d(");
      graphene_matrix_to_float (&self->matrix, f);
      for (i = 0; i < 16; i++)
        {
          if (i > 0)
            g_string_append (string, ", ");
          string_append_double (string, f[i]);
        }
      g_string_append (string, ")");
    }
}

static GskTransform *
gsk_matrix_transform_apply (GskTransform *transform,
                            GskTransform *apply_to)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  return gsk_transform_matrix_with_category (apply_to,
                                             &self->matrix,
                                             transform->category);
}

static GskTransform *
gsk_matrix_transform_invert (GskTransform *transform,
                             GskTransform *next)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;
  graphene_matrix_t inverse;

  if (!graphene_matrix_inverse (&self->matrix, &inverse))
    {
      gsk_transform_unref (next);
      return NULL;
    }

  return gsk_transform_matrix_with_category (next,
                                             &inverse,
                                             transform->category);
}

static gboolean
gsk_matrix_transform_equal (GskTransform *first_transform,
                            GskTransform *second_transform)
{
  GskMatrixTransform *first = (GskMatrixTransform *) first_transform;
  GskMatrixTransform *second = (GskMatrixTransform *) second_transform;

  if (graphene_matrix_equal_fast (&first->matrix, &second->matrix))
    return TRUE;

  return graphene_matrix_equal (&first->matrix, &second->matrix);
}

static const GskTransformClass GSK_TRANSFORM_TRANSFORM_CLASS =
{
  sizeof (GskMatrixTransform),
  "GskMatrixTransform",
  gsk_matrix_transform_finalize,
  gsk_matrix_transform_to_matrix,
  gsk_matrix_transform_apply_2d,
  gsk_matrix_transform_apply_affine,
  gsk_matrix_transform_apply_translate,
  gsk_matrix_transform_print,
  gsk_matrix_transform_apply,
  gsk_matrix_transform_invert,
  gsk_matrix_transform_equal,
};

static GskTransform *
gsk_transform_matrix_with_category (GskTransform            *next,
                                    const graphene_matrix_t *matrix,
                                    GskTransformCategory     category)
{
  GskMatrixTransform *result = gsk_transform_alloc (&GSK_TRANSFORM_TRANSFORM_CLASS, category, next);

  graphene_matrix_init_from_matrix (&result->matrix, matrix);

  return &result->parent;
}

/**
 * gsk_transform_matrix:
 * @next: (nullable) (transfer full): the next transform
 * @matrix: the matrix to multiply @next with
 *
 * Multiplies @next with the given @matrix.
 *
 * Returns: The new transform
 **/
GskTransform *
gsk_transform_matrix (GskTransform            *next,
                      const graphene_matrix_t *matrix)
{
  return gsk_transform_matrix_with_category (next, matrix, GSK_TRANSFORM_CATEGORY_UNKNOWN);
}

/* }}} */
/* {{{ TRANSLATE */

typedef struct _GskTranslateTransform GskTranslateTransform;

struct _GskTranslateTransform
{
  GskTransform parent;

  graphene_point3d_t point;
};

static void
gsk_translate_transform_finalize (GskTransform *self)
{
}

static void
gsk_translate_transform_to_matrix (GskTransform      *transform,
                                   graphene_matrix_t *out_matrix)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  graphene_matrix_init_translate (out_matrix, &self->point);
}

static void
gsk_translate_transform_apply_2d (GskTransform *transform,
                                  float        *out_xx,
                                  float        *out_yx,
                                  float        *out_xy,
                                  float        *out_yy,
                                  float        *out_dx,
                                  float        *out_dy)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  g_assert (self->point.z == 0.0);

  *out_dx += *out_xx * self->point.x + *out_xy * self->point.y;
  *out_dy += *out_yx * self->point.x + *out_yy * self->point.y;
}

static void
gsk_translate_transform_apply_affine (GskTransform *transform,
                                      float        *out_scale_x,
                                      float        *out_scale_y,
                                      float        *out_dx,
                                      float        *out_dy)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  g_assert (self->point.z == 0.0);

  *out_dx += *out_scale_x * self->point.x;
  *out_dy += *out_scale_y * self->point.y;
}

static void
gsk_translate_transform_apply_translate (GskTransform *transform,
                                         float        *out_dx,
                                         float        *out_dy)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  g_assert (self->point.z == 0.0);

  *out_dx += self->point.x;
  *out_dy += self->point.y;
}

static GskTransform *
gsk_translate_transform_apply (GskTransform *transform,
                               GskTransform *apply_to)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  return gsk_transform_translate_3d (apply_to, &self->point);
}

static GskTransform *
gsk_translate_transform_invert (GskTransform *transform,
                                GskTransform *next)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  return gsk_transform_translate_3d (next, &GRAPHENE_POINT3D_INIT (-self->point.x, -self->point.y, -self->point.z));
}

static gboolean
gsk_translate_transform_equal (GskTransform *first_transform,
                               GskTransform *second_transform)
{
  GskTranslateTransform *first = (GskTranslateTransform *) first_transform;
  GskTranslateTransform *second = (GskTranslateTransform *) second_transform;

  return G_APPROX_VALUE (first->point.x, second->point.x, FLT_EPSILON) &&
         G_APPROX_VALUE (first->point.y, second->point.y, FLT_EPSILON) &&
         G_APPROX_VALUE (first->point.z, second->point.z, FLT_EPSILON);
}

static void
gsk_translate_transform_print (GskTransform *transform,
                               GString      *string)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  if (self->point.z == 0)
    g_string_append (string, "translate(");
  else
    g_string_append (string, "translate3d(");

  string_append_double (string, self->point.x);
  g_string_append (string, ", ");
  string_append_double (string, self->point.y);
  if (self->point.z != 0)
    {
      g_string_append (string, ", ");
      string_append_double (string, self->point.z);
    }
  g_string_append (string, ")");
}

static const GskTransformClass GSK_TRANSLATE_TRANSFORM_CLASS =
{
  sizeof (GskTranslateTransform),
  "GskTranslateTransform",
  gsk_translate_transform_finalize,
  gsk_translate_transform_to_matrix,
  gsk_translate_transform_apply_2d,
  gsk_translate_transform_apply_affine,
  gsk_translate_transform_apply_translate,
  gsk_translate_transform_print,
  gsk_translate_transform_apply,
  gsk_translate_transform_invert,
  gsk_translate_transform_equal,
};

/**
 * gsk_transform_translate:
 * @next: (nullable) (transfer full): the next transform
 * @point: the point to translate the transform by
 *
 * Translates @next in 2-dimensional space by @point.
 *
 * Returns: (nullable): The new transform
 **/
GskTransform *
gsk_transform_translate (GskTransform           *next,
                         const graphene_point_t *point)
{
  graphene_point3d_t point3d;

  graphene_point3d_init (&point3d, point->x, point->y, 0);

  return gsk_transform_translate_3d (next, &point3d);
}

/**
 * gsk_transform_translate_3d:
 * @next: (nullable) (transfer full): the next transform
 * @point: the point to translate the transform by
 *
 * Translates @next by @point.
 *
 * Returns: (nullable): The new transform
 **/
GskTransform *
gsk_transform_translate_3d (GskTransform             *next,
                            const graphene_point3d_t *point)
{
  GskTranslateTransform *result;

  if (graphene_point3d_equal (point, graphene_point3d_zero ()))
    return next;

  if (gsk_transform_has_class (next, &GSK_TRANSLATE_TRANSFORM_CLASS))
    {
      GskTranslateTransform *t = (GskTranslateTransform *) next;
      GskTransform *r = gsk_transform_translate_3d (gsk_transform_ref (next->next),
                                                    &GRAPHENE_POINT3D_INIT(t->point.x + point->x,
                                                                           t->point.y + point->y,
                                                                           t->point.z + point->z));
      gsk_transform_unref (next);
      return r;
    }

  result = gsk_transform_alloc (&GSK_TRANSLATE_TRANSFORM_CLASS,
                                point->z == 0.0 ? GSK_TRANSFORM_CATEGORY_2D_TRANSLATE
                                                : GSK_TRANSFORM_CATEGORY_3D,
                                next);

  graphene_point3d_init_from_point (&result->point, point);

  return &result->parent;
}

/* }}} */
/* {{{ ROTATE */

typedef struct _GskRotateTransform GskRotateTransform;

struct _GskRotateTransform
{
  GskTransform parent;

  float angle;
};

static void
gsk_rotate_transform_finalize (GskTransform *self)
{
}

static inline void
_sincos (float  deg,
         float *out_s,
         float *out_c)
{
  if (deg == 90.0)
    {
      *out_c = 0.0;
      *out_s = 1.0;
    }
  else if (deg == 180.0)
    {
      *out_c = -1.0;
      *out_s = 0.0;
    }
  else if (deg == 270.0)
    {
      *out_c = 0.0;
      *out_s = -1.0;
    }
  else if (deg == 0.0)
    {
      *out_c = 1.0;
      *out_s = 0.0;
    }
  else
    {
      float angle = deg * M_PI / 180.0;

#ifdef HAVE_SINCOSF
      sincosf (angle, out_s, out_c);
#else
      *out_s = sinf (angle);
      *out_c = cosf (angle);
#endif

    }
}

static void
gsk_rotate_transform_to_matrix (GskTransform      *transform,
                                graphene_matrix_t *out_matrix)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;
  float c, s;

  _sincos (self->angle, &s, &c);

  graphene_matrix_init_from_2d (out_matrix,
                                c, s,
                                -s, c,
                                0, 0);
}

static void
gsk_rotate_transform_apply_2d (GskTransform *transform,
                               float        *out_xx,
                               float        *out_yx,
                               float        *out_xy,
                               float        *out_yy,
                               float        *out_dx,
                               float        *out_dy)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;
  float s, c, xx, xy, yx, yy;

  _sincos (self->angle, &s, &c);

  xx =  c * *out_xx + s * *out_xy;
  yx =  c * *out_yx + s * *out_yy;
  xy = -s * *out_xx + c * *out_xy;
  yy = -s * *out_yx + c * *out_yy;

  *out_xx = xx;
  *out_yx = yx;
  *out_xy = xy;
  *out_yy = yy;
}

static GskTransform *
gsk_rotate_transform_apply (GskTransform *transform,
                            GskTransform *apply_to)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;

  return gsk_transform_rotate (apply_to, self->angle);
}

static GskTransform *
gsk_rotate_transform_invert (GskTransform *transform,
                             GskTransform *next)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;

  return gsk_transform_rotate (next, - self->angle);
}

static gboolean
gsk_rotate_transform_equal (GskTransform *first_transform,
                            GskTransform *second_transform)
{
  GskRotateTransform *first = (GskRotateTransform *) first_transform;
  GskRotateTransform *second = (GskRotateTransform *) second_transform;

  return G_APPROX_VALUE (first->angle, second->angle, 0.01f);
}

static void
gsk_rotate_transform_print (GskTransform *transform,
                            GString      *string)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;

  g_string_append (string, "rotate(");
  string_append_double (string, self->angle);
  g_string_append (string, ")");
}

static const GskTransformClass GSK_ROTATE_TRANSFORM_CLASS =
{
  sizeof (GskRotateTransform),
  "GskRotateTransform",
  gsk_rotate_transform_finalize,
  gsk_rotate_transform_to_matrix,
  gsk_rotate_transform_apply_2d,
  NULL,
  NULL,
  gsk_rotate_transform_print,
  gsk_rotate_transform_apply,
  gsk_rotate_transform_invert,
  gsk_rotate_transform_equal,
};

static inline float
normalize_angle (float angle)
{

  if (angle >= 0 && angle < 360)
    return angle;

  while (angle >= 360)
    angle -= 360;
  while (angle < 0)
    angle += 360;

  /* Due to precision issues we may end up with a result that is just
   * past the allowed range when rounded. For example, something like
   * -epsilon + 360 when rounded to a float may end up with 360.
   * So, we handle these cases by returning the exact value 0.
   */

  if (angle >= 360)
    angle = 0;

  g_assert (angle < 360.0);
  g_assert (angle >= 0.0);

  return angle;
}

/**
 * gsk_transform_rotate:
 * @next: (nullable) (transfer full): the next transform
 * @angle: the rotation angle, in degrees (clockwise)
 *
 * Rotates @next @angle degrees in 2D - or in 3D-speak, around the Z axis.
 * The rotation happens around the origin point of (0, 0).
 *
 * Returns: (nullable): The new transform
 */
GskTransform *
gsk_transform_rotate (GskTransform *next,
                      float         angle)
{
  GskRotateTransform *result;

  if (angle == 0.0f)
    return next;

  if (gsk_transform_has_class (next, &GSK_ROTATE_TRANSFORM_CLASS))
    {
      GskTransform *r  = gsk_transform_rotate (gsk_transform_ref (next->next),
                                               ((GskRotateTransform *) next)->angle + angle);
      gsk_transform_unref (next);
      return r;
    }

  result = gsk_transform_alloc (&GSK_ROTATE_TRANSFORM_CLASS,
                                GSK_TRANSFORM_CATEGORY_2D,
                                next);

  result->angle = normalize_angle (angle);

  return &result->parent;
}

/* }}} */
/* {{{ ROTATE 3D */

typedef struct _GskRotate3dTransform GskRotate3dTransform;

struct _GskRotate3dTransform
{
  GskTransform parent;

  float angle;
  graphene_vec3_t axis;
};

static void
gsk_rotate3d_transform_finalize (GskTransform *self)
{
}

static void
gsk_rotate3d_transform_to_matrix (GskTransform      *transform,
                                  graphene_matrix_t *out_matrix)
{
  GskRotate3dTransform *self = (GskRotate3dTransform *) transform;

  graphene_matrix_init_rotate (out_matrix, self->angle, &self->axis);
}

static GskTransform *
gsk_rotate3d_transform_apply (GskTransform *transform,
                              GskTransform *apply_to)
{
  GskRotate3dTransform *self = (GskRotate3dTransform *) transform;

  return gsk_transform_rotate_3d (apply_to, self->angle, &self->axis);
}

static GskTransform *
gsk_rotate3d_transform_invert (GskTransform *transform,
                               GskTransform *next)
{
  GskRotate3dTransform *self = (GskRotate3dTransform *) transform;

  return gsk_transform_rotate_3d (next, - self->angle, &self->axis);
}

static gboolean
gsk_rotate3d_transform_equal (GskTransform *first_transform,
                              GskTransform *second_transform)
{
  GskRotate3dTransform *first = (GskRotate3dTransform *) first_transform;
  GskRotate3dTransform *second = (GskRotate3dTransform *) second_transform;

  return G_APPROX_VALUE (first->angle, second->angle, 0.01f) &&
         graphene_vec3_equal (&first->axis, &second->axis);
}

static void
gsk_rotate3d_transform_print (GskTransform *transform,
                            GString      *string)
{
  GskRotate3dTransform *self = (GskRotate3dTransform *) transform;
  float f[3];
  guint i;

  g_string_append (string, "rotate3d(");
  graphene_vec3_to_float (&self->axis, f);
  for (i = 0; i < 3; i++)
    {
      string_append_double (string, f[i]);
      g_string_append (string, ", ");
    }
  string_append_double (string, self->angle);
  g_string_append (string, ")");
}

static const GskTransformClass GSK_ROTATE3D_TRANSFORM_CLASS =
{
  sizeof (GskRotate3dTransform),
  "GskRotate3dTransform",
  gsk_rotate3d_transform_finalize,
  gsk_rotate3d_transform_to_matrix,
  NULL,
  NULL,
  NULL,
  gsk_rotate3d_transform_print,
  gsk_rotate3d_transform_apply,
  gsk_rotate3d_transform_invert,
  gsk_rotate3d_transform_equal,
};

/**
 * gsk_transform_rotate_3d:
 * @next: (nullable) (transfer full): the next transform
 * @angle: the rotation angle, in degrees (clockwise)
 * @axis: The rotation axis
 *
 * Rotates @next @angle degrees around @axis.
 *
 * For a rotation in 2D space, use [method@Gsk.Transform.rotate]
 *
 * Returns: (nullable): The new transform
 */
GskTransform *
gsk_transform_rotate_3d (GskTransform          *next,
                         float                  angle,
                         const graphene_vec3_t *axis)
{
  GskRotate3dTransform *result;

  if (graphene_vec3_get_x (axis) == 0.0 && graphene_vec3_get_y (axis) == 0.0)
    return gsk_transform_rotate (next, angle);

  if (angle == 0.0f)
    return next;

  result = gsk_transform_alloc (&GSK_ROTATE3D_TRANSFORM_CLASS,
                                GSK_TRANSFORM_CATEGORY_3D,
                                next);

  result->angle = normalize_angle (angle);
  graphene_vec3_init_from_vec3 (&result->axis, axis);

  return &result->parent;
}

/* }}} */
/* {{{ SKEW */

typedef struct _GskSkewTransform GskSkewTransform;

struct _GskSkewTransform
{
  GskTransform parent;

  float skew_x;
  float skew_y;
};

static void
gsk_skew_transform_finalize (GskTransform *self)
{
}

#define DEG_TO_RAD(x) ((x) / 180.f * G_PI)
#define RAD_TO_DEG(x) ((x) * 180.f / G_PI)

static void
gsk_skew_transform_to_matrix (GskTransform      *transform,
                              graphene_matrix_t *out_matrix)
{
  GskSkewTransform *self = (GskSkewTransform *) transform;

  graphene_matrix_init_skew (out_matrix,
                             DEG_TO_RAD (self->skew_x),
                             DEG_TO_RAD (self->skew_y));
}

static void
gsk_skew_transform_apply_2d (GskTransform *transform,
                             float        *out_xx,
                             float        *out_yx,
                             float        *out_xy,
                             float        *out_yy,
                             float        *out_dx,
                             float        *out_dy)
{
  graphene_matrix_t sm, mat;

  gsk_skew_transform_to_matrix (transform, &sm);
  graphene_matrix_init_from_2d (&mat, *out_xx, *out_yx,
                                      *out_xy, *out_yy,
                                      *out_dx, *out_dy);

  graphene_matrix_multiply (&sm, &mat, &mat);

  *out_xx = graphene_matrix_get_value (&mat, 0, 0);
  *out_yx = graphene_matrix_get_value (&mat, 0, 1);
  *out_xy = graphene_matrix_get_value (&mat, 1, 0);
  *out_yy = graphene_matrix_get_value (&mat, 1, 1);
  *out_dx = graphene_matrix_get_value (&mat, 3, 0);
  *out_dy = graphene_matrix_get_value (&mat, 3, 1);
}

static GskTransform *
gsk_skew_transform_apply (GskTransform *transform,
                          GskTransform *apply_to)
{
  GskSkewTransform *self = (GskSkewTransform *) transform;

  return gsk_transform_skew (apply_to, self->skew_x, self->skew_y);
}

static void
gsk_skew_transform_print (GskTransform *transform,
                          GString      *string)
{
  GskSkewTransform *self = (GskSkewTransform *) transform;

  if (self->skew_y == 0)
    {
      g_string_append (string, "skewX(");
      string_append_double (string, self->skew_x);
      g_string_append (string, ")");
    }
  else if (self->skew_x == 0)
    {
      g_string_append (string, "skewY(");
      string_append_double (string, self->skew_y);
      g_string_append (string, ")");
    }
  else
    {
      g_string_append (string, "skew(");
      string_append_double (string, self->skew_x);
      g_string_append (string, ", ");
      string_append_double (string, self->skew_y);
      g_string_append (string, ")");
    }
}

static GskTransform *
gsk_skew_transform_invert (GskTransform *transform,
                           GskTransform *next)
{
  GskSkewTransform *self = (GskSkewTransform *) transform;
  float tx, ty;
  graphene_matrix_t matrix;

  tx = tanf (DEG_TO_RAD (self->skew_x));
  ty = tanf (DEG_TO_RAD (self->skew_y));

  graphene_matrix_init_from_2d (&matrix,
                                1 / (1 - tx * ty),
                                - ty / (1 - tx * ty),
                                - tx / (1 - tx * ty),
                                1 / (1 - tx * ty),
                                0, 0);
  return gsk_transform_matrix_with_category (next,
                                             &matrix,
                                             GSK_TRANSFORM_CATEGORY_2D);
}

static gboolean
gsk_skew_transform_equal (GskTransform *first_transform,
                          GskTransform *second_transform)
{
  GskSkewTransform *first = (GskSkewTransform *) first_transform;
  GskSkewTransform *second = (GskSkewTransform *) second_transform;

  return G_APPROX_VALUE (first->skew_x, second->skew_x, FLT_EPSILON) &&
         G_APPROX_VALUE (first->skew_y, second->skew_y, FLT_EPSILON);
}

static const GskTransformClass GSK_SKEW_TRANSFORM_CLASS =
{
  sizeof (GskSkewTransform),
  "GskSkewTransform",
  gsk_skew_transform_finalize,
  gsk_skew_transform_to_matrix,
  gsk_skew_transform_apply_2d,
  NULL,
  NULL,
  gsk_skew_transform_print,
  gsk_skew_transform_apply,
  gsk_skew_transform_invert,
  gsk_skew_transform_equal,
};

/**
 * gsk_transform_skew:
 * @next: (nullable) (transfer full): the next transform
 * @skew_x: skew factor, in degrees, on the X axis
 * @skew_y: skew factor, in degrees, on the Y axis
 *
 * Applies a skew transform.
 *
 * Returns: (nullable): The new transform
 *
 * Since: 4.6
 */
GskTransform *
gsk_transform_skew (GskTransform *next,
                    float         skew_x,
                    float         skew_y)
{
  GskSkewTransform *result;

  if (skew_x == 0 && skew_y == 0)
    return next;

  result = gsk_transform_alloc (&GSK_SKEW_TRANSFORM_CLASS,
                                GSK_TRANSFORM_CATEGORY_2D,
                                next);

  result->skew_x = skew_x;
  result->skew_y = skew_y;

  return &result->parent;
}
/* }}} */
/*  {{{ SCALE */

typedef struct _GskScaleTransform GskScaleTransform;

struct _GskScaleTransform
{
  GskTransform parent;

  float factor_x;
  float factor_y;
  float factor_z;
};

static void
gsk_scale_transform_finalize (GskTransform *self)
{
}

static void
gsk_scale_transform_to_matrix (GskTransform      *transform,
                               graphene_matrix_t *out_matrix)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  graphene_matrix_init_scale (out_matrix, self->factor_x, self->factor_y, self->factor_z);
}

static void
gsk_scale_transform_apply_2d (GskTransform *transform,
                              float        *out_xx,
                              float        *out_yx,
                              float        *out_xy,
                              float        *out_yy,
                              float        *out_dx,
                              float        *out_dy)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  g_assert (self->factor_z == 1.0);

  *out_xx *= self->factor_x;
  *out_yx *= self->factor_x;
  *out_xy *= self->factor_y;
  *out_yy *= self->factor_y;
}

static void
gsk_scale_transform_apply_affine (GskTransform *transform,
                                  float        *out_scale_x,
                                  float        *out_scale_y,
                                  float        *out_dx,
                                  float        *out_dy)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  g_assert (self->factor_z == 1.0);

  *out_scale_x *= self->factor_x;
  *out_scale_y *= self->factor_y;
}

static GskTransform *
gsk_scale_transform_apply (GskTransform *transform,
                           GskTransform *apply_to)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  return gsk_transform_scale_3d (apply_to, self->factor_x, self->factor_y, self->factor_z);
}

static GskTransform *
gsk_scale_transform_invert (GskTransform *transform,
                            GskTransform *next)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  return gsk_transform_scale_3d (next,
                                 1.f / self->factor_x,
                                 1.f / self->factor_y,
                                 1.f / self->factor_z);
}

static gboolean
gsk_scale_transform_equal (GskTransform *first_transform,
                           GskTransform *second_transform)
{
  GskScaleTransform *first = (GskScaleTransform *) first_transform;
  GskScaleTransform *second = (GskScaleTransform *) second_transform;

  return G_APPROX_VALUE (first->factor_x, second->factor_x, FLT_EPSILON) &&
         G_APPROX_VALUE (first->factor_y, second->factor_y, FLT_EPSILON) &&
         G_APPROX_VALUE (first->factor_z, second->factor_z, FLT_EPSILON);
}

static void
gsk_scale_transform_print (GskTransform *transform,
                           GString      *string)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  if (self->factor_z == 1.0)
    {
      g_string_append (string, "scale(");
      string_append_double (string, self->factor_x);
      if (self->factor_x != self->factor_y)
        {
          g_string_append (string, ", ");
          string_append_double (string, self->factor_y);
        }
      g_string_append (string, ")");
    }
  else
    {
      g_string_append (string, "scale3d(");
      string_append_double (string, self->factor_x);
      g_string_append (string, ", ");
      string_append_double (string, self->factor_y);
      g_string_append (string, ", ");
      string_append_double (string, self->factor_z);
      g_string_append (string, ")");
    }
}

static const GskTransformClass GSK_SCALE_TRANSFORM_CLASS =
{
  sizeof (GskScaleTransform),
  "GskScaleTransform",
  gsk_scale_transform_finalize,
  gsk_scale_transform_to_matrix,
  gsk_scale_transform_apply_2d,
  gsk_scale_transform_apply_affine,
  NULL,
  gsk_scale_transform_print,
  gsk_scale_transform_apply,
  gsk_scale_transform_invert,
  gsk_scale_transform_equal,
};

/**
 * gsk_transform_scale:
 * @next: (nullable) (transfer full): the next transform
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 *
 * Scales @next in 2-dimensional space by the given factors.
 *
 * Use [method@Gsk.Transform.scale_3d] to scale in all 3 dimensions.
 *
 * Returns: (nullable): The new transform
 **/
GskTransform *
gsk_transform_scale (GskTransform *next,
                     float         factor_x,
                     float         factor_y)
{
  return gsk_transform_scale_3d (next, factor_x, factor_y, 1.0);
}

/**
 * gsk_transform_scale_3d:
 * @next: (nullable) (transfer full): the next transform
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 * @factor_z: scaling factor on the Z axis
 *
 * Scales @next by the given factors.
 *
 * Returns: (nullable): The new transform
 **/
GskTransform *
gsk_transform_scale_3d (GskTransform *next,
                        float         factor_x,
                        float         factor_y,
                        float         factor_z)
{
  GskScaleTransform *result;

  if (factor_x == 1 && factor_y == 1 && factor_z == 1)
    return next;

  if (gsk_transform_has_class (next, &GSK_SCALE_TRANSFORM_CLASS))
    {
      GskScaleTransform *scale = (GskScaleTransform *) next;
      GskTransform *r = gsk_transform_scale_3d (gsk_transform_ref (next->next),
                                                scale->factor_x * factor_x,
                                                scale->factor_y * factor_y,
                                                scale->factor_z * factor_z);
      gsk_transform_unref (next);
      return r;
    }

  result = gsk_transform_alloc (&GSK_SCALE_TRANSFORM_CLASS,
                                factor_z != 1.0 ? GSK_TRANSFORM_CATEGORY_3D
                                                : GSK_TRANSFORM_CATEGORY_2D_AFFINE,
                                next);

  result->factor_x = factor_x;
  result->factor_y = factor_y;
  result->factor_z = factor_z;

  return &result->parent;
}

/* }}} */
/* {{{ PERSPECTIVE */

typedef struct _GskPerspectiveTransform GskPerspectiveTransform;

struct _GskPerspectiveTransform
{
  GskTransform parent;

  float depth;
};

static void
gsk_perspective_transform_finalize (GskTransform *self)
{
}

static void
gsk_perspective_transform_to_matrix (GskTransform      *transform,
                                     graphene_matrix_t *out_matrix)
{
  GskPerspectiveTransform *self = (GskPerspectiveTransform *) transform;
  float f[16] = { 1.f, 0.f, 0.f,  0.f,
                  0.f, 1.f, 0.f,  0.f,
                  0.f, 0.f, 1.f, self->depth ? -1.f / self->depth : 0.f,
                  0.f, 0.f, 0.f,  1.f };

  graphene_matrix_init_from_float (out_matrix, f);
}


static GskTransform *
gsk_perspective_transform_apply (GskTransform *transform,
                                 GskTransform *apply_to)
{
  GskPerspectiveTransform *self = (GskPerspectiveTransform *) transform;

  return gsk_transform_perspective (apply_to, self->depth);
}

static GskTransform *
gsk_perspective_transform_invert (GskTransform *transform,
                                  GskTransform *next)
{
  GskPerspectiveTransform *self = (GskPerspectiveTransform *) transform;

  return gsk_transform_perspective (next, - self->depth);
}

static gboolean
gsk_perspective_transform_equal (GskTransform *first_transform,
                                 GskTransform *second_transform)
{
  GskPerspectiveTransform *first = (GskPerspectiveTransform *) first_transform;
  GskPerspectiveTransform *second = (GskPerspectiveTransform *) second_transform;

  return G_APPROX_VALUE (first->depth, second->depth, 0.001f);
}

static void
gsk_perspective_transform_print (GskTransform *transform,
                                 GString      *string)
{
  GskPerspectiveTransform *self = (GskPerspectiveTransform *) transform;

  g_string_append (string, "perspective(");
  string_append_double (string, self->depth);
  g_string_append (string, ")");
}

static const GskTransformClass GSK_PERSPECTIVE_TRANSFORM_CLASS =
{
  sizeof (GskPerspectiveTransform),
  "GskPerspectiveTransform",
  gsk_perspective_transform_finalize,
  gsk_perspective_transform_to_matrix,
  NULL,
  NULL,
  NULL,
  gsk_perspective_transform_print,
  gsk_perspective_transform_apply,
  gsk_perspective_transform_invert,
  gsk_perspective_transform_equal,
};

/**
 * gsk_transform_perspective:
 * @next: (nullable) (transfer full): the next transform
 * @depth: distance of the z=0 plane. Lower values give a more
 *   flattened pyramid and therefore a more pronounced
 *   perspective effect.
 *
 * Applies a perspective projection transform.
 *
 * This transform scales points in X and Y based on their Z value,
 * scaling points with positive Z values away from the origin, and
 * those with negative Z values towards the origin. Points
 * on the z=0 plane are unchanged.
 *
 * Returns: The new transform
 */
GskTransform *
gsk_transform_perspective (GskTransform *next,
                           float         depth)
{
  GskPerspectiveTransform *result;

  if (gsk_transform_has_class (next, &GSK_PERSPECTIVE_TRANSFORM_CLASS))
    {
      GskTransform *r = gsk_transform_perspective (gsk_transform_ref (next->next),
                                                   ((GskPerspectiveTransform *) next)->depth + depth);
      gsk_transform_unref (next);
      return r;
    }

  result = gsk_transform_alloc (&GSK_PERSPECTIVE_TRANSFORM_CLASS,
                                GSK_TRANSFORM_CATEGORY_ANY,
                                next);

  result->depth = depth;

  return &result->parent;
}

/* }}} */
/* {{{ PUBLIC API */

/**
 * gsk_transform_ref:
 * @self: (nullable): a `GskTransform`
 *
 * Acquires a reference on the given `GskTransform`.
 *
 * Returns: (nullable) (transfer none): the `GskTransform` with an additional reference
 */
GskTransform *
gsk_transform_ref (GskTransform *self)
{
  if (self == NULL)
    return NULL;

  return g_atomic_rc_box_acquire (self);
}

/**
 * gsk_transform_unref:
 * @self: (nullable): a `GskTransform`
 *
 * Releases a reference on the given `GskTransform`.
 *
 * If the reference was the last, the resources associated to the @self are
 * freed.
 */
void
gsk_transform_unref (GskTransform *self)
{
  if (self == NULL)
    return;

  g_atomic_rc_box_release_full (self, (GDestroyNotify) gsk_transform_finalize);
}

/**
 * gsk_transform_print:
 * @self: (nullable): a `GskTransform`
 * @string:  The string to print into
 *
 * Converts @self into a human-readable string representation suitable
 * for printing.
 *
 * The result of this function can later be parsed with
 * [func@Gsk.Transform.parse].
 */
void
gsk_transform_print (GskTransform *self,
                     GString      *string)
{
  g_return_if_fail (string != NULL);

  if (self == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  if (self->next != NULL)
    {
      gsk_transform_print (self->next, string);
      g_string_append (string, " ");
    }

  self->transform_class->print (self, string);
}

/**
 * gsk_transform_to_string:
 * @self: (nullable): a `GskTransform`
 *
 * Converts a matrix into a string that is suitable for printing.
 *
 * The resulting string can be parsed with [func@Gsk.Transform.parse].
 *
 * This is a wrapper around [method@Gsk.Transform.print].
 *
 * Returns: A new string for @self
 */
char *
gsk_transform_to_string (GskTransform *self)
{
  GString *string;

  string = g_string_new ("");

  gsk_transform_print (self, string);

  return g_string_free (string, FALSE);
}

/**
 * gsk_transform_to_matrix:
 * @self: (nullable): a `GskTransform`
 * @out_matrix: (out caller-allocates): The matrix to set
 *
 * Computes the actual value of @self and stores it in @out_matrix.
 *
 * The previous value of @out_matrix will be ignored.
 */
void
gsk_transform_to_matrix (GskTransform      *self,
                         graphene_matrix_t *out_matrix)
{
  graphene_matrix_t m;

  if (self == NULL)
    {
      graphene_matrix_init_identity (out_matrix);
      return;
    }

  gsk_transform_to_matrix (self->next, out_matrix);
  self->transform_class->to_matrix (self, &m);
  graphene_matrix_multiply (&m, out_matrix, out_matrix);
}

/**
 * gsk_transform_to_2d:
 * @self: a 2D `GskTransform`
 * @out_xx: (out): return location for the xx member
 * @out_yx: (out): return location for the yx member
 * @out_xy: (out): return location for the xy member
 * @out_yy: (out): return location for the yy member
 * @out_dx: (out): return location for the x0 member
 * @out_dy: (out): return location for the y0 member
 *
 * Converts a `GskTransform` to a 2D transformation matrix.
 *
 * @self must be a 2D transformation. If you are not
 * sure, use gsk_transform_get_category() >=
 * %GSK_TRANSFORM_CATEGORY_2D to check.
 *
 * The returned values have the following layout:
 *
 * ```
 *   | xx yx |   |  a  b  0 |
 *   | xy yy | = |  c  d  0 |
 *   | dx dy |   | tx ty  1 |
 * ```
 *
 * This function can be used to convert between a `GskTransform`
 * and a matrix type from other 2D drawing libraries, in particular
 * Cairo.
 */
void
gsk_transform_to_2d (GskTransform *self,
                     float        *out_xx,
                     float        *out_yx,
                     float        *out_xy,
                     float        *out_yy,
                     float        *out_dx,
                     float        *out_dy)
{
  *out_xx = 1.0f;
  *out_yx = 0.0f;
  *out_xy = 0.0f;
  *out_yy = 1.0f;
  *out_dx = 0.0f;
  *out_dy = 0.0f;

  if (self == NULL)
    return;

  if (G_UNLIKELY (self->category < GSK_TRANSFORM_CATEGORY_2D))
    {
      char *s = gsk_transform_to_string (self);
      g_warning ("Given transform \"%s\" is not a 2D transform.", s);
      g_free (s);
      return;
    }

  gsk_transform_to_2d (self->next,
                       out_xx, out_yx,
                       out_xy, out_yy,
                       out_dx, out_dy);

  self->transform_class->apply_2d (self,
                                   out_xx, out_yx,
                                   out_xy, out_yy,
                                   out_dx, out_dy);
}

/**
 * gsk_transform_to_2d_components:
 * @self: a `GskTransform`
 * @out_skew_x: (out): return location for the skew factor
 *   in the  x direction
 * @out_skew_y: (out): return location for the skew factor
 *   in the  y direction
 * @out_scale_x: (out): return location for the scale
 *   factor in the x direction
 * @out_scale_y: (out): return location for the scale
 *   factor in the y direction
 * @out_angle: (out): return location for the rotation angle
 * @out_dx: (out): return location for the translation
 *   in the x direction
 * @out_dy: (out): return location for the translation
 *   in the y direction
 *
 * Converts a `GskTransform` to 2D transformation factors.
 *
 * To recreate an equivalent transform from the factors returned
 * by this function, use
 *
 *     gsk_transform_skew (
 *         gsk_transform_scale (
 *             gsk_transform_rotate (
 *                 gsk_transform_translate (NULL, &GRAPHENE_POINT_T (dx, dy)),
 *                 angle),
 *             scale_x, scale_y),
 *         skew_x, skew_y)
 *
 * @self must be a 2D transformation. If you are not sure, use
 *
 *     gsk_transform_get_category() >= %GSK_TRANSFORM_CATEGORY_2D
 *
 * to check.
 *
 * Since: 4.6
 */
void
gsk_transform_to_2d_components (GskTransform *self,
                                float        *out_skew_x,
                                float        *out_skew_y,
                                float        *out_scale_x,
                                float        *out_scale_y,
                                float        *out_angle,
                                float        *out_dx,
                                float        *out_dy)
{
  float a, b, c, d, e, f;

  gsk_transform_to_2d (self, &a, &b, &c, &d, &e, &f);

  *out_dx = e;
  *out_dy = f;

#define sign(f) ((f) < 0 ? -1 : 1)

  if (a != 0 || b != 0)
    {
      float det = a * d - b * c;
      float r = sqrtf (a*a + b*b);

      *out_angle = RAD_TO_DEG (sign (b) * acosf (a / r));
      *out_scale_x = r;
      *out_scale_y = det / r;
      *out_skew_x = RAD_TO_DEG (atanf ((a*c + b*d) / (r*r)));
      *out_skew_y = 0;
    }
  else if (c != 0 || d != 0)
    {
      float det = a * d - b * c;
      float s = sqrtf (c*c + d*d);

      *out_angle = RAD_TO_DEG (G_PI/2 - sign (d) * acosf (-c / s));
      *out_scale_x = det / s;
      *out_scale_y = s;
      *out_skew_x = 0;
      *out_skew_y = RAD_TO_DEG (atanf ((a*c + b*d) / (s*s)));
    }
  else
    {
      *out_angle = 0;
      *out_scale_x = 0;
      *out_scale_y = 0;
      *out_skew_x = 0;
      *out_skew_y = 0;
    }
}

/**
 * gsk_transform_to_affine:
 * @self: a `GskTransform`
 * @out_scale_x: (out): return location for the scale
 *   factor in the x direction
 * @out_scale_y: (out): return location for the scale
 *   factor in the y direction
 * @out_dx: (out): return location for the translation
 *   in the x direction
 * @out_dy: (out): return location for the translation
 *   in the y direction
 *
 * Converts a `GskTransform` to 2D affine transformation factors.
 *
 * To recreate an equivalent transform from the factors returned
 * by this function, use
 *
 *     gsk_transform_scale (gsk_transform_translate (NULL,
 *                                                   &GRAPHENE_POINT_T (dx, dy)),
 *                          sx, sy)
 *
 * @self must be a 2D affine transformation. If you are not
 * sure, use
 *
 *     gsk_transform_get_category() >= %GSK_TRANSFORM_CATEGORY_2D_AFFINE
 *
 * to check.
 */
void
gsk_transform_to_affine (GskTransform *self,
                         float        *out_scale_x,
                         float        *out_scale_y,
                         float        *out_dx,
                         float        *out_dy)
{
  *out_scale_x = 1.0f;
  *out_scale_y = 1.0f;
  *out_dx = 0.0f;
  *out_dy = 0.0f;

  if (self == NULL)
    return;

  if (G_UNLIKELY (self->category < GSK_TRANSFORM_CATEGORY_2D_AFFINE))
    {
      char *s = gsk_transform_to_string (self);
      g_warning ("Given transform \"%s\" is not an affine 2D transform.", s);
      g_free (s);
      return;
    }

  gsk_transform_to_affine (self->next,
                           out_scale_x, out_scale_y,
                           out_dx, out_dy);

  self->transform_class->apply_affine (self,
                                       out_scale_x, out_scale_y,
                                       out_dx, out_dy);
}

/**
 * gsk_transform_to_translate:
 * @self: a `GskTransform`
 * @out_dx: (out): return location for the translation
 *   in the x direction
 * @out_dy: (out): return location for the translation
 *   in the y direction
 *
 * Converts a `GskTransform` to a translation operation.
 *
 * @self must be a 2D transformation. If you are not
 * sure, use
 *
 *     gsk_transform_get_category() >= %GSK_TRANSFORM_CATEGORY_2D_TRANSLATE
 *
 * to check.
 */
void
gsk_transform_to_translate (GskTransform *self,
                            float        *out_dx,
                            float        *out_dy)
{
  *out_dx = 0.0f;
  *out_dy = 0.0f;

  if (self == NULL)
    return;

  if (G_UNLIKELY (self->category < GSK_TRANSFORM_CATEGORY_2D_TRANSLATE))
    {
      char *s = gsk_transform_to_string (self);
      g_warning ("Given transform \"%s\" is not an affine 2D translation.", s);
      g_free (s);

      return;
    }

  gsk_transform_to_translate (self->next, out_dx, out_dy);

  self->transform_class->apply_translate (self, out_dx, out_dy);
}

/**
 * gsk_transform_transform:
 * @next: (nullable) (transfer full): Transform to apply @other to
 * @other: (nullable):  Transform to apply
 *
 * Applies all the operations from @other to @next.
 *
 * Returns: (nullable): The new transform
 */
GskTransform *
gsk_transform_transform (GskTransform *next,
                         GskTransform *other)
{
  if (other == NULL)
    return next;

  if (next == NULL)
    return gsk_transform_ref (other);

  if (gsk_transform_is_identity (next))
    {
      /* ref before unref to avoid catastrophe when other == next */
      other = gsk_transform_ref (other);
      gsk_transform_unref (next);
      return other;
    }

  next = gsk_transform_transform (next, other->next);
  return other->transform_class->apply (other, next);
}

/**
 * gsk_transform_invert:
 * @self: (nullable) (transfer full): Transform to invert
 *
 * Inverts the given transform.
 *
 * If @self is not invertible, %NULL is returned.
 * Note that inverting %NULL also returns %NULL, which is
 * the correct inverse of %NULL. If you need to differentiate
 * between those cases, you should check @self is not %NULL
 * before calling this function.
 *
 * Returns: (nullable): The inverted transform
 */
GskTransform *
gsk_transform_invert (GskTransform *self)
{
  GskTransform *result = NULL;
  GskTransform *cur;

  for (cur = self; cur; cur = cur->next)
    {
      result = cur->transform_class->invert (cur, result);
      if (result == NULL)
        break;
    }

  gsk_transform_unref (self);

  return result;
}

/**
 * gsk_transform_equal:
 * @first: (nullable): the first transform
 * @second: (nullable): the second transform
 *
 * Checks two transforms for equality.
 *
 * Returns: %TRUE if the two transforms perform the same operation
 */
gboolean
gsk_transform_equal (GskTransform *first,
                     GskTransform *second)
{
  if (first == second)
    return TRUE;

  if (first == NULL)
    return gsk_transform_is_identity (second);

  if (second == NULL)
    return gsk_transform_is_identity (first);

  if (first->transform_class != second->transform_class)
    return FALSE;

  if (!gsk_transform_equal (first->next, second->next))
    return FALSE;

  return first->transform_class->equal (first, second);
}

/**
 * gsk_transform_get_category:
 * @self: (nullable): A `GskTransform`
 *
 * Returns the category this transform belongs to.
 *
 * Returns: The category of the transform
 **/
GskTransformCategory
(gsk_transform_get_category) (GskTransform *self)
{
  if (self == NULL)
    return GSK_TRANSFORM_CATEGORY_IDENTITY;

  return self->category;
}

/*
 * gsk_transform_new: (constructor):
 *
 * Creates a new identity transform.
 *
 * This function is meant to be used by language
 * bindings. For C code, this is equivalent to using %NULL.
 *
 * Returns: A new identity transform
 */
GskTransform *
gsk_transform_new (void)
{
  return gsk_transform_alloc (&GSK_IDENTITY_TRANSFORM_CLASS, GSK_TRANSFORM_CATEGORY_IDENTITY, NULL);
}

/**
 * gsk_transform_transform_bounds:
 * @self: a `GskTransform`
 * @rect: a `graphene_rect_t`
 * @out_rect: (out caller-allocates): return location for the bounds
 *   of the transformed rectangle
 *
 * Transforms a `graphene_rect_t` using the given transform @self.
 *
 * The result is the bounding box containing the coplanar quad.
 */
void
gsk_transform_transform_bounds (GskTransform          *self,
                                const graphene_rect_t *rect,
                                graphene_rect_t       *out_rect)
{
  switch (gsk_transform_get_category (self))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      graphene_rect_init_from_rect (out_rect, rect);
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;

        gsk_transform_to_translate (self, &dx, &dy);
        graphene_rect_init (out_rect,
                            rect->origin.x + dx,
                            rect->origin.y + dy,
                            rect->size.width,
                            rect->size.height);
      }
    break;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy, scale_x, scale_y;

        gsk_transform_to_affine (self, &scale_x, &scale_y, &dx, &dy);

        graphene_rect_init (out_rect,
                            (rect->origin.x * scale_x) + dx,
                            (rect->origin.y * scale_y) + dy,
                            rect->size.width * scale_x,
                            rect->size.height * scale_y);
      }
    break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    default:
      {
        graphene_matrix_t mat;

        gsk_transform_to_matrix (self, &mat);
        gsk_matrix_transform_bounds (&mat, rect, out_rect);
      }
      break;
    }
}

/**
 * gsk_transform_transform_point:
 * @self: a `GskTransform`
 * @point: a `graphene_point_t`
 * @out_point: (out caller-allocates): return location for
 *   the transformed point
 *
 * Transforms a `graphene_point_t` using the given transform @self.
 */
void
gsk_transform_transform_point (GskTransform           *self,
                               const graphene_point_t *point,
                               graphene_point_t       *out_point)
{
  switch (gsk_transform_get_category (self))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      *out_point = *point;
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;

        gsk_transform_to_translate (self, &dx, &dy);
        out_point->x = point->x + dx;
        out_point->y = point->y + dy;
      }
    break;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy, scale_x, scale_y;

        gsk_transform_to_affine (self, &scale_x, &scale_y, &dx, &dy);

        out_point->x = (point->x * scale_x) + dx;
        out_point->y = (point->y * scale_y) + dy;
      }
    break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    default:
      {
        graphene_matrix_t mat;

        gsk_transform_to_matrix (self, &mat);
        gsk_matrix_transform_point (&mat, point, out_point);
      }
      break;
    }
}

static guint
gsk_transform_parse_float (GtkCssParser *parser,
                           guint         n,
                           gpointer      data)
{
  float *f = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  f[n] = d;
  return 1;
}

static guint
gsk_transform_parse_scale (GtkCssParser *parser,
                           guint         n,
                           gpointer      data)
{
  float *f = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  f[n] = d;
  f[1] = d;
  return 1;
}

gboolean
gsk_transform_parser_parse (GtkCssParser  *parser,
                            GskTransform **out_transform)
{
  const GtkCssToken *token;
  GskTransform *transform = NULL;
  float f[16] = { 0, };
  gboolean parsed_something = FALSE;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is_ident (token, "none"))
    {
      gtk_css_parser_consume_token (parser);
      *out_transform = NULL;
      return TRUE;
    }

  while (TRUE)
    {
      if (gtk_css_token_is_function (token, "matrix"))
        {
          graphene_matrix_t matrix;
          if (!gtk_css_parser_consume_function (parser, 6, 6, gsk_transform_parse_float, f))
            goto fail;

          graphene_matrix_init_from_2d (&matrix, f[0], f[1], f[2], f[3], f[4], f[5]);
          transform = gsk_transform_matrix_with_category (transform,
                                                          &matrix,
                                                          GSK_TRANSFORM_CATEGORY_2D);
        }
      else if (gtk_css_token_is_function (token, "matrix3d"))
        {
          graphene_matrix_t matrix;
          if (!gtk_css_parser_consume_function (parser, 16, 16, gsk_transform_parse_float, f))
            goto fail;

          graphene_matrix_init_from_float (&matrix, f);
          transform = gsk_transform_matrix (transform, &matrix);
        }
      else if (gtk_css_token_is_function (token, "perspective"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_perspective (transform, f[0]);
        }
      else if (gtk_css_token_is_function (token, "rotate") ||
               gtk_css_token_is_function (token, "rotateZ"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_rotate (transform, f[0]);
        }
      else if (gtk_css_token_is_function (token, "rotate3d"))
        {
          graphene_vec3_t axis;

          if (!gtk_css_parser_consume_function (parser, 4, 4, gsk_transform_parse_float, f))
            goto fail;

          graphene_vec3_init (&axis, f[0], f[1], f[2]);
          transform = gsk_transform_rotate_3d (transform, f[3], &axis);
        }
      else if (gtk_css_token_is_function (token, "rotateX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_rotate_3d (transform, f[0], graphene_vec3_x_axis ());
        }
      else if (gtk_css_token_is_function (token, "rotateY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_rotate_3d (transform, f[0], graphene_vec3_y_axis ());
        }
      else if (gtk_css_token_is_function (token, "scale"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 2, gsk_transform_parse_scale, f))
            goto fail;

          transform = gsk_transform_scale (transform, f[0], f[1]);
        }
      else if (gtk_css_token_is_function (token, "scale3d"))
        {
          if (!gtk_css_parser_consume_function (parser, 3, 3, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_scale_3d (transform, f[0], f[1], f[2]);
        }
      else if (gtk_css_token_is_function (token, "scaleX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_scale (transform, f[0], 1.f);
        }
      else if (gtk_css_token_is_function (token, "scaleY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_scale (transform, 1.f, f[0]);
        }
      else if (gtk_css_token_is_function (token, "scaleZ"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_scale_3d (transform, 1.f, 1.f, f[0]);
        }
      else if (gtk_css_token_is_function (token, "translate"))
        {
          f[1] = 0.f;
          if (!gtk_css_parser_consume_function (parser, 1, 2, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (f[0], f[1]));
        }
      else if (gtk_css_token_is_function (token, "translate3d"))
        {
          if (!gtk_css_parser_consume_function (parser, 3, 3, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (f[0], f[1], f[2]));
        }
      else if (gtk_css_token_is_function (token, "translateX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (f[0], 0.f));
        }
      else if (gtk_css_token_is_function (token, "translateY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0.f, f[0]));
        }
      else if (gtk_css_token_is_function (token, "translateZ"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (0.f, 0.f, f[0]));
        }
      else if (gtk_css_token_is_function (token, "skew"))
        {
          if (!gtk_css_parser_consume_function (parser, 2, 2, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_skew (transform, f[0], f[1]);
        }
      else if (gtk_css_token_is_function (token, "skewX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_skew (transform, f[0], 0);
        }
      else if (gtk_css_token_is_function (token, "skewY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gsk_transform_parse_float, f))
            goto fail;

          transform = gsk_transform_skew (transform, 0, f[0]);
        }
      else
        {
          break;
        }

      parsed_something = TRUE;
      token = gtk_css_parser_get_token (parser);
    }

  if (!parsed_something)
    {
      gtk_css_parser_error_syntax (parser, "Expected a transform");
      goto fail;
    }

  *out_transform = transform;
  return TRUE;

fail:
  gsk_transform_unref (transform);
  *out_transform = NULL;
  return FALSE;
}

/**
 * gsk_transform_parse:
 * @string: the string to parse
 * @out_transform: (out): The location to put the transform in
 *
 * Parses the given @string into a transform and puts it in
 * @out_transform.
 *
 * Strings printed via [method@Gsk.Transform.to_string]
 * can be read in again successfully using this function.
 *
 * If @string does not describe a valid transform, %FALSE is
 * returned and %NULL is put in @out_transform.
 *
 * Returns: %TRUE if @string described a valid transform.
 */
gboolean
gsk_transform_parse (const char    *string,
                     GskTransform **out_transform)
{
  GtkCssParser *parser;
  GBytes *bytes;
  gboolean result;

  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (out_transform != NULL, FALSE);

  bytes = g_bytes_new_static (string, strlen (string));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  result = gsk_transform_parser_parse (parser, out_transform);

  if (result && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      g_clear_pointer (out_transform, gsk_transform_unref);
      result = FALSE;
    }
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return result; 
}

/* Some of the graphene_matrix_transform apis yield unexpected
 * results with projective matrices, since they silently drop
 * the w component, so we provide working alternatives here.
 */
void
gsk_matrix_transform_point (const graphene_matrix_t *m,
                            const graphene_point_t  *p,
                            graphene_point_t        *res)
{
  graphene_vec4_t vec4;
  float w;

  graphene_vec4_init (&vec4, p->x, p->y, 0.0f, 1.0f);
  graphene_matrix_transform_vec4 (m, &vec4, &vec4);

  w = graphene_vec4_get_w (&vec4);
  res->x = graphene_vec4_get_x (&vec4) / w;
  res->y = graphene_vec4_get_y (&vec4) / w;
}

void
gsk_matrix_transform_point3d (const graphene_matrix_t  *m,
                              const graphene_point3d_t *p,
                              graphene_point3d_t       *res)
{
  graphene_vec4_t vec4;
  float w;

  graphene_vec4_init (&vec4, p->x, p->y, 0.0f, 1.0f);
  graphene_matrix_transform_vec4 (m, &vec4, &vec4);

  w = graphene_vec4_get_w (&vec4);
  res->x = graphene_vec4_get_x (&vec4) / w;
  res->y = graphene_vec4_get_y (&vec4) / w;
  res->z = graphene_vec4_get_z (&vec4) / w;
}

void
gsk_matrix_transform_rect (const graphene_matrix_t *m,
                           const graphene_rect_t   *r,
                           graphene_quad_t         *res)
{
  graphene_point_t ret[4];
  graphene_rect_t rr;

  graphene_rect_normalize_r (r, &rr);

#define TRANSFORM_POINT(matrix, rect, corner, out_p)   do {\
  graphene_vec4_t __s; \
  graphene_point_t __p; \
  float w; \
  graphene_rect_get_ ## corner (rect, &__p); \
  graphene_vec4_init (&__s, __p.x, __p.y, 0.f, 1.f); \
  graphene_matrix_transform_vec4 (matrix, &__s, &__s); \
  w = graphene_vec4_get_w (&__s); \
  out_p.x = graphene_vec4_get_x (&__s) / w; \
  out_p.y = graphene_vec4_get_y (&__s) / w;           } while (0)

  TRANSFORM_POINT (m, &rr, top_left, ret[0]);
  TRANSFORM_POINT (m, &rr, top_right, ret[1]);
  TRANSFORM_POINT (m, &rr, bottom_right, ret[2]);
  TRANSFORM_POINT (m, &rr, bottom_left, ret[3]);

#undef TRANSFORM_POINT

  graphene_quad_init (res, &ret[0], &ret[1], &ret[2], &ret[3]);
}
void
gsk_matrix_transform_bounds (const graphene_matrix_t *m,
                             const graphene_rect_t   *r,
                             graphene_rect_t         *res)
{
  graphene_quad_t q;

  gsk_matrix_transform_rect (m, r, &q);
  graphene_quad_bounds (&q, res);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
