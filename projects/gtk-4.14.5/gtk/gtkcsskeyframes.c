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

#include "gtkcsskeyframesprivate.h"

#include "gtkcssstyleprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkstylepropertyprivate.h"

#include "gtkprivate.h"

#include <stdlib.h>
#include <string.h>

struct _GtkCssKeyframes {
  int ref_count;                /* ref count */
  int n_keyframes;              /* number of keyframes (at least 2 for 0% and 100% */
  double *keyframe_progress;    /* ordered array of n_keyframes of [0..1] */
  int n_properties;             /* number of properties used by keyframes */
  guint *property_ids;          /* ordered array of n_properties property ids */
  GtkCssValue **values;         /* 2D array: n_keyframes * n_properties of (value or NULL) for all the keyframes */
};

#define KEYFRAMES_VALUE(keyframes, k, p) ((keyframes)->values[(k) * (keyframes)->n_properties + (p)])

GtkCssKeyframes *
_gtk_css_keyframes_ref (GtkCssKeyframes *keyframes)
{
  g_return_val_if_fail (keyframes != NULL, NULL);

  keyframes->ref_count++;

  return keyframes;
}

void
_gtk_css_keyframes_unref (GtkCssKeyframes *keyframes)
{
  guint k, p;

  g_return_if_fail (keyframes != NULL);

  keyframes->ref_count--;
  if (keyframes->ref_count > 0)
    return;

  g_free (keyframes->keyframe_progress);
  g_free (keyframes->property_ids);

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      for (p = 0; p < keyframes->n_properties; p++)
        {
          _gtk_css_value_unref (KEYFRAMES_VALUE (keyframes, k, p));
          KEYFRAMES_VALUE (keyframes, k, p) = NULL;
        }
    }
  g_free (keyframes->values);

  g_free (keyframes);
}

static guint
gtk_css_keyframes_add_keyframe (GtkCssKeyframes *keyframes,
                                double           progress)
{
  guint k, p;

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      if (keyframes->keyframe_progress[k] == progress)
        {
          for (p = 0; p < keyframes->n_properties; p++)
            {
              if (KEYFRAMES_VALUE (keyframes, k, p) == NULL)
                continue;

              _gtk_css_value_unref (KEYFRAMES_VALUE (keyframes, k, p));
              KEYFRAMES_VALUE (keyframes, k, p) = NULL;

              /* XXX: GC properties that are now unset
               * in all keyframes?
               */
            }
          return k;
        }
      else if (keyframes->keyframe_progress[k] > progress)
        break;
    }

  keyframes->n_keyframes++;
  keyframes->keyframe_progress = g_realloc (keyframes->keyframe_progress, sizeof (double) * keyframes->n_keyframes);
  memmove (keyframes->keyframe_progress + k + 1, keyframes->keyframe_progress + k, sizeof (double) * (keyframes->n_keyframes - k - 1));
  keyframes->keyframe_progress[k] = progress;

  if (keyframes->n_properties)
    {
      gsize size = sizeof (GtkCssValue *) * keyframes->n_properties;
      
      keyframes->values = g_realloc (keyframes->values, sizeof (GtkCssValue *) * keyframes->n_keyframes * keyframes->n_properties);
      memmove (&KEYFRAMES_VALUE (keyframes, k + 1, 0), &KEYFRAMES_VALUE (keyframes, k, 0), size * (keyframes->n_keyframes - k - 1));
      memset (&KEYFRAMES_VALUE (keyframes, k, 0), 0, size);
    }

  return k;
}

static guint
gtk_css_keyframes_lookup_property (GtkCssKeyframes *keyframes,
                                   guint            property_id)
{
  guint p;

  for (p = 0; p < keyframes->n_properties; p++)
    {
      if (keyframes->property_ids[p] == property_id)
        return p;
      else if (keyframes->property_ids[p] > property_id)
        break;
    }

  keyframes->n_properties++;
  keyframes->property_ids = g_realloc (keyframes->property_ids, sizeof (guint) * keyframes->n_properties);
  memmove (keyframes->property_ids + p + 1, keyframes->property_ids + p, sizeof (guint) * (keyframes->n_properties - p - 1));
  keyframes->property_ids[p] = property_id;

  if (keyframes->n_properties > 1)
    {
      guint old_n_properties = keyframes->n_properties - 1;
      int k;
      
      keyframes->values = g_realloc (keyframes->values, sizeof (GtkCssValue *) * keyframes->n_keyframes * keyframes->n_properties);

      if (p + 1 < keyframes->n_properties)
        {
          memmove (&KEYFRAMES_VALUE (keyframes, keyframes->n_keyframes - 1, p + 1),
                   &keyframes->values[(keyframes->n_keyframes - 1) * old_n_properties + p],
                   sizeof (GtkCssValue *) * (keyframes->n_properties - p - 1));
        }
      KEYFRAMES_VALUE (keyframes, keyframes->n_keyframes - 1, p) = NULL;

      for (k = keyframes->n_keyframes - 2; k >= 0; k--)
        {
          memmove (&KEYFRAMES_VALUE (keyframes, k, p + 1),
                   &keyframes->values[k * old_n_properties + p],
                   sizeof (GtkCssValue *) * old_n_properties);
          KEYFRAMES_VALUE (keyframes, k, p) = NULL;
        }
    }
  else
    {
      keyframes->values = g_new0 (GtkCssValue *, keyframes->n_keyframes);
    }

  return p;
}

static GtkCssKeyframes *
gtk_css_keyframes_alloc (void)
{
  GtkCssKeyframes *keyframes;

  keyframes = g_new0 (GtkCssKeyframes, 1);
  keyframes->ref_count = 1;

  return keyframes;
}

static GtkCssKeyframes *
gtk_css_keyframes_new (void)
{
  GtkCssKeyframes *keyframes;

  keyframes = gtk_css_keyframes_alloc ();

  gtk_css_keyframes_add_keyframe (keyframes, 0);
  gtk_css_keyframes_add_keyframe (keyframes, 1);

  return keyframes;
}

static gboolean
keyframes_set_value (GtkCssKeyframes     *keyframes,
                     guint                k,
                     GtkCssStyleProperty *property,
                     GtkCssValue         *value)
{
  guint p;

  if (!_gtk_css_style_property_is_animated (property))
    return FALSE;

  p = gtk_css_keyframes_lookup_property (keyframes, _gtk_css_style_property_get_id (property));
  
  if (KEYFRAMES_VALUE (keyframes, k, p))
    _gtk_css_value_unref (KEYFRAMES_VALUE (keyframes, k, p));

  KEYFRAMES_VALUE (keyframes, k, p) = _gtk_css_value_ref (value);

  return TRUE;
}

static gboolean
gtk_css_keyframes_parse_declaration (GtkCssKeyframes *keyframes,
                                     guint            k,
                                     GtkCssParser    *parser)
{
  GtkStyleProperty *property;
  GtkCssValue *value;
  char *name;

  name = gtk_css_parser_consume_ident (parser);
  if (name == NULL)
    {
      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        gtk_css_parser_error_syntax (parser, "Expected a property name");
      return FALSE;
    }

  property = _gtk_style_property_lookup (name);
  if (property == NULL)
    {
      gtk_css_parser_error_value (parser, "No property named '%s'", name);
      g_free (name);
      return FALSE;
    }

  g_free (name);

  if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_parser_error_syntax (parser, "Expected a ':'");
      return FALSE;
    }

  value = _gtk_style_property_parse_value (property, parser);
  if (value == NULL)
    return FALSE;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (parser, "Junk at end of value");
      _gtk_css_value_unref (value);
      return FALSE;
    }

  if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
    {
      GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
      gboolean animatable = FALSE;
      guint i;

      for (i = 0; i < _gtk_css_shorthand_property_get_n_subproperties (shorthand); i++)
        {
          GtkCssStyleProperty *child = _gtk_css_shorthand_property_get_subproperty (shorthand, i);
          GtkCssValue *sub = _gtk_css_array_value_get_nth (value, i);
          
          animatable |= keyframes_set_value (keyframes, k, child, sub);
        }

      if (!animatable)
        gtk_css_parser_error_value (parser, "shorthand '%s' cannot be animated", _gtk_style_property_get_name (property));
    }
  else if (GTK_IS_CSS_STYLE_PROPERTY (property))
    {
      if (!keyframes_set_value (keyframes, k, GTK_CSS_STYLE_PROPERTY (property), value))
        gtk_css_parser_error_value (parser, "Cannot animate property '%s'", _gtk_style_property_get_name (property));
    }
  else
    {
      g_assert_not_reached ();
    }
      
  _gtk_css_value_unref (value);

  return TRUE;
}

static gboolean
gtk_css_keyframes_parse_block (GtkCssKeyframes *keyframes,
                               guint            k,
                               GtkCssParser    *parser)
{
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_parser_error_syntax (parser, "Expected '{'");
      return FALSE;
    }

  gtk_css_parser_start_block (parser);

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_EOF);
      gtk_css_keyframes_parse_declaration (keyframes, k, parser);
      gtk_css_parser_end_block (parser);
    }

  gtk_css_parser_end_block (parser);

  return TRUE;
}

GtkCssKeyframes *
_gtk_css_keyframes_parse (GtkCssParser *parser)
{
  GtkCssKeyframes *keyframes;
  double progress;
  guint k;

  g_return_val_if_fail (parser != NULL, NULL);

  keyframes = gtk_css_keyframes_new ();

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      if (gtk_css_parser_try_ident (parser, "from"))
        progress = 0;
      else if (gtk_css_parser_try_ident (parser, "to"))
        progress = 1;
      else if (gtk_css_parser_consume_percentage (parser, &progress))
        {
          if (progress < 0 || progress > 100)
            {
              /* XXX: should we skip over the block here? */
              gtk_css_parser_error_value (parser, "percentages must be between 0%% and 100%%");
              _gtk_css_keyframes_unref (keyframes);
              return NULL;
            }
          progress /= 100;
        }
      else
        {
          _gtk_css_keyframes_unref (keyframes);
          return NULL;
        }

      k = gtk_css_keyframes_add_keyframe (keyframes, progress);

      if (!gtk_css_keyframes_parse_block (keyframes, k, parser))
        {
          _gtk_css_keyframes_unref (keyframes);
          return NULL;
        }
    }

  return keyframes;
}

static int
compare_property_by_name (gconstpointer a,
                          gconstpointer b,
                          gpointer      data)
{
  GtkCssKeyframes *keyframes = data;

  return strcmp (_gtk_style_property_get_name (GTK_STYLE_PROPERTY (
                    _gtk_css_style_property_lookup_by_id (keyframes->property_ids[*(const guint *) a]))),
                 _gtk_style_property_get_name (GTK_STYLE_PROPERTY (
                    _gtk_css_style_property_lookup_by_id (keyframes->property_ids[*(const guint *) b]))));
}

void
_gtk_css_keyframes_print (GtkCssKeyframes *keyframes,
                          GString         *string)
{
  guint k, p;
  guint *sorted;

  g_return_if_fail (keyframes != NULL);
  g_return_if_fail (string != NULL);

  sorted = g_new (guint, keyframes->n_properties);
  for (p = 0; p < keyframes->n_properties; p++)
    sorted[p] = p;
  g_qsort_with_data (sorted, keyframes->n_properties, sizeof (guint), compare_property_by_name, keyframes);

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      /* useful for 0% and 100% which might be empty */
      gboolean opened = FALSE;

      for (p = 0; p < keyframes->n_properties; p++)
        {
          if (KEYFRAMES_VALUE (keyframes, k, sorted[p]) == NULL)
            continue;

          if (!opened)
            {
              if (keyframes->keyframe_progress[k] == 0.0)
                g_string_append (string, "  from {\n");
              else if (keyframes->keyframe_progress[k] == 1.0)
                g_string_append (string, "  to {\n");
              else
                g_string_append_printf (string, "  %g%% {\n", keyframes->keyframe_progress[k] * 100);
              opened = TRUE;
            }
          
          g_string_append_printf (string, "    %s: ", _gtk_style_property_get_name (
                                                        GTK_STYLE_PROPERTY (
                                                          _gtk_css_style_property_lookup_by_id (
                                                            keyframes->property_ids[sorted[p]]))));
          _gtk_css_value_print (KEYFRAMES_VALUE (keyframes, k, sorted[p]), string);
          g_string_append (string, ";\n");
        }

      if (opened)
        g_string_append (string, "  }\n");
    }

  g_free (sorted);
}

GtkCssKeyframes *
_gtk_css_keyframes_compute (GtkCssKeyframes  *keyframes,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *style,
                            GtkCssStyle      *parent_style)
{
  GtkCssKeyframes *resolved;
  guint k, p;

  g_return_val_if_fail (keyframes != NULL, NULL);
  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);
  g_return_val_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style), NULL);

  resolved = gtk_css_keyframes_alloc ();
  resolved->n_keyframes = keyframes->n_keyframes;
  resolved->keyframe_progress = g_memdup2 (keyframes->keyframe_progress, keyframes->n_keyframes * sizeof (double));
  resolved->n_properties = keyframes->n_properties;
  resolved->property_ids = g_memdup2 (keyframes->property_ids, keyframes->n_properties * sizeof (guint));
  resolved->values = g_new0 (GtkCssValue *, resolved->n_keyframes * resolved->n_properties);

  for (p = 0; p < resolved->n_properties; p++)
    {
      for (k = 0; k < resolved->n_keyframes; k++)
        {
          if (KEYFRAMES_VALUE (keyframes, k, p) == NULL)
            continue;

          KEYFRAMES_VALUE (resolved, k, p) =  _gtk_css_value_compute (KEYFRAMES_VALUE (keyframes, k, p),
                                                                      resolved->property_ids[p],
                                                                      provider,
                                                                      style,
                                                                      parent_style);
        }
    }

  return resolved;
}

guint
_gtk_css_keyframes_get_n_properties (GtkCssKeyframes *keyframes)
{
  g_return_val_if_fail (keyframes != NULL, 0);

  return keyframes->n_properties;
}

guint
_gtk_css_keyframes_get_property_id (GtkCssKeyframes *keyframes,
                                    guint            id)
{
  g_return_val_if_fail (keyframes != NULL, 0);
  g_return_val_if_fail (id < keyframes->n_properties, 0);

  return keyframes->property_ids[id];
}

GtkCssValue *
_gtk_css_keyframes_get_value (GtkCssKeyframes *keyframes,
                              guint            id,
                              double           progress,
                              GtkCssValue     *default_value)
{
  GtkCssValue *start_value, *end_value, *result;
  double start_progress, end_progress;
  guint k;

  g_return_val_if_fail (keyframes != NULL, 0);
  g_return_val_if_fail (id < keyframes->n_properties, 0);

  start_value = default_value;
  start_progress = 0.0;
  end_value = default_value;
  end_progress = 1.0;

  for (k = 0; k < keyframes->n_keyframes; k++)
    {
      if (KEYFRAMES_VALUE (keyframes, k, id) == NULL)
        continue;

      if (keyframes->keyframe_progress[k] == progress)
        {
          return _gtk_css_value_ref (KEYFRAMES_VALUE (keyframes, k, id));
        }
      else if (keyframes->keyframe_progress[k] < progress)
        {
          start_value = KEYFRAMES_VALUE (keyframes, k, id);
          start_progress = keyframes->keyframe_progress[k];
        }
      else
        {
          end_value = KEYFRAMES_VALUE (keyframes, k, id);
          end_progress = keyframes->keyframe_progress[k];
          break;
        }
    }

  progress = (progress - start_progress) / (end_progress - start_progress);

  result = _gtk_css_value_transition (start_value,
                                      end_value,
                                      keyframes->property_ids[id],
                                      progress);

  /* XXX: Dear spec, what's the correct thing to do here? */
  if (result == NULL)
    return _gtk_css_value_ref (start_value);

  return result;
}

