/* GStreamer TTML subtitle parser
 * Copyright (C) <2015> British Broadcasting Corporation
 *   Authors:
 *     Chris Bass <dash@rd.bbc.co.uk>
 *     Peter Taylour <dash@rd.bbc.co.uk>
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

/*
 * Parses subtitle files encoded using the EBU-TT-D profile of TTML, as defined
 * in https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3380.pdf
 * and http://www.w3.org/TR/ttaf1-dfxp/, respectively.
 */

#include <glib.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "ttmlparse.h"
#include "subtitle.h"
#include "subtitlemeta.h"

#define DEFAULT_CELLRES_X 32
#define DEFAULT_CELLRES_Y 15
#define MAX_FONT_FAMILY_NAME_LENGTH 128
#define NSECONDS_IN_DAY 24 * 3600 * GST_SECOND

#define TTML_CHAR_NULL 0x00
#define TTML_CHAR_SPACE 0x20
#define TTML_CHAR_TAB 0x09
#define TTML_CHAR_LF 0x0A
#define TTML_CHAR_CR 0x0D

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

static gchar *ttml_get_xml_property (const xmlNode * node, const char *name);
static gpointer ttml_copy_tree_element (gconstpointer src, gpointer data);

typedef struct _TtmlStyleSet TtmlStyleSet;
typedef struct _TtmlElement TtmlElement;
typedef struct _TtmlScene TtmlScene;

typedef enum
{
  TTML_ELEMENT_TYPE_STYLE,
  TTML_ELEMENT_TYPE_REGION,
  TTML_ELEMENT_TYPE_BODY,
  TTML_ELEMENT_TYPE_DIV,
  TTML_ELEMENT_TYPE_P,
  TTML_ELEMENT_TYPE_SPAN,
  TTML_ELEMENT_TYPE_ANON_SPAN,
  TTML_ELEMENT_TYPE_BR
} TtmlElementType;

typedef enum
{
  TTML_WHITESPACE_MODE_NONE,
  TTML_WHITESPACE_MODE_DEFAULT,
  TTML_WHITESPACE_MODE_PRESERVE,
} TtmlWhitespaceMode;

struct _TtmlElement
{
  TtmlElementType type;
  gchar *id;
  TtmlWhitespaceMode whitespace_mode;
  gchar **styles;
  gchar *region;
  GstClockTime begin;
  GstClockTime end;
  TtmlStyleSet *style_set;
  gchar *text;
};

/* Represents a static scene consisting of one or more trees of elements that
 * should be visible over a specific period of time. */
struct _TtmlScene
{
  GstClockTime begin;
  GstClockTime end;
  GList *trees;
  GstBuffer *buf;
};

struct _TtmlStyleSet
{
  GHashTable *table;
};


static TtmlStyleSet *
ttml_style_set_new (void)
{
  TtmlStyleSet *ret = g_new0 (TtmlStyleSet, 1);
  ret->table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  return ret;
}


static void
ttml_style_set_delete (TtmlStyleSet * style_set)
{
  if (style_set) {
    g_hash_table_unref (style_set->table);
    g_free (style_set);
  }
}


/* If attribute with name @attr_name already exists in @style_set, its value
 * will be replaced by @attr_value. */
static gboolean
ttml_style_set_add_attr (TtmlStyleSet * style_set, const gchar * attr_name,
    const gchar * attr_value)
{
  return g_hash_table_insert (style_set->table, g_strdup (attr_name),
      g_strdup (attr_value));
}


static gboolean
ttml_style_set_contains_attr (TtmlStyleSet * style_set, const gchar * attr_name)
{
  return g_hash_table_contains (style_set->table, attr_name);
}


static const gchar *
ttml_style_set_get_attr (TtmlStyleSet * style_set, const gchar * attr_name)
{
  return g_hash_table_lookup (style_set->table, attr_name);
}


static guint8
ttml_hex_pair_to_byte (const gchar * hex_pair)
{
  gint hi_digit, lo_digit;

  hi_digit = g_ascii_xdigit_value (*hex_pair);
  lo_digit = g_ascii_xdigit_value (*(hex_pair + 1));
  return (hi_digit << 4) + lo_digit;
}


/* Color strings in EBU-TT-D can have the form "#RRBBGG" or "#RRBBGGAA". */
static GstSubtitleColor
ttml_parse_colorstring (const gchar * color)
{
  guint length;
  const gchar *c = NULL;
  GstSubtitleColor ret = { 0, 0, 0, 0 };

  if (!color)
    return ret;

  length = strlen (color);
  if (((length == 7) || (length == 9)) && *color == '#') {
    c = color + 1;

    ret.r = ttml_hex_pair_to_byte (c);
    ret.g = ttml_hex_pair_to_byte (c + 2);
    ret.b = ttml_hex_pair_to_byte (c + 4);

    if (length == 7)
      ret.a = G_MAXUINT8;
    else
      ret.a = ttml_hex_pair_to_byte (c + 6);

    GST_CAT_LOG (ttmlparse_debug, "Returning color - r:%u  b:%u  g:%u  a:%u",
        ret.r, ret.b, ret.g, ret.a);
  } else {
    GST_CAT_ERROR (ttmlparse_debug, "Invalid color string: %s", color);
  }

  return ret;
}


static void
ttml_style_set_print (TtmlStyleSet * style_set)
{
  GHashTableIter iter;
  gpointer attr_name, attr_value;

  if (!style_set) {
    GST_CAT_LOG (ttmlparse_debug, "\t\t[NULL]");
    return;
  }

  g_hash_table_iter_init (&iter, style_set->table);
  while (g_hash_table_iter_next (&iter, &attr_name, &attr_value)) {
    GST_CAT_LOG (ttmlparse_debug, "\t\t%s: %s", (const gchar *) attr_name,
        (const gchar *) attr_value);
  }
}


static TtmlStyleSet *
ttml_parse_style_set (const xmlNode * node)
{
  TtmlStyleSet *s;
  gchar *value = NULL;
  xmlAttrPtr attr;

  value = ttml_get_xml_property (node, "id");
  if (!value) {
    GST_CAT_ERROR (ttmlparse_debug, "styles must have an ID.");
    return NULL;
  }
  g_free (value);

  s = ttml_style_set_new ();

  for (attr = node->properties; attr != NULL; attr = attr->next) {
    if (attr->ns && ((g_strcmp0 ((const gchar *) attr->ns->prefix, "tts") == 0)
            || (g_strcmp0 ((const gchar *) attr->ns->prefix, "itts") == 0)
            || (g_strcmp0 ((const gchar *) attr->ns->prefix, "ebutts") == 0))) {
      ttml_style_set_add_attr (s, (const gchar *) attr->name,
          (const gchar *) attr->children->content);
    }
  }

  return s;
}


static void
ttml_delete_element (TtmlElement * element)
{
  g_free ((gpointer) element->id);
  if (element->styles)
    g_strfreev (element->styles);
  g_free ((gpointer) element->region);
  ttml_style_set_delete (element->style_set);
  g_free ((gpointer) element->text);
  g_free (element);
}


static gchar *
ttml_get_xml_property (const xmlNode * node, const char *name)
{
  xmlChar *xml_string = NULL;
  gchar *gst_string = NULL;

  g_return_val_if_fail (strlen (name) < 128, NULL);

  xml_string = xmlGetProp (node, (xmlChar *) name);
  if (!xml_string)
    return NULL;
  gst_string = g_strdup ((gchar *) xml_string);
  xmlFree (xml_string);
  return gst_string;
}


/* EBU-TT-D timecodes have format hours:minutes:seconds[.fraction] */
static GstClockTime
ttml_parse_timecode (const gchar * timestring)
{
  gchar **strings;
  guint64 hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
  GstClockTime time = GST_CLOCK_TIME_NONE;

  GST_CAT_LOG (ttmlparse_debug, "time string: %s", timestring);

  strings = g_strsplit (timestring, ":", 3);
  if (g_strv_length (strings) != 3U) {
    GST_CAT_ERROR (ttmlparse_debug, "badly formatted time string: %s",
        timestring);
    return time;
  }

  hours = g_ascii_strtoull (strings[0], NULL, 10U);
  minutes = g_ascii_strtoull (strings[1], NULL, 10U);
  if (g_strstr_len (strings[2], -1, ".")) {
    guint n_digits;
    gchar **substrings = g_strsplit (strings[2], ".", 2);
    seconds = g_ascii_strtoull (substrings[0], NULL, 10U);
    n_digits = strlen (substrings[1]);
    milliseconds = g_ascii_strtoull (substrings[1], NULL, 10U);
    milliseconds =
        (guint64) (milliseconds * pow (10.0, (3 - (double) n_digits)));
    g_strfreev (substrings);
  } else {
    seconds = g_ascii_strtoull (strings[2], NULL, 10U);
  }

  if (minutes > 59 || seconds > 60) {
    GST_CAT_ERROR (ttmlparse_debug, "invalid time string "
        "(minutes or seconds out-of-bounds): %s\n", timestring);
  }

  g_strfreev (strings);
  GST_CAT_LOG (ttmlparse_debug,
      "hours: %" G_GUINT64_FORMAT "  minutes: %" G_GUINT64_FORMAT
      "  seconds: %" G_GUINT64_FORMAT "  milliseconds: %" G_GUINT64_FORMAT "",
      hours, minutes, seconds, milliseconds);

  time = hours * GST_SECOND * 3600
      + minutes * GST_SECOND * 60
      + seconds * GST_SECOND + milliseconds * GST_MSECOND;

  return time;
}


static TtmlElement *
ttml_parse_element (const xmlNode * node)
{
  TtmlElement *element;
  TtmlElementType type;
  gchar *value;

  GST_CAT_DEBUG (ttmlparse_debug, "Element name: %s",
      (const char *) node->name);
  if ((g_strcmp0 ((const char *) node->name, "style") == 0)) {
    type = TTML_ELEMENT_TYPE_STYLE;
  } else if ((g_strcmp0 ((const char *) node->name, "region") == 0)) {
    type = TTML_ELEMENT_TYPE_REGION;
  } else if ((g_strcmp0 ((const char *) node->name, "body") == 0)) {
    type = TTML_ELEMENT_TYPE_BODY;
  } else if ((g_strcmp0 ((const char *) node->name, "div") == 0)) {
    type = TTML_ELEMENT_TYPE_DIV;
  } else if ((g_strcmp0 ((const char *) node->name, "p") == 0)) {
    type = TTML_ELEMENT_TYPE_P;
  } else if ((g_strcmp0 ((const char *) node->name, "span") == 0)) {
    type = TTML_ELEMENT_TYPE_SPAN;
  } else if ((g_strcmp0 ((const char *) node->name, "text") == 0)) {
    type = TTML_ELEMENT_TYPE_ANON_SPAN;
  } else if ((g_strcmp0 ((const char *) node->name, "br") == 0)) {
    type = TTML_ELEMENT_TYPE_BR;
  } else {
    return NULL;
  }

  element = g_new0 (TtmlElement, 1);
  element->type = type;

  if ((value = ttml_get_xml_property (node, "id"))) {
    element->id = g_strdup (value);
    g_free (value);
  }

  if ((value = ttml_get_xml_property (node, "style"))) {
    element->styles = g_strsplit (value, " ", 0);
    GST_CAT_DEBUG (ttmlparse_debug, "%u style(s) referenced in element.",
        g_strv_length (element->styles));
    g_free (value);
  }

  if (element->type == TTML_ELEMENT_TYPE_STYLE
      || element->type == TTML_ELEMENT_TYPE_REGION) {
    TtmlStyleSet *ss;
    ss = ttml_parse_style_set (node);
    if (ss)
      element->style_set = ss;
    else
      GST_CAT_WARNING (ttmlparse_debug,
          "Style or Region contains no styling attributes.");
  }

  if ((value = ttml_get_xml_property (node, "region"))) {
    element->region = g_strdup (value);
    g_free (value);
  }

  if ((value = ttml_get_xml_property (node, "begin"))) {
    element->begin = ttml_parse_timecode (value);
    g_free (value);
  } else {
    element->begin = GST_CLOCK_TIME_NONE;
  }

  if ((value = ttml_get_xml_property (node, "end"))) {
    element->end = ttml_parse_timecode (value);
    g_free (value);
  } else {
    element->end = GST_CLOCK_TIME_NONE;
  }

  if (node->content) {
    GST_CAT_LOG (ttmlparse_debug, "Node content: %s", node->content);
    element->text = g_strdup ((const gchar *) node->content);
  }

  if (element->type == TTML_ELEMENT_TYPE_BR)
    element->text = g_strdup ("\n");

  if ((value = ttml_get_xml_property (node, "space"))) {
    if (g_strcmp0 (value, "preserve") == 0)
      element->whitespace_mode = TTML_WHITESPACE_MODE_PRESERVE;
    else if (g_strcmp0 (value, "default") == 0)
      element->whitespace_mode = TTML_WHITESPACE_MODE_DEFAULT;
    g_free (value);
  }

  return element;
}


static GNode *
ttml_parse_body (const xmlNode * node)
{
  GNode *ret;
  TtmlElement *element;

  GST_CAT_LOG (ttmlparse_debug, "parsing node %s", node->name);
  element = ttml_parse_element (node);
  if (element)
    ret = g_node_new (element);
  else
    return NULL;

  for (node = node->children; node != NULL; node = node->next) {
    GNode *descendants = NULL;
    if ((descendants = ttml_parse_body (node)))
      g_node_append (ret, descendants);
  }

  return ret;
}


/* Update the fields of a GstSubtitleStyleSet, @style_set, according to the
 * values defined in a TtmlStyleSet, @tss, and a given cell resolution. */
static void
ttml_update_style_set (GstSubtitleStyleSet * style_set, TtmlStyleSet * tss,
    guint cellres_x, guint cellres_y)
{
  const gchar *attr;

  if ((attr = ttml_style_set_get_attr (tss, "textDirection"))) {
    if (g_strcmp0 (attr, "rtl") == 0)
      style_set->text_direction = GST_SUBTITLE_TEXT_DIRECTION_RTL;
    else
      style_set->text_direction = GST_SUBTITLE_TEXT_DIRECTION_LTR;
  }

  if ((attr = ttml_style_set_get_attr (tss, "fontFamily"))) {
    if (strlen (attr) <= MAX_FONT_FAMILY_NAME_LENGTH) {
      g_free (style_set->font_family);
      style_set->font_family = g_strdup (attr);
    } else {
      GST_CAT_WARNING (ttmlparse_debug,
          "Ignoring font family name as it's overly long.");
    }
  }

  if ((attr = ttml_style_set_get_attr (tss, "fontSize"))) {
    style_set->font_size = g_ascii_strtod (attr, NULL) / 100.0;
  }
  style_set->font_size *= (1.0 / cellres_y);

  if ((attr = ttml_style_set_get_attr (tss, "lineHeight"))) {
    if (g_strcmp0 (attr, "normal") == 0)
      style_set->line_height = -1;
    else
      style_set->line_height = g_ascii_strtod (attr, NULL) / 100.0;
  }

  if ((attr = ttml_style_set_get_attr (tss, "textAlign"))) {
    if (g_strcmp0 (attr, "left") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_LEFT;
    else if (g_strcmp0 (attr, "center") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_CENTER;
    else if (g_strcmp0 (attr, "right") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_RIGHT;
    else if (g_strcmp0 (attr, "end") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_END;
    else
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_START;
  }

  if ((attr = ttml_style_set_get_attr (tss, "color"))) {
    style_set->color = ttml_parse_colorstring (attr);
  }

  if ((attr = ttml_style_set_get_attr (tss, "backgroundColor"))) {
    style_set->background_color = ttml_parse_colorstring (attr);
  }

  if ((attr = ttml_style_set_get_attr (tss, "fontStyle"))) {
    if (g_strcmp0 (attr, "italic") == 0)
      style_set->font_style = GST_SUBTITLE_FONT_STYLE_ITALIC;
    else
      style_set->font_style = GST_SUBTITLE_FONT_STYLE_NORMAL;
  }

  if ((attr = ttml_style_set_get_attr (tss, "fontWeight"))) {
    if (g_strcmp0 (attr, "bold") == 0)
      style_set->font_weight = GST_SUBTITLE_FONT_WEIGHT_BOLD;
    else
      style_set->font_weight = GST_SUBTITLE_FONT_WEIGHT_NORMAL;
  }

  if ((attr = ttml_style_set_get_attr (tss, "textDecoration"))) {
    if (g_strcmp0 (attr, "underline") == 0)
      style_set->text_decoration = GST_SUBTITLE_TEXT_DECORATION_UNDERLINE;
    else
      style_set->text_decoration = GST_SUBTITLE_TEXT_DECORATION_NONE;
  }

  if ((attr = ttml_style_set_get_attr (tss, "unicodeBidi"))) {
    if (g_strcmp0 (attr, "embed") == 0)
      style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_EMBED;
    else if (g_strcmp0 (attr, "bidiOverride") == 0)
      style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_OVERRIDE;
    else
      style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_NORMAL;
  }

  if ((attr = ttml_style_set_get_attr (tss, "wrapOption"))) {
    if (g_strcmp0 (attr, "noWrap") == 0)
      style_set->wrap_option = GST_SUBTITLE_WRAPPING_OFF;
    else
      style_set->wrap_option = GST_SUBTITLE_WRAPPING_ON;
  }

  if ((attr = ttml_style_set_get_attr (tss, "multiRowAlign"))) {
    if (g_strcmp0 (attr, "start") == 0)
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_START;
    else if (g_strcmp0 (attr, "center") == 0)
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER;
    else if (g_strcmp0 (attr, "end") == 0)
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_END;
    else
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO;
  }

  if ((attr = ttml_style_set_get_attr (tss, "linePadding"))) {
    style_set->line_padding = g_ascii_strtod (attr, NULL);
    style_set->line_padding *= (1.0 / cellres_x);
  }

  if ((attr = ttml_style_set_get_attr (tss, "origin"))) {
    gchar *c;
    style_set->origin_x = g_ascii_strtod (attr, &c) / 100.0;
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-')
      ++c;
    style_set->origin_y = g_ascii_strtod (c, NULL) / 100.0;
  }

  if ((attr = ttml_style_set_get_attr (tss, "extent"))) {
    gchar *c;
    style_set->extent_w = g_ascii_strtod (attr, &c) / 100.0;
    if ((style_set->origin_x + style_set->extent_w) > 1.0) {
      style_set->extent_w = 1.0 - style_set->origin_x;
    }
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-')
      ++c;
    style_set->extent_h = g_ascii_strtod (c, NULL) / 100.0;
    if ((style_set->origin_y + style_set->extent_h) > 1.0) {
      style_set->extent_h = 1.0 - style_set->origin_y;
    }
  }

  if ((attr = ttml_style_set_get_attr (tss, "displayAlign"))) {
    if (g_strcmp0 (attr, "center") == 0)
      style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_CENTER;
    else if (g_strcmp0 (attr, "after") == 0)
      style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_AFTER;
    else
      style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_BEFORE;
  }

  if ((attr = ttml_style_set_get_attr (tss, "padding"))) {
    gchar **decimals;
    guint n_decimals;
    guint i;

    decimals = g_strsplit (attr, "%", 0);
    n_decimals = g_strv_length (decimals) - 1;
    for (i = 0; i < n_decimals; ++i)
      g_strstrip (decimals[i]);

    switch (n_decimals) {
      case 1:
        style_set->padding_start = style_set->padding_end =
            style_set->padding_before = style_set->padding_after =
            g_ascii_strtod (decimals[0], NULL) / 100.0;
        break;

      case 2:
        style_set->padding_before = style_set->padding_after =
            g_ascii_strtod (decimals[0], NULL) / 100.0;
        style_set->padding_start = style_set->padding_end =
            g_ascii_strtod (decimals[1], NULL) / 100.0;
        break;

      case 3:
        style_set->padding_before = g_ascii_strtod (decimals[0], NULL) / 100.0;
        style_set->padding_start = style_set->padding_end =
            g_ascii_strtod (decimals[1], NULL) / 100.0;
        style_set->padding_after = g_ascii_strtod (decimals[2], NULL) / 100.0;
        break;

      case 4:
        style_set->padding_before = g_ascii_strtod (decimals[0], NULL) / 100.0;
        style_set->padding_end = g_ascii_strtod (decimals[1], NULL) / 100.0;
        style_set->padding_after = g_ascii_strtod (decimals[2], NULL) / 100.0;
        style_set->padding_start = g_ascii_strtod (decimals[3], NULL) / 100.0;
        break;
    }
    g_strfreev (decimals);

    /* Padding values in TTML files are relative to the region width & height;
     * make them relative to the overall display width & height like all other
     * dimensions. */
    style_set->padding_before *= style_set->extent_h;
    style_set->padding_after *= style_set->extent_h;
    style_set->padding_end *= style_set->extent_w;
    style_set->padding_start *= style_set->extent_w;
  }

  if ((attr = ttml_style_set_get_attr (tss, "writingMode"))) {
    if (g_str_has_prefix (attr, "rl"))
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_RLTB;
    else if ((g_strcmp0 (attr, "tbrl") == 0)
        || (g_strcmp0 (attr, "tb") == 0))
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_TBRL;
    else if (g_strcmp0 (attr, "tblr") == 0)
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_TBLR;
    else
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_LRTB;
  }

  if ((attr = ttml_style_set_get_attr (tss, "showBackground"))) {
    if (g_strcmp0 (attr, "whenActive") == 0)
      style_set->show_background = GST_SUBTITLE_BACKGROUND_MODE_WHEN_ACTIVE;
    else
      style_set->show_background = GST_SUBTITLE_BACKGROUND_MODE_ALWAYS;
  }

  if ((attr = ttml_style_set_get_attr (tss, "overflow"))) {
    if (g_strcmp0 (attr, "visible") == 0)
      style_set->overflow = GST_SUBTITLE_OVERFLOW_MODE_VISIBLE;
    else
      style_set->overflow = GST_SUBTITLE_OVERFLOW_MODE_HIDDEN;
  }

  if ((attr = ttml_style_set_get_attr (tss, "fillLineGap"))) {
    if (g_strcmp0 (attr, "true") == 0)
      style_set->fill_line_gap = TRUE;
  }
}


static TtmlStyleSet *
ttml_style_set_copy (TtmlStyleSet * style_set)
{
  GHashTableIter iter;
  gpointer attr_name, attr_value;
  TtmlStyleSet *ret = ttml_style_set_new ();

  g_hash_table_iter_init (&iter, style_set->table);
  while (g_hash_table_iter_next (&iter, &attr_name, &attr_value)) {
    ttml_style_set_add_attr (ret, (const gchar *) attr_name,
        (const gchar *) attr_value);
  }

  return ret;
}


/* set2 overrides set1. Unlike style inheritance, merging will result in all
 * values from set1 being merged into set2. */
static TtmlStyleSet *
ttml_style_set_merge (TtmlStyleSet * set1, TtmlStyleSet * set2)
{
  TtmlStyleSet *ret = NULL;

  if (set1) {
    ret = ttml_style_set_copy (set1);

    if (set2) {
      GHashTableIter iter;
      gpointer attr_name, attr_value;

      g_hash_table_iter_init (&iter, set2->table);
      while (g_hash_table_iter_next (&iter, &attr_name, &attr_value)) {
        ttml_style_set_add_attr (ret, (const gchar *) attr_name,
            (const gchar *) attr_value);
      }
    }
  } else if (set2) {
    ret = ttml_style_set_copy (set2);
  }

  return ret;
}


static gchar *
ttml_get_relative_font_size (const gchar * parent_size,
    const gchar * child_size)
{
  guint psize = (guint) g_ascii_strtoull (parent_size, NULL, 10U);
  guint csize = (guint) g_ascii_strtoull (child_size, NULL, 10U);
  csize = (csize * psize) / 100U;
  return g_strdup_printf ("%u%%", csize);
}


static TtmlStyleSet *
ttml_style_set_inherit (TtmlStyleSet * parent, TtmlStyleSet * child)
{
  TtmlStyleSet *ret = NULL;
  GHashTableIter iter;
  gpointer attr_name, attr_value;

  if (child) {
    ret = ttml_style_set_copy (child);
  } else {
    ret = ttml_style_set_new ();
  }

  if (!parent)
    return ret;

  g_hash_table_iter_init (&iter, parent->table);
  while (g_hash_table_iter_next (&iter, &attr_name, &attr_value)) {
    /* In TTML, if an element which has a defined fontSize is the child of an
     * element that also has a defined fontSize, the child's font size is
     * relative to that of its parent. If its parent doesn't have a defined
     * fontSize, then the child's fontSize is relative to the document's cell
     * size. Therefore, if the former is true, we calculate the value of
     * fontSize based on the parent's fontSize; otherwise, we simply keep
     * the value defined in the child's style set. */
    if (g_strcmp0 ((const gchar *) attr_name, "fontSize") == 0
        && ttml_style_set_contains_attr (ret, "fontSize")) {
      const gchar *original_child_font_size =
          ttml_style_set_get_attr (ret, "fontSize");
      gchar *scaled_child_font_size =
          ttml_get_relative_font_size ((const gchar *) attr_value,
          original_child_font_size);
      GST_CAT_LOG (ttmlparse_debug, "Calculated font size: %s",
          scaled_child_font_size);
      ttml_style_set_add_attr (ret, (const gchar *) attr_name,
          scaled_child_font_size);
      g_free (scaled_child_font_size);
    }

    /* Not all styling attributes are inherited in TTML. */
    if (g_strcmp0 ((const gchar *) attr_name, "backgroundColor") != 0
        && g_strcmp0 ((const gchar *) attr_name, "origin") != 0
        && g_strcmp0 ((const gchar *) attr_name, "extent") != 0
        && g_strcmp0 ((const gchar *) attr_name, "displayAlign") != 0
        && g_strcmp0 ((const gchar *) attr_name, "overflow") != 0
        && g_strcmp0 ((const gchar *) attr_name, "padding") != 0
        && g_strcmp0 ((const gchar *) attr_name, "writingMode") != 0
        && g_strcmp0 ((const gchar *) attr_name, "showBackground") != 0
        && g_strcmp0 ((const gchar *) attr_name, "unicodeBidi") != 0) {
      if (!ttml_style_set_contains_attr (ret, (const gchar *) attr_name)) {
        ttml_style_set_add_attr (ret, (const gchar *) attr_name,
            (const gchar *) attr_value);
      }
    }
  }

  return ret;
}


/*
 * Returns TRUE iff @element1 and @element2 reference the same set of styles.
 * If neither @element1 nor @element2 reference any styles, they are considered
 * to have matching styling and, hence, TRUE is returned.
 */
static gboolean
ttml_element_styles_match (TtmlElement * element1, TtmlElement * element2)
{
  const gchar *const *strv;
  gint i;

  if (!element1 || !element2 || (!element1->styles && element2->styles) ||
      (element1->styles && !element2->styles))
    return FALSE;

  if (!element1->styles && !element2->styles)
    return TRUE;

  strv = (const gchar * const *) element2->styles;

  if (g_strv_length (element1->styles) != g_strv_length (element2->styles))
    return FALSE;

  for (i = 0; i < g_strv_length (element1->styles); ++i) {
    if (!g_strv_contains (strv, element1->styles[i]))
      return FALSE;
  }

  return TRUE;
}


static gchar *
ttml_get_element_type_string (TtmlElement * element)
{
  switch (element->type) {
    case TTML_ELEMENT_TYPE_STYLE:
      return g_strdup ("<style>");
      break;
    case TTML_ELEMENT_TYPE_REGION:
      return g_strdup ("<region>");
      break;
    case TTML_ELEMENT_TYPE_BODY:
      return g_strdup ("<body>");
      break;
    case TTML_ELEMENT_TYPE_DIV:
      return g_strdup ("<div>");
      break;
    case TTML_ELEMENT_TYPE_P:
      return g_strdup ("<p>");
      break;
    case TTML_ELEMENT_TYPE_SPAN:
      return g_strdup ("<span>");
      break;
    case TTML_ELEMENT_TYPE_ANON_SPAN:
      return g_strdup ("<anon-span>");
      break;
    case TTML_ELEMENT_TYPE_BR:
      return g_strdup ("<br>");
      break;
    default:
      return g_strdup ("Unknown");
      break;
  }
}


/* Merge styles referenced by an element. */
static gboolean
ttml_resolve_styles (GNode * node, gpointer data)
{
  TtmlStyleSet *tmp = NULL;
  TtmlElement *element, *style;
  GHashTable *styles_table;
  gchar *type_string;
  guint i;

  styles_table = (GHashTable *) data;
  element = node->data;

  type_string = ttml_get_element_type_string (element);
  GST_CAT_LOG (ttmlparse_debug, "Element type: %s", type_string);
  g_free (type_string);

  if (!element->styles)
    return FALSE;

  for (i = 0; i < g_strv_length (element->styles); ++i) {
    tmp = element->style_set;
    style = g_hash_table_lookup (styles_table, element->styles[i]);
    if (style) {
      GST_CAT_LOG (ttmlparse_debug, "Merging style %s...", element->styles[i]);
      element->style_set = ttml_style_set_merge (element->style_set,
          style->style_set);
      ttml_style_set_delete (tmp);
    } else {
      GST_CAT_WARNING (ttmlparse_debug,
          "Element references an unknown style (%s)", element->styles[i]);
    }
  }

  GST_CAT_LOG (ttmlparse_debug, "Style set after merging:");
  ttml_style_set_print (element->style_set);

  return FALSE;
}


static void
ttml_resolve_referenced_styles (GList * trees, GHashTable * styles_table)
{
  GList *tree;

  for (tree = g_list_first (trees); tree; tree = tree->next) {
    GNode *root = (GNode *) tree->data;
    g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, ttml_resolve_styles,
        styles_table);
  }
}


/* Inherit styling attributes from parent. */
static gboolean
ttml_inherit_styles (GNode * node, gpointer data)
{
  TtmlStyleSet *tmp = NULL;
  TtmlElement *element, *parent;
  gchar *type_string;

  element = node->data;

  type_string = ttml_get_element_type_string (element);
  GST_CAT_LOG (ttmlparse_debug, "Element type: %s", type_string);
  g_free (type_string);

  if (node->parent) {
    parent = node->parent->data;
    if (parent->style_set) {
      tmp = element->style_set;
      if (element->type == TTML_ELEMENT_TYPE_ANON_SPAN ||
          element->type == TTML_ELEMENT_TYPE_BR) {
        element->style_set = ttml_style_set_merge (parent->style_set,
            element->style_set);
        element->styles = g_strdupv (parent->styles);
      } else {
        element->style_set = ttml_style_set_inherit (parent->style_set,
            element->style_set);
      }
      ttml_style_set_delete (tmp);
    }
  }

  GST_CAT_LOG (ttmlparse_debug, "Style set after inheriting:");
  ttml_style_set_print (element->style_set);

  return FALSE;
}


static void
ttml_inherit_element_styles (GList * trees)
{
  GList *tree;

  for (tree = g_list_first (trees); tree; tree = tree->next) {
    GNode *root = (GNode *) tree->data;
    g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, ttml_inherit_styles,
        NULL);
  }
}


/* If whitespace_mode isn't explicitly set for this element, inherit from its
 * parent. If this element is the root of the tree, set whitespace_mode to
 * that of the overall document. */
static gboolean
ttml_inherit_element_whitespace_mode (GNode * node, gpointer data)
{
  TtmlWhitespaceMode *doc_mode = (TtmlWhitespaceMode *) data;
  TtmlElement *element = node->data;
  TtmlElement *parent;

  if (element->whitespace_mode != TTML_WHITESPACE_MODE_NONE)
    return FALSE;

  if (G_NODE_IS_ROOT (node)) {
    element->whitespace_mode = *doc_mode;
    return FALSE;
  }

  parent = node->parent->data;
  element->whitespace_mode = parent->whitespace_mode;
  return FALSE;
}


static void
ttml_inherit_whitespace_mode (GNode * tree, TtmlWhitespaceMode doc_mode)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
      ttml_inherit_element_whitespace_mode, &doc_mode);
}


static gboolean
ttml_free_node_data (GNode * node, gpointer data)
{
  TtmlElement *element = node->data;
  ttml_delete_element (element);
  return FALSE;
}


static void
ttml_delete_tree (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1, ttml_free_node_data,
      NULL);
  g_node_destroy (tree);
}


typedef struct
{
  GstClockTime begin;
  GstClockTime end;
} ClipWindow;

static gboolean
ttml_clip_element_period (GNode * node, gpointer data)
{
  TtmlElement *element = node->data;
  ClipWindow *window = data;

  if (!GST_CLOCK_TIME_IS_VALID (element->begin)) {
    return FALSE;
  }

  if (element->begin > window->end || element->end < window->begin) {
    ttml_delete_tree (node);
    node = NULL;
    return FALSE;
  }

  element->begin = MAX (element->begin, window->begin);
  element->end = MIN (element->end, window->end);
  return FALSE;
}


static void
ttml_apply_time_window (GNode * tree, GstClockTime window_begin,
    GstClockTime window_end)
{
  ClipWindow window;
  window.begin = window_begin;
  window.end = window_end;

  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
      ttml_clip_element_period, &window);
}


static gboolean
ttml_resolve_element_timings (GNode * node, gpointer data)
{
  TtmlElement *element, *leaf;

  leaf = element = node->data;

  if (GST_CLOCK_TIME_IS_VALID (leaf->begin)
      && GST_CLOCK_TIME_IS_VALID (leaf->end)) {
    GST_CAT_LOG (ttmlparse_debug, "Leaf node already has timing.");
    return FALSE;
  }

  /* Inherit timings from ancestor. */
  while (node->parent && !GST_CLOCK_TIME_IS_VALID (element->begin)) {
    node = node->parent;
    element = node->data;
  }

  if (!GST_CLOCK_TIME_IS_VALID (element->begin)) {
    GST_CAT_WARNING (ttmlparse_debug,
        "No timing found for element; setting to Root Temporal Extent.");
    leaf->begin = 0;
    leaf->end = NSECONDS_IN_DAY;
  } else {
    leaf->begin = element->begin;
    leaf->end = element->end;
    GST_CAT_LOG (ttmlparse_debug, "Leaf begin: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (leaf->begin));
    GST_CAT_LOG (ttmlparse_debug, "Leaf end: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (leaf->end));
  }

  return FALSE;
}


static void
ttml_resolve_timings (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      ttml_resolve_element_timings, NULL);
}


static gboolean
ttml_resolve_leaf_region (GNode * node, gpointer data)
{
  TtmlElement *element, *leaf;
  leaf = element = node->data;

  while (node->parent && !element->region) {
    node = node->parent;
    element = node->data;
  }

  if (element->region) {
    leaf->region = g_strdup (element->region);
    GST_CAT_LOG (ttmlparse_debug, "Leaf region: %s", leaf->region);
  } else {
    GST_CAT_WARNING (ttmlparse_debug, "No region found above leaf element.");
  }

  return FALSE;
}


static void
ttml_resolve_regions (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      ttml_resolve_leaf_region, NULL);
}


typedef struct
{
  GstClockTime start_time;
  GstClockTime next_transition_time;
} TrState;


static gboolean
ttml_update_transition_time (GNode * node, gpointer data)
{
  TtmlElement *element = node->data;
  TrState *state = (TrState *) data;

  if ((element->begin < state->next_transition_time)
      && (!GST_CLOCK_TIME_IS_VALID (state->start_time)
          || (element->begin > state->start_time))) {
    state->next_transition_time = element->begin;
    GST_CAT_LOG (ttmlparse_debug,
        "Updating next transition time to element begin time (%"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (state->next_transition_time));
    return FALSE;
  }

  if ((element->end < state->next_transition_time)
      && (element->end > state->start_time)) {
    state->next_transition_time = element->end;
    GST_CAT_LOG (ttmlparse_debug,
        "Updating next transition time to element end time (%"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (state->next_transition_time));
  }

  return FALSE;
}


/* Return details about the next transition after @time. */
static GstClockTime
ttml_find_next_transition (GList * trees, GstClockTime time)
{
  TrState state;
  state.start_time = time;
  state.next_transition_time = GST_CLOCK_TIME_NONE;

  for (trees = g_list_first (trees); trees; trees = trees->next) {
    GNode *tree = (GNode *) trees->data;
    g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
        ttml_update_transition_time, &state);
  }

  GST_CAT_LOG (ttmlparse_debug, "Next transition is at %" GST_TIME_FORMAT,
      GST_TIME_ARGS (state.next_transition_time));

  return state.next_transition_time;
}


/* Remove nodes from tree that are not visible at @time. */
static GNode *
ttml_remove_nodes_by_time (GNode * node, GstClockTime time)
{
  GNode *child, *next_child;
  TtmlElement *element;
  element = node->data;

  child = node->children;
  next_child = child ? child->next : NULL;
  while (child) {
    ttml_remove_nodes_by_time (child, time);
    child = next_child;
    next_child = child ? child->next : NULL;
  }

  if (!node->children && ((element->begin > time) || (element->end <= time))) {
    ttml_delete_tree (node);
    node = NULL;
  }

  return node;
}


/* Return a list of trees containing the elements and their ancestors that are
 * visible at @time. */
static GList *
ttml_get_active_trees (GList * element_trees, GstClockTime time)
{
  GList *tree;
  GList *ret = NULL;

  for (tree = g_list_first (element_trees); tree; tree = tree->next) {
    GNode *root = g_node_copy_deep ((GNode *) tree->data,
        ttml_copy_tree_element, NULL);
    GST_CAT_LOG (ttmlparse_debug, "There are %u nodes in tree.",
        g_node_n_nodes (root, G_TRAVERSE_ALL));
    root = ttml_remove_nodes_by_time (root, time);
    if (root) {
      GST_CAT_LOG (ttmlparse_debug,
          "After filtering there are %u nodes in tree.", g_node_n_nodes (root,
              G_TRAVERSE_ALL));

      ret = g_list_append (ret, root);
    } else {
      GST_CAT_LOG (ttmlparse_debug,
          "All elements have been filtered from tree.");
    }
  }

  GST_CAT_DEBUG (ttmlparse_debug, "There are %u trees in returned list.",
      g_list_length (ret));
  return ret;
}


static GList *
ttml_create_scenes (GList * region_trees)
{
  TtmlScene *cur_scene = NULL;
  GList *output_scenes = NULL;
  GList *active_trees = NULL;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;

  while ((timestamp = ttml_find_next_transition (region_trees, timestamp))
      != GST_CLOCK_TIME_NONE) {
    GST_CAT_LOG (ttmlparse_debug,
        "Next transition found at time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (timestamp));
    if (cur_scene) {
      cur_scene->end = timestamp;
      output_scenes = g_list_append (output_scenes, cur_scene);
    }

    active_trees = ttml_get_active_trees (region_trees, timestamp);
    GST_CAT_LOG (ttmlparse_debug, "There will be %u active regions after "
        "transition", g_list_length (active_trees));

    if (active_trees) {
      cur_scene = g_new0 (TtmlScene, 1);
      cur_scene->begin = timestamp;
      cur_scene->trees = active_trees;
    } else {
      cur_scene = NULL;
    }
  }

  return output_scenes;
}


/* Handle element whitespace in accordance with section 7.2.3 of the TTML
 * specification. Specifically, this function implements the
 * white-space-collapse="true" and linefeed-treatment="treat-as-space"
 * behaviours. Note that stripping of whitespace at the start and end of line
 * areas (suppress-at-line-break="auto" and
 * white-space-treatment="ignore-if-surrounding-linefeed" behaviours) can only
 * be done by the renderer once the text from multiple elements has been laid
 * out in line areas. */
static gboolean
ttml_handle_element_whitespace (GNode * node, gpointer data)
{
  TtmlElement *element = node->data;
  guint space_count = 0;
  guint textlen;
  gchar *c;

  if (!element->text || (element->type == TTML_ELEMENT_TYPE_BR) ||
      (element->whitespace_mode == TTML_WHITESPACE_MODE_PRESERVE)) {
    return FALSE;
  }

  textlen = strlen (element->text);
  for (c = element->text; TRUE; c = g_utf8_next_char (c)) {

    gchar buf[6] = { 0 };
    gunichar u = g_utf8_get_char (c);
    gint nbytes = g_unichar_to_utf8 (u, buf);

    /* Replace each newline or tab with a space. */
    if (nbytes == 1 && (buf[0] == TTML_CHAR_LF || buf[0] == TTML_CHAR_TAB)) {
      *c = ' ';
      buf[0] = TTML_CHAR_SPACE;
    }

    /* Collapse runs of whitespace. */
    if (nbytes == 1 && (buf[0] == TTML_CHAR_SPACE || buf[0] == TTML_CHAR_CR)) {
      ++space_count;
    } else {
      if (space_count > 1) {
        gchar *new_head = c - space_count + 1;
        g_strlcpy (new_head, c, textlen);
        c = new_head;
      }
      space_count = 0;
      if (nbytes == 1 && buf[0] == TTML_CHAR_NULL)
        break;
    }
  }

  return FALSE;
}


static void
ttml_handle_whitespace (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      ttml_handle_element_whitespace, NULL);
}


static GNode *
ttml_filter_content_nodes (GNode * node)
{
  GNode *child, *next_child;
  TtmlElement *element = node->data;
  TtmlElement *parent = node->parent ? node->parent->data : NULL;

  child = node->children;
  next_child = child ? child->next : NULL;
  while (child) {
    ttml_filter_content_nodes (child);
    child = next_child;
    next_child = child ? child->next : NULL;
  }

  /* Only text content in <p>s and <span>s is significant. */
  if (element->type == TTML_ELEMENT_TYPE_ANON_SPAN
      && parent->type != TTML_ELEMENT_TYPE_P
      && parent->type != TTML_ELEMENT_TYPE_SPAN) {
    ttml_delete_element (element);
    g_node_destroy (node);
    node = NULL;
  }

  return node;
}


/* Store in @table child elements of @node with name @element_name. A child
 * element with the same ID as an existing entry in @table will overwrite the
 * existing entry. */
static void
ttml_store_unique_children (xmlNodePtr node, const gchar * element_name,
    GHashTable * table)
{
  xmlNodePtr ptr;

  for (ptr = node->children; ptr; ptr = ptr->next) {
    if (xmlStrcmp (ptr->name, (const xmlChar *) element_name) == 0) {
      TtmlElement *element = ttml_parse_element (ptr);
      gboolean new_key;

      if (element) {
        new_key = g_hash_table_insert (table, g_strdup (element->id), element);
        if (!new_key)
          GST_CAT_WARNING (ttmlparse_debug,
              "Document contains two %s elements with the same ID (\"%s\").",
              element_name, element->id);
      }
    }
  }
}


/* Parse style and region elements from @head and store in their respective
 * hash tables for future reference. */
static void
ttml_parse_head (xmlNodePtr head, GHashTable * styles_table,
    GHashTable * regions_table)
{
  xmlNodePtr node;

  for (node = head->children; node; node = node->next) {
    if (xmlStrcmp (node->name, (const xmlChar *) "styling") == 0)
      ttml_store_unique_children (node, "style", styles_table);
    if (xmlStrcmp (node->name, (const xmlChar *) "layout") == 0)
      ttml_store_unique_children (node, "region", regions_table);
  }
}


/* Remove nodes that do not belong to @region, or are not an ancestor of a node
 * belonging to @region. */
static GNode *
ttml_remove_nodes_by_region (GNode * node, const gchar * region)
{
  GNode *child, *next_child;
  TtmlElement *element;
  element = node->data;

  child = node->children;
  next_child = child ? child->next : NULL;
  while (child) {
    ttml_remove_nodes_by_region (child, region);
    child = next_child;
    next_child = child ? child->next : NULL;
  }

  if ((element->type == TTML_ELEMENT_TYPE_ANON_SPAN
          || element->type != TTML_ELEMENT_TYPE_BR)
      && element->region && (g_strcmp0 (element->region, region) != 0)) {
    ttml_delete_element (element);
    g_node_destroy (node);
    return NULL;
  }
  if (element->type != TTML_ELEMENT_TYPE_ANON_SPAN
      && element->type != TTML_ELEMENT_TYPE_BR && !node->children) {
    ttml_delete_element (element);
    g_node_destroy (node);
    return NULL;
  }

  return node;
}


static TtmlElement *
ttml_copy_element (const TtmlElement * element)
{
  TtmlElement *ret = g_new0 (TtmlElement, 1);

  ret->type = element->type;
  if (element->id)
    ret->id = g_strdup (element->id);
  ret->whitespace_mode = element->whitespace_mode;
  if (element->styles)
    ret->styles = g_strdupv (element->styles);
  if (element->region)
    ret->region = g_strdup (element->region);
  ret->begin = element->begin;
  ret->end = element->end;
  if (element->style_set)
    ret->style_set = ttml_style_set_copy (element->style_set);
  if (element->text)
    ret->text = g_strdup (element->text);

  return ret;
}


static gpointer
ttml_copy_tree_element (gconstpointer src, gpointer data)
{
  return ttml_copy_element ((TtmlElement *) src);
}


/* Split the body tree into a set of trees, each containing only the elements
 * belonging to a single region. Returns a list of trees, one per region, each
 * with the corresponding region element at its root. */
static GList *
ttml_split_body_by_region (GNode * body, GHashTable * regions)
{
  GHashTableIter iter;
  gpointer key, value;
  GList *ret = NULL;

  g_hash_table_iter_init (&iter, regions);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    gchar *region_name = (gchar *) key;
    TtmlElement *region = (TtmlElement *) value;
    GNode *region_node = g_node_new (ttml_copy_element (region));
    GNode *body_copy = g_node_copy_deep (body, ttml_copy_tree_element, NULL);

    GST_CAT_DEBUG (ttmlparse_debug, "Creating tree for region %s", region_name);
    GST_CAT_LOG (ttmlparse_debug, "Copy of body has %u nodes.",
        g_node_n_nodes (body_copy, G_TRAVERSE_ALL));

    body_copy = ttml_remove_nodes_by_region (body_copy, region_name);
    if (body_copy) {
      GST_CAT_LOG (ttmlparse_debug, "Copy of body now has %u nodes.",
          g_node_n_nodes (body_copy, G_TRAVERSE_ALL));

      /* Reparent tree to region node. */
      g_node_prepend (region_node, body_copy);
    }
    GST_CAT_LOG (ttmlparse_debug, "Final tree has %u nodes.",
        g_node_n_nodes (region_node, G_TRAVERSE_ALL));
    ret = g_list_append (ret, region_node);
  }

  GST_CAT_DEBUG (ttmlparse_debug, "Returning %u trees.", g_list_length (ret));
  return ret;
}


static gint
ttml_add_text_to_buffer (GstBuffer * buf, const gchar * text)
{
  GstMemory *mem;
  GstMapInfo map;
  guint ret;

  if (gst_buffer_n_memory (buf) == gst_buffer_get_max_memory ())
    return -1;

  mem = gst_allocator_alloc (NULL, strlen (text) + 1, NULL);
  if (!gst_memory_map (mem, &map, GST_MAP_WRITE))
    GST_CAT_ERROR (ttmlparse_debug, "Failed to map memory.");

  g_strlcpy ((gchar *) map.data, text, map.size);
  GST_CAT_DEBUG (ttmlparse_debug, "Inserted following text into buffer: \"%s\"",
      (gchar *) map.data);
  gst_memory_unmap (mem, &map);

  ret = gst_buffer_n_memory (buf);
  gst_buffer_insert_memory (buf, -1, mem);
  return ret;
}


/* Create a GstSubtitleElement from @element, add it to @block, and insert its
 * associated text in @buf. */
static gboolean
ttml_add_element (GstSubtitleBlock * block, TtmlElement * element,
    GstBuffer * buf, guint cellres_x, guint cellres_y)
{
  GstSubtitleStyleSet *element_style = NULL;
  guint buffer_index;
  GstSubtitleElement *sub_element = NULL;

  buffer_index = ttml_add_text_to_buffer (buf, element->text);
  if (buffer_index == -1) {
    GST_CAT_WARNING (ttmlparse_debug,
        "Reached maximum element count for buffer - discarding element.");
    return FALSE;
  }

  GST_CAT_DEBUG (ttmlparse_debug, "Inserted text at index %u in GstBuffer.",
      buffer_index);

  element_style = gst_subtitle_style_set_new ();
  ttml_update_style_set (element_style, element->style_set,
      cellres_x, cellres_y);
  sub_element = gst_subtitle_element_new (element_style, buffer_index,
      (element->whitespace_mode != TTML_WHITESPACE_MODE_PRESERVE));

  gst_subtitle_block_add_element (block, sub_element);
  GST_CAT_DEBUG (ttmlparse_debug,
      "Added element to block; there are now %u elements in the block.",
      gst_subtitle_block_get_element_count (block));
  return TRUE;
}


/* Return TRUE if @color is totally transparent. */
static gboolean
ttml_color_is_transparent (const GstSubtitleColor * color)
{
  if (!color)
    return FALSE;
  else
    return (color->a == 0);
}


/* Blend @color2 over @color1 and return the resulting color. This is currently
 * a dummy implementation that simply returns color2 as long as it's
 * not fully transparent. */
/* TODO: Implement actual blending of colors. */
static GstSubtitleColor
ttml_blend_colors (GstSubtitleColor color1, GstSubtitleColor color2)
{
  if (ttml_color_is_transparent (&color2))
    return color1;
  else
    return color2;
}


static void
ttml_warn_of_mispositioned_element (TtmlElement * element)
{
  gchar *type = ttml_get_element_type_string (element);
  GST_CAT_WARNING (ttmlparse_debug, "Ignoring illegally positioned %s element.",
      type);
  g_free (type);
}


/* Create the subtitle region and its child blocks and elements for @tree,
 * inserting element text in @buf. Ownership of created region is transferred
 * to caller. */
static GstSubtitleRegion *
ttml_create_subtitle_region (GNode * tree, GstBuffer * buf, guint cellres_x,
    guint cellres_y)
{
  GstSubtitleRegion *region = NULL;
  GstSubtitleStyleSet *region_style;
  GstSubtitleColor block_color;
  TtmlElement *element;
  GNode *node;

  element = tree->data;         /* Region element */
  region_style = gst_subtitle_style_set_new ();
  ttml_update_style_set (region_style, element->style_set, cellres_x,
      cellres_y);
  region = gst_subtitle_region_new (region_style);

  node = tree->children;
  if (!node)
    return region;

  element = node->data;         /* Body element */
  block_color =
      ttml_parse_colorstring (ttml_style_set_get_attr (element->style_set,
          "backgroundColor"));

  for (node = node->children; node; node = node->next) {
    GNode *p_node;
    GstSubtitleColor div_color;

    element = node->data;
    if (element->type != TTML_ELEMENT_TYPE_DIV) {
      ttml_warn_of_mispositioned_element (element);
      continue;
    }
    div_color =
        ttml_parse_colorstring (ttml_style_set_get_attr (element->style_set,
            "backgroundColor"));
    block_color = ttml_blend_colors (block_color, div_color);

    for (p_node = node->children; p_node; p_node = p_node->next) {
      GstSubtitleBlock *block = NULL;
      GstSubtitleStyleSet *block_style;
      GNode *content_node;
      GstSubtitleColor p_color;

      element = p_node->data;
      if (element->type != TTML_ELEMENT_TYPE_P) {
        ttml_warn_of_mispositioned_element (element);
        continue;
      }
      p_color =
          ttml_parse_colorstring (ttml_style_set_get_attr (element->style_set,
              "backgroundColor"));
      block_color = ttml_blend_colors (block_color, p_color);

      block_style = gst_subtitle_style_set_new ();
      ttml_update_style_set (block_style, element->style_set, cellres_x,
          cellres_y);
      block_style->background_color = block_color;
      block = gst_subtitle_block_new (block_style);

      for (content_node = p_node->children; content_node;
          content_node = content_node->next) {
        GNode *anon_node;
        element = content_node->data;

        if (element->type == TTML_ELEMENT_TYPE_BR
            || element->type == TTML_ELEMENT_TYPE_ANON_SPAN) {
          if (!ttml_add_element (block, element, buf, cellres_x, cellres_y))
            GST_CAT_WARNING (ttmlparse_debug,
                "Failed to add element to buffer.");
        } else if (element->type == TTML_ELEMENT_TYPE_SPAN) {
          /* Loop through anon-span children of this span. */
          for (anon_node = content_node->children; anon_node;
              anon_node = anon_node->next) {
            element = anon_node->data;

            if (element->type == TTML_ELEMENT_TYPE_BR
                || element->type == TTML_ELEMENT_TYPE_ANON_SPAN) {
              if (!ttml_add_element (block, element, buf, cellres_x, cellres_y))
                GST_CAT_WARNING (ttmlparse_debug,
                    "Failed to add element to buffer.");
            } else {
              ttml_warn_of_mispositioned_element (element);
            }
          }
        } else {
          ttml_warn_of_mispositioned_element (element);
        }
      }

      if (gst_subtitle_block_get_element_count (block) > 0) {
        gst_subtitle_region_add_block (region, block);
        GST_CAT_DEBUG (ttmlparse_debug,
            "Added block to region; there are now %u blocks in the region.",
            gst_subtitle_region_get_block_count (region));
      } else {
        gst_subtitle_block_unref (block);
      }
    }
  }

  return region;
}


/* For each scene, create data objects to describe the layout and styling of
 * that scene and attach it as metadata to the GstBuffer that will be used to
 * carry that scene's text. */
static void
ttml_attach_scene_metadata (GList * scenes, guint cellres_x, guint cellres_y)
{
  GList *scene_entry;

  for (scene_entry = g_list_first (scenes); scene_entry;
      scene_entry = scene_entry->next) {
    TtmlScene *scene = scene_entry->data;
    GList *region_tree;
    GPtrArray *regions = g_ptr_array_new_with_free_func (
        (GDestroyNotify) gst_subtitle_region_unref);

    scene->buf = gst_buffer_new ();
    GST_BUFFER_PTS (scene->buf) = scene->begin;
    GST_BUFFER_DURATION (scene->buf) = (scene->end - scene->begin);

    for (region_tree = g_list_first (scene->trees); region_tree;
        region_tree = region_tree->next) {
      GNode *tree = (GNode *) region_tree->data;
      GstSubtitleRegion *region;

      region = ttml_create_subtitle_region (tree, scene->buf, cellres_x,
          cellres_y);
      if (region)
        g_ptr_array_add (regions, region);
    }

    gst_buffer_add_subtitle_meta (scene->buf, regions);
  }
}


static GList *
create_buffer_list (GList * scenes)
{
  GList *ret = NULL;

  while (scenes) {
    TtmlScene *scene = scenes->data;
    ret = g_list_prepend (ret, gst_buffer_ref (scene->buf));
    scenes = scenes->next;
  }
  return g_list_reverse (ret);
}


static void
ttml_delete_scene (TtmlScene * scene)
{
  if (scene->trees)
    g_list_free_full (scene->trees, (GDestroyNotify) ttml_delete_tree);
  if (scene->buf)
    gst_buffer_unref (scene->buf);
  g_free (scene);
}


static void
ttml_assign_region_times (GList * region_trees, GstClockTime doc_begin,
    GstClockTime doc_duration)
{
  GList *tree;

  for (tree = g_list_first (region_trees); tree; tree = tree->next) {
    GNode *region_node = (GNode *) tree->data;
    TtmlElement *region = (TtmlElement *) region_node->data;
    const gchar *show_background_value =
        ttml_style_set_get_attr (region->style_set, "showBackground");
    gboolean always_visible =
        (g_strcmp0 (show_background_value, "whenActive") != 0);

    GstSubtitleColor region_color = { 0, 0, 0, 0 };
    if (ttml_style_set_contains_attr (region->style_set, "backgroundColor"))
      region_color =
          ttml_parse_colorstring (ttml_style_set_get_attr (region->style_set,
              "backgroundColor"));

    if (always_visible && !ttml_color_is_transparent (&region_color)) {
      GST_CAT_DEBUG (ttmlparse_debug, "Assigning times to region.");
      /* If the input XML document was not encapsulated in a container that
       * provides timing information for the document as a whole (i.e., its
       * PTS and duration) and the region background should be always visible,
       * set region start time to 0 and end time to 24 hours. This ensures that
       * regions with showBackground="always" are visible for the entirety of
       * any real-world stream. */
      region->begin = (doc_begin != GST_CLOCK_TIME_NONE) ? doc_begin : 0;
      region->end = (doc_duration != GST_CLOCK_TIME_NONE) ?
          region->begin + doc_duration : NSECONDS_IN_DAY;
    }
  }
}


/*
 * Promotes @node to the position of its parent, setting the prev, next and
 * parent pointers of @node to that of its original parent. The replaced parent
 * is freed. Should be called only on nodes that are the sole child of their
 * parent, otherwise sibling nodes may be leaked.
 */
static void
ttml_promote_node (GNode * node)
{
  GNode *parent_node = node->parent;
  TtmlElement *parent_element;

  if (!parent_node)
    return;
  parent_element = (TtmlElement *) parent_node->data;

  node->prev = parent_node->prev;
  if (!node->prev)
    parent_node->parent->children = node;
  else
    node->prev->next = node;
  node->next = parent_node->next;
  if (node->next)
    node->next->prev = node;
  node->parent = parent_node->parent;

  parent_node->prev = parent_node->next = NULL;
  parent_node->parent = parent_node->children = NULL;
  g_node_destroy (parent_node);
  ttml_delete_element (parent_element);
}


/*
 * Returns TRUE if @element is of a type that can be joined with another
 * joinable element.
 */
static gboolean
ttml_element_is_joinable (TtmlElement * element)
{
  return element->type == TTML_ELEMENT_TYPE_ANON_SPAN ||
      element->type == TTML_ELEMENT_TYPE_BR;
}


/* Joins adjacent inline element in @tree that have the same styling. */
static void
ttml_join_region_tree_inline_elements (GNode * tree)
{
  GNode *n1, *n2;

  for (n1 = tree; n1; n1 = n1->next) {
    if (n1->children) {
      TtmlElement *element = (TtmlElement *) n1->data;
      ttml_join_region_tree_inline_elements (n1->children);
      if (element->type == TTML_ELEMENT_TYPE_SPAN &&
          g_node_n_children (n1) == 1) {
        GNode *child = n1->children;
        if (n1 == tree)
          tree = child;
        ttml_promote_node (child);
        n1 = child;
      }
    }
  }

  n1 = tree;
  n2 = tree->next;

  while (n1 && n2) {
    TtmlElement *e1 = (TtmlElement *) n1->data;
    TtmlElement *e2 = (TtmlElement *) n2->data;

    if (ttml_element_is_joinable (e1) &&
        ttml_element_is_joinable (e2) && ttml_element_styles_match (e1, e2)) {
      gchar *tmp = e1->text;
      GST_CAT_LOG (ttmlparse_debug,
          "Joining adjacent element text \"%s\" & \"%s\"", e1->text, e2->text);
      e1->text = g_strconcat (e1->text, e2->text, NULL);
      e1->type = TTML_ELEMENT_TYPE_ANON_SPAN;
      g_free (tmp);

      ttml_delete_element (e2);
      g_node_destroy (n2);
      n2 = n1->next;
    } else {
      n1 = n2;
      n2 = n2->next;
    }
  }
}


static void
ttml_join_inline_elements (GList * scenes)
{
  GList *scene_entry;

  for (scene_entry = g_list_first (scenes); scene_entry;
      scene_entry = scene_entry->next) {
    TtmlScene *scene = scene_entry->data;
    GList *region_tree;

    for (region_tree = g_list_first (scene->trees); region_tree;
        region_tree = region_tree->next) {
      GNode *tree = (GNode *) region_tree->data;
      ttml_join_region_tree_inline_elements (tree);
    }
  }
}


static xmlNodePtr
ttml_find_child (xmlNodePtr parent, const gchar * name)
{
  xmlNodePtr child = parent->children;
  while (child && xmlStrcmp (child->name, (const xmlChar *) name) != 0)
    child = child->next;
  return child;
}

#define XML_START_TAG "<?xml"
#define TTML_END_TAG "</tt>"

guint
ttml_parse (const gchar * input, GstClockTime begin, GstClockTime duration,
    GList ** parsed)
{
  xmlDocPtr doc;
  xmlNodePtr root_node, head_node, body_node;

  GHashTable *styles_table, *regions_table;
  GList *output_buffers = NULL;
  gchar *value;
  guint cellres_x, cellres_y;
  TtmlWhitespaceMode doc_whitespace_mode = TTML_WHITESPACE_MODE_DEFAULT;
  guint consumed = 0;
  guint start_offset = 0;
  gchar *start_xml, *end_tt;

  g_return_val_if_fail (parsed != NULL, 0);

  *parsed = NULL;
  if (!g_utf8_validate (input, -1, NULL)) {
    GST_CAT_ERROR (ttmlparse_debug, "Input isn't valid UTF-8.");
    return 0;
  }
  GST_CAT_LOG (ttmlparse_debug, "Input:\n%s", input);

  start_xml = g_strstr_len (input, strlen (input), XML_START_TAG);
  end_tt = g_strstr_len (input, strlen (input), TTML_END_TAG);

  if (!start_xml || !end_tt) {
    GST_CAT_DEBUG (ttmlparse_debug, "Need more data");
    return 0;
  }

  consumed = end_tt - input + strlen (TTML_END_TAG);
  start_offset = start_xml - input;

  styles_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) ttml_delete_element);
  regions_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) ttml_delete_element);

  /* Parse input. */
  doc = xmlReadMemory (start_xml, consumed - start_offset, "any_doc_name",
      NULL, 0);
  if (!doc) {
    GST_CAT_ERROR (ttmlparse_debug, "Failed to parse document.");
    return 0;
  }

  root_node = xmlDocGetRootElement (doc);

  if (xmlStrcmp (root_node->name, (const xmlChar *) "tt") != 0) {
    GST_CAT_ERROR (ttmlparse_debug, "Root element of document is not tt:tt.");
    xmlFreeDoc (doc);
    return 0;
  }

  if ((value = ttml_get_xml_property (root_node, "cellResolution"))) {
    gchar *ptr = value;
    cellres_x = (guint) g_ascii_strtoull (ptr, &ptr, 10U);
    cellres_y = (guint) g_ascii_strtoull (ptr, NULL, 10U);
    g_free (value);
  } else {
    cellres_x = DEFAULT_CELLRES_X;
    cellres_y = DEFAULT_CELLRES_Y;
  }

  GST_CAT_DEBUG (ttmlparse_debug, "cellres_x: %u   cellres_y: %u", cellres_x,
      cellres_y);

  if ((value = ttml_get_xml_property (root_node, "space"))) {
    if (g_strcmp0 (value, "preserve") == 0) {
      GST_CAT_DEBUG (ttmlparse_debug, "Preserving whitespace...");
      doc_whitespace_mode = TTML_WHITESPACE_MODE_PRESERVE;
    }
    g_free (value);
  }

  if (!(head_node = ttml_find_child (root_node, "head"))) {
    GST_CAT_ERROR (ttmlparse_debug, "No <head> element found.");
    xmlFreeDoc (doc);
    return 0;
  }

  ttml_parse_head (head_node, styles_table, regions_table);

  if ((body_node = ttml_find_child (root_node, "body"))) {
    GNode *body_tree;
    GList *region_trees = NULL;
    GList *scenes = NULL;

    body_tree = ttml_parse_body (body_node);
    GST_CAT_LOG (ttmlparse_debug, "body_tree tree contains %u nodes.",
        g_node_n_nodes (body_tree, G_TRAVERSE_ALL));
    GST_CAT_LOG (ttmlparse_debug, "body_tree tree height is %u",
        g_node_max_height (body_tree));

    ttml_inherit_whitespace_mode (body_tree, doc_whitespace_mode);
    ttml_handle_whitespace (body_tree);
    ttml_filter_content_nodes (body_tree);
    if (GST_CLOCK_TIME_IS_VALID (begin) && GST_CLOCK_TIME_IS_VALID (duration))
      ttml_apply_time_window (body_tree, begin, begin + duration);
    ttml_resolve_timings (body_tree);
    ttml_resolve_regions (body_tree);
    region_trees = ttml_split_body_by_region (body_tree, regions_table);
    ttml_resolve_referenced_styles (region_trees, styles_table);
    ttml_inherit_element_styles (region_trees);
    ttml_assign_region_times (region_trees, begin, duration);
    scenes = ttml_create_scenes (region_trees);
    GST_CAT_LOG (ttmlparse_debug, "There are %u scenes in all.",
        g_list_length (scenes));
    ttml_join_inline_elements (scenes);
    ttml_attach_scene_metadata (scenes, cellres_x, cellres_y);
    output_buffers = create_buffer_list (scenes);

    g_list_free_full (scenes, (GDestroyNotify) ttml_delete_scene);
    g_list_free_full (region_trees, (GDestroyNotify) ttml_delete_tree);
    ttml_delete_tree (body_tree);
  }

  xmlFreeDoc (doc);
  g_hash_table_destroy (styles_table);
  g_hash_table_destroy (regions_table);

  *parsed = output_buffers;

  return consumed;
}
