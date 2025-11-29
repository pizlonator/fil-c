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

#include "gtkcssanimatedstyleprivate.h"

#include "gtkcssanimationprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssdynamicprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstaticstyleprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkprivate.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssAnimatedStyle, gtk_css_animated_style, GTK_TYPE_CSS_STYLE)


static GtkCssSection *
gtk_css_animated_style_get_section (GtkCssStyle *style,
                                    guint        id)
{
  GtkCssAnimatedStyle *animated = GTK_CSS_ANIMATED_STYLE (style);

  return gtk_css_style_get_section (animated->style, id);
}

static gboolean
gtk_css_animated_style_is_static (GtkCssStyle *style)
{
  GtkCssAnimatedStyle *animated = (GtkCssAnimatedStyle *)style;
  guint i;

  for (i = 0; i < animated->n_animations; i ++)
    {
      if (!_gtk_style_animation_is_static (animated->animations[i]))
        return FALSE;
    }

  return TRUE;
}

static GtkCssStaticStyle *
gtk_css_animated_style_get_static_style (GtkCssStyle *style)
{
  /* This is called a lot, so we avoid a dynamic type check here */
  GtkCssAnimatedStyle *animated = (GtkCssAnimatedStyle *) style;

  return (GtkCssStaticStyle *)animated->style;
}

static void
gtk_css_animated_style_dispose (GObject *object)
{
  GtkCssAnimatedStyle *style = GTK_CSS_ANIMATED_STYLE (object);
  guint i;

  for (i = 0; i < style->n_animations; i ++)
    gtk_style_animation_unref (style->animations[i]);

  style->n_animations = 0;
  g_free (style->animations);
  style->animations = NULL;

  G_OBJECT_CLASS (gtk_css_animated_style_parent_class)->dispose (object);
}

static void
gtk_css_animated_style_finalize (GObject *object)
{
  GtkCssAnimatedStyle *style = GTK_CSS_ANIMATED_STYLE (object);

  g_object_unref (style->style);

  G_OBJECT_CLASS (gtk_css_animated_style_parent_class)->finalize (object);
}

static void
gtk_css_animated_style_class_init (GtkCssAnimatedStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssStyleClass *style_class = GTK_CSS_STYLE_CLASS (klass);

  object_class->dispose = gtk_css_animated_style_dispose;
  object_class->finalize = gtk_css_animated_style_finalize;

  style_class->get_section = gtk_css_animated_style_get_section;
  style_class->is_static = gtk_css_animated_style_is_static;
  style_class->get_static_style = gtk_css_animated_style_get_static_style;
}

static void
gtk_css_animated_style_init (GtkCssAnimatedStyle *style)
{
}

#define DEFINE_UNSHARE(TYPE, NAME) \
static inline void \
unshare_ ## NAME (GtkCssAnimatedStyle *animated) \
{ \
  GtkCssStyle *style = (GtkCssStyle *)animated; \
  if (style->NAME == animated->style->NAME) \
    { \
      gtk_css_values_unref ((GtkCssValues *)style->NAME); \
      style->NAME = (TYPE *)gtk_css_values_copy ((GtkCssValues *)animated->style->NAME); \
    } \
}

DEFINE_UNSHARE (GtkCssCoreValues, core)
DEFINE_UNSHARE (GtkCssBackgroundValues, background)
DEFINE_UNSHARE (GtkCssBorderValues, border)
DEFINE_UNSHARE (GtkCssIconValues, icon)
DEFINE_UNSHARE (GtkCssOutlineValues, outline)
DEFINE_UNSHARE (GtkCssFontValues, font)
DEFINE_UNSHARE (GtkCssFontVariantValues, font_variant)
DEFINE_UNSHARE (GtkCssAnimationValues, animation)
DEFINE_UNSHARE (GtkCssTransitionValues, transition)
DEFINE_UNSHARE (GtkCssSizeValues, size)
DEFINE_UNSHARE (GtkCssOtherValues, other)

static inline void
gtk_css_take_value (GtkCssValue **variable,
                    GtkCssValue  *value)
{
  if (*variable)
    gtk_css_value_unref (*variable);
  *variable = value;
}

void
gtk_css_animated_style_set_animated_value (GtkCssAnimatedStyle *animated,
                                           guint                id,
                                           GtkCssValue         *value)
{
  GtkCssStyle *style = (GtkCssStyle *)animated;

  gtk_internal_return_if_fail (GTK_IS_CSS_ANIMATED_STYLE (style));
  gtk_internal_return_if_fail (value != NULL);

  switch (id)
    {
    case GTK_CSS_PROPERTY_COLOR:
      unshare_core (animated);
      gtk_css_take_value (&style->core->color, value);
      break;
    case GTK_CSS_PROPERTY_DPI:
      unshare_core (animated);
      gtk_css_take_value (&style->core->dpi, value);
      break;
    case GTK_CSS_PROPERTY_FONT_SIZE:
      unshare_core (animated);
      gtk_css_take_value (&style->core->font_size, value);
      break;
    case GTK_CSS_PROPERTY_ICON_PALETTE:
      unshare_core (animated);
      gtk_css_take_value (&style->core->icon_palette, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_color, value);
      break;
    case GTK_CSS_PROPERTY_FONT_FAMILY:
      unshare_font (animated);
      gtk_css_take_value (&style->font->font_family, value);
      break;
    case GTK_CSS_PROPERTY_FONT_STYLE:
      unshare_font (animated);
      gtk_css_take_value (&style->font->font_style, value);
      break;
    case GTK_CSS_PROPERTY_FONT_WEIGHT:
      unshare_font (animated);
      gtk_css_take_value (&style->font->font_weight, value);
      break;
    case GTK_CSS_PROPERTY_FONT_STRETCH:
      unshare_font (animated);
      gtk_css_take_value (&style->font->font_stretch, value);
      break;
    case GTK_CSS_PROPERTY_LETTER_SPACING:
      unshare_font (animated);
      gtk_css_take_value (&style->font->letter_spacing, value);
      break;
    case GTK_CSS_PROPERTY_LINE_HEIGHT:
      unshare_font (animated);
      gtk_css_take_value (&style->font->line_height, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_LINE:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->text_decoration_line, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->text_decoration_color, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->text_decoration_style, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_TRANSFORM:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->text_transform, value);
      break;
    case GTK_CSS_PROPERTY_FONT_KERNING:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->font_kerning, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->font_variant_ligatures, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_POSITION:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->font_variant_position, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_CAPS:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->font_variant_caps, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->font_variant_numeric, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->font_variant_alternates, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN:
      unshare_font_variant (animated);
      gtk_css_take_value (&style->font_variant->font_variant_east_asian, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_SHADOW:
      unshare_font (animated);
      gtk_css_take_value (&style->font->text_shadow, value);
      break;
    case GTK_CSS_PROPERTY_BOX_SHADOW:
      unshare_background (animated);
      gtk_css_take_value (&style->background->box_shadow, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_TOP:
      unshare_size (animated);
      gtk_css_take_value (&style->size->margin_top, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_LEFT:
      unshare_size (animated);
      gtk_css_take_value (&style->size->margin_left, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_BOTTOM:
      unshare_size (animated);
      gtk_css_take_value (&style->size->margin_bottom, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_RIGHT:
      unshare_size (animated);
      gtk_css_take_value (&style->size->margin_right, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_TOP:
      unshare_size (animated);
      gtk_css_take_value (&style->size->padding_top, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_LEFT:
      unshare_size (animated);
      gtk_css_take_value (&style->size->padding_left, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_BOTTOM:
      unshare_size (animated);
      gtk_css_take_value (&style->size->padding_bottom, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_RIGHT:
      unshare_size (animated);
      gtk_css_take_value (&style->size->padding_right, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_STYLE:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_top_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_top_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_LEFT_STYLE:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_left_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_left_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_bottom_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_bottom_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_right_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_right_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_top_left_radius, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_top_right_radius, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_bottom_right_radius, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_bottom_left_radius, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_STYLE:
      unshare_outline (animated);
      gtk_css_take_value (&style->outline->outline_style, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
      unshare_outline (animated);
      gtk_css_take_value (&style->outline->outline_width, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_OFFSET:
      unshare_outline (animated);
      gtk_css_take_value (&style->outline->outline_offset, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_CLIP:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_clip, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_ORIGIN:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_origin, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_SIZE:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_size, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_POSITION:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_position, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_top_color, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_right_color, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_bottom_color, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_left_color, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_COLOR:
      unshare_outline (animated);
      gtk_css_take_value (&style->outline->outline_color, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_REPEAT:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_repeat, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_image, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE:
      unshare_background (animated);
      gtk_css_take_value (&style->background->background_blend_mode, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_image_source, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_image_repeat, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_image_slice, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH:
      unshare_border (animated);
      gtk_css_take_value (&style->border->border_image_width, value);
      break;
    case GTK_CSS_PROPERTY_ICON_SOURCE:
      unshare_other (animated);
      gtk_css_take_value (&style->other->icon_source, value);
      break;
    case GTK_CSS_PROPERTY_ICON_SIZE:
      unshare_icon (animated);
      gtk_css_take_value (&style->icon->icon_size, value);
      break;
    case GTK_CSS_PROPERTY_ICON_SHADOW:
      unshare_icon (animated);
      gtk_css_take_value (&style->icon->icon_shadow, value);
      break;
    case GTK_CSS_PROPERTY_ICON_STYLE:
      unshare_icon (animated);
      gtk_css_take_value (&style->icon->icon_style, value);
      break;
    case GTK_CSS_PROPERTY_ICON_TRANSFORM:
      unshare_other (animated);
      gtk_css_take_value (&style->other->icon_transform, value);
      break;
    case GTK_CSS_PROPERTY_ICON_FILTER:
      unshare_other (animated);
      gtk_css_take_value (&style->other->icon_filter, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_SPACING:
      unshare_size (animated);
      gtk_css_take_value (&style->size->border_spacing, value);
      break;
    case GTK_CSS_PROPERTY_TRANSFORM:
      unshare_other (animated);
      gtk_css_take_value (&style->other->transform, value);
      break;
    case GTK_CSS_PROPERTY_TRANSFORM_ORIGIN:
      unshare_other (animated);
      gtk_css_take_value (&style->other->transform_origin, value);
      break;
    case GTK_CSS_PROPERTY_MIN_WIDTH:
      unshare_size (animated);
      gtk_css_take_value (&style->size->min_width, value);
      break;
    case GTK_CSS_PROPERTY_MIN_HEIGHT:
      unshare_size (animated);
      gtk_css_take_value (&style->size->min_height, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_PROPERTY:
      unshare_transition (animated);
      gtk_css_take_value (&style->transition->transition_property, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_DURATION:
      unshare_transition (animated);
      gtk_css_take_value (&style->transition->transition_duration, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION:
      unshare_transition (animated);
      gtk_css_take_value (&style->transition->transition_timing_function, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_DELAY:
      unshare_transition (animated);
      gtk_css_take_value (&style->transition->transition_delay, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_NAME:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_name, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_DURATION:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_duration, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_timing_function, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_iteration_count, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_DIRECTION:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_direction, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_play_state, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_DELAY:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_delay, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_FILL_MODE:
      unshare_animation (animated);
      gtk_css_take_value (&style->animation->animation_fill_mode, value);
      break;
    case GTK_CSS_PROPERTY_OPACITY:
      unshare_other (animated);
      gtk_css_take_value (&style->other->opacity, value);
      break;
    case GTK_CSS_PROPERTY_FILTER:
      unshare_other (animated);
      gtk_css_take_value (&style->other->filter, value);
      break;
    case GTK_CSS_PROPERTY_CARET_COLOR:
      unshare_font (animated);
      gtk_css_take_value (&style->font->caret_color, value);
      break;
    case GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR:
      unshare_font (animated);
      gtk_css_take_value (&style->font->secondary_caret_color, value);
      break;
    case GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS:
      unshare_font (animated);
      gtk_css_take_value (&style->font->font_feature_settings, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS:
      unshare_font (animated);
      gtk_css_take_value (&style->font->font_variation_settings, value);
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

GtkCssValue *
gtk_css_animated_style_get_intrinsic_value (GtkCssAnimatedStyle *style,
                                            guint                id)
{
  return gtk_css_style_get_value (style->style, id);
}

static GPtrArray *
gtk_css_animated_style_create_dynamic (GPtrArray   *animations,
                                       GtkCssStyle *style,
                                       gint64       timestamp)
{
  guint i;

  /* XXX: Check animations if they have dynamic values */

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      if (gtk_css_value_is_dynamic (gtk_css_style_get_value (style, i)))
        {
          if (!animations)
            animations = g_ptr_array_new ();

          g_ptr_array_insert (animations, 0, gtk_css_dynamic_new (timestamp));
          break;
        }
    }

  return animations;
}

/* TRANSITIONS */

typedef struct _TransitionInfo TransitionInfo;
struct _TransitionInfo {
  guint index;                  /* index into value arrays */
  gboolean pending;             /* TRUE if we still need to handle it */
};

static void
transition_info_add (TransitionInfo    infos[GTK_CSS_PROPERTY_N_PROPERTIES],
                     GtkStyleProperty *property,
                     guint             index)
{
  if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
    {
      GtkCssShorthandProperty *shorthand = (GtkCssShorthandProperty *) property;
      guint len = _gtk_css_shorthand_property_get_n_subproperties (shorthand);
      guint i;

      for (i = 0; i < len; i++)
        {
          GtkCssStyleProperty *prop = _gtk_css_shorthand_property_get_subproperty (shorthand, i);
          guint id;

          if (!_gtk_css_style_property_is_animated ((GtkCssStyleProperty *) prop))
            continue;

          id = _gtk_css_style_property_get_id ((GtkCssStyleProperty *) prop);
          infos[id].index = index;
          infos[id].pending = TRUE;
        }
    }
  else
    {
      guint id;

      if (!_gtk_css_style_property_is_animated ((GtkCssStyleProperty *) property))
        return;

      id = _gtk_css_style_property_get_id ((GtkCssStyleProperty *) property);
      g_assert (id < GTK_CSS_PROPERTY_N_PROPERTIES);
      infos[id].index = index;
      infos[id].pending = TRUE;
    }
}

static void
transition_infos_set (TransitionInfo  infos[GTK_CSS_PROPERTY_N_PROPERTIES],
                      GtkCssValue    *transitions)
{
  guint i;

  for (i = 0; i < _gtk_css_array_value_get_n_values (transitions); i++)
    {
      GtkStyleProperty *property;
      GtkCssValue *prop_value;

      prop_value = _gtk_css_array_value_get_nth (transitions, i);
      if (g_ascii_strcasecmp (_gtk_css_ident_value_get (prop_value), "all") == 0)
        {
          const guint len = _gtk_css_style_property_get_n_properties ();
          guint j;

          for (j = 0; j < len; j++)
            {
              property = (GtkStyleProperty *)_gtk_css_style_property_lookup_by_id (j);
              transition_info_add (infos, property, i);
            }
        }
      else
        {
          property = _gtk_style_property_lookup (_gtk_css_ident_value_get (prop_value));
          if (property)
            transition_info_add (infos, property, i);
        }
    }
}

static GtkStyleAnimation *
gtk_css_animated_style_find_transition (GtkCssAnimatedStyle *style,
                                        guint                property_id)
{
  guint i;

  for (i = 0; i < style->n_animations; i ++)
    {
      GtkStyleAnimation *animation = style->animations[i];

      if (!_gtk_css_transition_is_transition (animation))
        continue;

      if (_gtk_css_transition_get_property ((GtkCssTransition *)animation) == property_id)
        return animation;
    }

  return NULL;
}

static GPtrArray *
gtk_css_animated_style_create_css_transitions (GPtrArray   *animations,
                                               GtkCssStyle *base_style,
                                               gint64       timestamp,
                                               GtkCssStyle *source)
{
  TransitionInfo transitions[GTK_CSS_PROPERTY_N_PROPERTIES] = { { 0, } };
  GtkCssValue *durations, *delays, *timing_functions;
  gboolean source_is_animated;
  guint i;

  durations = base_style->transition->transition_duration;
  delays = base_style->transition->transition_delay;
  timing_functions = base_style->transition->transition_timing_function;

  if (_gtk_css_array_value_get_n_values (durations) == 1 &&
      _gtk_css_array_value_get_n_values (delays) == 1 &&
      _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, 0), 100) +
      _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, 0), 100) == 0)
    return animations;

  transition_infos_set (transitions, base_style->transition->transition_property);

  source_is_animated = GTK_IS_CSS_ANIMATED_STYLE (source);
  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      GtkStyleAnimation *animation;
      GtkCssValue *start, *end;
      double duration, delay;

      if (!transitions[i].pending)
        continue;

      duration = _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, transitions[i].index), 100);
      delay = _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, transitions[i].index), 100);
      if (duration + delay == 0.0)
        continue;

      if (source_is_animated)
        {
          start = gtk_css_animated_style_get_intrinsic_value ((GtkCssAnimatedStyle *)source, i);
          end = gtk_css_style_get_value (base_style, i);

          if (_gtk_css_value_equal (start, end))
            {
              animation = gtk_css_animated_style_find_transition ((GtkCssAnimatedStyle *)source, i);
              if (animation)
                {
                  animation = _gtk_style_animation_advance (animation, timestamp);
                  if (!animations)
                    animations = g_ptr_array_new ();

                  g_ptr_array_add (animations, animation);
                }

              continue;
            }
        }

      if (_gtk_css_value_equal (gtk_css_style_get_value (source, i),
                                gtk_css_style_get_value (base_style, i)))
        continue;

      animation = _gtk_css_transition_new (i,
                                           gtk_css_style_get_value (source, i),
                                           _gtk_css_array_value_get_nth (timing_functions, i),
                                           timestamp,
                                           duration * G_USEC_PER_SEC,
                                           delay * G_USEC_PER_SEC);
      if (!animations)
        animations = g_ptr_array_new ();

      g_ptr_array_add (animations, animation);
    }

  return animations;
}

static GtkStyleAnimation *
gtk_css_animated_style_find_animation (GtkStyleAnimation **animations,
                                       guint               n_animations,
                                       const char         *name)
{
  guint i;

  for (i = 0; i < n_animations; i ++)
    {
      GtkStyleAnimation *animation = animations[i];

      if (!_gtk_css_animation_is_animation (animation))
        continue;

      if (g_str_equal (_gtk_css_animation_get_name ((GtkCssAnimation *)animation), name))
        return animation;
    }

  return NULL;
}

static GPtrArray *
gtk_css_animated_style_create_css_animations (GPtrArray        *animations,
                                              GtkCssStyle      *base_style,
                                              GtkCssStyle      *parent_style,
                                              gint64            timestamp,
                                              GtkStyleProvider *provider,
                                              GtkCssStyle      *source)
{
  GtkCssValue *durations, *delays, *timing_functions, *animation_names;
  GtkCssValue *iteration_counts, *directions, *play_states, *fill_modes;
  gboolean source_is_animated;
  guint i;

  animation_names = base_style->animation->animation_name;

  if (_gtk_css_array_value_get_n_values (animation_names) == 1)
    {
      const char *name = _gtk_css_ident_value_get (_gtk_css_array_value_get_nth (animation_names, 0));

      if (g_ascii_strcasecmp (name, "none") == 0)
        return animations;
    }

  durations = base_style->animation->animation_duration;
  delays = base_style->animation->animation_delay;
  timing_functions = base_style->animation->animation_timing_function;
  iteration_counts = base_style->animation->animation_iteration_count;
  directions = base_style->animation->animation_direction;
  play_states = base_style->animation->animation_play_state;
  fill_modes = base_style->animation->animation_fill_mode;
  source_is_animated = GTK_IS_CSS_ANIMATED_STYLE (source);

  for (i = 0; i < _gtk_css_array_value_get_n_values (animation_names); i++)
    {
      GtkStyleAnimation *animation = NULL;
      GtkCssKeyframes *keyframes;
      const char *name;

      name = _gtk_css_ident_value_get (_gtk_css_array_value_get_nth (animation_names, i));
      if (g_ascii_strcasecmp (name, "none") == 0)
        continue;

      if (animations)
        animation = gtk_css_animated_style_find_animation ((GtkStyleAnimation **)animations->pdata, animations->len, name);

      if (animation)
        continue;

      if (source_is_animated)
        animation = gtk_css_animated_style_find_animation ((GtkStyleAnimation **)GTK_CSS_ANIMATED_STYLE (source)->animations,
                                                           GTK_CSS_ANIMATED_STYLE (source)->n_animations,
                                                           name);

      if (animation)
        {
          animation = _gtk_css_animation_advance_with_play_state ((GtkCssAnimation *)animation,
                                                                  timestamp,
                                                                  _gtk_css_play_state_value_get (_gtk_css_array_value_get_nth (play_states, i)));
        }
      else
        {
          keyframes = gtk_style_provider_get_keyframes (provider, name);
          if (keyframes == NULL)
            continue;

          keyframes = _gtk_css_keyframes_compute (keyframes, provider, base_style, parent_style);

          animation = _gtk_css_animation_new (name,
                                              keyframes,
                                              timestamp,
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, i), 100) * G_USEC_PER_SEC,
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, i), 100) * G_USEC_PER_SEC,
                                              _gtk_css_array_value_get_nth (timing_functions, i),
                                              _gtk_css_direction_value_get (_gtk_css_array_value_get_nth (directions, i)),
                                              _gtk_css_play_state_value_get (_gtk_css_array_value_get_nth (play_states, i)),
                                              _gtk_css_fill_mode_value_get (_gtk_css_array_value_get_nth (fill_modes, i)),
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (iteration_counts, i), 100));
          _gtk_css_keyframes_unref (keyframes);
        }

      if (!animations)
        animations = g_ptr_array_sized_new (16);

      g_ptr_array_add (animations, animation);
    }

  return animations;
}

/* PUBLIC API */

static void
gtk_css_animated_style_apply_animations (GtkCssAnimatedStyle *style)
{
  guint i;

  for (i = 0; i < style->n_animations; i ++)
    {
      GtkStyleAnimation *animation = style->animations[i];

      _gtk_style_animation_apply_values (animation, style);
    }
}

GtkCssStyle *
gtk_css_animated_style_new (GtkCssStyle      *base_style,
                            GtkCssStyle      *parent_style,
                            gint64            timestamp,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *previous_style)
{
  GtkCssAnimatedStyle *result;
  GtkCssStyle *style;
  GPtrArray *animations = NULL;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (base_style), NULL);
  gtk_internal_return_val_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style), NULL);
  gtk_internal_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), NULL);
  gtk_internal_return_val_if_fail (previous_style == NULL || GTK_IS_CSS_STYLE (previous_style), NULL);

  if (timestamp == 0)
    return g_object_ref (base_style);

  if (previous_style != NULL)
    animations = gtk_css_animated_style_create_css_transitions (animations, base_style, timestamp, previous_style);

  animations = gtk_css_animated_style_create_css_animations (animations, base_style, parent_style, timestamp, provider, previous_style);
  animations = gtk_css_animated_style_create_dynamic (animations, base_style, timestamp);

  if (animations == NULL)
    return g_object_ref (base_style);

  result = g_object_new (GTK_TYPE_CSS_ANIMATED_STYLE, NULL);

  result->style = g_object_ref (base_style);
  result->current_time = timestamp;
  result->n_animations = animations->len;
  result->animations = g_ptr_array_free (animations, FALSE);

  style = (GtkCssStyle *)result;
  style->core = (GtkCssCoreValues *)gtk_css_values_ref ((GtkCssValues *)base_style->core);
  style->background = (GtkCssBackgroundValues *)gtk_css_values_ref ((GtkCssValues *)base_style->background);
  style->border = (GtkCssBorderValues *)gtk_css_values_ref ((GtkCssValues *)base_style->border);
  style->icon = (GtkCssIconValues *)gtk_css_values_ref ((GtkCssValues *)base_style->icon);
  style->outline = (GtkCssOutlineValues *)gtk_css_values_ref ((GtkCssValues *)base_style->outline);
  style->font = (GtkCssFontValues *)gtk_css_values_ref ((GtkCssValues *)base_style->font);
  style->font_variant = (GtkCssFontVariantValues *)gtk_css_values_ref ((GtkCssValues *)base_style->font_variant);
  style->animation = (GtkCssAnimationValues *)gtk_css_values_ref ((GtkCssValues *)base_style->animation);
  style->transition = (GtkCssTransitionValues *)gtk_css_values_ref ((GtkCssValues *)base_style->transition);
  style->size = (GtkCssSizeValues *)gtk_css_values_ref ((GtkCssValues *)base_style->size);
  style->other = (GtkCssOtherValues *)gtk_css_values_ref ((GtkCssValues *)base_style->other);

  gtk_css_animated_style_apply_animations (result);

  return GTK_CSS_STYLE (result);
}

GtkCssStyle *
gtk_css_animated_style_new_advance (GtkCssAnimatedStyle *source,
                                    GtkCssStyle         *base_style,
                                    gint64               timestamp)
{
  GtkCssAnimatedStyle *result;
  GtkCssStyle *style;
  GPtrArray *animations;
  guint i;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_ANIMATED_STYLE (source), NULL);
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (base_style), NULL);

  if (timestamp == 0)
    return g_object_ref (source->style);

  if (timestamp == source->current_time)
    return g_object_ref (GTK_CSS_STYLE (source));

  gtk_internal_return_val_if_fail (timestamp > source->current_time, NULL);

  animations = NULL;
  for (i = 0; i < source->n_animations; i ++)
    {
      GtkStyleAnimation *animation = source->animations[i];

      if (_gtk_style_animation_is_finished (animation))
        continue;

      if (!animations)
        animations = g_ptr_array_sized_new (16);

      animation = _gtk_style_animation_advance (animation, timestamp);
      g_ptr_array_add (animations, animation);
    }

  if (animations == NULL)
    return g_object_ref (source->style);

  result = g_object_new (GTK_TYPE_CSS_ANIMATED_STYLE, NULL);

  result->style = g_object_ref (base_style);
  result->current_time = timestamp;
  result->n_animations = animations->len;
  result->animations = g_ptr_array_free (animations, FALSE);

  style = (GtkCssStyle *)result;
  style->core = (GtkCssCoreValues *)gtk_css_values_ref ((GtkCssValues *)base_style->core);
  style->background = (GtkCssBackgroundValues *)gtk_css_values_ref ((GtkCssValues *)base_style->background);
  style->border = (GtkCssBorderValues *)gtk_css_values_ref ((GtkCssValues *)base_style->border);
  style->icon = (GtkCssIconValues *)gtk_css_values_ref ((GtkCssValues *)base_style->icon);
  style->outline = (GtkCssOutlineValues *)gtk_css_values_ref ((GtkCssValues *)base_style->outline);
  style->font = (GtkCssFontValues *)gtk_css_values_ref ((GtkCssValues *)base_style->font);
  style->font_variant = (GtkCssFontVariantValues *)gtk_css_values_ref ((GtkCssValues *)base_style->font_variant);
  style->animation = (GtkCssAnimationValues *)gtk_css_values_ref ((GtkCssValues *)base_style->animation);
  style->transition = (GtkCssTransitionValues *)gtk_css_values_ref ((GtkCssValues *)base_style->transition);
  style->size = (GtkCssSizeValues *)gtk_css_values_ref ((GtkCssValues *)base_style->size);
  style->other = (GtkCssOtherValues *)gtk_css_values_ref ((GtkCssValues *)base_style->other);

  gtk_css_animated_style_apply_animations (result);

  return GTK_CSS_STYLE (result);
}
