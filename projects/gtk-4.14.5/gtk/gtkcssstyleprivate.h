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

#pragma once

#include <glib-object.h>
#include <gtk/css/gtkcss.h>

#include "gtk/gtkbitmaskprivate.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STYLE           (gtk_css_style_get_type ())
#define GTK_CSS_STYLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STYLE, GtkCssStyle))
#define GTK_CSS_STYLE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STYLE, GtkCssStyleClass))
#define GTK_IS_CSS_STYLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STYLE))
#define GTK_IS_CSS_STYLE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STYLE))
#define GTK_CSS_STYLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STYLE, GtkCssStyleClass))

typedef enum {
  GTK_CSS_CORE_VALUES,
  GTK_CSS_CORE_INITIAL_VALUES,
  GTK_CSS_BACKGROUND_VALUES,
  GTK_CSS_BACKGROUND_INITIAL_VALUES,
  GTK_CSS_BORDER_VALUES,
  GTK_CSS_BORDER_INITIAL_VALUES,
  GTK_CSS_ICON_VALUES,
  GTK_CSS_ICON_INITIAL_VALUES,
  GTK_CSS_OUTLINE_VALUES,
  GTK_CSS_OUTLINE_INITIAL_VALUES,
  GTK_CSS_FONT_VALUES,
  GTK_CSS_FONT_INITIAL_VALUES,
  GTK_CSS_FONT_VARIANT_VALUES,
  GTK_CSS_FONT_VARIANT_INITIAL_VALUES,
  GTK_CSS_ANIMATION_VALUES,
  GTK_CSS_ANIMATION_INITIAL_VALUES,
  GTK_CSS_TRANSITION_VALUES,
  GTK_CSS_TRANSITION_INITIAL_VALUES,
  GTK_CSS_SIZE_VALUES,
  GTK_CSS_SIZE_INITIAL_VALUES,
  GTK_CSS_OTHER_VALUES,
  GTK_CSS_OTHER_INITIAL_VALUES,
} GtkCssValuesType;

typedef struct _GtkCssValues GtkCssValues;
typedef struct _GtkCssCoreValues GtkCssCoreValues;
typedef struct _GtkCssBackgroundValues GtkCssBackgroundValues;
typedef struct _GtkCssBorderValues GtkCssBorderValues;
typedef struct _GtkCssIconValues GtkCssIconValues;
typedef struct _GtkCssOutlineValues GtkCssOutlineValues;
typedef struct _GtkCssFontValues GtkCssFontValues;
typedef struct _GtkCssFontVariantValues GtkCssFontVariantValues;
typedef struct _GtkCssAnimationValues GtkCssAnimationValues;
typedef struct _GtkCssTransitionValues GtkCssTransitionValues;
typedef struct _GtkCssSizeValues GtkCssSizeValues;
typedef struct _GtkCssOtherValues GtkCssOtherValues;

struct _GtkCssValues {
  int ref_count;
  GtkCssValuesType type;
};

struct _GtkCssCoreValues {
  GtkCssValues base;
  GtkCssValue *color;
  GtkCssValue *dpi;
  GtkCssValue *font_size;
  GtkCssValue *icon_palette;
};

struct _GtkCssBackgroundValues {
  GtkCssValues base;
  GtkCssValue *background_color;
  GtkCssValue *box_shadow;
  GtkCssValue *background_clip;
  GtkCssValue *background_origin;
  GtkCssValue *background_size;
  GtkCssValue *background_position;
  GtkCssValue *background_repeat;
  GtkCssValue *background_image;
  GtkCssValue *background_blend_mode;
};

struct _GtkCssBorderValues {
  GtkCssValues base;
  GtkCssValue *border_top_style;
  GtkCssValue *border_top_width;
  GtkCssValue *border_left_style;
  GtkCssValue *border_left_width;
  GtkCssValue *border_bottom_style;
  GtkCssValue *border_bottom_width;
  GtkCssValue *border_right_style;
  GtkCssValue *border_right_width;
  GtkCssValue *border_top_left_radius;
  GtkCssValue *border_top_right_radius;
  GtkCssValue *border_bottom_right_radius;
  GtkCssValue *border_bottom_left_radius;
  GtkCssValue *border_top_color; // NULL if currentColor
  GtkCssValue *border_right_color; // NULL if currentColor
  GtkCssValue *border_bottom_color; // NULL if currentColor
  GtkCssValue *border_left_color; // NULL if currentColor
  GtkCssValue *border_image_source;
  GtkCssValue *border_image_repeat;
  GtkCssValue *border_image_slice;
  GtkCssValue *border_image_width;
};

struct _GtkCssIconValues {
  GtkCssValues base;
  GtkCssValue *icon_size;
  GtkCssValue *icon_shadow;
  GtkCssValue *icon_style;
};


struct _GtkCssOutlineValues {
  GtkCssValues base;
  GtkCssValue *outline_style;
  GtkCssValue *outline_width;
  GtkCssValue *outline_offset;
  GtkCssValue *outline_color; // NULL if currentColor
};

struct _GtkCssFontValues {
  GtkCssValues base;
  GtkCssValue *font_family;
  GtkCssValue *font_style;
  GtkCssValue *font_weight;
  GtkCssValue *font_stretch;
  GtkCssValue *letter_spacing;
  GtkCssValue *text_shadow;
  GtkCssValue *caret_color; // NULL if currentColor
  GtkCssValue *secondary_caret_color; // NULL if currentColor
  GtkCssValue *font_feature_settings;
  GtkCssValue *font_variation_settings;
  GtkCssValue *line_height;
};

struct _GtkCssFontVariantValues {
  GtkCssValues base;
  GtkCssValue *text_decoration_line;
  GtkCssValue *text_decoration_color; // NULL if currentColor
  GtkCssValue *text_decoration_style;
  GtkCssValue *text_transform;
  GtkCssValue *font_kerning;
  GtkCssValue *font_variant_ligatures;
  GtkCssValue *font_variant_position;
  GtkCssValue *font_variant_caps;
  GtkCssValue *font_variant_numeric;
  GtkCssValue *font_variant_alternates;
  GtkCssValue *font_variant_east_asian;
};

struct _GtkCssAnimationValues {
  GtkCssValues base;
  GtkCssValue *animation_name;
  GtkCssValue *animation_duration;
  GtkCssValue *animation_timing_function;
  GtkCssValue *animation_iteration_count;
  GtkCssValue *animation_direction;
  GtkCssValue *animation_play_state;
  GtkCssValue *animation_delay;
  GtkCssValue *animation_fill_mode;
};

struct _GtkCssTransitionValues {
  GtkCssValues base;
  GtkCssValue *transition_property;
  GtkCssValue *transition_duration;
  GtkCssValue *transition_timing_function;
  GtkCssValue *transition_delay;
};

struct _GtkCssSizeValues {
  GtkCssValues base;
  GtkCssValue *margin_top;
  GtkCssValue *margin_left;
  GtkCssValue *margin_bottom;
  GtkCssValue *margin_right;
  GtkCssValue *padding_top;
  GtkCssValue *padding_left;
  GtkCssValue *padding_bottom;
  GtkCssValue *padding_right;
  GtkCssValue *border_spacing;
  GtkCssValue *min_width;
  GtkCssValue *min_height;
};

struct _GtkCssOtherValues {
  GtkCssValues base;
  GtkCssValue *icon_source;
  GtkCssValue *icon_transform;
  GtkCssValue *icon_filter;
  GtkCssValue *transform;
  GtkCssValue *transform_origin;
  GtkCssValue *opacity;
  GtkCssValue *filter;
};

/* typedef struct _GtkCssStyle           GtkCssStyle; */
typedef struct _GtkCssStyleClass      GtkCssStyleClass;

struct _GtkCssStyle
{
  GObject parent;

  GtkCssCoreValues        *core;
  GtkCssBackgroundValues  *background;
  GtkCssBorderValues      *border;
  GtkCssIconValues        *icon;
  GtkCssOutlineValues     *outline;
  GtkCssFontValues        *font;
  GtkCssFontVariantValues *font_variant;
  GtkCssAnimationValues   *animation;
  GtkCssTransitionValues  *transition;
  GtkCssSizeValues        *size;
  GtkCssOtherValues       *other;
};

struct _GtkCssStyleClass
{
  GObjectClass parent_class;

  /* Get the section the value at the given id was declared at or NULL if unavailable.
   * Optional: default impl will just return NULL */
  GtkCssSection *       (* get_section)                         (GtkCssStyle            *style,
                                                                 guint                   id);
  /* TRUE if this style will require changes based on timestamp */
  gboolean              (* is_static)                           (GtkCssStyle            *style);

  GtkCssStaticStyle *   (* get_static_style)                    (GtkCssStyle            *style);
};

GType                   gtk_css_style_get_type                  (void) G_GNUC_CONST;

GtkCssValue *           gtk_css_style_get_value                 (GtkCssStyle            *style,
                                                                 guint                   id) G_GNUC_PURE;
GtkCssSection *         gtk_css_style_get_section               (GtkCssStyle            *style,
                                                                 guint                   id) G_GNUC_PURE;
gboolean                gtk_css_style_is_static                 (GtkCssStyle            *style) G_GNUC_PURE;
GtkCssStaticStyle *     gtk_css_style_get_static_style          (GtkCssStyle            *style);

char *                  gtk_css_style_to_string                 (GtkCssStyle            *style);
gboolean                gtk_css_style_print                     (GtkCssStyle            *style,
                                                                 GString                *string,
                                                                 guint                   indent,
                                                                 gboolean                skip_initial);

PangoTextTransform      gtk_css_style_get_pango_text_transform  (GtkCssStyle            *style);
char *                  gtk_css_style_compute_font_features     (GtkCssStyle            *style);
PangoAttrList *         gtk_css_style_get_pango_attributes      (GtkCssStyle            *style);
PangoFontDescription *  gtk_css_style_get_pango_font            (GtkCssStyle            *style);

GtkCssValues *gtk_css_values_new   (GtkCssValuesType  type);
GtkCssValues *gtk_css_values_ref   (GtkCssValues     *values);
void          gtk_css_values_unref (GtkCssValues     *values);
GtkCssValues *gtk_css_values_copy  (GtkCssValues     *values);

void gtk_css_core_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_background_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_border_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_icon_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_outline_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_font_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_font_variant_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_animation_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_transition_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_size_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);
void gtk_css_other_values_compute_changes_and_affects (GtkCssStyle *style1,
                                                      GtkCssStyle *style2,
                                                      GtkBitmask    **changes,
                                                      GtkCssAffects *affects);

G_END_DECLS

