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
 * Authors: Alexander Larsson <alexl@gnome.org>
 */

#pragma once

#include "gtkenums.h"
#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

GtkCssValue *   _gtk_css_blend_mode_value_new         (GskBlendMode       blend_mode);
GtkCssValue *   _gtk_css_blend_mode_value_try_parse   (GtkCssParser      *parser);
GskBlendMode    _gtk_css_blend_mode_value_get         (const GtkCssValue *value);

GtkCssValue *   _gtk_css_border_style_value_new       (GtkBorderStyle     border_style);
GtkCssValue *   _gtk_css_border_style_value_try_parse (GtkCssParser      *parser);
GtkBorderStyle  _gtk_css_border_style_value_get       (const GtkCssValue *value);

GtkCssValue *   _gtk_css_font_size_value_new          (GtkCssFontSize     size);
GtkCssValue *   _gtk_css_font_size_value_try_parse    (GtkCssParser      *parser);
GtkCssFontSize  _gtk_css_font_size_value_get          (const GtkCssValue *value);
double          gtk_css_font_size_get_default_px      (GtkStyleProvider  *provider,
                                                       GtkCssStyle       *style);

GtkCssValue *   _gtk_css_font_style_value_new         (PangoStyle         style);
GtkCssValue *   _gtk_css_font_style_value_try_parse   (GtkCssParser      *parser);
PangoStyle      _gtk_css_font_style_value_get         (const GtkCssValue *value);

GtkCssValue *   gtk_css_font_weight_value_try_parse   (GtkCssParser      *parser);
PangoWeight     gtk_css_font_weight_value_get         (const GtkCssValue *value);

GtkCssValue *   _gtk_css_font_stretch_value_new       (PangoStretch       stretch);
GtkCssValue *   _gtk_css_font_stretch_value_try_parse (GtkCssParser      *parser);
PangoStretch    _gtk_css_font_stretch_value_get       (const GtkCssValue *value);

GtkCssValue *         _gtk_css_text_decoration_line_value_new     (GtkTextDecorationLine  line);
GtkTextDecorationLine _gtk_css_text_decoration_line_try_parse_one (GtkCssParser          *parser,
                                                                   GtkTextDecorationLine  base);
GtkTextDecorationLine _gtk_css_text_decoration_line_value_get     (const GtkCssValue     *value);

GtkCssValue *          _gtk_css_text_decoration_style_value_new   (GtkTextDecorationStyle  style);
GtkCssValue *          _gtk_css_text_decoration_style_value_try_parse (GtkCssParser           *parser);
GtkTextDecorationStyle _gtk_css_text_decoration_style_value_get       (const GtkCssValue      *value);

GtkCssValue *   _gtk_css_area_value_new               (GtkCssArea         area);
GtkCssValue *   _gtk_css_area_value_try_parse         (GtkCssParser      *parser);
GtkCssArea      _gtk_css_area_value_get               (const GtkCssValue *value);

GtkCssValue *   _gtk_css_direction_value_new          (GtkCssDirection    direction);
GtkCssValue *   _gtk_css_direction_value_try_parse    (GtkCssParser      *parser);
GtkCssDirection _gtk_css_direction_value_get          (const GtkCssValue *value);

GtkCssValue *   _gtk_css_play_state_value_new         (GtkCssPlayState    play_state);
GtkCssValue *   _gtk_css_play_state_value_try_parse   (GtkCssParser      *parser);
GtkCssPlayState _gtk_css_play_state_value_get         (const GtkCssValue *value);

GtkCssValue *   _gtk_css_fill_mode_value_new          (GtkCssFillMode     fill_mode);
GtkCssValue *   _gtk_css_fill_mode_value_try_parse    (GtkCssParser      *parser);
GtkCssFillMode  _gtk_css_fill_mode_value_get          (const GtkCssValue *value);

GtkCssValue *   _gtk_css_icon_style_value_new         (GtkCssIconStyle    icon_style);
GtkCssValue *   _gtk_css_icon_style_value_try_parse   (GtkCssParser      *parser);
GtkCssIconStyle _gtk_css_icon_style_value_get         (const GtkCssValue *value);

GtkCssValue *     _gtk_css_font_kerning_value_new       (GtkCssFontKerning  kerning);
GtkCssValue *     _gtk_css_font_kerning_value_try_parse (GtkCssParser      *parser);
GtkCssFontKerning _gtk_css_font_kerning_value_get       (const GtkCssValue *value);

GtkCssValue *        _gtk_css_font_variant_position_value_new       (GtkCssFontVariantPosition  position);
GtkCssValue *        _gtk_css_font_variant_position_value_try_parse (GtkCssParser         *parser);
GtkCssFontVariantPosition _gtk_css_font_variant_position_value_get       (const GtkCssValue    *value);

GtkCssValue *         _gtk_css_font_variant_caps_value_new       (GtkCssFontVariantCaps  caps);
GtkCssValue *         _gtk_css_font_variant_caps_value_try_parse (GtkCssParser          *parser);
GtkCssFontVariantCaps _gtk_css_font_variant_caps_value_get       (const GtkCssValue     *value);

GtkCssValue *        _gtk_css_font_variant_alternate_value_new       (GtkCssFontVariantAlternate  alternates);
GtkCssValue *        _gtk_css_font_variant_alternate_value_try_parse (GtkCssParser         *parser);
GtkCssFontVariantAlternate _gtk_css_font_variant_alternate_value_get       (const GtkCssValue    *value);

GtkCssValue *        _gtk_css_font_variant_ligature_value_new          (GtkCssFontVariantLigature  ligatures);
GtkCssFontVariantLigature _gtk_css_font_variant_ligature_try_parse_one (GtkCssParser               *parser,
                                                                        GtkCssFontVariantLigature   base);
GtkCssFontVariantLigature _gtk_css_font_variant_ligature_value_get     (const GtkCssValue          *value);

GtkCssValue *        _gtk_css_font_variant_numeric_value_new          (GtkCssFontVariantNumeric     numeric);
GtkCssFontVariantNumeric _gtk_css_font_variant_numeric_try_parse_one (GtkCssParser               *parser,
                                                                      GtkCssFontVariantNumeric    base);
GtkCssFontVariantNumeric _gtk_css_font_variant_numeric_value_get     (const GtkCssValue          *value);


GtkCssValue *        _gtk_css_font_variant_east_asian_value_new          (GtkCssFontVariantEastAsian east_asian);
GtkCssFontVariantEastAsian _gtk_css_font_variant_east_asian_try_parse_one (GtkCssParser               *parser,
                                                                      GtkCssFontVariantEastAsian base);
GtkCssFontVariantEastAsian _gtk_css_font_variant_east_asian_value_get     (const GtkCssValue          *value);

GtkCssValue *          _gtk_css_text_transform_value_new       (GtkTextTransform transform);
GtkCssValue *          _gtk_css_text_transform_value_try_parse (GtkCssParser           *parser);
GtkTextTransform       _gtk_css_text_transform_value_get       (const GtkCssValue      *value);
G_END_DECLS

