/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcsstransformvalueprivate.h"

#include <math.h>
#include <string.h>

#include "gtkcssnumbervalueprivate.h"
#include "gsktransform.h"

typedef union _GtkCssTransform GtkCssTransform;

typedef enum {
  GTK_CSS_TRANSFORM_NONE,
  GTK_CSS_TRANSFORM_MATRIX,
  GTK_CSS_TRANSFORM_TRANSLATE,
  GTK_CSS_TRANSFORM_ROTATE,
  GTK_CSS_TRANSFORM_SCALE,
  GTK_CSS_TRANSFORM_SKEW,
  GTK_CSS_TRANSFORM_SKEW_X,
  GTK_CSS_TRANSFORM_SKEW_Y,
  GTK_CSS_TRANSFORM_PERSPECTIVE
} GtkCssTransformType;

union _GtkCssTransform {
  GtkCssTransformType type;
  struct {
    GtkCssTransformType type;
    graphene_matrix_t   matrix;
  }                   matrix;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *x;
    GtkCssValue        *y;
    GtkCssValue        *z;
  }                   translate, scale;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *x;
    GtkCssValue        *y;
  }                   skew;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *x;
    GtkCssValue        *y;
    GtkCssValue        *z;
    GtkCssValue        *angle;
  }                   rotate;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *skew;
  }                   skew_x, skew_y;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *depth;
  }                   perspective;
};

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint             n_transforms;
  GtkCssTransform   transforms[1];
};

static GtkCssValue *    gtk_css_transform_value_alloc           (guint                  n_values);
static gboolean         gtk_css_transform_value_is_none         (const GtkCssValue     *value);

static void
gtk_css_transform_clear (GtkCssTransform *transform)
{
  switch (transform->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      _gtk_css_value_unref (transform->translate.x);
      _gtk_css_value_unref (transform->translate.y);
      _gtk_css_value_unref (transform->translate.z);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      _gtk_css_value_unref (transform->rotate.x);
      _gtk_css_value_unref (transform->rotate.y);
      _gtk_css_value_unref (transform->rotate.z);
      _gtk_css_value_unref (transform->rotate.angle);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      _gtk_css_value_unref (transform->scale.x);
      _gtk_css_value_unref (transform->scale.y);
      _gtk_css_value_unref (transform->scale.z);
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      _gtk_css_value_unref (transform->skew.x);
      _gtk_css_value_unref (transform->skew.y);
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      _gtk_css_value_unref (transform->skew_x.skew);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      _gtk_css_value_unref (transform->skew_y.skew);
      break;
    case GTK_CSS_TRANSFORM_PERSPECTIVE:
      _gtk_css_value_unref (transform->perspective.depth);
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean
gtk_css_transform_init_identity (GtkCssTransform     *transform,
                                 GtkCssTransformType  type)
{
  switch (type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      graphene_matrix_init_identity (&transform->matrix.matrix);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      transform->translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      transform->rotate.x = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.y = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->rotate.angle = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      transform->scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      transform->skew.x = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      transform->skew.y = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      transform->skew_x.skew = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      transform->skew_y.skew = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_PERSPECTIVE:
      return FALSE;

    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }

  transform->type = type;

  return TRUE;
}

static GskTransform *
gtk_css_transform_apply (const GtkCssTransform   *transform,
                         GskTransform            *next)
{
  graphene_matrix_t skew;

  switch (transform->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      return gsk_transform_matrix (next, &transform->matrix.matrix);

    case GTK_CSS_TRANSFORM_TRANSLATE:
      return gsk_transform_translate_3d (next,
                                         &GRAPHENE_POINT3D_INIT (
                                             _gtk_css_number_value_get (transform->translate.x, 100),
                                             _gtk_css_number_value_get (transform->translate.y, 100),
                                              _gtk_css_number_value_get (transform->translate.z, 100)
                                         ));

    case GTK_CSS_TRANSFORM_ROTATE:
      {
        graphene_vec3_t axis;

        graphene_vec3_init (&axis,
                            _gtk_css_number_value_get (transform->rotate.x, 1),
                            _gtk_css_number_value_get (transform->rotate.y, 1),
                            _gtk_css_number_value_get (transform->rotate.z, 1));
        return gsk_transform_rotate_3d (next,
                                        _gtk_css_number_value_get (transform->rotate.angle, 100),
                                        &axis);
      }

    case GTK_CSS_TRANSFORM_SCALE:
      return gsk_transform_scale_3d (next,
                                     _gtk_css_number_value_get (transform->scale.x, 1),
                                     _gtk_css_number_value_get (transform->scale.y, 1),
                                     _gtk_css_number_value_get (transform->scale.z, 1));

    case GTK_CSS_TRANSFORM_SKEW:
      graphene_matrix_init_skew (&skew,
                                 _gtk_css_number_value_get (transform->skew.x, 100) / 180.0f * G_PI,
                                 _gtk_css_number_value_get (transform->skew.y, 100) / 180.0f * G_PI);
      return gsk_transform_matrix (next, &skew);

    case GTK_CSS_TRANSFORM_SKEW_X:
      graphene_matrix_init_skew (&skew,
                                 _gtk_css_number_value_get (transform->skew_x.skew, 100) / 180.0f * G_PI,
                                 0);
      return gsk_transform_matrix (next, &skew);

    case GTK_CSS_TRANSFORM_SKEW_Y:
      graphene_matrix_init_skew (&skew,
                                 0,
                                 _gtk_css_number_value_get (transform->skew_y.skew, 100) / 180.0f * G_PI);
      return gsk_transform_matrix (next, &skew);

    case GTK_CSS_TRANSFORM_PERSPECTIVE:
      return gsk_transform_perspective (next,
                                        _gtk_css_number_value_get (transform->perspective.depth, 100));

    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  return NULL;
}

/* NB: The returned matrix may be invalid */
static GskTransform *
gtk_css_transform_value_compute_transform (const GtkCssValue *value)
{
  GskTransform *transform;
  guint i;

  transform = NULL;

  for (i = 0; i < value->n_transforms; i++)
    {
      transform = gtk_css_transform_apply (&value->transforms[i], transform);
    }

  return transform;
}

static void
gtk_css_value_transform_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->n_transforms; i++)
    gtk_css_transform_clear (&value->transforms[i]);

  g_free (value);
}

/* returns TRUE if dest == src */
static gboolean
gtk_css_transform_compute (GtkCssTransform  *dest,
                           GtkCssTransform  *src,
                           guint             property_id,
                           GtkStyleProvider *provider,
                           GtkCssStyle      *style,
                           GtkCssStyle      *parent_style)
{
  dest->type = src->type;

  switch (src->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      memcpy (dest, src, sizeof (GtkCssTransform));
      return TRUE;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      dest->translate.x = _gtk_css_value_compute (src->translate.x, property_id, provider, style, parent_style);
      dest->translate.y = _gtk_css_value_compute (src->translate.y, property_id, provider, style, parent_style);
      dest->translate.z = _gtk_css_value_compute (src->translate.z, property_id, provider, style, parent_style);
      return dest->translate.x == src->translate.x
          && dest->translate.y == src->translate.y
          && dest->translate.z == src->translate.z;
    case GTK_CSS_TRANSFORM_ROTATE:
      dest->rotate.x = _gtk_css_value_compute (src->rotate.x, property_id, provider, style, parent_style);
      dest->rotate.y = _gtk_css_value_compute (src->rotate.y, property_id, provider, style, parent_style);
      dest->rotate.z = _gtk_css_value_compute (src->rotate.z, property_id, provider, style, parent_style);
      dest->rotate.angle = _gtk_css_value_compute (src->rotate.angle, property_id, provider, style, parent_style);
      return dest->rotate.x == src->rotate.x
          && dest->rotate.y == src->rotate.y
          && dest->rotate.z == src->rotate.z
          && dest->rotate.angle == src->rotate.angle;
    case GTK_CSS_TRANSFORM_SCALE:
      dest->scale.x = _gtk_css_value_compute (src->scale.x, property_id, provider, style, parent_style);
      dest->scale.y = _gtk_css_value_compute (src->scale.y, property_id, provider, style, parent_style);
      dest->scale.z = _gtk_css_value_compute (src->scale.z, property_id, provider, style, parent_style);
      return dest->scale.x == src->scale.x
          && dest->scale.y == src->scale.y
          && dest->scale.z == src->scale.z;
    case GTK_CSS_TRANSFORM_SKEW:
      dest->skew.x = _gtk_css_value_compute (src->skew.x, property_id, provider, style, parent_style);
      dest->skew.y = _gtk_css_value_compute (src->skew.y, property_id, provider, style, parent_style);
      return dest->skew.x == src->skew.x
          && dest->skew.y == src->skew.y;
    case GTK_CSS_TRANSFORM_SKEW_X:
      dest->skew_x.skew = _gtk_css_value_compute (src->skew_x.skew, property_id, provider, style, parent_style);
      return dest->skew_x.skew == src->skew_x.skew;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      dest->skew_y.skew = _gtk_css_value_compute (src->skew_y.skew, property_id, provider, style, parent_style);
      return dest->skew_y.skew == src->skew_y.skew;
    case GTK_CSS_TRANSFORM_PERSPECTIVE:
      dest->perspective.depth = _gtk_css_value_compute (src->perspective.depth, property_id, provider, style, parent_style);
      return dest->perspective.depth == src->perspective.depth;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static GtkCssValue *
gtk_css_value_transform_compute (GtkCssValue      *value,
                                 guint             property_id,
                                 GtkStyleProvider *provider,
                                 GtkCssStyle      *style,
                                 GtkCssStyle      *parent_style)
{
  GtkCssValue *result;
  gboolean changes;
  guint i;

  /* Special case the 99% case of "none" */
  if (gtk_css_transform_value_is_none (value))
    return _gtk_css_value_ref (value);

  changes = FALSE;
  result = gtk_css_transform_value_alloc (value->n_transforms);

  for (i = 0; i < value->n_transforms; i++)
    {
      changes |= !gtk_css_transform_compute (&result->transforms[i],
                                             &value->transforms[i],
                                             property_id,
                                             provider,
                                             style,
                                             parent_style);
    }

  if (!changes)
    {
      _gtk_css_value_unref (result);
      result = _gtk_css_value_ref (value);
    }

  return result;
}

static gboolean
gtk_css_transform_equal (const GtkCssTransform *transform1,
                         const GtkCssTransform *transform2)
{
  if (transform1->type != transform2->type)
    return FALSE;

  switch (transform1->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      {
        guint i, j;

        for (i = 0; i < 4; i++)
          for (j = 0; j < 4; j++)
            {
              if (graphene_matrix_get_value (&transform1->matrix.matrix, i, j)
                  != graphene_matrix_get_value (&transform2->matrix.matrix, i, j))
                return FALSE;
            }
        return TRUE;
      }
    case GTK_CSS_TRANSFORM_TRANSLATE:
      return _gtk_css_value_equal (transform1->translate.x, transform2->translate.x)
          && _gtk_css_value_equal (transform1->translate.y, transform2->translate.y)
          && _gtk_css_value_equal (transform1->translate.z, transform2->translate.z);
    case GTK_CSS_TRANSFORM_ROTATE:
      return _gtk_css_value_equal (transform1->rotate.x, transform2->rotate.x)
          && _gtk_css_value_equal (transform1->rotate.y, transform2->rotate.y)
          && _gtk_css_value_equal (transform1->rotate.z, transform2->rotate.z)
          && _gtk_css_value_equal (transform1->rotate.angle, transform2->rotate.angle);
    case GTK_CSS_TRANSFORM_SCALE:
      return _gtk_css_value_equal (transform1->scale.x, transform2->scale.x)
          && _gtk_css_value_equal (transform1->scale.y, transform2->scale.y)
          && _gtk_css_value_equal (transform1->scale.z, transform2->scale.z);
    case GTK_CSS_TRANSFORM_SKEW:
      return _gtk_css_value_equal (transform1->skew.x, transform2->skew.x)
          && _gtk_css_value_equal (transform1->skew.y, transform2->skew.y);
    case GTK_CSS_TRANSFORM_SKEW_X:
      return _gtk_css_value_equal (transform1->skew_x.skew, transform2->skew_x.skew);
    case GTK_CSS_TRANSFORM_SKEW_Y:
      return _gtk_css_value_equal (transform1->skew_y.skew, transform2->skew_y.skew);
    case GTK_CSS_TRANSFORM_PERSPECTIVE:
      return _gtk_css_value_equal (transform1->perspective.depth, transform2->perspective.depth);
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static gboolean
gtk_css_value_transform_equal (const GtkCssValue *value1,
                               const GtkCssValue *value2)
{
  const GtkCssValue *larger;
  guint i, n;

  n = MIN (value1->n_transforms, value2->n_transforms);
  for (i = 0; i < n; i++)
    {
      if (!gtk_css_transform_equal (&value1->transforms[i], &value2->transforms[i]))
        return FALSE;
    }

  larger = value1->n_transforms > value2->n_transforms ? value1 : value2;

  for (; i < larger->n_transforms; i++)
    {
      GtkCssTransform transform;

      if (!gtk_css_transform_init_identity (&transform, larger->transforms[i].type))
        return FALSE;

      if (!gtk_css_transform_equal (&larger->transforms[i], &transform))
        {
          gtk_css_transform_clear (&transform);
          return FALSE;
        }

      gtk_css_transform_clear (&transform);
    }

  return TRUE;
}

static void
gtk_css_transform_transition_default (GtkCssTransform       *result,
                                      const GtkCssTransform *start,
                                      const GtkCssTransform *end,
                                      guint                  property_id,
                                      double                 progress)
{
  graphene_matrix_t start_mat, end_mat;
  GskTransform *trans;

  result->type = GTK_CSS_TRANSFORM_MATRIX;

  if (start)
    trans = gtk_css_transform_apply (start, NULL);
  else
    trans = NULL;
  gsk_transform_to_matrix (trans, &start_mat);
  gsk_transform_unref (trans);

  if (end)
    trans = gtk_css_transform_apply (end, NULL);
  else
    trans = NULL;
  gsk_transform_to_matrix (trans, &end_mat);
  gsk_transform_unref (trans);

  graphene_matrix_interpolate (&start_mat,
                               &end_mat,
                               progress,
                               &result->matrix.matrix);
}

static void
gtk_css_transform_transition (GtkCssTransform       *result,
                              const GtkCssTransform *start,
                              const GtkCssTransform *end,
                              guint                  property_id,
                              double                 progress)
{
  result->type = start->type;

  switch (start->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      graphene_matrix_interpolate (&start->matrix.matrix,
                                   &end->matrix.matrix,
                                   progress,
                                   &result->matrix.matrix);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      result->translate.x = _gtk_css_value_transition (start->translate.x, end->translate.x, property_id, progress);
      result->translate.y = _gtk_css_value_transition (start->translate.y, end->translate.y, property_id, progress);
      result->translate.z = _gtk_css_value_transition (start->translate.z, end->translate.z, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      result->rotate.x = _gtk_css_value_transition (start->rotate.x, end->rotate.x, property_id, progress);
      result->rotate.y = _gtk_css_value_transition (start->rotate.y, end->rotate.y, property_id, progress);
      result->rotate.z = _gtk_css_value_transition (start->rotate.z, end->rotate.z, property_id, progress);
      result->rotate.angle = _gtk_css_value_transition (start->rotate.angle, end->rotate.angle, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      result->scale.x = _gtk_css_value_transition (start->scale.x, end->scale.x, property_id, progress);
      result->scale.y = _gtk_css_value_transition (start->scale.y, end->scale.y, property_id, progress);
      result->scale.z = _gtk_css_value_transition (start->scale.z, end->scale.z, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      result->skew.x = _gtk_css_value_transition (start->skew.x, end->skew.x, property_id, progress);
      result->skew.y = _gtk_css_value_transition (start->skew.y, end->skew.y, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      result->skew_x.skew = _gtk_css_value_transition (start->skew_x.skew, end->skew_x.skew, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      result->skew_y.skew = _gtk_css_value_transition (start->skew_y.skew, end->skew_y.skew, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_PERSPECTIVE:
      gtk_css_transform_transition_default (result, start, end, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkCssValue *
gtk_css_value_transform_transition (GtkCssValue *start,
                                    GtkCssValue *end,
                                    guint        property_id,
                                    double       progress)
{
  GtkCssValue *result;
  guint i, n;

  if (gtk_css_transform_value_is_none (start))
    {
      if (gtk_css_transform_value_is_none (end))
        return _gtk_css_value_ref (start);

      n = 0;
    }
  else if (gtk_css_transform_value_is_none (end))
    {
      n = 0;
    }
  else
    {
      n = MIN (start->n_transforms, end->n_transforms);
    }

  /* Check transforms are compatible. If not, transition between
   * their result matrices.
   */
  for (i = 0; i < n; i++)
    {
      if (start->transforms[i].type != end->transforms[i].type)
        {
          GskTransform *transform;
          graphene_matrix_t start_matrix, end_matrix;

          transform = gtk_css_transform_value_compute_transform (start);
          gsk_transform_to_matrix (transform, &start_matrix);
          gsk_transform_unref (transform);

          transform = gtk_css_transform_value_compute_transform (end);
          gsk_transform_to_matrix (transform, &end_matrix);
          gsk_transform_unref (transform);

          result = gtk_css_transform_value_alloc (1);
          result->transforms[0].type = GTK_CSS_TRANSFORM_MATRIX;
          graphene_matrix_interpolate (&start_matrix, &end_matrix, progress, &result->transforms[0].matrix.matrix);

          return result;
        }
    }

  result = gtk_css_transform_value_alloc (MAX (start->n_transforms, end->n_transforms));

  for (i = 0; i < n; i++)
    {
      gtk_css_transform_transition (&result->transforms[i],
                                    &start->transforms[i],
                                    &end->transforms[i],
                                    property_id,
                                    progress);
    }

  for (; i < start->n_transforms; i++)
    {
      GtkCssTransform transform;

      if (gtk_css_transform_init_identity (&transform, start->transforms[i].type))
        {
          gtk_css_transform_transition (&result->transforms[i],
                                        &start->transforms[i],
                                        &transform,
                                        property_id,
                                        progress);
          gtk_css_transform_clear (&transform);
        }
      else
        {
          gtk_css_transform_transition_default (&result->transforms[i],
                                                &start->transforms[i],
                                                NULL,
                                                property_id,
                                                progress);
        }
    }
  for (; i < end->n_transforms; i++)
    {
      GtkCssTransform transform;

      if (gtk_css_transform_init_identity (&transform, end->transforms[i].type))
        {
          gtk_css_transform_transition (&result->transforms[i],
                                        &transform,
                                        &end->transforms[i],
                                        property_id,
                                        progress);
          gtk_css_transform_clear (&transform);
        }
      else
        {
          gtk_css_transform_transition_default (&result->transforms[i],
                                                NULL,
                                                &end->transforms[i],
                                                property_id,
                                                progress);
        }
    }

  g_assert (i == MAX (start->n_transforms, end->n_transforms));

  return result;
}

static void
gtk_css_transform_print (const GtkCssTransform *transform,
                         GString               *string)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (transform->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      if (graphene_matrix_is_2d (&transform->matrix.matrix))
        {
          g_string_append (string, "matrix(");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 0, 0));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 0, 1));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 0, 2));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 1, 0));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 1, 1));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 1, 2));
          g_string_append (string, buf);
          g_string_append (string, ")");
        }
      else
        {
          guint i;

          g_string_append (string, "matrix3d(");
          for (i = 0; i < 16; i++)
            {
              g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, i / 4, i % 4));
              g_string_append (string, buf);
              if (i < 15)
                g_string_append (string, ", ");
            }
          g_string_append (string, ")");
        }
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      g_string_append (string, "translate3d(");
      _gtk_css_value_print (transform->translate.x, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->translate.y, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->translate.z, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      g_string_append (string, "rotate3d(");
      _gtk_css_value_print (transform->rotate.x, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->rotate.y, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->rotate.z, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->rotate.angle, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      if (_gtk_css_number_value_get (transform->scale.z, 100) == 1)
        {
          g_string_append (string, "scale(");
          _gtk_css_value_print (transform->scale.x, string);
          if (!_gtk_css_value_equal (transform->scale.x, transform->scale.y))
            {
              g_string_append (string, ", ");
              _gtk_css_value_print (transform->scale.y, string);
            }
          g_string_append (string, ")");
        }
      else
        {
          g_string_append (string, "scale3d(");
          _gtk_css_value_print (transform->scale.x, string);
          g_string_append (string, ", ");
          _gtk_css_value_print (transform->scale.y, string);
          g_string_append (string, ", ");
          _gtk_css_value_print (transform->scale.z, string);
          g_string_append (string, ")");
        }
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      g_string_append (string, "skew(");
      _gtk_css_value_print (transform->skew.x, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->skew.y, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      g_string_append (string, "skewX(");
      _gtk_css_value_print (transform->skew_x.skew, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      g_string_append (string, "skewY(");
      _gtk_css_value_print (transform->skew_y.skew, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_PERSPECTIVE:
      g_string_append (string, "perspective(");
      _gtk_css_value_print (transform->perspective.depth, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_css_value_transform_print (const GtkCssValue *value,
                               GString           *string)
{
  guint i;

  if (gtk_css_transform_value_is_none (value))
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->n_transforms; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');

      gtk_css_transform_print (&value->transforms[i], string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_TRANSFORM = {
  "GtkCssTransformValue",
  gtk_css_value_transform_free,
  gtk_css_value_transform_compute,
  gtk_css_value_transform_equal,
  gtk_css_value_transform_transition,
  NULL,
  NULL,
  gtk_css_value_transform_print
};

static GtkCssValue transform_none_singleton = { &GTK_CSS_VALUE_TRANSFORM, 1, TRUE, 0, {  { GTK_CSS_TRANSFORM_NONE } } };

static GtkCssValue *
gtk_css_transform_value_alloc (guint n_transforms)
{
  GtkCssValue *result;
           
  g_return_val_if_fail (n_transforms > 0, NULL);
         
  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_TRANSFORM, sizeof (GtkCssValue) + sizeof (GtkCssTransform) * (n_transforms - 1));
  result->n_transforms = n_transforms;
            
  return result;
}

GtkCssValue *
_gtk_css_transform_value_new_none (void)
{
  return _gtk_css_value_ref (&transform_none_singleton);
}

static gboolean
gtk_css_transform_value_is_none (const GtkCssValue *value)
{
  return value->n_transforms == 0;
}

static guint
gtk_css_transform_parse_float (GtkCssParser *parser,
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
gtk_css_transform_parse_length (GtkCssParser *parser,
                                guint         n,
                                gpointer      data)
{
  GtkCssValue **values = data;

  values[n] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
  if (values[n] == NULL)
    return 0;

  return 1;
}

static guint
gtk_css_transform_parse_angle (GtkCssParser *parser,
                               guint         n,
                               gpointer      data)
{
  GtkCssValue **values = data;

  values[n] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
  if (values[n] == NULL)
    return 0;

  return 1;
}

static guint
gtk_css_transform_parse_number (GtkCssParser *parser,
                                guint         n,
                                gpointer      data)
{
  GtkCssValue **values = data;

  values[n] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
  if (values[n] == NULL)
    return 0;

  return 1;
}

static guint
gtk_css_transform_parse_rotate3d (GtkCssParser *parser,
                                  guint         n,
                                  gpointer      data)
{
  GtkCssTransform *transform = data;

  switch (n)
  {
    case 0:
      transform->rotate.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->rotate.x == NULL)
        return 0;
      break;

    case 1:
      transform->rotate.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->rotate.y == NULL)
        return 0;
      break;

    case 2:
      transform->rotate.z = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->rotate.z == NULL)
        return 0;
      break;

    case 3:
      transform->rotate.angle = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->rotate.angle == NULL)
        return 0;
      break;

    default:
      g_assert_not_reached();
      return 0;
  }

  return 1;
}

GtkCssValue *
_gtk_css_transform_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value;
  GArray *array;
  guint i;
  gboolean computed = TRUE;

  if (gtk_css_parser_try_ident (parser, "none"))
    return _gtk_css_transform_value_new_none ();

  array = g_array_new (FALSE, FALSE, sizeof (GtkCssTransform));

  while (TRUE)
    {
      GtkCssTransform transform = { 0, };

      if (gtk_css_parser_has_function (parser, "matrix"))
        {
          float f[6];

          if (!gtk_css_parser_consume_function (parser, 6, 6, gtk_css_transform_parse_float, f))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_MATRIX;
          graphene_matrix_init_from_2d (&transform.matrix.matrix, f[0], f[1], f[2], f[3], f[4], f[5]);
        }
      else if (gtk_css_parser_has_function (parser, "matrix3d"))
        {
          float f[16];

          if (!gtk_css_parser_consume_function (parser, 16, 16, gtk_css_transform_parse_float, f))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_MATRIX;
          graphene_matrix_init_from_float (&transform.matrix.matrix, f);
        }
      else if (gtk_css_parser_has_function (parser, "perspective"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_length, &transform.perspective.depth))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_PERSPECTIVE;
          computed = computed && gtk_css_value_is_computed (transform.perspective.depth);
        }
      else if (gtk_css_parser_has_function (parser, "rotate") ||
               gtk_css_parser_has_function (parser, "rotateZ"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_angle, &transform.rotate.angle))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_ROTATE;
          transform.rotate.x = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
          transform.rotate.y = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
          transform.rotate.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          computed = computed && gtk_css_value_is_computed (transform.rotate.angle);
        }
      else if (gtk_css_parser_has_function (parser, "rotate3d"))
        {
          if (!gtk_css_parser_consume_function (parser, 4, 4, gtk_css_transform_parse_rotate3d, &transform))
            {
              g_clear_pointer (&transform.rotate.x, gtk_css_value_unref);
              g_clear_pointer (&transform.rotate.y, gtk_css_value_unref);
              g_clear_pointer (&transform.rotate.z, gtk_css_value_unref);
              g_clear_pointer (&transform.rotate.angle, gtk_css_value_unref);
              goto fail;
            }

          transform.type = GTK_CSS_TRANSFORM_ROTATE;
          computed = computed && gtk_css_value_is_computed (transform.rotate.angle) &&
                                 gtk_css_value_is_computed (transform.rotate.x) &&
                                 gtk_css_value_is_computed (transform.rotate.y) &&
                                 gtk_css_value_is_computed (transform.rotate.z);
        }
      else if (gtk_css_parser_has_function (parser, "rotateX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_angle, &transform.rotate.angle))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_ROTATE;
          transform.rotate.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          transform.rotate.y = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
          transform.rotate.z = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
          computed = computed && gtk_css_value_is_computed (transform.rotate.angle);
        }
      else if (gtk_css_parser_has_function (parser, "rotateY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_angle, &transform.rotate.angle))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_ROTATE;
          transform.rotate.x = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
          transform.rotate.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          transform.rotate.z = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
          computed = computed && gtk_css_value_is_computed (transform.rotate.angle);
        }
      else if (gtk_css_parser_has_function (parser, "scale"))
        {
          GtkCssValue *values[2] = { NULL, NULL };

          if (!gtk_css_parser_consume_function (parser, 1, 2, gtk_css_transform_parse_number, values))
            {
              g_clear_pointer (&values[0], gtk_css_value_unref);
              g_clear_pointer (&values[1], gtk_css_value_unref);
              goto fail;
            }

          transform.type = GTK_CSS_TRANSFORM_SCALE;
          transform.scale.x = values[0];
          if (values[1])
            transform.scale.y = values[1];
          else
            transform.scale.y = gtk_css_value_ref (values[0]);
          transform.scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          computed = computed && gtk_css_value_is_computed (transform.scale.x) &&
                                 gtk_css_value_is_computed (transform.scale.y);
        }
      else if (gtk_css_parser_has_function (parser, "scale3d"))
        {
          GtkCssValue *values[3] = { NULL, NULL };

          if (!gtk_css_parser_consume_function (parser, 3, 3, gtk_css_transform_parse_number, values))
            {
              g_clear_pointer (&values[0], gtk_css_value_unref);
              g_clear_pointer (&values[1], gtk_css_value_unref);
              g_clear_pointer (&values[2], gtk_css_value_unref);
              goto fail;
            }

          transform.type = GTK_CSS_TRANSFORM_SCALE;
          transform.scale.x = values[0];
          transform.scale.y = values[1];
          transform.scale.z = values[2];
          computed = computed && gtk_css_value_is_computed (transform.scale.x) &&
                                 gtk_css_value_is_computed (transform.scale.y) &&
                                 gtk_css_value_is_computed (transform.scale.z);
        }
      else if (gtk_css_parser_has_function (parser, "scaleX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_number, &transform.scale.x))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_SCALE;
          transform.scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          transform.scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          computed = computed && gtk_css_value_is_computed (transform.scale.x);
        }
      else if (gtk_css_parser_has_function (parser, "scaleY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_number, &transform.scale.y))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_SCALE;
          transform.scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          transform.scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          computed = computed && gtk_css_value_is_computed (transform.scale.y);
        }
      else if (gtk_css_parser_has_function (parser, "scaleZ"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_number, &transform.scale.z))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_SCALE;
          transform.scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          transform.scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
          computed = computed && gtk_css_value_is_computed (transform.scale.z);
        }
      else if (gtk_css_parser_has_function (parser, "skew"))
        {
          GtkCssValue *values[2] = { NULL, NULL };

          if (!gtk_css_parser_consume_function (parser, 2, 2, gtk_css_transform_parse_angle, values))
            {
              g_clear_pointer (&values[0], gtk_css_value_unref);
              g_clear_pointer (&values[1], gtk_css_value_unref);
              goto fail;
            }

          transform.type = GTK_CSS_TRANSFORM_SKEW;
          transform.skew.x = values[0];
          transform.skew.y = values[1];
          computed = computed && gtk_css_value_is_computed (transform.skew.x) &&
                                 gtk_css_value_is_computed (transform.skew.y);
        }
      else if (gtk_css_parser_has_function (parser, "skewX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_angle, &transform.skew_x.skew))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_SKEW_X;
          computed = computed && gtk_css_value_is_computed (transform.skew_x.skew);
        }
      else if (gtk_css_parser_has_function (parser, "skewY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_angle, &transform.skew_y.skew))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_SKEW_Y;
          computed = computed && gtk_css_value_is_computed (transform.skew_y.skew);
        }
      else if (gtk_css_parser_has_function (parser, "translate"))
        {
          GtkCssValue *values[2] = { NULL, NULL };

          if (!gtk_css_parser_consume_function (parser, 1, 2, gtk_css_transform_parse_length, values))
            {
              g_clear_pointer (&values[0], gtk_css_value_unref);
              g_clear_pointer (&values[1], gtk_css_value_unref);
              goto fail;
            }

          transform.type = GTK_CSS_TRANSFORM_TRANSLATE;
          transform.translate.x = values[0];
          if (values[1])
            transform.translate.y = values[1];
          else
            transform.translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
          transform.translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
          computed = computed && gtk_css_value_is_computed (transform.translate.x) &&
                                 gtk_css_value_is_computed (transform.translate.y);
        }
      else if (gtk_css_parser_has_function (parser, "translate3d"))
        {
          GtkCssValue *values[3] = { NULL, NULL };

          if (!gtk_css_parser_consume_function (parser, 3, 3, gtk_css_transform_parse_length, values))
            {
              g_clear_pointer (&values[0], gtk_css_value_unref);
              g_clear_pointer (&values[1], gtk_css_value_unref);
              g_clear_pointer (&values[2], gtk_css_value_unref);
              goto fail;
            }

          transform.type = GTK_CSS_TRANSFORM_TRANSLATE;
          transform.translate.x = values[0];
          transform.translate.y = values[1];
          transform.translate.z = values[2];
          computed = computed && gtk_css_value_is_computed (transform.translate.x) &&
                                 gtk_css_value_is_computed (transform.translate.y) &&
                                 gtk_css_value_is_computed (transform.translate.z);
        }
      else if (gtk_css_parser_has_function (parser, "translateX"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_length, &transform.translate.x))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_TRANSLATE;
          transform.translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
          transform.translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
          computed = computed && gtk_css_value_is_computed (transform.translate.x);
        }
      else if (gtk_css_parser_has_function (parser, "translateY"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_length, &transform.translate.y))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_TRANSLATE;
          transform.translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
          transform.translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
          computed = computed && gtk_css_value_is_computed (transform.translate.y);
        }
      else if (gtk_css_parser_has_function (parser, "translateZ"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_transform_parse_length, &transform.translate.z))
            goto fail;

          transform.type = GTK_CSS_TRANSFORM_TRANSLATE;
          transform.translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
          transform.translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
          computed = computed && gtk_css_value_is_computed (transform.translate.z);
        }
      else
        {
          break;
        }

      g_array_append_val (array, transform);
    }

  if (array->len == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a transform");
      goto fail;
    }

  value = gtk_css_transform_value_alloc (array->len);
  value->is_computed = computed;
  memcpy (value->transforms, array->data, sizeof (GtkCssTransform) * array->len);

  g_array_free (array, TRUE);

  return value;

fail:
  for (i = 0; i < array->len; i++)
    {
      gtk_css_transform_clear (&g_array_index (array, GtkCssTransform, i));
    }
  g_array_free (array, TRUE);
  return NULL;
}

GskTransform *
gtk_css_transform_value_get_transform (const GtkCssValue *transform)
{
  g_return_val_if_fail (transform->class == &GTK_CSS_VALUE_TRANSFORM, FALSE);

  if (transform->n_transforms == 0)
    return NULL;

  return gtk_css_transform_value_compute_transform (transform);
}

