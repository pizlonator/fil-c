/*
 * Copyright © 2012 Red Hat Inc.
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

#include "config.h"

#include "gtkprivate.h"
#include "gtkcssstyleprivate.h"

#include "gtkcssanimationprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssfontvariationsvalueprivate.h"
#include "gtkcssfontfeaturesvalueprivate.h"
#include "gtkcsslineheightvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_ABSTRACT_TYPE (GtkCssStyle, gtk_css_style, G_TYPE_OBJECT)

static GtkCssSection *
gtk_css_style_real_get_section (GtkCssStyle *style,
                                guint        id)
{
  return NULL;
}

static gboolean
gtk_css_style_real_is_static (GtkCssStyle *style)
{
  return TRUE;
}


static void
gtk_css_style_finalize (GObject *object)
{
  GtkCssStyle *style = GTK_CSS_STYLE (object);

  gtk_css_values_unref ((GtkCssValues *)style->core);
  gtk_css_values_unref ((GtkCssValues *)style->background);
  gtk_css_values_unref ((GtkCssValues *)style->border);
  gtk_css_values_unref ((GtkCssValues *)style->icon);
  gtk_css_values_unref ((GtkCssValues *)style->outline);
  gtk_css_values_unref ((GtkCssValues *)style->font);
  gtk_css_values_unref ((GtkCssValues *)style->font_variant);
  gtk_css_values_unref ((GtkCssValues *)style->animation);
  gtk_css_values_unref ((GtkCssValues *)style->transition);
  gtk_css_values_unref ((GtkCssValues *)style->size);
  gtk_css_values_unref ((GtkCssValues *)style->other);

  G_OBJECT_CLASS (gtk_css_style_parent_class)->finalize (object);
}

static void
gtk_css_style_class_init (GtkCssStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_style_finalize;

  klass->get_section = gtk_css_style_real_get_section;
  klass->is_static = gtk_css_style_real_is_static;
}

static void
gtk_css_style_init (GtkCssStyle *style)
{
}

GtkCssValue *
gtk_css_style_get_value (GtkCssStyle *style,
                         guint        id)
{
  switch (id)
    {
    case GTK_CSS_PROPERTY_COLOR:
      return style->core->color;
    case GTK_CSS_PROPERTY_DPI:
      return style->core->dpi;
    case GTK_CSS_PROPERTY_FONT_SIZE:
      return style->core->font_size;
    case GTK_CSS_PROPERTY_ICON_PALETTE:
      return style->core->icon_palette;
    case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
      return style->background->background_color;
    case GTK_CSS_PROPERTY_FONT_FAMILY:
      return style->font->font_family;
    case GTK_CSS_PROPERTY_FONT_STYLE:
      return style->font->font_style;
    case GTK_CSS_PROPERTY_FONT_WEIGHT:
      return style->font->font_weight;
    case GTK_CSS_PROPERTY_FONT_STRETCH:
      return style->font->font_stretch;
    case GTK_CSS_PROPERTY_LETTER_SPACING:
      return style->font->letter_spacing;
    case GTK_CSS_PROPERTY_LINE_HEIGHT:
      return style->font->line_height;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_LINE:
      return style->font_variant->text_decoration_line;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR:
      return style->font_variant->text_decoration_color ? style->font_variant->text_decoration_color : style->core->color;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE:
      return style->font_variant->text_decoration_style;
    case GTK_CSS_PROPERTY_TEXT_TRANSFORM:
      return style->font_variant->text_transform;
    case GTK_CSS_PROPERTY_FONT_KERNING:
      return style->font_variant->font_kerning;
    case GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES:
      return style->font_variant->font_variant_ligatures;
    case GTK_CSS_PROPERTY_FONT_VARIANT_POSITION:
      return style->font_variant->font_variant_position;
    case GTK_CSS_PROPERTY_FONT_VARIANT_CAPS:
      return style->font_variant->font_variant_caps;
    case GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC:
      return style->font_variant->font_variant_numeric;
    case GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES:
      return style->font_variant->font_variant_alternates;
    case GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN:
      return style->font_variant->font_variant_east_asian;
    case GTK_CSS_PROPERTY_TEXT_SHADOW:
      return style->font->text_shadow;
    case GTK_CSS_PROPERTY_BOX_SHADOW:
      return style->background->box_shadow;
    case GTK_CSS_PROPERTY_MARGIN_TOP:
      return style->size->margin_top;
    case GTK_CSS_PROPERTY_MARGIN_LEFT:
      return style->size->margin_left;
    case GTK_CSS_PROPERTY_MARGIN_BOTTOM:
      return style->size->margin_bottom;
    case GTK_CSS_PROPERTY_MARGIN_RIGHT:
      return style->size->margin_right;
    case GTK_CSS_PROPERTY_PADDING_TOP:
      return style->size->padding_top;
    case GTK_CSS_PROPERTY_PADDING_LEFT:
      return style->size->padding_left;
    case GTK_CSS_PROPERTY_PADDING_BOTTOM:
      return style->size->padding_bottom;
    case GTK_CSS_PROPERTY_PADDING_RIGHT:
      return style->size->padding_right;
    case GTK_CSS_PROPERTY_BORDER_TOP_STYLE:
      return style->border->border_top_style;
    case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
      return style->border->border_top_width;
    case GTK_CSS_PROPERTY_BORDER_LEFT_STYLE:
      return style->border->border_left_style;
    case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
      return style->border->border_left_width;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE:
      return style->border->border_bottom_style;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
      return style->border->border_bottom_width;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE:
      return style->border->border_right_style;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
      return style->border->border_right_width;
    case GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS:
      return style->border->border_top_left_radius;
    case GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS:
      return style->border->border_top_right_radius;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS:
      return style->border->border_bottom_right_radius;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS:
      return style->border->border_bottom_left_radius;
    case GTK_CSS_PROPERTY_OUTLINE_STYLE:
      return style->outline->outline_style;
    case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
      return style->outline->outline_width;
    case GTK_CSS_PROPERTY_OUTLINE_OFFSET:
      return style->outline->outline_offset;
    case GTK_CSS_PROPERTY_BACKGROUND_CLIP:
      return style->background->background_clip;
    case GTK_CSS_PROPERTY_BACKGROUND_ORIGIN:
      return style->background->background_origin;
    case GTK_CSS_PROPERTY_BACKGROUND_SIZE:
      return style->background->background_size;
    case GTK_CSS_PROPERTY_BACKGROUND_POSITION:
      return style->background->background_position;
    case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
      return style->border->border_top_color ? style->border->border_top_color : style->core->color;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
      return style->border->border_right_color ? style->border->border_right_color : style->core->color;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
      return style->border->border_bottom_color ? style->border->border_bottom_color : style->core->color;
    case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
      return style->border->border_left_color ? style->border->border_left_color: style->core->color;
    case GTK_CSS_PROPERTY_OUTLINE_COLOR:
      return style->outline->outline_color ? style->outline->outline_color : style->core->color;
    case GTK_CSS_PROPERTY_BACKGROUND_REPEAT:
      return style->background->background_repeat;
    case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      return style->background->background_image;
    case GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE:
      return style->background->background_blend_mode;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
      return style->border->border_image_source;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT:
      return style->border->border_image_repeat;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE:
      return style->border->border_image_slice;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH:
      return style->border->border_image_width;
    case GTK_CSS_PROPERTY_ICON_SOURCE:
      return style->other->icon_source;
    case GTK_CSS_PROPERTY_ICON_SIZE:
      return style->icon->icon_size;
    case GTK_CSS_PROPERTY_ICON_SHADOW:
      return style->icon->icon_shadow;
    case GTK_CSS_PROPERTY_ICON_STYLE:
      return style->icon->icon_style;
    case GTK_CSS_PROPERTY_ICON_TRANSFORM:
      return style->other->icon_transform;
    case GTK_CSS_PROPERTY_ICON_FILTER:
      return style->other->icon_filter;
    case GTK_CSS_PROPERTY_BORDER_SPACING:
      return style->size->border_spacing;
    case GTK_CSS_PROPERTY_TRANSFORM:
      return style->other->transform;
    case GTK_CSS_PROPERTY_TRANSFORM_ORIGIN:
      return style->other->transform_origin;
    case GTK_CSS_PROPERTY_MIN_WIDTH:
      return style->size->min_width;
    case GTK_CSS_PROPERTY_MIN_HEIGHT:
      return style->size->min_height;
    case GTK_CSS_PROPERTY_TRANSITION_PROPERTY:
      return style->transition->transition_property;
    case GTK_CSS_PROPERTY_TRANSITION_DURATION:
      return style->transition->transition_duration;
    case GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION:
      return style->transition->transition_timing_function;
    case GTK_CSS_PROPERTY_TRANSITION_DELAY:
      return style->transition->transition_delay;
    case GTK_CSS_PROPERTY_ANIMATION_NAME:
      return style->animation->animation_name;
    case GTK_CSS_PROPERTY_ANIMATION_DURATION:
      return style->animation->animation_duration;
    case GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION:
      return style->animation->animation_timing_function;
    case GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT:
      return style->animation->animation_iteration_count;
    case GTK_CSS_PROPERTY_ANIMATION_DIRECTION:
      return style->animation->animation_direction;
    case GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE:
      return style->animation->animation_play_state;
    case GTK_CSS_PROPERTY_ANIMATION_DELAY:
      return style->animation->animation_delay;
    case GTK_CSS_PROPERTY_ANIMATION_FILL_MODE:
      return style->animation->animation_fill_mode;
    case GTK_CSS_PROPERTY_OPACITY:
      return style->other->opacity;
    case GTK_CSS_PROPERTY_FILTER:
      return style->other->filter;
    case GTK_CSS_PROPERTY_CARET_COLOR:
      return style->font->caret_color ? style->font->caret_color : style->core->color;
    case GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR:
      return style->font->secondary_caret_color ? style->font->secondary_caret_color : style->core->color;
    case GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS:
      return style->font->font_feature_settings;
    case GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS:
      return style->font->font_variation_settings;

    default:
      g_assert_not_reached ();
    }
}

GtkCssSection *
gtk_css_style_get_section (GtkCssStyle *style,
                           guint        id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  return GTK_CSS_STYLE_GET_CLASS (style)->get_section (style, id);
}

gboolean
gtk_css_style_is_static (GtkCssStyle *style)
{
  return GTK_CSS_STYLE_GET_CLASS (style)->is_static (style);
}

GtkCssStaticStyle *
gtk_css_style_get_static_style (GtkCssStyle *style)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  return GTK_CSS_STYLE_GET_CLASS (style)->get_static_style (style);
}

/*
 * gtk_css_style_print:
 * @style: a `GtkCssStyle`
 * @string: the `GString` to print to
 * @indent: level of indentation to use
 * @skip_initial: %TRUE to skip properties that have their initial value
 *
 * Print the @style to @string, in CSS format. Every property is printed
 * on a line by itself, indented by @indent spaces. If @skip_initial is
 * %TRUE, properties are only printed if their value in @style is different
 * from the initial value of the property.
 *
 * Returns: %TRUE is any properties have been printed
 */
gboolean
gtk_css_style_print (GtkCssStyle *style,
                     GString     *string,
                     guint        indent,
                     gboolean     skip_initial)
{
  guint i;
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssSection *section;
      GtkCssStyleProperty *prop;
      GtkCssValue *value;
      const char *name;

      section = gtk_css_style_get_section (style, i);
      if (!section && skip_initial)
        continue;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));
      value = gtk_css_style_get_value (style, i);

      g_string_append_printf (string, "%*s%s: ", indent, "", name);
      _gtk_css_value_print (value, string);
      g_string_append_c (string, ';');

      if (section)
        {
          g_string_append (string, " /* ");
          gtk_css_section_print (section, string);
          g_string_append (string, " */");
        }

      g_string_append_c (string, '\n');

      retval = TRUE;
    }

  return retval;
}

char *
gtk_css_style_to_string (GtkCssStyle *style)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  string = g_string_new ("");

  gtk_css_style_print (style, string, 0, FALSE);

  return g_string_free (string, FALSE);
}

static PangoUnderline
get_pango_underline_from_style (GtkTextDecorationStyle style)
{
  switch (style)
    {
    case GTK_CSS_TEXT_DECORATION_STYLE_DOUBLE:
      return PANGO_UNDERLINE_DOUBLE;
    case GTK_CSS_TEXT_DECORATION_STYLE_WAVY:
      return PANGO_UNDERLINE_ERROR;
    case GTK_CSS_TEXT_DECORATION_STYLE_SOLID:
    default:
      return PANGO_UNDERLINE_SINGLE;
    }

  g_return_val_if_reached (PANGO_UNDERLINE_SINGLE);
}

PangoTextTransform
gtk_css_style_get_pango_text_transform (GtkCssStyle *style)
{
  switch (_gtk_css_text_transform_value_get (style->font_variant->text_transform))
    {
    case GTK_CSS_TEXT_TRANSFORM_NONE:
      return PANGO_TEXT_TRANSFORM_NONE;
    case GTK_CSS_TEXT_TRANSFORM_LOWERCASE:
      return PANGO_TEXT_TRANSFORM_LOWERCASE;
    case GTK_CSS_TEXT_TRANSFORM_UPPERCASE:
      return PANGO_TEXT_TRANSFORM_UPPERCASE;
    case GTK_CSS_TEXT_TRANSFORM_CAPITALIZE:
      return PANGO_TEXT_TRANSFORM_CAPITALIZE;
    default:
      return PANGO_TEXT_TRANSFORM_NONE;
    }
}

static PangoOverline
get_pango_overline_from_style (GtkTextDecorationStyle style)
{
  return PANGO_OVERLINE_SINGLE;
}

static PangoAttrList *
add_pango_attr (PangoAttrList  *attrs,
                PangoAttribute *attr)
{
  if (attrs == NULL)
    attrs = pango_attr_list_new ();

  pango_attr_list_insert (attrs, attr);

  return attrs;
}

static void
append_separated (GString    **s,
                  const char  *text)
{
  if (G_UNLIKELY (!*s))
    *s = g_string_new (NULL);

  if ((*s)->len > 0)
    g_string_append (*s, ", ");

  g_string_append (*s, text);
}

char *
gtk_css_style_compute_font_features (GtkCssStyle *style)
{
  GtkCssFontVariantLigature ligatures;
  GtkCssFontVariantNumeric numeric;
  GtkCssFontVariantEastAsian east_asian;
  char *settings;
  GString *s = NULL;

  switch (_gtk_css_font_kerning_value_get (style->font_variant->font_kerning))
    {
    case GTK_CSS_FONT_KERNING_NORMAL:
      append_separated (&s, "kern 1");
      break;
    case GTK_CSS_FONT_KERNING_NONE:
      append_separated (&s, "kern 0");
      break;
    case GTK_CSS_FONT_KERNING_AUTO:
    default:
      break;
    }

  ligatures = _gtk_css_font_variant_ligature_value_get (style->font_variant->font_variant_ligatures);
  if (ligatures == GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL)
    {
      /* all defaults */
    }
  else if (ligatures == GTK_CSS_FONT_VARIANT_LIGATURE_NONE)
    append_separated (&s, "liga 0, clig 0, dlig 0, hlig 0, calt 0");
  else
    {
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_COMMON_LIGATURES)
        append_separated (&s, "liga 1, clig 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_COMMON_LIGATURES)
        append_separated (&s, "liga 0, clig 0");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_DISCRETIONARY_LIGATURES)
        append_separated (&s, "dlig 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_DISCRETIONARY_LIGATURES)
        append_separated (&s, "dlig 0");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_HISTORICAL_LIGATURES)
        append_separated (&s, "hlig 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_HISTORICAL_LIGATURES)
        append_separated (&s, "hlig 0");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_CONTEXTUAL)
        append_separated (&s, "calt 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_CONTEXTUAL)
        append_separated (&s, "calt 0");
    }

  switch (_gtk_css_font_variant_position_value_get (style->font_variant->font_variant_position))
    {
    case GTK_CSS_FONT_VARIANT_POSITION_SUB:
      append_separated (&s, "subs 1");
      break;
    case GTK_CSS_FONT_VARIANT_POSITION_SUPER:
      append_separated (&s, "sups 1");
      break;
    case GTK_CSS_FONT_VARIANT_POSITION_NORMAL:
    default:
      break;
    }

  numeric = _gtk_css_font_variant_numeric_value_get (style->font_variant->font_variant_numeric);
  if (numeric == GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL)
    {
      /* all defaults */
    }
  else
    {
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_LINING_NUMS)
        append_separated (&s, "lnum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_OLDSTYLE_NUMS)
        append_separated (&s, "onum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_PROPORTIONAL_NUMS)
        append_separated (&s, "pnum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_TABULAR_NUMS)
        append_separated (&s, "tnum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS)
        append_separated (&s, "frac 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS)
        append_separated (&s, "afrc 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_ORDINAL)
        append_separated (&s, "ordn 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_SLASHED_ZERO)
        append_separated (&s, "zero 1");
    }

  switch (_gtk_css_font_variant_alternate_value_get (style->font_variant->font_variant_alternates))
    {
    case GTK_CSS_FONT_VARIANT_ALTERNATE_HISTORICAL_FORMS:
      append_separated (&s, "hist 1");
      break;
    case GTK_CSS_FONT_VARIANT_ALTERNATE_NORMAL:
    default:
      break;
    }

  east_asian = _gtk_css_font_variant_east_asian_value_get (style->font_variant->font_variant_east_asian);
  if (east_asian == GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL)
    {
      /* all defaults */
    }
  else
    {
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS78)
        append_separated (&s, "jp78 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS83)
        append_separated (&s, "jp83 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS90)
        append_separated (&s, "jp90 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS04)
        append_separated (&s, "jp04 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED)
        append_separated (&s, "smpl 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL)
        append_separated (&s, "trad 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH)
        append_separated (&s, "fwid 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_PROPORTIONAL)
        append_separated (&s, "pwid 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_RUBY)
        append_separated (&s, "ruby 1");
    }

  settings = gtk_css_font_features_value_get_features (style->font->font_feature_settings);
  if (settings)
    {
      append_separated (&s, settings);
      g_free (settings);
    }

  if (s)
    return g_string_free (s, FALSE);
  else
    return NULL;
}

PangoAttrList *
gtk_css_style_get_pango_attributes (GtkCssStyle *style)
{
  PangoAttrList *attrs = NULL;
  GtkTextDecorationLine decoration_line;
  GtkTextDecorationStyle decoration_style;
  const GdkRGBA *color;
  const GdkRGBA *decoration_color;
  double letter_spacing;

  /* text-decoration */
  decoration_line = _gtk_css_text_decoration_line_value_get (style->font_variant->text_decoration_line);
  decoration_style = _gtk_css_text_decoration_style_value_get (style->font_variant->text_decoration_style);
  color = gtk_css_color_value_get_rgba (style->core->color);
  decoration_color = gtk_css_color_value_get_rgba (style->font_variant->text_decoration_color
                                                   ? style->font_variant->text_decoration_color
                                                   : style->core->color);

  if (decoration_line & GTK_CSS_TEXT_DECORATION_LINE_UNDERLINE)
    {
      attrs = add_pango_attr (attrs, pango_attr_underline_new (get_pango_underline_from_style (decoration_style)));
      if (!gdk_rgba_equal (color, decoration_color))
        attrs = add_pango_attr (attrs, pango_attr_underline_color_new (decoration_color->red * 65535. + 0.5,
                                                                       decoration_color->green * 65535. + 0.5,
                                                                       decoration_color->blue * 65535. + 0.5));
    }
  if (decoration_line & GTK_CSS_TEXT_DECORATION_LINE_OVERLINE)
    {
      attrs = add_pango_attr (attrs, pango_attr_overline_new (get_pango_overline_from_style (decoration_style)));
      if (!gdk_rgba_equal (color, decoration_color))
        attrs = add_pango_attr (attrs, pango_attr_overline_color_new (decoration_color->red * 65535. + 0.5,
                                                                      decoration_color->green * 65535. + 0.5,
                                                                      decoration_color->blue * 65535. + 0.5));
    }
  if (decoration_line & GTK_CSS_TEXT_DECORATION_LINE_LINE_THROUGH)
    {
      attrs = add_pango_attr (attrs, pango_attr_strikethrough_new (TRUE));
      if (!gdk_rgba_equal (color, decoration_color))
        attrs = add_pango_attr (attrs, pango_attr_strikethrough_color_new (decoration_color->red * 65535. + 0.5,
                                                                           decoration_color->green * 65535. + 0.5,
                                                                           decoration_color->blue * 65535. + 0.5));
    }

  /* letter-spacing */
  letter_spacing = _gtk_css_number_value_get (style->font->letter_spacing, 100);
  if (letter_spacing != 0)
    {
      attrs = add_pango_attr (attrs, pango_attr_letter_spacing_new (letter_spacing * PANGO_SCALE));
    }

  /* line-height */
  {
    double height = gtk_css_line_height_value_get (style->font->line_height);
    if (height != 0.0)
      {
        if (gtk_css_number_value_get_dimension (style->font->line_height) == GTK_CSS_DIMENSION_LENGTH)
          attrs = add_pango_attr (attrs, pango_attr_line_height_new_absolute (height * PANGO_SCALE));
        else
          attrs = add_pango_attr (attrs, pango_attr_line_height_new (height));
      }
   }

  /* casing variants */
  switch (_gtk_css_font_variant_caps_value_get (style->font_variant->font_variant_caps))
    {
    case GTK_CSS_FONT_VARIANT_CAPS_SMALL_CAPS:
      attrs = add_pango_attr (attrs, pango_attr_variant_new (PANGO_VARIANT_SMALL_CAPS));
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_ALL_SMALL_CAPS:
      attrs = add_pango_attr (attrs, pango_attr_variant_new (PANGO_VARIANT_ALL_SMALL_CAPS));
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_PETITE_CAPS:
      attrs = add_pango_attr (attrs, pango_attr_variant_new (PANGO_VARIANT_PETITE_CAPS));
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_ALL_PETITE_CAPS:
      attrs = add_pango_attr (attrs, pango_attr_variant_new (PANGO_VARIANT_ALL_PETITE_CAPS));
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_UNICASE:
      attrs = add_pango_attr (attrs, pango_attr_variant_new (PANGO_VARIANT_UNICASE));
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_TITLING_CAPS:
      attrs = add_pango_attr (attrs, pango_attr_variant_new (PANGO_VARIANT_TITLE_CAPS));
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_NORMAL:
    default:
      break;
    }

  /* OpenType features */
  {
    char *font_features = gtk_css_style_compute_font_features (style);

    if (font_features)
      {
        attrs = add_pango_attr (attrs, pango_attr_font_features_new (font_features));
        g_free (font_features);
      }
  }

  /* text-transform */
  {
    PangoTextTransform transform = gtk_css_style_get_pango_text_transform (style);

    if (transform != PANGO_TEXT_TRANSFORM_NONE)
      attrs = add_pango_attr (attrs, pango_attr_text_transform_new (transform));
  }

  return attrs;
}

PangoFontDescription *
gtk_css_style_get_pango_font (GtkCssStyle *style)
{
  PangoFontDescription *description;
  GtkCssValue *v;
  char *str;

  description = pango_font_description_new ();

  v = style->font->font_family;
  if (_gtk_css_array_value_get_n_values (v) > 1)
    {
      int i;
      GString *s = g_string_new ("");

      for (i = 0; i < _gtk_css_array_value_get_n_values (v); i++)
        {
          if (i > 0)
            g_string_append (s, ",");
          g_string_append (s, _gtk_css_string_value_get (_gtk_css_array_value_get_nth (v, i)));
        }

      pango_font_description_set_family (description, s->str);
      g_string_free (s, TRUE);
    }
  else
    {
      pango_font_description_set_family (description,
                                         _gtk_css_string_value_get (_gtk_css_array_value_get_nth (v, 0)));
    }

  v = style->core->font_size;
  pango_font_description_set_absolute_size (description, round (_gtk_css_number_value_get (v, 100) * PANGO_SCALE));

  v = style->font->font_style;
  pango_font_description_set_style (description, _gtk_css_font_style_value_get (v));

  v = style->font->font_weight;
  pango_font_description_set_weight (description, _gtk_css_number_value_get (v, 100));

  v = style->font->font_stretch;
  pango_font_description_set_stretch (description, _gtk_css_font_stretch_value_get (v));

  v = style->font->font_variation_settings;
  str = gtk_css_font_variations_value_get_variations (v);
  if (str)
    pango_font_description_set_variations (description, str);
  g_free (str);

  return description;
}

/* Refcounted value structs */

static const int values_size[] = {
  sizeof (GtkCssCoreValues),
  sizeof (GtkCssBackgroundValues),
  sizeof (GtkCssBorderValues),
  sizeof (GtkCssIconValues),
  sizeof (GtkCssOutlineValues),
  sizeof (GtkCssFontValues),
  sizeof (GtkCssFontVariantValues),
  sizeof (GtkCssAnimationValues),
  sizeof (GtkCssTransitionValues),
  sizeof (GtkCssSizeValues),
  sizeof (GtkCssOtherValues)
};

#define TYPE_INDEX(type) ((type) - ((type) % 2))
#define VALUES_SIZE(type) (values_size[(type) / 2])
#define N_VALUES(type) ((VALUES_SIZE(type) - sizeof (GtkCssValues)) / sizeof (GtkCssValue *))

#define GET_VALUES(v) (GtkCssValue **)((guint8 *)(v) + sizeof (GtkCssValues))

GtkCssValues *gtk_css_values_ref (GtkCssValues *values)
{
  values->ref_count++;

  return values;
}

static void
gtk_css_values_free (GtkCssValues *values)
{
  int i;
  GtkCssValue **v = GET_VALUES (values);

  for (i = 0; i < N_VALUES (values->type); i++)
    {
      if (v[i])
        gtk_css_value_unref (v[i]);
    }

  g_free (values);
}

void gtk_css_values_unref (GtkCssValues *values)
{
  if (!values)
    return;

  values->ref_count--;

  if (values->ref_count == 0)
    gtk_css_values_free (values);
}

GtkCssValues *
gtk_css_values_copy (GtkCssValues *values)
{
  GtkCssValues *copy;
  GtkCssValue **v, **v2;
  int i;

  copy = gtk_css_values_new (TYPE_INDEX(values->type));

  v = GET_VALUES (values);
  v2 = GET_VALUES (copy);

  for (i = 0; i < N_VALUES (values->type); i++)
    {
      if (v[i])
        v2[i] = gtk_css_value_ref (v[i]);
    }

  return copy;
}

GtkCssValues *
gtk_css_values_new (GtkCssValuesType type)
{
  GtkCssValues *values;

  values = (GtkCssValues *)g_malloc0 (VALUES_SIZE(type));
  values->ref_count = 1;
  values->type = type;

  return values;
}
