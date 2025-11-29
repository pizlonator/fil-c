/* gtkpango.c - pango-related utilities
 *
 * Copyright (c) 2010 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */
/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtkpangoprivate.h"
#include <pango/pangocairo.h>
#include "gtkbuilderprivate.h"

static gboolean
attr_list_merge_filter (PangoAttribute *attribute,
                        gpointer        list)
{
  pango_attr_list_change (list, pango_attribute_copy (attribute));
  return FALSE;
}

/*
 * _gtk_pango_attr_list_merge:
 * @into: (nullable): a `PangoAttrList` where attributes are merged
 * @from: (nullable): a `PangoAttrList` with the attributes to merge
 *
 * Merges attributes from @from into @into.
 *
 * Returns: the merged list.
 */
PangoAttrList *
_gtk_pango_attr_list_merge (PangoAttrList *into,
                            PangoAttrList *from)
{
  if (from)
    {
      if (into)
        pango_attr_list_filter (from, attr_list_merge_filter, into);
      else
        return pango_attr_list_ref (from);
    }

  return into;
}

static PangoAttribute *
attribute_from_text (GtkBuilder  *builder,
                     const char  *name,
                     const char  *value,
                     GError     **error)
{
  PangoAttribute *attribute = NULL;
  PangoAttrType type;
  PangoLanguage *language;
  PangoFontDescription *font_desc;
  GdkRGBA *color;
  GValue val = G_VALUE_INIT;

  if (!gtk_builder_value_from_string_type (builder, PANGO_TYPE_ATTR_TYPE, name, &val, error))
    return NULL;

  type = g_value_get_enum (&val);
  g_value_unset (&val);

  switch (type)
    {
      /* PangoAttrLanguage */
    case PANGO_ATTR_LANGUAGE:
      if ((language = pango_language_from_string (value)))
        {
          attribute = pango_attr_language_new (language);
          g_value_init (&val, G_TYPE_INT);
        }
      break;
      /* PangoAttrInt */
    case PANGO_ATTR_STYLE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STYLE, value, &val, error))
        attribute = pango_attr_style_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_WEIGHT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_WEIGHT, value, &val, error))
        attribute = pango_attr_weight_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_VARIANT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_VARIANT, value, &val, error))
        attribute = pango_attr_variant_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_STRETCH:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STRETCH, value, &val, error))
        attribute = pango_attr_stretch_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_UNDERLINE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_UNDERLINE, value, &val, NULL))
        attribute = pango_attr_underline_new (g_value_get_enum (&val));
      else
        {
          /* XXX: allow boolean for backwards compat, so ignore error */
          /* Deprecate this somehow */
          g_value_unset (&val);
          if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
            attribute = pango_attr_underline_new (g_value_get_boolean (&val));
        }
      break;
    case PANGO_ATTR_STRIKETHROUGH:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_strikethrough_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_GRAVITY:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY, value, &val, error))
        attribute = pango_attr_gravity_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_GRAVITY_HINT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY_HINT, value, &val, error))
        attribute = pango_attr_gravity_hint_new (g_value_get_enum (&val));
      break;
      /* PangoAttrString */
    case PANGO_ATTR_FAMILY:
      attribute = pango_attr_family_new (value);
      g_value_init (&val, G_TYPE_INT);
      break;

      /* PangoAttrSize */
    case PANGO_ATTR_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_size_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_ABSOLUTE_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_size_new_absolute (g_value_get_int (&val));
      break;

      /* PangoAttrFontDesc */
    case PANGO_ATTR_FONT_DESC:
      if ((font_desc = pango_font_description_from_string (value)))
        {
          attribute = pango_attr_font_desc_new (font_desc);
          pango_font_description_free (font_desc);
          g_value_init (&val, G_TYPE_INT);
        }
      break;
      /* PangoAttrColor */
    case PANGO_ATTR_FOREGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_foreground_new (color->red * 65535,
                                                 color->green * 65535,
                                                 color->blue * 65535);
        }
      break;
    case PANGO_ATTR_BACKGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_background_new (color->red * 65535,
                                                 color->green * 65535,
                                                 color->blue * 65535);
        }
      break;
    case PANGO_ATTR_UNDERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_underline_color_new (color->red * 65535,
                                                      color->green * 65535,
                                                      color->blue * 65535);
        }
      break;
    case PANGO_ATTR_STRIKETHROUGH_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_strikethrough_color_new (color->red * 65535,
                                                          color->green * 65535,
                                                          color->blue * 65535);
        }
      break;
      /* PangoAttrShape */
    case PANGO_ATTR_SHAPE:
      /* Unsupported for now */
      break;
      /* PangoAttrFloat */
    case PANGO_ATTR_SCALE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_DOUBLE, value, &val, error))
        attribute = pango_attr_scale_new (g_value_get_double (&val));
      break;
    case PANGO_ATTR_LETTER_SPACING:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_letter_spacing_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_RISE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_rise_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_FALLBACK:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_fallback_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_FONT_FEATURES:
      attribute = pango_attr_font_features_new (value);
      break;
    case PANGO_ATTR_FOREGROUND_ALPHA:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_foreground_alpha_new ((guint16)g_value_get_int (&val));
      break;
    case PANGO_ATTR_BACKGROUND_ALPHA:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_background_alpha_new ((guint16)g_value_get_int (&val));
      break;
    case PANGO_ATTR_ALLOW_BREAKS:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_allow_breaks_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_SHOW:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_SHOW_FLAGS, value, &val, error))
        attribute = pango_attr_show_new (g_value_get_flags (&val));
      break;
    case PANGO_ATTR_INSERT_HYPHENS:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_insert_hyphens_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_OVERLINE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_OVERLINE, value, &val, NULL))
        attribute = pango_attr_overline_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_OVERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_overline_color_new (color->red * 65535,
                                                     color->green * 65535,
                                                     color->blue * 65535);
        }
      break;
    case PANGO_ATTR_LINE_HEIGHT:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_DOUBLE, value, &val, error))
        attribute = pango_attr_line_height_new (g_value_get_double (&val));
      break;
    case PANGO_ATTR_ABSOLUTE_LINE_HEIGHT:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_line_height_new_absolute (g_value_get_int (&val) * PANGO_SCALE);
      break;
    case PANGO_ATTR_TEXT_TRANSFORM:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_TEXT_TRANSFORM, value, &val, error))
        attribute = pango_attr_text_transform_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_WORD:
      attribute = pango_attr_word_new ();
      break;
    case PANGO_ATTR_SENTENCE:
      attribute = pango_attr_sentence_new ();
      break;
    case PANGO_ATTR_BASELINE_SHIFT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_BASELINE_SHIFT, value, &val, NULL))
        attribute = pango_attr_baseline_shift_new (g_value_get_enum (&val));
      else if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, NULL))
        attribute = pango_attr_baseline_shift_new (g_value_get_enum (&val));
      else
        g_set_error (error,
                     GTK_BUILDER_ERROR,
                     GTK_BUILDER_ERROR_INVALID_VALUE,
                     "Could not parse '%s' as baseline shift value", value);
      break;
    case PANGO_ATTR_FONT_SCALE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_FONT_SCALE, value, &val, error))
        attribute = pango_attr_font_scale_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_INVALID:
    default:
      break;
    }

  g_value_unset (&val);

  return attribute;
}

void
gtk_pango_attribute_start_element (GtkBuildableParseContext  *context,
                                   const char                *element_name,
                                   const char               **names,
                                   const char               **values,
                                   gpointer                   user_data,
                                   GError                   **error)
{
  GtkPangoAttributeParserData *data = user_data;

  if (strcmp (element_name, "attribute") == 0)
    {
      PangoAttribute *attr = NULL;
      const char *name = NULL;
      const char *value = NULL;
      const char *start = NULL;
      const char *end = NULL;
      guint start_val = 0;
      guint end_val = G_MAXUINT;
      GValue val = G_VALUE_INIT;

      if (!_gtk_builder_check_parent (data->builder, context, "attributes", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "value", &value,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "start", &start,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "end", &end,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (start)
        {
          if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, start, &val, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }
          start_val = g_value_get_uint (&val);
          g_value_unset (&val);
        }

      if (end)
        {
          if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, end, &val, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }
          end_val = g_value_get_uint (&val);
          g_value_unset (&val);
        }

      attr = attribute_from_text (data->builder, name, value, error);
      if (!attr)
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      attr->start_index = start_val;
      attr->end_index = end_val;

      if (!data->attrs)
        data->attrs = pango_attr_list_new ();

      pango_attr_list_insert (data->attrs, attr);
    }
  else if (strcmp (element_name, "attributes") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}

const char *
pango_style_to_string (PangoStyle style)
{
  switch (style)
    {
    case PANGO_STYLE_NORMAL:
      return "normal";
    case PANGO_STYLE_OBLIQUE:
      return "oblique";
    case PANGO_STYLE_ITALIC:
      return "italic";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_variant_to_string (PangoVariant variant)
{
  switch (variant)
    {
    case PANGO_VARIANT_NORMAL:
      return "normal";
    case PANGO_VARIANT_SMALL_CAPS:
      return "small_caps";
    case PANGO_VARIANT_ALL_SMALL_CAPS:
      return "all_small_caps";
    case PANGO_VARIANT_PETITE_CAPS:
      return "petite_caps";
    case PANGO_VARIANT_ALL_PETITE_CAPS:
      return "all_petite_caps";
    case PANGO_VARIANT_UNICASE:
      return "unicase";
    case PANGO_VARIANT_TITLE_CAPS:
      return "title_caps";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_stretch_to_string (PangoStretch stretch)
{
  switch (stretch)
    {
    case PANGO_STRETCH_ULTRA_CONDENSED:
      return "ultra_condensed";
    case PANGO_STRETCH_EXTRA_CONDENSED:
      return "extra_condensed";
    case PANGO_STRETCH_CONDENSED:
      return "condensed";
    case PANGO_STRETCH_SEMI_CONDENSED:
      return "semi_condensed";
    case PANGO_STRETCH_NORMAL:
      return "normal";
    case PANGO_STRETCH_SEMI_EXPANDED:
      return "semi_expanded";
    case PANGO_STRETCH_EXPANDED:
      return "expanded";
    case PANGO_STRETCH_EXTRA_EXPANDED:
      return "extra_expanded";
    case PANGO_STRETCH_ULTRA_EXPANDED:
      return "ultra_expanded";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_underline_to_string (PangoUnderline value)
{
  switch (value)
    {
    case PANGO_UNDERLINE_NONE:
      return "none";
    case PANGO_UNDERLINE_SINGLE:
    case PANGO_UNDERLINE_SINGLE_LINE:
      return "single";
    case PANGO_UNDERLINE_DOUBLE:
    case PANGO_UNDERLINE_DOUBLE_LINE:
      return "double";
    case PANGO_UNDERLINE_LOW:
      return "low";
    case PANGO_UNDERLINE_ERROR:
    case PANGO_UNDERLINE_ERROR_LINE:
      return "error";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_overline_to_string (PangoOverline value)
{
  switch (value)
    {
    case PANGO_OVERLINE_NONE:
      return "none";
    case PANGO_OVERLINE_SINGLE:
      return "single";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_wrap_mode_to_string (PangoWrapMode mode)
{
  /* Keep these in sync with gtk_wrap_mode_to_string() */
  switch (mode)
    {
    case PANGO_WRAP_WORD:
      return "word";
    case PANGO_WRAP_CHAR:
      return "char";
    case PANGO_WRAP_WORD_CHAR:
      return "word-char";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_align_to_string (PangoAlignment align)
{
  switch (align)
    {
    case PANGO_ALIGN_LEFT:
      return "left";
    case PANGO_ALIGN_CENTER:
      return "center";
    case PANGO_ALIGN_RIGHT:
      return "right";
    default:
      g_assert_not_reached ();
    }
}

static void
accumulate_font_attributes (PangoFontDescription *font,
                            GPtrArray *names,
                            GPtrArray *values)
{
  char weight[60];
  char size[60];

  g_ptr_array_add (names, g_strdup ("style"));
  g_ptr_array_add (values, g_strdup (pango_style_to_string (pango_font_description_get_style (font))));

  g_ptr_array_add (names, g_strdup ("variant"));
  g_ptr_array_add (values, g_strdup (pango_variant_to_string (pango_font_description_get_variant (font))));

  g_ptr_array_add (names, g_strdup ("stretch"));
  g_ptr_array_add (values, g_strdup (pango_stretch_to_string (pango_font_description_get_stretch (font))));

  g_ptr_array_add (names, g_strdup ("family-name"));
  g_ptr_array_add (values, g_strdup (pango_font_description_get_family (font)));

  g_snprintf (weight, sizeof weight, "%d", pango_font_description_get_weight (font));
  g_ptr_array_add (names, g_strdup ("weight"));
  g_ptr_array_add (values, g_strdup (weight));

  g_snprintf (size, sizeof size, "%i", pango_font_description_get_size (font) / PANGO_SCALE);
  g_ptr_array_add (names, g_strdup ("size"));
  g_ptr_array_add (values, g_strdup (size));
}

void
gtk_pango_get_font_attributes (PangoFontDescription   *font,
                               char                 ***attribute_names,
                               char                 ***attribute_values)
{
  GPtrArray *names = g_ptr_array_new_null_terminated (6, g_free, TRUE);
  GPtrArray *values = g_ptr_array_new_null_terminated (6, g_free, TRUE);

  accumulate_font_attributes (font, names, values);

  *attribute_names = g_strdupv ((char **) names->pdata);
  *attribute_values = g_strdupv ((char **) values->pdata);

  g_ptr_array_unref (names);
  g_ptr_array_unref (values);
}

/*
 * gtk_pango_get_default_attributes:
 * @layout: the `PangoLayout` from which to get attributes
 * @attribute_names: (out) (array zero-terminated=1) (transfer full): the attribute names
 * @attribute_values: (out) (array zero-terminated=1) (transfer full): the attribute values
 *
 * Returns the default text attributes from @layout,
 * after translating them from Pango attributes to atspi
 * attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the `Text` interface in widgets using Pango
 * layouts.
 */
void
gtk_pango_get_default_attributes (PangoLayout   *layout,
                                  char        ***attribute_names,
                                  char        ***attribute_values)
{
  PangoContext *context;
  GPtrArray *names = g_ptr_array_new_null_terminated (16, g_free, TRUE);
  GPtrArray *values = g_ptr_array_new_null_terminated (16, g_free, TRUE);

  context = pango_layout_get_context (layout);
  if (context)
    {
      PangoLanguage *language;
      PangoFontDescription *font;

      language = pango_context_get_language (context);
      if (language)
        {
          g_ptr_array_add (names, g_strdup ("language"));
          g_ptr_array_add (values, g_strdup (pango_language_to_string (language)));
        }

      font = pango_context_get_font_description (context);
      if (font)
        accumulate_font_attributes (font, names, values);
    }

  g_ptr_array_add (names, g_strdup ("justification"));
  g_ptr_array_add (values,
                   g_strdup (pango_align_to_string (pango_layout_get_alignment (layout))));

  g_ptr_array_add (names, g_strdup ("wrap-mode"));
  g_ptr_array_add (values,
                   g_strdup (pango_wrap_mode_to_string (pango_layout_get_wrap (layout))));

  g_ptr_array_add (names, g_strdup ("strikethrough"));
  g_ptr_array_add (values, g_strdup ("false"));

  g_ptr_array_add (names, g_strdup ("underline"));
  g_ptr_array_add (values, g_strdup ("false"));

  g_ptr_array_add (names, g_strdup ("rise"));
  g_ptr_array_add (values, g_strdup ("0"));

  g_ptr_array_add (names, g_strdup ("scale"));
  g_ptr_array_add (values, g_strdup ("1"));

  g_ptr_array_add (names, g_strdup ("bg-full-height"));
  g_ptr_array_add (values, g_strdup ("0"));

  g_ptr_array_add (names, g_strdup ("pixels-inside-wrap"));
  g_ptr_array_add (values, g_strdup ("0"));

  g_ptr_array_add (names, g_strdup ("pixels-below-lines"));
  g_ptr_array_add (values, g_strdup ("0"));

  g_ptr_array_add (names, g_strdup ("pixels-above-lines"));
  g_ptr_array_add (values, g_strdup ("0"));

  g_ptr_array_add (names, g_strdup ("editable"));
  g_ptr_array_add (values, g_strdup ("false"));

  g_ptr_array_add (names, g_strdup ("invisible"));
  g_ptr_array_add (values, g_strdup ("false"));

  g_ptr_array_add (names, g_strdup ("indent"));
  g_ptr_array_add (values, g_strdup ("0"));

  g_ptr_array_add (names, g_strdup ("right-margin"));
  g_ptr_array_add (values, g_strdup ("0"));

  g_ptr_array_add (names, g_strdup ("left-margin"));
  g_ptr_array_add (values, g_strdup ("0"));

  *attribute_names = g_strdupv ((char **) names->pdata);
  *attribute_values = g_strdupv ((char **) values->pdata);

  g_ptr_array_unref (names);
  g_ptr_array_unref (values);
}

/*
 * gtk_pango_get_run_attributes:
 * @layout: the `PangoLayout` to get the attributes from
 * @offset: the offset at which the attributes are wanted
 * @attribute_names: (array zero-terminated=1) (out) (transfer full): the
 *   attribute names
 * @attribute_values: (array zero-terminated=1) (out) (transfer full): the
 *   attribute values
 * @start_offset: return location for the starting offset
 *   of the current run
 * @end_offset: return location for the ending offset of the
 *   current run
 *
 * Finds the “run” around index (i.e. the maximal range of characters
 * where the set of applicable attributes remains constant) and
 * returns the starting and ending offsets for it.
 *
 * The attributes for the run are added to @attributes, after
 * translating them from Pango attributes to atspi attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the #AtkText interface in widgets using Pango
 * layouts.
 */
void
gtk_pango_get_run_attributes (PangoLayout     *layout,
                              unsigned int     offset,
                              char          ***attribute_names,
                              char          ***attribute_values,
                              unsigned int    *start_offset,
                              unsigned int    *end_offset)
{
  PangoAttrIterator *iter;
  PangoAttrList *attr;
  PangoAttrString *pango_string;
  PangoAttrInt *pango_int;
  PangoAttrColor *pango_color;
  PangoAttrLanguage *pango_lang;
  PangoAttrFloat *pango_float;
  int index, start_index, end_index;
  gboolean is_next;
  glong len;
  const char *text;
  GPtrArray *names, *values;

  text = pango_layout_get_text (layout);
  len = g_utf8_strlen (text, -1);

  /* Grab the attributes of the PangoLayout, if any */
  attr = pango_layout_get_attributes (layout);

  if (attr == NULL)
    {
      *attribute_names = g_new0 (char *, 1);
      *attribute_values = g_new0 (char *, 1);
      *start_offset = 0;
      *end_offset = len;
      return;
    }

  iter = pango_attr_list_get_iterator (attr);
  /* Get invariant range offsets */
  /* If offset out of range, set offset in range */
  if (offset > len)
    offset = len;
  else if (offset < 0)
    offset = 0;

  names = g_ptr_array_new_null_terminated (16, g_free, TRUE);
  values = g_ptr_array_new_null_terminated (16, g_free, TRUE);

  index = g_utf8_offset_to_pointer (text, offset) - text;
  pango_attr_iterator_range (iter, &start_index, &end_index);
  is_next = TRUE;
  while (is_next)
    {
      if (index >= start_index && index < end_index)
        {
          *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
          if (end_index == G_MAXINT) /* Last iterator */
            end_index = len;

          *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
          break;
        }
      is_next = pango_attr_iterator_next (iter);
      pango_attr_iterator_range (iter, &start_index, &end_index);
    }

  /* Get attributes */
  pango_string = (PangoAttrString *) pango_attr_iterator_get (iter, PANGO_ATTR_FAMILY);
  if (pango_string != NULL)
    {
      g_ptr_array_add (names, g_strdup ("family-name"));
      g_ptr_array_add (values, g_strdup_printf ("%s", pango_string->value));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_STYLE);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("style"));
      g_ptr_array_add (values, g_strdup (pango_style_to_string (pango_int->value)));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_WEIGHT);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("weight"));
      g_ptr_array_add (values, g_strdup_printf ("%i", pango_int->value));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_VARIANT);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("variant"));
      g_ptr_array_add (values, g_strdup (pango_variant_to_string (pango_int->value)));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_STRETCH);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("stretch"));
      g_ptr_array_add (values, g_strdup (pango_stretch_to_string (pango_int->value)));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_SIZE);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("size"));
      g_ptr_array_add (values, g_strdup_printf ("%i", pango_int->value / PANGO_SCALE));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_UNDERLINE);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("underline"));
      g_ptr_array_add (values, g_strdup (pango_underline_to_string (pango_int->value)));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_OVERLINE);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("overline"));
      g_ptr_array_add (values, g_strdup (pango_overline_to_string (pango_int->value)));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_STRIKETHROUGH);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("strikethrough"));
      g_ptr_array_add (values, pango_int->value ? g_strdup ("true") : g_strdup ("false"));
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_RISE);
  if (pango_int != NULL)
    {
      g_ptr_array_add (names, g_strdup ("rise"));
      g_ptr_array_add (values, g_strdup_printf ("%i", pango_int->value));
    }

  pango_lang = (PangoAttrLanguage *) pango_attr_iterator_get (iter, PANGO_ATTR_LANGUAGE);
  if (pango_lang != NULL)
    {
      g_ptr_array_add (names, g_strdup ("language"));
      g_ptr_array_add (values, g_strdup (pango_language_to_string (pango_lang->value)));
    }

  pango_float = (PangoAttrFloat *) pango_attr_iterator_get (iter, PANGO_ATTR_SCALE);
  if (pango_float != NULL)
    {
      g_ptr_array_add (names, g_strdup ("scale"));
      g_ptr_array_add (values, g_strdup_printf ("%g", pango_float->value));
    }

  pango_color = (PangoAttrColor *) pango_attr_iterator_get (iter, PANGO_ATTR_FOREGROUND);
  if (pango_color != NULL)
    {
      g_ptr_array_add (names, g_strdup ("fg-color"));
      g_ptr_array_add (values, g_strdup_printf ("%u,%u,%u",
                                                pango_color->color.red,
                                                pango_color->color.green,
                                                pango_color->color.blue));
    }

  pango_color = (PangoAttrColor *) pango_attr_iterator_get (iter, PANGO_ATTR_BACKGROUND);
  if (pango_color != NULL)
    {
      g_ptr_array_add (names, g_strdup ("bg-color"));
      g_ptr_array_add (values, g_strdup_printf ("%u,%u,%u",
                                                pango_color->color.red,
                                                pango_color->color.green,
                                                pango_color->color.blue));
    }

  pango_attr_iterator_destroy (iter);

  *attribute_names = g_strdupv ((char **) names->pdata);
  *attribute_values = g_strdupv ((char **) values->pdata);

  g_ptr_array_unref (names);
  g_ptr_array_unref (values);
}

/*
 * gtk_pango_move_chars:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of characters to move from @offset
 *
 * Returns the position that is @count characters from the
 * given @offset. @count may be positive or negative.
 *
 * For the purpose of this function, characters are defined
 * by what Pango considers cursor positions.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_chars (PangoLayout *layout,
                      int          offset,
                      int          count)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_cursor_position);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_cursor_position);

      count++;
    }

  return offset;
}

/*
 * gtk_pango_move_words:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of words to move from @offset
 *
 * Returns the position that is @count words from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a word end, otherwise it will be a word start.
 * See the Pango documentation for details on how
 * word starts and ends are defined.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_words (PangoLayout  *layout,
                      int           offset,
                      int           count)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_word_end);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_word_start);

      count++;
    }

  return offset;
}

/*
 * gtk_pango_move_sentences:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of sentences to move from @offset
 *
 * Returns the position that is @count sentences from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a sentence end, otherwise it will be a sentence start.
 * See the Pango documentation for details on how
 * sentence starts and ends are defined.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_sentences (PangoLayout  *layout,
                          int           offset,
                          int           count)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_sentence_end);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_sentence_start);

      count++;
    }

  return offset;
}

#if 0
/*
 * gtk_pango_move_lines:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of lines to move from @offset
 *
 * Returns the position that is @count lines from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is negative, the returned position will
 * be the start of a line, else it will be the end of
 * line.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_lines (PangoLayout *layout,
                      int          offset,
                      int          count)
{
  GSList *lines, *l;
  PangoLayoutLine *line;
  int num;
  const char *text;
  int pos, line_pos;
  int index;
  int len;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  lines = pango_layout_get_lines (layout);
  line = NULL;

  num = 0;
  for (l = lines; l; l = l->next)
    {
      line = l->data;
      if (index < line->start_index + line->length)
        break;
      num++;
    }

  if (count < 0)
    {
      num += count;
      if (num < 0)
        num = 0;

      line = g_slist_nth_data (lines, num);

      return g_utf8_pointer_to_offset (text, text + line->start_index);
    }
  else
    {
      line_pos = index - line->start_index;

      len = g_slist_length (lines);
      num += count;
      if (num >= len || (count == 0 && num == len - 1))
        return g_utf8_strlen (text, -1) - 1;

      line = l->data;
      pos = line->start_index + line_pos;
      if (pos >= line->start_index + line->length)
        pos = line->start_index + line->length - 1;

      return g_utf8_pointer_to_offset (text, text + pos);
    }
}
#endif

/*
 * gtk_pango_is_inside_word:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a word.
 *
 * Returns: %TRUE if @offset is inside a word
 */
static gboolean
gtk_pango_is_inside_word (PangoLayout  *layout,
                          int           offset)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_word_start || attrs[offset].is_word_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_word_start;

  return FALSE;
}

/*
 * gtk_pango_is_inside_sentence:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a sentence.
 *
 * Returns: %TRUE if @offset is inside a sentence
 */
static gboolean
gtk_pango_is_inside_sentence (PangoLayout  *layout,
                              int           offset)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_sentence_start || attrs[offset].is_sentence_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_sentence_start;

  return FALSE;
}

static void
gtk_pango_get_line_start (PangoLayout *layout,
                          int          offset,
                          int         *start_offset,
                          int         *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL;
  int index, start_index, length, end_index;
  const char *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = pango_layout_line_get_start_index (line);
      length = pango_layout_line_get_length (line);
      end_index = start_index + length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (pango_layout_iter_next_line (iter))
            end_index = pango_layout_line_get_start_index (pango_layout_iter_get_line (iter));

          found = TRUE;
          break;
        }

      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = pango_layout_line_get_start_index (prev_line) + pango_layout_line_get_length (prev_line);
      end_index = start_index;
    }

  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

char *
gtk_pango_get_string_at (PangoLayout                  *layout,
                         unsigned int                  offset,
                         GtkAccessibleTextGranularity  granularity,
                         unsigned int                 *start_offset,
                         unsigned int                 *end_offset)
{
  const char *text;
  int start, end;
  const PangoLogAttr *attrs;
  int n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (granularity)
    {
    case GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER:
      end = gtk_pango_move_chars (layout, end, 1);
      break;

    case GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD:
      if (!attrs[start].is_word_start)
        start = gtk_pango_move_words (layout, start, -1);
      if (gtk_pango_is_inside_word (layout, end))
        end = gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      break;

    case GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE:
      if (!attrs[start].is_sentence_start)
        start = gtk_pango_move_sentences (layout, start, -1);
      if (gtk_pango_is_inside_sentence (layout, end))
        end = gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      break;

    case GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE:
      gtk_pango_get_line_start (layout, offset, &start, &end);
      break;

    case GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH:
      /* FIXME: In theory, a layout can hold more than one paragraph */
      start = 0;
      end = g_utf8_strlen (text, -1);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}
