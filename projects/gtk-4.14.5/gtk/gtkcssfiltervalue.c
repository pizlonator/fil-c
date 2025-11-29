/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Red Hat, Inc.
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

#include "gtkcssbgsizevalueprivate.h"

#include <math.h>
#include <string.h>

#include "gtkcssfiltervalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowvalueprivate.h"

typedef union _GtkCssFilter GtkCssFilter;

typedef enum {
  GTK_CSS_FILTER_NONE,
  GTK_CSS_FILTER_BLUR,
  GTK_CSS_FILTER_BRIGHTNESS,
  GTK_CSS_FILTER_CONTRAST,
  GTK_CSS_FILTER_DROP_SHADOW,
  GTK_CSS_FILTER_GRAYSCALE,
  GTK_CSS_FILTER_HUE_ROTATE,
  GTK_CSS_FILTER_INVERT,
  GTK_CSS_FILTER_OPACITY,
  GTK_CSS_FILTER_SATURATE,
  GTK_CSS_FILTER_SEPIA
} GtkCssFilterType;

union _GtkCssFilter {
  GtkCssFilterType       type;
  struct {
    GtkCssFilterType     type;
    GtkCssValue         *value;
  }            blur, brightness, contrast, drop_shadow, grayscale, hue_rotate, invert, opacity, saturate, sepia;
};

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint                 n_filters;
  GtkCssFilter          filters[1];
};

static GtkCssValue *    gtk_css_filter_value_alloc           (guint                  n_values);
static gboolean         gtk_css_filter_value_is_none         (const GtkCssValue     *value);

static void
gtk_css_filter_clear (GtkCssFilter *filter)
{
  switch (filter->type)
    {
    case GTK_CSS_FILTER_BRIGHTNESS:
      _gtk_css_value_unref (filter->brightness.value);
      break;
    case GTK_CSS_FILTER_CONTRAST:
      _gtk_css_value_unref (filter->contrast.value);
      break;
    case GTK_CSS_FILTER_GRAYSCALE:
      _gtk_css_value_unref (filter->grayscale.value);
      break;
    case GTK_CSS_FILTER_HUE_ROTATE:
      _gtk_css_value_unref (filter->hue_rotate.value);
      break;
    case GTK_CSS_FILTER_INVERT:
      _gtk_css_value_unref (filter->invert.value);
      break;
    case GTK_CSS_FILTER_OPACITY:
      _gtk_css_value_unref (filter->opacity.value);
      break;
    case GTK_CSS_FILTER_SATURATE:
      _gtk_css_value_unref (filter->saturate.value);
      break;
    case GTK_CSS_FILTER_SEPIA:
      _gtk_css_value_unref (filter->sepia.value);
      break;
    case GTK_CSS_FILTER_BLUR:
      _gtk_css_value_unref (filter->blur.value);
      break;
    case GTK_CSS_FILTER_DROP_SHADOW:
      _gtk_css_value_unref (filter->drop_shadow.value);
      break;
    case GTK_CSS_FILTER_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_css_filter_init_identity (GtkCssFilter       *filter,
                              const GtkCssFilter *other)
{
  switch (other->type)
    {
    case GTK_CSS_FILTER_BRIGHTNESS:
      filter->brightness.value = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_FILTER_CONTRAST:
      filter->contrast.value = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_FILTER_GRAYSCALE:
      filter->grayscale.value = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_FILTER_HUE_ROTATE:
      filter->hue_rotate.value = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_FILTER_INVERT:
      filter->invert.value = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_FILTER_OPACITY:
      filter->opacity.value = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_FILTER_SATURATE:
      filter->saturate.value = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_FILTER_SEPIA:
      filter->sepia.value = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_FILTER_BLUR:
      filter->blur.value = _gtk_css_number_value_new (0, GTK_CSS_PX);
      break;
    case GTK_CSS_FILTER_DROP_SHADOW:
      filter->drop_shadow.value = gtk_css_shadow_value_new_filter (other->drop_shadow.value);
      break;
    case GTK_CSS_FILTER_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  filter->type = other->type;
}

#define R 0.2126
#define G 0.7152
#define B 0.0722

static gboolean
gtk_css_filter_get_matrix (const GtkCssFilter *filter,
                           graphene_matrix_t  *matrix,
                           graphene_vec4_t    *offset)
{
  double value;

  switch (filter->type)
    {
    case GTK_CSS_FILTER_BRIGHTNESS:
      value = _gtk_css_number_value_get (filter->brightness.value, 1.0);
      graphene_matrix_init_scale (matrix, value, value, value);
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      break;

    case GTK_CSS_FILTER_CONTRAST:
      value = _gtk_css_number_value_get (filter->contrast.value, 1.0);
      graphene_matrix_init_scale (matrix, value, value, value);
      graphene_vec4_init (offset, 0.5 - 0.5 * value, 0.5 - 0.5 * value, 0.5 - 0.5 * value, 0.0);
      break;

    case GTK_CSS_FILTER_GRAYSCALE:
      value = _gtk_css_number_value_get (filter->grayscale.value, 1.0);
      graphene_matrix_init_from_float (matrix, (float[16]) {
                                           1.0 - (1.0 - R) * value, R * value, R * value, 0.0,
                                           G * value, 1.0 - (1.0 - G) * value, G * value, 0.0,
                                           B * value, B * value, 1.0 - (1.0 - B) * value, 0.0,
                                           0.0, 0.0, 0.0, 1.0
                                       });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      break;

    case GTK_CSS_FILTER_HUE_ROTATE:
      {
        double c, s;
        value = _gtk_css_number_value_get (filter->grayscale.value, 1.0) * G_PI / 180.0;
        c = cos (value);
        s = sin (value);
        graphene_matrix_init_from_float (matrix, (float[16]) {
                                             0.213 + 0.787 * c - 0.213 * s,
                                             0.213 - 0.213 * c + 0.143 * s,
                                             0.213 - 0.213 * c - 0.787 * s,
                                             0,
                                             0.715 - 0.715 * c - 0.715 * s,
                                             0.715 + 0.285 * c + 0.140 * s,
                                             0.715 - 0.715 * c + 0.715 * s,
                                             0,
                                             0.072 - 0.072 * c + 0.928 * s,
                                             0.072 - 0.072 * c - 0.283 * s,
                                             0.072 + 0.928 * c + 0.072 * s,
                                             0,
                                             0, 0, 0, 1
                                         });
        graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      }
      break;

    case GTK_CSS_FILTER_INVERT:
      value = _gtk_css_number_value_get (filter->invert.value, 1.0);
      graphene_matrix_init_scale (matrix, 1.0 - 2 * value, 1.0 - 2 * value, 1.0 - 2 * value);
      graphene_vec4_init (offset, value, value, value, 0.0);
      break;

    case GTK_CSS_FILTER_OPACITY:
      value = _gtk_css_number_value_get (filter->opacity.value, 1.0);
      graphene_matrix_init_from_float (matrix, (float[16]) {
                                           1.0, 0.0, 0.0, 0.0,
                                           0.0, 1.0, 0.0, 0.0,
                                           0.0, 0.0, 1.0, 0.0,
                                           0.0, 0.0, 0.0, value
                                       });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      break;

    case GTK_CSS_FILTER_SATURATE:
      value = _gtk_css_number_value_get (filter->saturate.value, 1.0);
      graphene_matrix_init_from_float (matrix, (float[16]) {
                                           R + (1.0 - R) * value, R - R * value, R - R * value, 0.0,
                                           G - G * value, G + (1.0 - G) * value, G - G * value, 0.0,
                                           B - B * value, B - B * value, B + (1.0 - B) * value, 0.0,
                                           0.0, 0.0, 0.0, 1.0
                                       });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      break;

    case GTK_CSS_FILTER_SEPIA:
      value = _gtk_css_number_value_get (filter->sepia.value, 1.0);
      graphene_matrix_init_from_float (matrix, (float[16]) {
                                           1.0 - 0.607 * value, 0.349 * value, 0.272 * value, 0.0,
                                           0.769 * value, 1.0 - 0.314 * value, 0.534 * value, 0.0,
                                           0.189 * value, 0.168 * value, 1.0 - 0.869 * value, 0.0,
                                           0.0, 0.0, 0.0, 1.0
                                       });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      break;

    case GTK_CSS_FILTER_NONE:
    case GTK_CSS_FILTER_BLUR:
    case GTK_CSS_FILTER_DROP_SHADOW:
      return FALSE;
    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

#undef R
#undef G
#undef B

static int
gtk_css_filter_value_compute_matrix (const GtkCssValue *value,
                                     int                first,
                                     graphene_matrix_t *matrix,
                                     graphene_vec4_t   *offset,
                                     gboolean          *all_opacity)
{
  graphene_matrix_t m, m2;
  graphene_vec4_t o, o2;
  int i;

  if (!gtk_css_filter_get_matrix (&value->filters[first], matrix, offset))
    return first;

  *all_opacity = value->filters[first].type == GTK_CSS_FILTER_OPACITY;

  for (i = first + 1; i < value->n_filters; i++)
    {
      if (!gtk_css_filter_get_matrix (&value->filters[i], &m, &o))
        return i;

      *all_opacity &= value->filters[i].type == GTK_CSS_FILTER_OPACITY;
      graphene_matrix_multiply (matrix, &m, &m2);
      graphene_matrix_transform_vec4 (&m, offset, &o2);

      graphene_matrix_init_from_matrix (matrix, &m2);
      graphene_vec4_add (&o, &o2, offset);
    }

  return value->n_filters;
}

static void
gtk_css_value_filter_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->n_filters; i++)
    gtk_css_filter_clear (&value->filters[i]);

  g_free (value);
}

/* returns TRUE if dest == src */
static gboolean
gtk_css_filter_compute (GtkCssFilter     *dest,
                        GtkCssFilter     *src,
                        guint             property_id,
                        GtkStyleProvider *provider,
                        GtkCssStyle      *style,
                        GtkCssStyle      *parent_style)
{
  dest->type = src->type;

  switch (src->type)
    {
    case GTK_CSS_FILTER_BRIGHTNESS:
      dest->brightness.value = _gtk_css_value_compute (src->brightness.value, property_id, provider, style, parent_style);
      return dest->brightness.value == src->brightness.value;

    case GTK_CSS_FILTER_CONTRAST:
      dest->contrast.value = _gtk_css_value_compute (src->contrast.value, property_id, provider, style, parent_style);
      return dest->contrast.value == src->contrast.value;

    case GTK_CSS_FILTER_GRAYSCALE:
      dest->grayscale.value = _gtk_css_value_compute (src->grayscale.value, property_id, provider, style, parent_style);
      return dest->grayscale.value == src->grayscale.value;

    case GTK_CSS_FILTER_HUE_ROTATE:
      dest->hue_rotate.value = _gtk_css_value_compute (src->hue_rotate.value, property_id, provider, style, parent_style);
      return dest->hue_rotate.value == src->hue_rotate.value;

    case GTK_CSS_FILTER_INVERT:
      dest->invert.value = _gtk_css_value_compute (src->invert.value, property_id, provider, style, parent_style);
      return dest->invert.value == src->invert.value;

    case GTK_CSS_FILTER_OPACITY:
      dest->opacity.value = _gtk_css_value_compute (src->opacity.value, property_id, provider, style, parent_style);
      return dest->opacity.value == src->opacity.value;

    case GTK_CSS_FILTER_SATURATE:
      dest->saturate.value = _gtk_css_value_compute (src->saturate.value, property_id, provider, style, parent_style);
      return dest->saturate.value == src->saturate.value;

    case GTK_CSS_FILTER_SEPIA:
      dest->sepia.value = _gtk_css_value_compute (src->sepia.value, property_id, provider, style, parent_style);
      return dest->sepia.value == src->sepia.value;

    case GTK_CSS_FILTER_BLUR:
      dest->blur.value = _gtk_css_value_compute (src->blur.value, property_id, provider, style, parent_style);
      return dest->blur.value == src->blur.value;

    case GTK_CSS_FILTER_DROP_SHADOW:
      dest->drop_shadow.value = _gtk_css_value_compute (src->drop_shadow.value, property_id, provider, style, parent_style);
      return dest->drop_shadow.value == src->drop_shadow.value;

    case GTK_CSS_FILTER_NONE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static GtkCssValue *
gtk_css_value_filter_compute (GtkCssValue      *value,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssValue *result;
  gboolean changes;
  guint i;

  /* Special case the 99% case of "none" */
  if (gtk_css_filter_value_is_none (value))
    return _gtk_css_value_ref (value);

  changes = FALSE;
  result = gtk_css_filter_value_alloc (value->n_filters);

  for (i = 0; i < value->n_filters; i++)
    {
      changes |= !gtk_css_filter_compute (&result->filters[i],
                                          &value->filters[i],
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
gtk_css_filter_equal (const GtkCssFilter *filter1,
                      const GtkCssFilter *filter2)
{
  if (filter1->type != filter2->type)
    return FALSE;

  switch (filter1->type)
    {
    case GTK_CSS_FILTER_BRIGHTNESS:
      return _gtk_css_value_equal (filter1->brightness.value, filter2->brightness.value);

    case GTK_CSS_FILTER_CONTRAST:
      return _gtk_css_value_equal (filter1->contrast.value, filter2->contrast.value);

    case GTK_CSS_FILTER_GRAYSCALE:
      return _gtk_css_value_equal (filter1->grayscale.value, filter2->grayscale.value);

    case GTK_CSS_FILTER_HUE_ROTATE:
      return _gtk_css_value_equal (filter1->hue_rotate.value, filter2->hue_rotate.value);

    case GTK_CSS_FILTER_INVERT:
      return _gtk_css_value_equal (filter1->invert.value, filter2->invert.value);

    case GTK_CSS_FILTER_OPACITY:
      return _gtk_css_value_equal (filter1->opacity.value, filter2->opacity.value);

    case GTK_CSS_FILTER_SATURATE:
      return _gtk_css_value_equal (filter1->saturate.value, filter2->saturate.value);

    case GTK_CSS_FILTER_SEPIA:
      return _gtk_css_value_equal (filter1->sepia.value, filter2->sepia.value);

    case GTK_CSS_FILTER_BLUR:
      return _gtk_css_value_equal (filter1->blur.value, filter2->blur.value);

    case GTK_CSS_FILTER_DROP_SHADOW:
      return _gtk_css_value_equal (filter1->drop_shadow.value, filter2->drop_shadow.value);

    case GTK_CSS_FILTER_NONE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static gboolean
gtk_css_value_filter_equal (const GtkCssValue *value1,
                            const GtkCssValue *value2)
{
  const GtkCssValue *larger;
  guint i, n;

  n = MIN (value1->n_filters, value2->n_filters);
  for (i = 0; i < n; i++)
    {
      if (!gtk_css_filter_equal (&value1->filters[i], &value2->filters[i]))
        return FALSE;
    }

  larger = value1->n_filters > value2->n_filters ? value1 : value2;

  for (; i < larger->n_filters; i++)
    {
      GtkCssFilter filter;

      gtk_css_filter_init_identity (&filter, &larger->filters[i]);

      if (!gtk_css_filter_equal (&larger->filters[i], &filter))
        {
          gtk_css_filter_clear (&filter);
          return FALSE;
        }

      gtk_css_filter_clear (&filter);
    }

  return TRUE;
}

static void
gtk_css_filter_transition (GtkCssFilter       *result,
                           const GtkCssFilter *start,
                           const GtkCssFilter *end,
                           guint               property_id,
                           double              progress)
{
  result->type = start->type;

  switch (start->type)
    {
    case GTK_CSS_FILTER_BRIGHTNESS:
      result->brightness.value = _gtk_css_value_transition (start->brightness.value, end->brightness.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_CONTRAST:
      result->contrast.value = _gtk_css_value_transition (start->contrast.value, end->contrast.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_GRAYSCALE:
      result->grayscale.value = _gtk_css_value_transition (start->grayscale.value, end->grayscale.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_HUE_ROTATE:
      result->hue_rotate.value = _gtk_css_value_transition (start->hue_rotate.value, end->hue_rotate.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_INVERT:
      result->invert.value = _gtk_css_value_transition (start->invert.value, end->invert.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_OPACITY:
      result->opacity.value = _gtk_css_value_transition (start->opacity.value, end->opacity.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_SATURATE:
      result->saturate.value = _gtk_css_value_transition (start->saturate.value, end->saturate.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_SEPIA:
      result->sepia.value = _gtk_css_value_transition (start->sepia.value, end->sepia.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_BLUR:
      result->blur.value = _gtk_css_value_transition (start->blur.value, end->blur.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_DROP_SHADOW:
      result->drop_shadow.value = _gtk_css_value_transition (start->drop_shadow.value, end->drop_shadow.value, property_id, progress);
      break;

    case GTK_CSS_FILTER_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkCssValue *
gtk_css_value_filter_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  GtkCssValue *result;
  guint i, n;

  if (gtk_css_filter_value_is_none (start))
    {
      if (gtk_css_filter_value_is_none (end))
        return _gtk_css_value_ref (start);

      n = 0;
    }
  else if (gtk_css_filter_value_is_none (end))
    {
      n = 0;
    }
  else
    {
      n = MIN (start->n_filters, end->n_filters);
    }

  /* Check filters are compatible. If not, transition between
   * their result matrices.
   */
  for (i = 0; i < n; i++)
    {
      if (start->filters[i].type != end->filters[i].type)
        {
          /* XXX: can we improve this? */
          return NULL;
        }
    }

  result = gtk_css_filter_value_alloc (MAX (start->n_filters, end->n_filters));

  for (i = 0; i < n; i++)
    {
      gtk_css_filter_transition (&result->filters[i],
                                 &start->filters[i],
                                 &end->filters[i],
                                 property_id,
                                 progress);
    }

  for (; i < start->n_filters; i++)
    {
      GtkCssFilter filter;

      gtk_css_filter_init_identity (&filter, &start->filters[i]);
      gtk_css_filter_transition (&result->filters[i],
                                 &start->filters[i],
                                 &filter,
                                 property_id,
                                 progress);
      gtk_css_filter_clear (&filter);
    }
  for (; i < end->n_filters; i++)
    {
      GtkCssFilter filter;

      gtk_css_filter_init_identity (&filter, &end->filters[i]);
      gtk_css_filter_transition (&result->filters[i],
                                 &filter,
                                 &end->filters[i],
                                 property_id,
                                 progress);
      gtk_css_filter_clear (&filter);
    }

  g_assert (i == MAX (start->n_filters, end->n_filters));

  return result;
}

static void
gtk_css_filter_print (const GtkCssFilter *filter,
                      GString            *string)
{
  switch (filter->type)
    {
    case GTK_CSS_FILTER_BRIGHTNESS:
      g_string_append (string, "brightness(");
      _gtk_css_value_print (filter->brightness.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_CONTRAST:
      g_string_append (string, "contrast(");
      _gtk_css_value_print (filter->contrast.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_GRAYSCALE:
      g_string_append (string, "grayscale(");
      _gtk_css_value_print (filter->grayscale.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_HUE_ROTATE:
      g_string_append (string, "hue-rotate(");
      _gtk_css_value_print (filter->hue_rotate.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_INVERT:
      g_string_append (string, "invert(");
      _gtk_css_value_print (filter->invert.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_OPACITY:
      g_string_append (string, "opacity(");
      _gtk_css_value_print (filter->opacity.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_SATURATE:
      g_string_append (string, "saturate(");
      _gtk_css_value_print (filter->saturate.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_SEPIA:
      g_string_append (string, "sepia(");
      _gtk_css_value_print (filter->sepia.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_BLUR:
      g_string_append (string, "blur(");
      _gtk_css_value_print (filter->blur.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_DROP_SHADOW:
      g_string_append (string, "drop-shadow(");
      _gtk_css_value_print (filter->drop_shadow.value, string);
      g_string_append (string, ")");
      break;

    case GTK_CSS_FILTER_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_css_value_filter_print (const GtkCssValue *value,
                            GString           *string)
{
  guint i;

  if (gtk_css_filter_value_is_none (value))
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->n_filters; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');

      gtk_css_filter_print (&value->filters[i], string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_FILTER = {
  "GtkCssFilterValue",
  gtk_css_value_filter_free,
  gtk_css_value_filter_compute,
  gtk_css_value_filter_equal,
  gtk_css_value_filter_transition,
  NULL,
  NULL,
  gtk_css_value_filter_print
};

static GtkCssValue filter_none_singleton = { &GTK_CSS_VALUE_FILTER, 1, TRUE, 0, {  { GTK_CSS_FILTER_NONE } } };

static GtkCssValue *
gtk_css_filter_value_alloc (guint n_filters)
{
  GtkCssValue *result;

  g_return_val_if_fail (n_filters > 0, NULL);

  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_FILTER, sizeof (GtkCssValue) + sizeof (GtkCssFilter) * (n_filters - 1));
  result->n_filters = n_filters;

  return result;
}

GtkCssValue *
gtk_css_filter_value_new_none (void)
{
  return _gtk_css_value_ref (&filter_none_singleton);
}

static gboolean
gtk_css_filter_value_is_none (const GtkCssValue *value)
{
  return value->n_filters == 0;
}

static guint
gtk_css_filter_parse_number (GtkCssParser *parser,
                             guint         n,
                             gpointer      data)
{
  GtkCssValue **values = data;

  values[n] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER | GTK_CSS_PARSE_PERCENT | GTK_CSS_POSITIVE_ONLY);
  if (values[n] == NULL)
    return 0;

  return 1;
}

static guint
gtk_css_filter_parse_length (GtkCssParser *parser,
                             guint         n,
                             gpointer      data)
{
  GtkCssValue **values = data;

  values[n] = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH | GTK_CSS_POSITIVE_ONLY);
  if (values[n] == NULL)
    return 0;

  return 1;
}

static guint
gtk_css_filter_parse_angle (GtkCssParser *parser,
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
gtk_css_filter_parse_shadow (GtkCssParser *parser,
                             guint         n,
                             gpointer      data)
{
  GtkCssValue **values = data;

  values[n] = gtk_css_shadow_value_parse_filter (parser);
  if (values[n] == NULL)
    return 0;

  return 1;
}

GtkCssValue *
gtk_css_filter_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value;
  GArray *array;
  guint i;
  gboolean computed = TRUE;

  if (gtk_css_parser_try_ident (parser, "none"))
    return gtk_css_filter_value_new_none ();

  array = g_array_new (FALSE, FALSE, sizeof (GtkCssFilter));

  while (TRUE)
    {
      GtkCssFilter filter;

      if (gtk_css_parser_has_function (parser, "blur"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_length, &filter.blur.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_BLUR;
          computed = computed && gtk_css_value_is_computed (filter.blur.value);
        }
      else if (gtk_css_parser_has_function (parser, "brightness"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_number, &filter.brightness.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_BRIGHTNESS;
          computed = computed && gtk_css_value_is_computed (filter.brightness.value);
        }
      else if (gtk_css_parser_has_function (parser, "contrast"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_number, &filter.contrast.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_CONTRAST;
          computed = computed && gtk_css_value_is_computed (filter.contrast.value);
        }
      else if (gtk_css_parser_has_function (parser, "grayscale"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_number, &filter.grayscale.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_GRAYSCALE;
          computed = computed && gtk_css_value_is_computed (filter.grayscale.value);
        }
      else if (gtk_css_parser_has_function (parser, "hue-rotate"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_angle, &filter.hue_rotate.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_HUE_ROTATE;
          computed = computed && gtk_css_value_is_computed (filter.hue_rotate.value);
        }
      else if (gtk_css_parser_has_function (parser, "invert"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_number, &filter.invert.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_INVERT;
          computed = computed && gtk_css_value_is_computed (filter.invert.value);
        }
      else if (gtk_css_parser_has_function (parser, "opacity"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_number, &filter.opacity.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_OPACITY;
          computed = computed && gtk_css_value_is_computed (filter.opacity.value);
        }
      else if (gtk_css_parser_has_function (parser, "saturate"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_number, &filter.saturate.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_SATURATE;
          computed = computed && gtk_css_value_is_computed (filter.saturate.value);
        }
      else if (gtk_css_parser_has_function (parser, "sepia"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_number, &filter.sepia.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_SEPIA;
          computed = computed && gtk_css_value_is_computed (filter.sepia.value);
        }
      else if (gtk_css_parser_has_function (parser, "drop-shadow"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_filter_parse_shadow, &filter.drop_shadow.value))
            goto fail;

          filter.type = GTK_CSS_FILTER_DROP_SHADOW;
          computed = computed && gtk_css_value_is_computed (filter.drop_shadow.value);
        }
      else
        {
          break;
        }

      g_array_append_val (array, filter);
    }

  if (array->len == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a filter");
      goto fail;
    }

  value = gtk_css_filter_value_alloc (array->len);
  memcpy (value->filters, array->data, sizeof (GtkCssFilter) * array->len);
  value->is_computed = computed;

  g_array_free (array, TRUE);

  return value;

fail:
  for (i = 0; i < array->len; i++)
    {
      gtk_css_filter_clear (&g_array_index (array, GtkCssFilter, i));
    }
  g_array_free (array, TRUE);
  return NULL;
}

void
gtk_css_filter_value_push_snapshot (const GtkCssValue *filter,
                                    GtkSnapshot       *snapshot)
{
  graphene_matrix_t matrix;
  graphene_vec4_t offset;
  gboolean all_opacity;
  int i, j;

  if (gtk_css_filter_value_is_none (filter))
    return;

  i = 0;
  while (i < filter->n_filters)
    {
      j = gtk_css_filter_value_compute_matrix (filter, i, &matrix, &offset, &all_opacity);
      if (i < j)
        {
          if (all_opacity)
            gtk_snapshot_push_opacity (snapshot, graphene_matrix_get_value (&matrix, 3, 3));
          else
            gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);
        }


      if (j < filter->n_filters)
        {
          if (filter->filters[j].type == GTK_CSS_FILTER_BLUR)
            {
              double std_dev = _gtk_css_number_value_get (filter->filters[j].blur.value, 100.0);
              gtk_snapshot_push_blur (snapshot, 2 * std_dev);
            }
          else if (filter->filters[j].type == GTK_CSS_FILTER_DROP_SHADOW)
            {
              gtk_css_shadow_value_push_snapshot (filter->filters[j].drop_shadow.value, snapshot);
            }
          else
            g_warning ("Don't know how to handle filter type %d", filter->filters[j].type);
        }

      i = j + 1;
    }
}

void
gtk_css_filter_value_pop_snapshot (const GtkCssValue *filter,
                                   GtkSnapshot       *snapshot)
{
  int i, j;

  if (gtk_css_filter_value_is_none (filter))
    return;

  i = 0;
  while (i < filter->n_filters)
    {
      for (j = i; j < filter->n_filters; j++)
        {
          if (filter->filters[j].type == GTK_CSS_FILTER_BLUR ||
              filter->filters[j].type == GTK_CSS_FILTER_DROP_SHADOW)
            break;
        }

      if (i < j)
        gtk_snapshot_pop (snapshot);

      if (j < filter->n_filters)
        {
          if (filter->filters[j].type == GTK_CSS_FILTER_BLUR)
            gtk_snapshot_pop (snapshot);
          else if (filter->filters[j].type == GTK_CSS_FILTER_DROP_SHADOW)
            gtk_css_shadow_value_pop_snapshot (filter->filters[j].drop_shadow.value, snapshot);
        }

      i = j + 1;
    }
}
