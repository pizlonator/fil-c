/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdwrite-enums.h"
#include "gstdwrite-utils.h"

/* XXX: MinGW's header does not define some enum values */
enum
{
  GST_DWRITE_TEXT_ALIGNMENT_LEADING,
  GST_DWRITE_TEXT_ALIGNMENT_TRAILING,
  GST_DWRITE_TEXT_ALIGNMENT_CENTER,
  GST_DWRITE_TEXT_ALIGNMENT_JUSTIFIED,
};

enum
{
  GST_DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
  GST_DWRITE_PARAGRAPH_ALIGNMENT_FAR,
  GST_DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
};

enum
{
  GST_DWRITE_FONT_WEIGHT_THIN = 100,
  GST_DWRITE_FONT_WEIGHT_EXTRA_LIGHT = 200,
  GST_DWRITE_FONT_WEIGHT_ULTRA_LIGHT = 200,
  GST_DWRITE_FONT_WEIGHT_LIGHT = 300,
  GST_DWRITE_FONT_WEIGHT_SEMI_LIGHT = 350,
  GST_DWRITE_FONT_WEIGHT_NORMAL = 400,
  GST_DWRITE_FONT_WEIGHT_REGULAR = 400,
  GST_DWRITE_FONT_WEIGHT_MEDIUM = 500,
  GST_DWRITE_FONT_WEIGHT_DEMI_BOLD = 600,
  GST_DWRITE_FONT_WEIGHT_SEMI_BOLD = 600,
  GST_DWRITE_FONT_WEIGHT_BOLD = 700,
  GST_DWRITE_FONT_WEIGHT_EXTRA_BOLD = 800,
  GST_DWRITE_FONT_WEIGHT_ULTRA_BOLD = 800,
  GST_DWRITE_FONT_WEIGHT_BLACK = 900,
  GST_DWRITE_FONT_WEIGHT_HEAVY = 900,
  GST_DWRITE_FONT_WEIGHT_EXTRA_BLACK = 950,
  GST_DWRITE_FONT_WEIGHT_ULTRA_BLACK = 950
};

enum
{
  GST_DWRITE_FONT_STYLE_NORMAL,
  GST_DWRITE_FONT_STYLE_OBLIQUE,
  GST_DWRITE_FONT_STYLE_ITALIC,
};

enum
{
  GST_DWRITE_FONT_STRETCH_UNDEFINED = 0,
  GST_DWRITE_FONT_STRETCH_ULTRA_CONDENSED = 1,
  GST_DWRITE_FONT_STRETCH_EXTRA_CONDENSED = 2,
  GST_DWRITE_FONT_STRETCH_CONDENSED = 3,
  GST_DWRITE_FONT_STRETCH_SEMI_CONDENSED = 4,
  GST_DWRITE_FONT_STRETCH_NORMAL = 5,
  GST_DWRITE_FONT_STRETCH_MEDIUM = 5,
  GST_DWRITE_FONT_STRETCH_SEMI_EXPANDED = 6,
  GST_DWRITE_FONT_STRETCH_EXPANDED = 7,
  GST_DWRITE_FONT_STRETCH_EXTRA_EXPANDED = 8,
  GST_DWRITE_FONT_STRETCH_ULTRA_EXPANDED = 9
};

GType
gst_dwrite_text_alignment_get_type (void)
{
  static GType text_align_type = 0;
  static const GEnumValue text_align_types[] = {
    {GST_DWRITE_TEXT_ALIGNMENT_LEADING, "DWRITE_TEXT_ALIGNMENT_LEADING",
        "leading"},
    {GST_DWRITE_TEXT_ALIGNMENT_TRAILING,
        "DWRITE_TEXT_ALIGNMENT_TRAILING", "trailing"},
    {GST_DWRITE_TEXT_ALIGNMENT_CENTER,
        "DWRITE_TEXT_ALIGNMENT_CENTER", "center"},
    {GST_DWRITE_TEXT_ALIGNMENT_JUSTIFIED,
        "DWRITE_TEXT_ALIGNMENT_JUSTIFIED", "justified"},
    {0, nullptr, nullptr},
  };

  GST_DWRITE_CALL_ONCE_BEGIN {
    text_align_type = g_enum_register_static ("GstDWriteTextAlignment",
        text_align_types);
  } GST_DWRITE_CALL_ONCE_END;

  return text_align_type;
}

GType
gst_dwrite_paragraph_alignment_get_type (void)
{
  static GType paragraph_align_type = 0;
  static const GEnumValue paragraph_align_types[] = {
    {GST_DWRITE_PARAGRAPH_ALIGNMENT_NEAR, "DWRITE_PARAGRAPH_ALIGNMENT_NEAR",
        "near"},
    {GST_DWRITE_PARAGRAPH_ALIGNMENT_FAR,
        "DWRITE_PARAGRAPH_ALIGNMENT_FAR", "far"},
    {GST_DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        "DWRITE_PARAGRAPH_ALIGNMENT_CENTER", "center"},
    {0, nullptr, nullptr},
  };

  GST_DWRITE_CALL_ONCE_BEGIN {
    paragraph_align_type =
        g_enum_register_static ("GstDWriteParagraphAlignment",
        paragraph_align_types);
  } GST_DWRITE_CALL_ONCE_END;

  return paragraph_align_type;
}

GType
gst_dwrite_font_weight_get_type (void)
{
  static GType font_weight_type = 0;
  static const GEnumValue font_weight_types[] = {
    {GST_DWRITE_FONT_WEIGHT_THIN, "DWRITE_FONT_WEIGHT_THIN", "thin"},
    {GST_DWRITE_FONT_WEIGHT_EXTRA_LIGHT, "DWRITE_FONT_WEIGHT_EXTRA_LIGHT",
        "extra-light"},
    {GST_DWRITE_FONT_WEIGHT_ULTRA_LIGHT,
        "DWRITE_FONT_WEIGHT_ULTRA_LIGHT", "ultra-light"},
    {GST_DWRITE_FONT_WEIGHT_LIGHT, "DWRITE_FONT_WEIGHT_LIGHT", "light"},
    {GST_DWRITE_FONT_WEIGHT_SEMI_LIGHT, "DWRITE_FONT_WEIGHT_SEMI_LIGHT",
        "semi-light"},
    {GST_DWRITE_FONT_WEIGHT_NORMAL, "DWRITE_FONT_WEIGHT_NORMAL", "normal"},
    {GST_DWRITE_FONT_WEIGHT_REGULAR, "DWRITE_FONT_WEIGHT_REGULAR",
        "regular"},
    {GST_DWRITE_FONT_WEIGHT_MEDIUM, "DWRITE_FONT_WEIGHT_MEDIUM", "medium"},
    {GST_DWRITE_FONT_WEIGHT_DEMI_BOLD, "DWRITE_FONT_WEIGHT_DEMI_BOLD",
        "demi-bold"},
    {GST_DWRITE_FONT_WEIGHT_SEMI_BOLD, "DWRITE_FONT_WEIGHT_SEMI_BOLD",
        "semi-bold"},
    {GST_DWRITE_FONT_WEIGHT_BOLD, "DWRITE_FONT_WEIGHT_BOLD", "bold"},
    {GST_DWRITE_FONT_WEIGHT_EXTRA_BOLD, "DWRITE_FONT_WEIGHT_EXTRA_BOLD",
        "extra-bold"},
    {GST_DWRITE_FONT_WEIGHT_ULTRA_BOLD, "DWRITE_FONT_WEIGHT_ULTRA_BOLD",
        "ultra-bold"},
    {GST_DWRITE_FONT_WEIGHT_BLACK, "DWRITE_FONT_WEIGHT_BLACK", "black"},
    {GST_DWRITE_FONT_WEIGHT_HEAVY, "DWRITE_FONT_WEIGHT_HEAVY", "heavy"},
    {GST_DWRITE_FONT_WEIGHT_EXTRA_BLACK, "DWRITE_FONT_WEIGHT_EXTRA_BLACK",
        "extra-black"},
    {GST_DWRITE_FONT_WEIGHT_ULTRA_BLACK, "DWRITE_FONT_WEIGHT_ULTRA_BLACK",
        "ultra-black"},
    {0, nullptr, nullptr},
  };

  GST_DWRITE_CALL_ONCE_BEGIN {
    font_weight_type = g_enum_register_static ("GstDWriteFontWeight",
        font_weight_types);
  } GST_DWRITE_CALL_ONCE_END;

  return font_weight_type;
}

GType
gst_dwrite_font_style_get_type (void)
{
  static GType font_style_type = 0;
  static const GEnumValue font_style_types[] = {
    {GST_DWRITE_FONT_STYLE_NORMAL, "DWRITE_FONT_STYLE_NORMAL", "normal"},
    {GST_DWRITE_FONT_STYLE_OBLIQUE, "DWRITE_FONT_STYLE_OBLIQUE", "oblique"},
    {GST_DWRITE_FONT_STYLE_ITALIC, "DWRITE_FONT_STYLE_ITALIC", "italic"},
    {0, nullptr, nullptr},
  };

  GST_DWRITE_CALL_ONCE_BEGIN {
    font_style_type = g_enum_register_static ("GstDWriteFontStyle",
        font_style_types);
  } GST_DWRITE_CALL_ONCE_END;

  return font_style_type;
}

GType
gst_dwrite_font_stretch_get_type (void)
{
  static GType font_stretch_type = 0;
  static const GEnumValue font_stretch_types[] = {
    {GST_DWRITE_FONT_STRETCH_UNDEFINED, "DWRITE_FONT_STRETCH_UNDEFINED",
        "undefined"},
    {GST_DWRITE_FONT_STRETCH_ULTRA_CONDENSED,
          "DWRITE_FONT_STRETCH_ULTRA_CONDENSED",
        "ultra-condensed"},
    {GST_DWRITE_FONT_STRETCH_EXTRA_CONDENSED,
          "DWRITE_FONT_STRETCH_EXTRA_CONDENSED",
        "extra-condensed"},
    {GST_DWRITE_FONT_STRETCH_CONDENSED, "DWRITE_FONT_STRETCH_CONDENSED",
        "condensed"},
    {GST_DWRITE_FONT_STRETCH_SEMI_CONDENSED,
          "DWRITE_FONT_STRETCH_SEMI_CONDENSED",
        "semi-condensed"},
    {GST_DWRITE_FONT_STRETCH_NORMAL, "DWRITE_FONT_STRETCH_NORMAL",
        "normal"},
    {GST_DWRITE_FONT_STRETCH_MEDIUM, "DWRITE_FONT_STRETCH_MEDIUM",
        "medium"},
    {GST_DWRITE_FONT_STRETCH_SEMI_EXPANDED,
          "DWRITE_FONT_STRETCH_SEMI_EXPANDED",
        "semi-expanded"},
    {GST_DWRITE_FONT_STRETCH_EXPANDED, "DWRITE_FONT_STRETCH_EXPANDED",
        "expanded"},
    {GST_DWRITE_FONT_STRETCH_EXTRA_EXPANDED,
          "DWRITE_FONT_STRETCH_EXTRA_EXPANDED",
        "extra-expanded"},
    {GST_DWRITE_FONT_STRETCH_ULTRA_EXPANDED,
          "DWRITE_FONT_STRETCH_ULTRA_EXPANDED",
        "ultra-expanded"},
    {0, nullptr, nullptr},
  };

  GST_DWRITE_CALL_ONCE_BEGIN {
    font_stretch_type = g_enum_register_static ("GstDWriteFontStretch",
        font_stretch_types);
  } GST_DWRITE_CALL_ONCE_END;

  return font_stretch_type;
}
