/* GStreamer
 * Copyright (C) 2013 CableLabs, Louisville, CO 80027
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *     @Author: Chengjun Wang <cjun.wang@samsung.com>
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
#include <gst/gst.h>
#include <pango/pangocairo.h>
#include <gstcea708decoder.h>
#include <string.h>

#define GST_CAT_DEFAULT gst_cea708_decoder_debug
GST_DEBUG_CATEGORY (gst_cea708_decoder_debug);

void
gst_cea708_decoder_init_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (gst_cea708_decoder_debug, "cc708decoder", 0,
      "CEA708 Closed Caption Decoder");
}

/* 708 colors are defined by 2 bits each for R,G,&B for a total of 64 color combinations */
static const gchar *color_names[] = {
  "black",
  "white",
  "red",
  "green",
  "blue",
  "yellow",
  "magenta",
  "cyan",
  NULL
};

static const gchar *font_names[] = {
  "serif",
  "courier",
  "times new roman",
  "helvetica",
  "Arial",
  "Dom Casual",
  "Coronet",
  "Gothic",
  NULL
};

static const gchar *pen_size_names[] = {
  "30",                         /*"small" */
  "36",                         /*"medium" */
  "42",                         /*"large" */
  NULL
};

/* G2 table defined in EIA/CEA-708 Spec */
static const gunichar g2_table[CC_MAX_CODE_SET_SIZE] = {
  ' ', 0xA0, 0, 0, 0, 0x2026, 0, 0,
  0, 0, 0x160, 0, 0x152, 0, 0, 0,
  0x2588, 0x2018, 0x2019, 0x201c, 0x201d, 0xB7, 0, 0,
  0, 0x2122, 0x161, 0, 0x153, 0x2120, 0, 0x178,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0x215b, 0x215c,
  0x215d, 0x215e, 0x2502, 0x2510, 0x2514, 0x2500, 0x2518, 0x250c,
};

static void gst_cea708dec_print_command_name (Cea708Dec * decoder, guint8 c);
static void gst_cea708dec_render_pangocairo (cea708Window * window);
static void
gst_cea708dec_adjust_values_with_fontdesc (cea708Window * window,
    PangoFontDescription * desc);
static gint
gst_cea708dec_text_list_add (GSList ** text_list,
    gint len, const gchar * format, ...);
static const PangoAlignment gst_cea708dec_get_align_mode (guint8 justify_mode);
static const gchar *gst_cea708dec_get_color_name (guint8 color);
static guint8 gst_cea708dec_map_minimum_color (guint8 color);
static void
gst_cea708dec_set_pen_color (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index);
static void
gst_cea708dec_set_window_attributes (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index);
static void
gst_cea708dec_set_pen_style (Cea708Dec * decoder, guint8 pen_style_id);
static void
gst_cea708dec_set_window_style (Cea708Dec * decoder, guint8 style_id);
static void
gst_cea708dec_define_window (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index);
static inline void
pango_span_markup_init (cea708PangoSpanControl * span_control);
static inline void
pango_span_markup_start (cea708PangoSpanControl * span_control,
    gchar * line_buffer, guint16 * index);
static inline void
pango_span_markup_txt (cea708PangoSpanControl * span_control,
    gchar * line_buffer, guint16 * index);
static inline void
pango_span_markup_end (cea708PangoSpanControl * span_control,
    gchar * line_buffer, guint16 * index);
static void
gst_cea708dec_show_pango_window (Cea708Dec * decoder, guint window_id);
static void
gst_cea708dec_clear_window_text (Cea708Dec * decoder, guint window_id);
static void
gst_cea708dec_scroll_window_up (Cea708Dec * decoder, guint window_id);
static void gst_cea708dec_init_window (Cea708Dec * decoder, guint window_id);
static void gst_cea708dec_clear_window (Cea708Dec * decoder, cea708Window * w);
static void
gst_cea708dec_set_pen_attributes (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index);
static void
gst_cea708dec_for_each_window (Cea708Dec * decoder,
    guint8 window_list, VisibilityControl visibility_control,
    const gchar * log_message, void (*function) (Cea708Dec * decoder,
        guint window_id));
static void
gst_cea708dec_process_command (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index);
static void get_cea708dec_bufcat (gpointer data, gpointer whole_buf);
static gboolean
gst_cea708dec_render_text (Cea708Dec * decoder, GSList ** text_list,
    gint length, guint window_id);
static void gst_cea708dec_window_add_char (Cea708Dec * decoder, gunichar c);
static void
gst_cea708dec_process_c2 (Cea708Dec * decoder, guint8 * dtvcc_buffer,
    int index);
static void
gst_cea708dec_process_g2 (Cea708Dec * decoder, guint8 * dtvcc_buffer,
    int index);
static void
gst_cea708dec_process_c3 (Cea708Dec * decoder, guint8 * dtvcc_buffer,
    int index);
static void
gst_cea708dec_process_g3 (Cea708Dec * decoder, guint8 * dtvcc_buffer,
    int index);
static void
gst_cea708dec_process_dtvcc_byte (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index);

/* For debug, print name of 708 command */
Cea708Dec *
gst_cea708dec_create (PangoContext * pango_context)
{
  int i;
  Cea708Dec *decoder = g_malloc (sizeof (Cea708Dec));;
  memset (decoder, 0, sizeof (Cea708Dec));

  /* Initialize 708 variables */
  for (i = 0; i < MAX_708_WINDOWS; i++) {
    decoder->cc_windows[i] = g_malloc (sizeof (cea708Window));
    gst_cea708dec_init_window (decoder, i);
  }
  decoder->desired_service = 1;
  decoder->use_ARGB = FALSE;
  decoder->pango_context = pango_context;
  return decoder;
}

void
gst_cea708dec_free (Cea708Dec * dec)
{
  int i;

  for (i = 0; i < MAX_708_WINDOWS; i++) {
    cea708Window *window = dec->cc_windows[i];
    gst_cea708dec_clear_window (dec, window);
    g_free (window);
  }
  memset (dec, 0, sizeof (Cea708Dec));
  g_free (dec);
}

void
gst_cea708dec_set_service_number (Cea708Dec * decoder, gint8 desired_service)
{
  int i = 0;
  gint8 previous_desired_service;
  previous_desired_service = decoder->desired_service;
  decoder->desired_service = desired_service;
  /* If there has been a change in the desired service number, then clear
   * the windows for the new service. */
  if (decoder->desired_service != previous_desired_service) {
    for (i = 0; i < MAX_708_WINDOWS; i++) {
      gst_cea708dec_init_window (decoder, i);
    }
    decoder->current_window = 0;
  }
}

gboolean
gst_cea708dec_process_dtvcc_packet (Cea708Dec * decoder, guint8 * dtvcc_buffer,
    gsize dtvcc_size)
{
  guint i;
  gboolean need_render = FALSE;
  cea708Window *window = NULL;
  guint window_id;

  /* Service block header (see CEA-708 6.2.1) */
  guint8 block_size;
  guint8 service_number;

  guint parse_index = 0;
#ifndef GST_DISABLE_GST_DEBUG
  guint8 sequence_number = (dtvcc_buffer[parse_index] & 0xC0) >> 6;
  guint8 pkt_size = DTVCC_PKT_SIZE (dtvcc_buffer[parse_index] & 0x3F);
#endif

  parse_index += 1;

  while (parse_index < dtvcc_size) {
    block_size = dtvcc_buffer[parse_index] & 0x1F;
    service_number = (dtvcc_buffer[parse_index] & 0xE0) >> 5;
    parse_index += 1;

    if (service_number == 7) {
      /* Get extended service number */
      service_number = dtvcc_buffer[parse_index] & 0x3F;
      parse_index += 1;
    }

    GST_LOG ("full_size:%" G_GSIZE_FORMAT
        " size=%d seq=%d block_size=%d service_num=%d", dtvcc_size, pkt_size,
        sequence_number, block_size, service_number);

    /* Process desired_service cc data */
    if (decoder->desired_service == service_number) {
      for (i = 0; i < block_size; i++) {
        /* The Dtvcc buffer contains a stream of commands, command parameters,
         * and characters which are the actual captions. Process commands and
         * store captions in simulated 708 windows: */
        gst_cea708dec_process_dtvcc_byte (decoder, dtvcc_buffer,
            parse_index + i);
      }

      for (window_id = 0; window_id < 8; window_id++) {
        window = decoder->cc_windows[window_id];
        GST_LOG ("window #%02d deleted:%d visible:%d updated:%d", window_id,
            window->deleted, window->visible, window->updated);
        if (!window->updated) {
          continue;
        }
        need_render = TRUE;
      }
    }

    parse_index += block_size;
  }

  return need_render;
}

static void
gst_cea708dec_process_dtvcc_byte (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index)
{
  guint8 c = dtvcc_buffer[index];

  if (decoder->output_ignore) {
    /* Ignore characters/parameters after a command. */
    /* GST_TRACE ("[%d] ignore %X", decoder->output_ignore, c); */
    decoder->output_ignore--;
    return;
  }
  GST_DEBUG ("processing 0x%02X", c);

  if (c >= 0x00 && c <= 0x1F) { /* C0 */
    if (c == 0x03) {            /* ETX */
      gst_cea708dec_process_command (decoder, dtvcc_buffer, index);
    } else if (c == 0x00 || c == 0x08 || c == 0x0C || c == 0x0D || c == 0x0E) {
      gst_cea708dec_window_add_char (decoder, c);
    } else if (c == 0x10) {     /* EXT1 */
      guint8 next_c = dtvcc_buffer[index + 1];
      if (next_c >= 0x00 && next_c <= 0x1F) {   /* C2 */
        gst_cea708dec_process_c2 (decoder, dtvcc_buffer, index + 1);
      } else if (next_c >= 0x20 && next_c <= 0x7F) {    /* G2 */
        gst_cea708dec_process_g2 (decoder, dtvcc_buffer, index + 1);
      } else if (next_c >= 0x80 && next_c <= 0x9F) {    /* C3 */
        gst_cea708dec_process_c3 (decoder, dtvcc_buffer, index + 1);
      } else if (next_c >= 0xA0 && next_c <= 0xFF) {    /* G3 */
        gst_cea708dec_process_g3 (decoder, dtvcc_buffer, index + 1);
      }
    } else if (c > 0x10 && c < 0x18) {
      decoder->output_ignore = 1;
      GST_INFO ("do not support 0x11-0x17");
    } else if (c >= 0x18) {     /* P16 */
      /*P16 do not support now */
      decoder->output_ignore = 2;
      GST_INFO ("do not support 0x18-0x1F");
    }
  } else if ((c >= 0x20) && (c <= 0x7F)) {      /* G0 */
    if (c == 0x7F) {
      gst_cea708dec_window_add_char (decoder, CC_SPECIAL_CODE_MUSIC_NOTE);
    } else {
      gst_cea708dec_window_add_char (decoder, c);
    }
  } else if ((c >= 0x80) && (c <= 0x9F)) {      /* C1 */
    gst_cea708dec_process_command (decoder, dtvcc_buffer, index);
  } else if ((c >= 0xA0) && (c <= 0xFF)) {      /* G1 */
    gst_cea708dec_window_add_char (decoder, c);
  }
}

/* For debug, print name of 708 command */
static void
gst_cea708dec_print_command_name (Cea708Dec * decoder, guint8 c)
{
  gchar buffer[32];
  const gchar *command = NULL;

  switch (c) {
    case CC_COMMAND_ETX:
      command = (const gchar *) "End of text";
      break;

    case CC_COMMAND_CW0:
    case CC_COMMAND_CW1:
    case CC_COMMAND_CW2:
    case CC_COMMAND_CW3:
    case CC_COMMAND_CW4:
    case CC_COMMAND_CW5:
    case CC_COMMAND_CW6:
    case CC_COMMAND_CW7:
      /* Set current window, no parameters */
      g_snprintf (buffer, sizeof (buffer), "Set current window %d", c & 0x3);
      command = buffer;
      break;

    case CC_COMMAND_CLW:
      command = (const gchar *) "Clear windows";
      break;

    case CC_COMMAND_DSW:
      command = (const gchar *) "Display windows";
      break;

    case CC_COMMAND_HDW:
      command = (const gchar *) "Hide windows";
      break;

    case CC_COMMAND_TGW:
      command = (const gchar *) "Toggle windows";
      break;

    case CC_COMMAND_DLW:
      command = (const gchar *) "Delete windows";
      break;

    case CC_COMMAND_DLY:
      command = (const gchar *) "Delay";
      break;

    case CC_COMMAND_DLC:
      command = (const gchar *) "Delay cancel";
      break;

    case CC_COMMAND_RST:
      command = (const gchar *) "Reset";
      break;

    case CC_COMMAND_SPA:
      command = (const gchar *) "Set pen attributes";
      break;

    case CC_COMMAND_SPC:
      command = (const gchar *) "Set pen color";
      break;

    case CC_COMMAND_SPL:
      command = (const gchar *) "Set pen location";
      break;

    case CC_COMMAND_SWA:
      command = (const gchar *) "Set window attributes";
      break;

    case CC_COMMAND_DF0:
    case CC_COMMAND_DF1:
    case CC_COMMAND_DF2:
    case CC_COMMAND_DF3:
    case CC_COMMAND_DF4:
    case CC_COMMAND_DF5:
    case CC_COMMAND_DF6:
    case CC_COMMAND_DF7:
      g_snprintf (buffer, sizeof (buffer), "define window %d", c & 0x3);
      command = buffer;
      break;

    default:
      if ((c > 0x80) && (c < 0x9F))
        command = (const gchar *) "Unknown";
      break;
  }                             /* switch */

  if (NULL != command) {
    GST_LOG ("Process 708 command (%02X): %s", c, command);
  }
}

static void
gst_cea708dec_render_pangocairo (cea708Window * window)
{
  cairo_t *crt;
  cairo_surface_t *surf;
  cairo_t *shadow;
  cairo_surface_t *surf_shadow;
  PangoRectangle ink_rec, logical_rec;
  gint width, height;

  pango_layout_get_pixel_extents (window->layout, &ink_rec, &logical_rec);

  width = logical_rec.width + window->shadow_offset;
  height = logical_rec.height + logical_rec.y + window->shadow_offset;

  surf_shadow = cairo_image_surface_create (CAIRO_FORMAT_A8, width, height);
  shadow = cairo_create (surf_shadow);

  /* clear shadow surface */
  cairo_set_operator (shadow, CAIRO_OPERATOR_CLEAR);
  cairo_paint (shadow);
  cairo_set_operator (shadow, CAIRO_OPERATOR_OVER);

  /* draw shadow text */
  cairo_save (shadow);
  cairo_set_source_rgba (shadow, 0.0, 0.0, 0.0, 0.5);
  cairo_translate (shadow, window->shadow_offset, window->shadow_offset);
  pango_cairo_show_layout (shadow, window->layout);
  cairo_restore (shadow);

  /* draw outline text */
  cairo_save (shadow);
  cairo_set_source_rgb (shadow, 0.0, 0.0, 0.0);
  cairo_set_line_width (shadow, window->outline_offset);
  pango_cairo_layout_path (shadow, window->layout);
  cairo_stroke (shadow);
  cairo_restore (shadow);

  cairo_destroy (shadow);

  window->text_image = g_realloc (window->text_image, 4 * width * height);

  surf = cairo_image_surface_create_for_data (window->text_image,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  crt = cairo_create (surf);
  cairo_set_operator (crt, CAIRO_OPERATOR_CLEAR);
  cairo_paint (crt);
  cairo_set_operator (crt, CAIRO_OPERATOR_OVER);

  /* set default color */
  cairo_set_source_rgb (crt, 1.0, 1.0, 1.0);

  cairo_save (crt);
  /* draw text */
  pango_cairo_show_layout (crt, window->layout);
  cairo_restore (crt);

  /* composite shadow with offset */
  cairo_set_operator (crt, CAIRO_OPERATOR_DEST_OVER);
  cairo_set_source_surface (crt, surf_shadow, 0.0, 0.0);
  cairo_paint (crt);

  cairo_destroy (crt);
  cairo_surface_destroy (surf_shadow);
  cairo_surface_destroy (surf);
  window->image_width = width;
  window->image_height = height;
}

static void
gst_cea708dec_adjust_values_with_fontdesc (cea708Window * window,
    PangoFontDescription * desc)
{
  gint font_size = pango_font_description_get_size (desc) / PANGO_SCALE;

  window->shadow_offset = (double) (font_size) / 13.0;
  window->outline_offset = (double) (font_size) / 15.0;
  if (window->outline_offset < MINIMUM_OUTLINE_OFFSET)
    window->outline_offset = MINIMUM_OUTLINE_OFFSET;
}

static gint
gst_cea708dec_text_list_add (GSList ** text_list,
    gint len, const gchar * format, ...)
{
  va_list args;
  gchar *str;

  va_start (args, format);

  str = g_malloc0 (len);
  len = g_vsnprintf (str, len, format, args);
  *text_list = g_slist_append (*text_list, str);
  GST_LOG ("added %p str[%d]: %s", str, len, str);

  va_end (args);
  return len;
}

static const PangoAlignment
gst_cea708dec_get_align_mode (guint8 justify_mode)
{
  guint align_mode = PANGO_ALIGN_LEFT;

  switch (justify_mode) {
    case JUSTIFY_LEFT:
      align_mode = PANGO_ALIGN_LEFT;
      break;
    case JUSTIFY_RIGHT:
      align_mode = PANGO_ALIGN_RIGHT;
      break;
    case JUSTIFY_CENTER:
      align_mode = PANGO_ALIGN_CENTER;
      break;
    case JUSTIFY_FULL:
    default:
      align_mode = PANGO_ALIGN_LEFT;
  }
  return align_mode;
}

static const gchar *
gst_cea708dec_get_color_name (guint8 color)
{
  guint index = 0;

  switch (color) {
    case CEA708_COLOR_BLACK:
      index = COLOR_TYPE_BLACK;
      break;
    case CEA708_COLOR_WHITE:
      index = COLOR_TYPE_WHITE;
      break;
    case CEA708_COLOR_RED:
      index = COLOR_TYPE_RED;
      break;
    case CEA708_COLOR_GREEN:
      index = COLOR_TYPE_GREEN;
      break;
    case CEA708_COLOR_BLUE:
      index = COLOR_TYPE_BLUE;
      break;
    case CEA708_COLOR_YELLOW:
      index = COLOR_TYPE_YELLOW;
      break;
    case CEA708_COLOR_MAGENTA:
      index = COLOR_TYPE_MAGENTA;
      break;
    case CEA708_COLOR_CYAN:
      index = COLOR_TYPE_CYAN;
      break;
    default:
      break;
  }

  return color_names[index];
}

static guint8
gst_cea708dec_map_minimum_color (guint8 color)
{
  /*According to spec minimum color list define */
  /*check R */
  switch ((color & 0x30) >> 4) {
    case 1:
      color &= 0xF;
      break;
    case 3:
      color &= 0x2F;
      break;
    default:
      break;
  }
  /*check G */
  switch ((color & 0xC) >> 2) {
    case 1:
      color &= 0x33;
      break;
    case 3:
      color &= 0x3B;
      break;
    default:
      break;
  }
  /*check B */
  switch (color & 0x3) {
    case 1:
      color &= 0x3C;
      break;
    case 3:
      color &= 0x3E;
      break;
    default:
      break;
  }

  return color;
}

static void
gst_cea708dec_set_pen_color (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];

  /* format
     fo1 fo0 fr1 fr0 fg1 fg0 fb1 fb0
     bo1 bo0 br1 br0 bg1 bg0 bb1 bb0
     0 0 er1 er0 eg1 eg0 eb1 eb0 */
  window->pen_color.fg_color =
      gst_cea708dec_map_minimum_color (dtvcc_buffer[index] & 0x3F);
  window->pen_color.fg_opacity = (dtvcc_buffer[index] & 0xC0) >> 6;
  window->pen_color.bg_color =
      gst_cea708dec_map_minimum_color (dtvcc_buffer[index + 1] & 0x3F);
  window->pen_color.bg_opacity = (dtvcc_buffer[index + 1] & 0xC0) >> 6;
  window->pen_color.edge_color =
      gst_cea708dec_map_minimum_color (dtvcc_buffer[index + 2] & 0x3F);
  GST_LOG ("pen_color fg=0x%x fg_op=0x%x bg=0x%x bg_op=0x%x edge=0x%x",
      window->pen_color.fg_color, window->pen_color.fg_opacity,
      window->pen_color.bg_color, window->pen_color.bg_opacity,
      window->pen_color.edge_color);
}

static void
gst_cea708dec_set_window_attributes (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];

  /* format
     fo1 fo0 fr1 fr0 fg1 fg0 fb1 fb0
     bt1 bt0 br1 br0 bg1 bg0 bb1 bb0
     bt2 ww  pd1 pd0 sd1 sd0 j1  j0
     es3 es2 es1 es0 ed1 ed0 de1 de0 */
  window->fill_color =
      gst_cea708dec_map_minimum_color (dtvcc_buffer[index] & 0x3F);
  window->fill_opacity = (dtvcc_buffer[index] & 0xC0) >> 6;
  window->border_color =
      gst_cea708dec_map_minimum_color (dtvcc_buffer[index + 1] & 0x3F);
  window->border_type =
      ((dtvcc_buffer[index + 1] & 0xC0) >> 6) | ((dtvcc_buffer[index +
              2] & 0x80) >> 5);
  window->word_wrap = (dtvcc_buffer[index + 2] & 0x40) ? TRUE : FALSE;
  window->justify_mode = dtvcc_buffer[index + 2] & 0x3;
  window->scroll_direction = (dtvcc_buffer[index + 2] & 0xC) >> 2;
  window->print_direction = (dtvcc_buffer[index + 2] & 0x30) >> 2;
  window->display_effect = (dtvcc_buffer[index + 3] & 0x3);
  window->effect_direction = (dtvcc_buffer[index + 3] & 0xC);
  window->effect_speed = (dtvcc_buffer[index + 3] & 0xF0) >> 4;

  GST_LOG ("Print direction = %d", window->print_direction);
}

static void
gst_cea708dec_set_pen_style (Cea708Dec * decoder, guint8 pen_style_id)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];

  window->pen_attributes.pen_size = PEN_SIZE_STANDARD;
  window->pen_attributes.font_style = FONT_STYLE_DEFAULT;
  window->pen_attributes.offset = PEN_OFFSET_NORMAL;
  window->pen_attributes.italics = FALSE;
  window->pen_attributes.underline = FALSE;
  window->pen_attributes.edge_type = EDGE_TYPE_NONE;
  window->pen_color.fg_color = CEA708_COLOR_WHITE;
  window->pen_color.fg_opacity = SOLID;
  window->pen_color.bg_color = CEA708_COLOR_BLACK;
  window->pen_color.bg_opacity = SOLID;
  window->pen_color.edge_color = CEA708_COLOR_BLACK;

  /* CEA-708 predefined pen style ids */
  switch (pen_style_id) {
    default:
    case PEN_STYLE_DEFAULT:
      window->pen_attributes.font_style = FONT_STYLE_DEFAULT;
      break;

    case PEN_STYLE_MONO_SERIF:
      window->pen_attributes.font_style = FONT_STYLE_MONO_SERIF;
      break;

    case PEN_STYLE_PROP_SERIF:
      window->pen_attributes.font_style = FONT_STYLE_PROP_SERIF;
      break;

    case PEN_STYLE_MONO_SANS:
      window->pen_attributes.font_style = FONT_STYLE_MONO_SANS;
      break;

    case PEN_STYLE_PROP_SANS:
      window->pen_attributes.font_style = FONT_STYLE_PROP_SANS;
      break;

    case PEN_STYLE_MONO_SANS_TRANSPARENT:
      window->pen_attributes.font_style = FONT_STYLE_MONO_SANS;
      window->pen_color.bg_opacity = TRANSPARENT;
      break;

    case PEN_STYLE_PROP_SANS_TRANSPARENT:
      window->pen_attributes.font_style = FONT_STYLE_PROP_SANS;
      window->pen_color.bg_opacity = TRANSPARENT;
      break;
  }
}

static void
gst_cea708dec_set_window_style (Cea708Dec * decoder, guint8 style_id)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];

  /* set the 'normal' styles first, then deviate in special cases below... */
  window->justify_mode = JUSTIFY_LEFT;
  window->print_direction = PRINT_DIR_LEFT_TO_RIGHT;
  window->scroll_direction = SCROLL_DIR_BOTTOM_TO_TOP;
  window->word_wrap = FALSE;
  window->effect_direction = EFFECT_DIR_LEFT_TO_RIGHT;
  window->display_effect = DISPLAY_EFFECT_SNAP;
  window->effect_speed = 0;
  window->fill_color = CEA708_COLOR_BLACK;
  window->fill_opacity = SOLID;

  /* CEA-708 predefined window style ids */
  switch (style_id) {
    default:
    case WIN_STYLE_NORMAL:
      break;

    case WIN_STYLE_TRANSPARENT:
      window->fill_opacity = TRANSPARENT;
      break;

    case WIN_STYLE_NORMAL_CENTERED:
      window->justify_mode = JUSTIFY_CENTER;
      break;

    case WIN_STYLE_NORMAL_WORD_WRAP:
      window->word_wrap = TRUE;
      break;

    case WIN_STYLE_TRANSPARENT_WORD_WRAP:
      window->fill_opacity = TRANSPARENT;
      window->word_wrap = TRUE;
      break;

    case WIN_STYLE_TRANSPARENT_CENTERED:
      window->fill_opacity = TRANSPARENT;
      window->justify_mode = JUSTIFY_CENTER;
      break;

    case WIN_STYLE_ROTATED:
      window->print_direction = PRINT_DIR_TOP_TO_BOTTOM;
      window->scroll_direction = SCROLL_DIR_RIGHT_TO_LEFT;
      break;
  }
}

/* Define window - window size, window style, pen style, anchor position, etc */
static void
gst_cea708dec_define_window (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];
  guint8 priority = 0;
  guint8 anchor_point = 0;
  guint8 relative_position = 0;
  guint8 anchor_vertical = 0;
  guint8 anchor_horizontal = 0;
  guint8 row_count = 0;
  guint8 column_count = 0;
  guint8 row_lock = 0;
  guint8 column_lock = 0;
  gboolean visible = FALSE;
  guint8 style_id = 0;
  guint8 pen_style_id = 0;
#ifndef GST_DISABLE_GST_DEBUG
  guint v_anchor = 0;
  guint h_anchor = 0;
#endif

  GST_LOG ("current_window=%d", decoder->current_window);
  GST_LOG ("dtvcc_buffer %02x %02x %02x %02x %02x %02x",
      dtvcc_buffer[index + 0], dtvcc_buffer[index + 1],
      dtvcc_buffer[index + 2], dtvcc_buffer[index + 3],
      dtvcc_buffer[index + 4], dtvcc_buffer[index + 5]);

  /* Initialize window structure */
  if (NULL != window) {
    if (window->deleted) {
      /* Spec says on window create (but not re-definition) the pen position
       * must be reset to 0
       * TODO: also set all text positions to the fill color */
      window->deleted = FALSE;
      window->pen_row = 0;
      window->pen_col = 0;
    }
    /* format of parameters:
       0 0 v rl cl p2 p1 p0
       rp av7 av6 av5 av4 av3 av1 av0
       ah7 ah6 ah5 ah4 ah3 ah2 ah1 ah0
       ap3 ap2 ap1 ap0 rc3 rc2 rc1 rc0
       0 0 cc5 cc4 cc3 cc2 cc1 cc0
       0 0 ws2 ws1 ws0 ps2 ps1 ps0 */

    /* parameter byte 0 */
    priority = dtvcc_buffer[index] & 0x7;
    column_lock = (dtvcc_buffer[index] & 0x8) ? TRUE : FALSE;
    row_lock = (dtvcc_buffer[index] & 0x10) ? TRUE : FALSE;
    visible = (dtvcc_buffer[index] & 0x20) ? TRUE : FALSE;

    /* parameter byte 1 */
    relative_position = (dtvcc_buffer[index + 1] & 0x80) ? TRUE : FALSE;
    anchor_vertical = dtvcc_buffer[index + 1] & 0x7F;

    /* parameter byte 2 */
    anchor_horizontal = dtvcc_buffer[index + 2];

    /* parameter byte 3 */
    anchor_point = (dtvcc_buffer[index + 3] & 0xF0) >> 4;
    row_count = (dtvcc_buffer[index + 3] & 0xF) + 1;

    /* parameter byte 4 */
    column_count = (dtvcc_buffer[index + 4] & 0x3F) + 1;

    /* parameter byte 5 */
    style_id = (dtvcc_buffer[index + 5] & 0x38) >> 3;
    pen_style_id = dtvcc_buffer[index + 5] & 0x7;

    window->screen_vertical = anchor_vertical;
    window->screen_horizontal = anchor_horizontal;

    if (relative_position == FALSE) {
      /* If position is in absolute coords, convert to percent */
      if (decoder->width == 0 || decoder->height == 0) {
        window->screen_vertical /= 100;
        window->screen_horizontal /= 100;
      } else if ((decoder->width * 9) % (decoder->height * 16) == 0) {
        window->screen_vertical /= SCREEN_HEIGHT_16_9;
        window->screen_horizontal /= SCREEN_WIDTH_16_9;
      } else if ((decoder->width * 3) % (decoder->height * 4) == 0) {
        window->screen_vertical /= SCREEN_HEIGHT_4_3;
        window->screen_horizontal /= SCREEN_WIDTH_4_3;
      } else {
        window->screen_vertical /= 100;
        window->screen_horizontal /= 100;
      }
      window->screen_vertical *= 100;
      window->screen_horizontal *= 100;
    }

    window->priority = priority;
    window->anchor_point = anchor_point;
    window->relative_position = relative_position;
    window->anchor_vertical = anchor_vertical;
    window->anchor_horizontal = anchor_horizontal;
    window->row_count = row_count;
    window->column_count = column_count;
    window->row_lock = row_lock;
    window->column_lock = column_lock;
    window->visible = visible;

    /* Make sure row/col limits are not too large */
    if (window->row_count > WINDOW_MAX_ROWS) {
      GST_WARNING ("window row count %d is too large", window->row_count);
      window->row_count = WINDOW_MAX_ROWS;
    }

    if (window->column_count > WINDOW_MAX_COLS) {
      GST_WARNING ("window column count %d is too large", window->column_count);
      window->column_count = WINDOW_MAX_COLS;
    }

    if (style_id != 0) {
      window->style_id = style_id;
    }

    if (pen_style_id != 0) {
      window->pen_style_id = pen_style_id;
    }

    gst_cea708dec_set_window_style (decoder, window->style_id);
    gst_cea708dec_set_pen_style (decoder, window->pen_style_id);
  }

  GST_LOG ("priority=%d anchor=%d relative_pos=%d anchor_v=%d anchor_h=%d",
      window->priority,
      window->anchor_point,
      window->relative_position,
      window->anchor_vertical, window->anchor_horizontal);

  GST_LOG ("row_count=%d col_count=%d row_lock=%d col_lock=%d visible=%d",
      window->row_count,
      window->column_count,
      window->row_lock, window->column_lock, window->visible);

  GST_LOG ("style_id=%d pen_style_id=%d screenH=%f screenV=%f v_offset=%d "
      "h_offset=%d v_anchor=%d h_anchor=%d",
      window->style_id,
      window->pen_style_id,
      window->screen_horizontal,
      window->screen_vertical,
      window->v_offset, window->h_offset, v_anchor, h_anchor);
}

static inline void
pango_span_markup_init (cea708PangoSpanControl * span_control)
{
  memset (span_control, 0, sizeof (cea708PangoSpanControl));
  span_control->size = PEN_SIZE_STANDARD;
  span_control->fg_color = CEA708_COLOR_WHITE;
  span_control->bg_color = CEA708_COLOR_INVALID;
  span_control->size = PEN_SIZE_STANDARD;
  span_control->font_style = FONT_STYLE_DEFAULT;
}

static inline void
pango_span_markup_start (cea708PangoSpanControl * span_control,
    gchar * line_buffer, guint16 * index)
{
  GST_LOG ("span_control start_flag:%d end_flag:%d txt_flag:%d",
      span_control->span_start_flag, span_control->span_end_flag,
      span_control->span_txt_flag);
  if (!span_control->span_start_flag) {
    g_strlcat (line_buffer, CEA708_PANGO_SPAN_MARKUP_START, LINEBUFFER_SIZE);
    *index += strlen (CEA708_PANGO_SPAN_MARKUP_START);
    span_control->span_start_flag = TRUE;
    span_control->span_end_flag = FALSE;
  } else {
    GST_WARNING ("warning span start  !!!");
  }
}

static inline void
pango_span_markup_txt (cea708PangoSpanControl * span_control,
    gchar * line_buffer, guint16 * index)
{
  GST_LOG ("span_control start_flag:%d end_flag:%d txt_flag:%d",
      span_control->span_start_flag, span_control->span_end_flag,
      span_control->span_txt_flag);
  if (span_control->span_start_flag && !span_control->span_txt_flag) {
    line_buffer[*index] = '>';
    *index = *index + 1;
    span_control->span_txt_flag = TRUE;
  } else {
    GST_WARNING ("warning span txt  !!!");
  }
}

static inline void
pango_span_markup_end (cea708PangoSpanControl * span_control,
    gchar * line_buffer, guint16 * index)
{
  GST_LOG ("span_control start_flag:%d end_flag:%d txt_flag:%d",
      span_control->span_start_flag, span_control->span_end_flag,
      span_control->span_txt_flag);
  if (span_control->span_start_flag && !span_control->span_end_flag) {
    g_strlcat (line_buffer, CEA708_PANGO_SPAN_MARKUP_END, LINEBUFFER_SIZE);
    *index = *index + strlen (CEA708_PANGO_SPAN_MARKUP_END);
    span_control->span_start_flag = FALSE;
    span_control->span_txt_flag = FALSE;
    span_control->span_end_flag = TRUE;
  } else {
    GST_WARNING ("line_buffer=%s", line_buffer);
    GST_WARNING ("warning span end  !!!");
  }
}

/* FIXME: Convert to GString ! */
static void
gst_cea708dec_show_pango_window (Cea708Dec * decoder, guint window_id)
{
  cea708Window *window = decoder->cc_windows[window_id];
  gint16 row, col;
  gboolean display = FALSE;     /* = TRUE when text lines should be written */
  gchar line_buffer[LINEBUFFER_SIZE];
  gchar outchar_utf8[CC_UTF8_MAX_LENGTH + 1] = { 0 };
  guint8 utf8_char_length;
  gint16 i, j;
  gint16 right_index;           /* within a single line of window text, the
                                 * index of the rightmost non-blank character */
  guint16 index;
  guint len = 0;

  cea708PangoSpanControl span_control;
  const gchar *fg_color = NULL;
  const gchar *bg_color = NULL;
  const gchar *pen_size = NULL;
  const gchar *font = NULL;

  GST_DEBUG ("window #%02d (visible:%d)", window_id, window->visible);

  window->updated = TRUE;

  if (!window->visible) {
    GST_DEBUG ("Window is not visible, skipping rendering");
    return;
  }

  for (row = 0; row < window->row_count; row++) {
    for (col = 0; col < window->column_count; col++) {
      GST_LOG ("window->text[%d][%d].c '%c'", row, col,
          window->text[row][col].c);
      if (window->text[row][col].c != ' ') {
        /* If there is a non-blank line, then display from there */
        display = TRUE;
      }
    }
  }

  if (!display) {
    GST_DEBUG ("No visible text, skipping rendering");
    return;
  }

  for (row = 0; row < window->row_count; row++) {
    for (col = 0; col < window->column_count; col++) {
      if (window->text[row][col].c != ' ') {

        memset (line_buffer, '\0', LINEBUFFER_SIZE);
        pango_span_markup_init (&span_control);
        /* Find the rightmost non-blank character on this line: */
        for (i = right_index = WINDOW_MAX_COLS - 1; i >= col; i--) {
          if (window->text[row][i].c != ' ') {
            right_index = i;
            break;
          }
        }

        /* Copy all of the characters in this row, from the current position
         * to the right edge */
        for (i = 0, index = 0;
            (i <= right_index) && (index < LINEBUFFER_SIZE - 15); i++) {
          cea708char *current = &window->text[row][i];
          GST_LOG ("Adding row=%d i=%d c=%c %d", row,
              i, current->c, current->c);

          do {
            GST_MEMDUMP ("line_buffer", (guint8 *) line_buffer, index);
            GST_INFO
                ("text[%d][%d] '%c' underline:%d , italics:%d , font_style:%d , pen_size : %d",
                row, i, current->c,
                current->pen_attributes.underline,
                current->pen_attributes.italics,
                current->pen_attributes.font_style,
                current->pen_attributes.pen_size);
            GST_INFO ("text[%d][%d] '%c' pen_color fg:0x%02X bg:0x%02x", row, i,
                current->c, current->pen_color.fg_color,
                current->pen_color.bg_color);
            GST_INFO
                ("span_control: span_next_flag = %d, underline = %d, italics = %d, font_style = %d, size = %d, fg_color = 0x%02X, bg_color = 0x%02X",
                span_control.span_next_flag, span_control.underline,
                span_control.italics, span_control.font_style,
                span_control.size, span_control.fg_color,
                span_control.bg_color);

            if ((current->pen_attributes.underline != span_control.underline)
                || (current->pen_attributes.italics != span_control.italics)
                || (current->pen_attributes.font_style !=
                    span_control.font_style)
                || (current->pen_attributes.pen_size != span_control.size)
                || (current->pen_color.fg_color != span_control.fg_color)
                || (current->pen_color.bg_color != span_control.bg_color)
                ) {
              GST_LOG ("Markup changed");

              /* check end span to next span start */
              if (!span_control.span_next_flag) {
                pango_span_markup_end (&span_control, line_buffer, &index);
                if (span_control.span_end_flag) {
                  pango_span_markup_init (&span_control);
                  span_control.span_next_flag = TRUE;
                  GST_INFO ("continue check next span !!!");
                  continue;
                }
              }

              pango_span_markup_start (&span_control, line_buffer, &index);

              /* Check for transitions to/from underline: */
              if (current->pen_attributes.underline) {
                g_strlcat (line_buffer,
                    CEA708_PANGO_SPAN_ATTRIBUTES_UNDERLINE_SINGLE,
                    sizeof (line_buffer));
                index += strlen (CEA708_PANGO_SPAN_ATTRIBUTES_UNDERLINE_SINGLE);
                span_control.underline = TRUE;
              }

              /* Check for transitions to/from italics: */
              if (current->pen_attributes.italics) {
                g_strlcat (line_buffer,
                    CEA708_PANGO_SPAN_ATTRIBUTES_STYLE_ITALIC,
                    sizeof (line_buffer));
                index += strlen (CEA708_PANGO_SPAN_ATTRIBUTES_STYLE_ITALIC);
                span_control.italics = TRUE;
              }

              /* FIXME : Something is totally wrong with the way fonts
               * are being handled. Shouldn't the font description (if
               * specified by the user) be written for everything ? */
              if (!decoder->default_font_desc) {
                font = font_names[current->pen_attributes.font_style];

                if (font) {
                  g_strlcat (line_buffer, CEA708_PANGO_SPAN_ATTRIBUTES_FONT,
                      sizeof (line_buffer));
                  index += strlen (CEA708_PANGO_SPAN_ATTRIBUTES_FONT);
                  line_buffer[index++] = 0x27;  /* ' */
                  g_strlcat (line_buffer, font, sizeof (line_buffer));
                  index += strlen (font);
                  span_control.font_style = current->pen_attributes.font_style;

                  /* Check for transitions to/from pen size  */
                  pen_size = pen_size_names[current->pen_attributes.pen_size];

                  line_buffer[index++] = ' ';
                  g_strlcat (line_buffer, pen_size, sizeof (line_buffer));
                  index += strlen (pen_size);
                  line_buffer[index++] = 0x27;  /* ' */
                  span_control.size = current->pen_attributes.pen_size;
                }
              }
              /* Regardless of the above, we want to remember the latest changes */
              span_control.font_style = current->pen_attributes.font_style;
              span_control.size = current->pen_attributes.pen_size;
              ;
              /* Check for transitions to/from foreground color  */
              fg_color =
                  gst_cea708dec_get_color_name (current->pen_color.fg_color);
              if (fg_color && current->pen_color.bg_opacity != TRANSPARENT) {
                g_strlcat (line_buffer, CEA708_PANGO_SPAN_ATTRIBUTES_FOREGROUND,
                    sizeof (line_buffer));
                index += strlen (CEA708_PANGO_SPAN_ATTRIBUTES_FOREGROUND);
                line_buffer[index++] = 0x27;    /* ' */
                g_strlcat (line_buffer, fg_color, sizeof (line_buffer));
                index += strlen (fg_color);
                line_buffer[index++] = 0x27;    /* ' */
                span_control.fg_color = current->pen_color.fg_color;
                GST_DEBUG ("span_control.fg_color updated to 0x%02x",
                    span_control.fg_color);
              } else
                GST_DEBUG
                    ("span_control.fg_color was NOT updated (still 0x%02x)",
                    span_control.fg_color);

              /* Check for transitions to/from background color  */
              bg_color =
                  gst_cea708dec_get_color_name (current->pen_color.bg_color);
              if (bg_color && current->pen_color.bg_opacity != TRANSPARENT) {
                g_strlcat (line_buffer, CEA708_PANGO_SPAN_ATTRIBUTES_BACKGROUND,
                    sizeof (line_buffer));
                index += strlen (CEA708_PANGO_SPAN_ATTRIBUTES_BACKGROUND);
                line_buffer[index++] = 0x27;    /* ' */
                g_strlcat (line_buffer, bg_color, sizeof (line_buffer));
                index += strlen (bg_color);
                line_buffer[index++] = 0x27;    /* ' */
                span_control.bg_color = current->pen_color.bg_color;
                GST_DEBUG ("span_control.bg_color updated to 0x%02x",
                    span_control.bg_color);
              } else
                GST_DEBUG
                    ("span_control.bg_color was NOT updated (still 0x%02x)",
                    span_control.bg_color);


              /*span text start */
              pango_span_markup_txt (&span_control, line_buffer, &index);
              GST_INFO ("span_next_flag = %d", span_control.span_next_flag);
            }
            span_control.span_next_flag = FALSE;
          } while (span_control.span_next_flag);


          /* Finally write the character */

          j = 0;


          switch (current->c) {
            case '&':
              g_snprintf (&(line_buffer[index]),
                  sizeof (line_buffer) - index - 1, "&amp;");
              index += 5;
              break;

            case '<':
              g_snprintf (&(line_buffer[index]),
                  sizeof (line_buffer) - index - 1, "&lt;");
              index += 4;
              break;

            case '>':
              g_snprintf (&(line_buffer[index]),
                  sizeof (line_buffer) - index - 1, "&gt;");
              index += 4;
              break;

            case '\'':
              g_snprintf (&(line_buffer[index]),
                  sizeof (line_buffer) - index - 1, "&apos;");
              index += 6;
              break;

            case '"':
              g_snprintf (&(line_buffer[index]),
                  sizeof (line_buffer) - index - 1, "&quot;");
              index += 6;
              break;

            default:
              /* FIXME : Use g_string_append_unichar() when switched to GString */
              utf8_char_length = g_unichar_to_utf8 (current->c, outchar_utf8);
              while (utf8_char_length > 0) {
                line_buffer[index++] = outchar_utf8[j++];
                utf8_char_length--;
              }
          }

        }

        /* pango markup span mode ends */
        if (span_control.underline || span_control.italics
            || (span_control.font_style != FONT_STYLE_DEFAULT)
            || (span_control.size != PEN_SIZE_STANDARD)
            || (span_control.fg_color != CEA708_COLOR_WHITE)
            || (span_control.bg_color != CEA708_COLOR_INVALID)
            ) {
          pango_span_markup_end (&span_control, line_buffer, &index);
          pango_span_markup_init (&span_control);
        }

        GST_LOG ("adding row[%d]: %s\nlength:%d", row, line_buffer, index);

        if (row != window->row_count - 1) {
          line_buffer[index++] = '\n';
        }

        len +=
            gst_cea708dec_text_list_add (&decoder->text_list, index + 1, "%s",
            line_buffer);
        break;
      }
    }

    if (col == window->column_count && row != window->row_count - 1) {
      len +=
          gst_cea708dec_text_list_add (&decoder->text_list, strlen ("\n") + 1,
          "\n");
    }
  }

  if (len == 0) {
    GST_LOG ("window %d had no text", window_id);
  } else {
    /* send text to output pad */
    gst_cea708dec_render_text (decoder, &decoder->text_list, len, window_id);
  }
}

static void
gst_cea708dec_clear_window_text (Cea708Dec * decoder, guint window_id)
{
  cea708Window *window = decoder->cc_windows[window_id];
  guint row, col;

  for (row = 0; row < WINDOW_MAX_ROWS; row++) {
    for (col = 0; col < WINDOW_MAX_COLS; col++) {
      window->text[row][col].c = ' ';
      window->text[row][col].justify_mode = window->justify_mode;
      window->text[row][col].pen_attributes = window->pen_attributes;
      window->text[row][col].pen_color = window->pen_color;
    }
  }
}

static void
gst_cea708dec_scroll_window_up (Cea708Dec * decoder, guint window_id)
{
  cea708Window *window = decoder->cc_windows[window_id];
  guint row, col;

  /* This function should be called to scroll the window up if bottom to
   * top scrolling is enabled and a carraige-return is encountered, or
   * word-wrapping */
  GST_LOG_OBJECT (decoder, "called for window: %d", window_id);

  /* start at row 1 to copy into row 0 (scrolling up) */
  for (row = 1; row < WINDOW_MAX_ROWS; row++) {
    for (col = 0; col < WINDOW_MAX_COLS; col++) {
      window->text[row - 1][col] = window->text[row][col];
    }
  }

  /* Clear the bottom row: */
  row = WINDOW_MAX_ROWS - 1;
  for (col = 0; col < WINDOW_MAX_COLS; col++) {
    window->text[row][col].c = ' ';
    window->text[row][col].justify_mode = window->justify_mode;
    window->text[row][col].pen_attributes = window->pen_attributes;
    window->text[row][col].pen_color = window->pen_color;
  }
}

static void
gst_cea708dec_clear_window (Cea708Dec * decoder, cea708Window * window)
{
  g_free (window->text_image);
  memset (window, 0, sizeof (cea708Window));
}

static void
gst_cea708dec_init_window (Cea708Dec * decoder, guint window_id)
{
  cea708Window *window = decoder->cc_windows[window_id];

  if (window_id >= MAX_708_WINDOWS) {
    GST_ERROR ("window_id outside of range %d", window_id);
    return;
  }

  window->priority = 0;
  window->anchor_point = 0;
  window->relative_position = 0;
  window->anchor_vertical = 0;
  window->anchor_horizontal = 0;
  window->screen_vertical = 0;
  window->screen_horizontal = 0;

  window->row_count = WINDOW_MAX_ROWS;
  window->column_count = WINDOW_MAX_COLS;
  window->row_lock = 0;
  window->column_lock = 0;
  window->visible = FALSE;
  window->style_id = 0;
  window->pen_style_id = 0;
  window->deleted = TRUE;
  window->pen_color.fg_color = CEA708_COLOR_WHITE;
  window->pen_color.fg_opacity = SOLID;
  window->pen_color.bg_color = CEA708_COLOR_BLACK;
  window->pen_color.bg_opacity = SOLID;
  window->pen_color.edge_color = CEA708_COLOR_BLACK;

  window->pen_attributes.pen_size = PEN_SIZE_STANDARD;
  window->pen_attributes.font_style = FONT_STYLE_DEFAULT;
  window->pen_attributes.offset = PEN_OFFSET_NORMAL;
  window->pen_attributes.italics = FALSE;
  window->pen_attributes.text_tag = TAG_DIALOG;
  window->pen_attributes.underline = FALSE;
  window->pen_attributes.edge_type = EDGE_TYPE_NONE;

  /* Init pen position */
  window->pen_row = 0;
  window->pen_col = 0;

  /* Initialize text array to all spaces. When sending window text, only
   * send if there are non-blank rows */
  gst_cea708dec_clear_window_text (decoder, window_id);

  /* window attributes */
  window->justify_mode = JUSTIFY_LEFT;
  window->print_direction = PRINT_DIR_LEFT_TO_RIGHT;
  window->scroll_direction = SCROLL_DIR_BOTTOM_TO_TOP;
  window->word_wrap = FALSE;
  window->display_effect = DISPLAY_EFFECT_SNAP;
  window->effect_direction = EFFECT_DIR_LEFT_TO_RIGHT;
  window->effect_speed = 0;
  window->fill_color = CEA708_COLOR_BLACK;
  window->fill_opacity = TRANSPARENT;
  window->border_type = BORDER_TYPE_NONE;
  window->border_color = CEA708_COLOR_BLACK;

  window->v_offset = 0;
  window->h_offset = 0;
  window->layout = NULL;
  window->shadow_offset = 0;
  window->outline_offset = 0;
  window->image_width = 0;
  window->image_height = 0;
  window->text_image = NULL;

}

static void
gst_cea708dec_set_pen_attributes (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];

  /* format
     tt3 tt2 tt1 tt0 o1 o0 s1 s0
     i u et2 et1 et0 fs2 fs1 fs0 */
  window->pen_attributes.pen_size = dtvcc_buffer[index] & 0x3;
  window->pen_attributes.text_tag = (dtvcc_buffer[index] & 0xF0) >> 4;
  window->pen_attributes.offset = (dtvcc_buffer[index] & 0xC0) >> 2;
  window->pen_attributes.font_style = dtvcc_buffer[index + 1] & 0x7;
  window->pen_attributes.italics =
      ((dtvcc_buffer[index + 1] & 0x80) >> 7) ? TRUE : FALSE;
  window->pen_attributes.underline =
      ((dtvcc_buffer[index + 1] & 0x40) >> 6) ? TRUE : FALSE;
  window->pen_attributes.edge_type = (dtvcc_buffer[index + 1] & 0x38) >> 3;

  GST_LOG ("pen_size=%d font=%d text_tag=%d offset=%d",
      window->pen_attributes.pen_size,
      window->pen_attributes.font_style,
      window->pen_attributes.text_tag, window->pen_attributes.offset);

  GST_LOG ("italics=%d underline=%d edge_type=%d",
      window->pen_attributes.italics,
      window->pen_attributes.underline, window->pen_attributes.edge_type);
}

static void
gst_cea708dec_for_each_window (Cea708Dec * decoder,
    guint8 window_list, VisibilityControl visibility_control,
    const gchar * log_message, void (*function) (Cea708Dec * decoder,
        guint window_id))
{
  guint i;

  GST_LOG ("window_list: %02x", window_list);

  for (i = 0; i < MAX_708_WINDOWS; i++) {
    if (WINDOW_IN_LIST_IS_ACTIVE (window_list)) {
      GST_LOG ("%s[%d]:%d %s v_offset=%d h_offset=%d",
          log_message, i, WINDOW_IN_LIST_IS_ACTIVE (window_list),
          (decoder->cc_windows[i]->visible) ? "visible" : "hidden",
          decoder->cc_windows[i]->v_offset, decoder->cc_windows[i]->h_offset);
      switch (visibility_control) {
        default:
        case NO_CHANGE:
          break;

        case SWITCH_TO_HIDE:
          decoder->cc_windows[i]->visible = FALSE;
          break;

        case SWITCH_TO_SHOW:
          decoder->cc_windows[i]->visible = TRUE;
          break;

        case TOGGLE:
          decoder->cc_windows[i]->visible = !decoder->cc_windows[i]->visible;
          break;
      }

      if (NULL != function) {
        function (decoder, i);
      }
    }

    window_list >>= 1;
  }
}

static void
gst_cea708dec_process_command (Cea708Dec * decoder,
    guint8 * dtvcc_buffer, int index)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];
  guint8 c = dtvcc_buffer[index];
  guint8 window_list = dtvcc_buffer[index + 1]; /* always the first arg (if any) */

  /* Process command codes */
  gst_cea708dec_print_command_name (decoder, c);
  switch (c) {
    case CC_COMMAND_ETX:       /* End of text */
      window->visible = TRUE;
      gst_cea708dec_show_pango_window (decoder, decoder->current_window);
      return;

    case CC_COMMAND_CW0:       /* Set current window */
    case CC_COMMAND_CW1:
    case CC_COMMAND_CW2:
    case CC_COMMAND_CW3:
    case CC_COMMAND_CW4:
    case CC_COMMAND_CW5:
    case CC_COMMAND_CW6:
    case CC_COMMAND_CW7:
      decoder->current_window = c & 0x03;
      GST_LOG ("Current window=%d", decoder->current_window);
      return;

    case CC_COMMAND_CLW:       /* Clear windows */
      decoder->output_ignore = 1;       /* 1 byte parameter = windowmap */

      /* Clear window data */
      gst_cea708dec_for_each_window (decoder, window_list, NO_CHANGE,
          "clear_window", gst_cea708dec_clear_window_text);
      return;

    case CC_COMMAND_DSW:       /* Display windows */
      decoder->output_ignore = 1;       /* 1 byte parameter = windowmap */

      /* Show window */
      gst_cea708dec_for_each_window (decoder, window_list, NO_CHANGE,
          "display_window", gst_cea708dec_show_pango_window);
      return;

    case CC_COMMAND_HDW:       /* Hide windows */
      decoder->output_ignore = 1;       /* 1 byte parameter = windowmap */

      /* Hide window */
      gst_cea708dec_for_each_window (decoder, window_list, SWITCH_TO_HIDE,
          "hide_window", NULL);
      return;

    case CC_COMMAND_TGW:       /* Toggle windows */
      decoder->output_ignore = 1;       /* 1 byte parameter = windowmap */

      /* Toggle windows - hide displayed windows, display hidden windows */
      gst_cea708dec_for_each_window (decoder, window_list, TOGGLE,
          "toggle_window", gst_cea708dec_show_pango_window);
      return;

    case CC_COMMAND_DLW:       /* Delete windows */
      decoder->output_ignore = 1;       /* 1 byte parameter = windowmap */

      /* Delete window */
      gst_cea708dec_for_each_window (decoder, window_list, NO_CHANGE,
          "delete_window", gst_cea708dec_init_window);
      return;

    case CC_COMMAND_DLY:       /* Delay */
      decoder->output_ignore = 1;       /* 1 byte parameter = delay in 1/10 sec */
      /* TODO: - process this command. */
      return;

    case CC_COMMAND_DLC:       /* Delay cancel */
      /* TODO: - process this command. */
      return;

      /* Reset */
    case CC_COMMAND_RST:
      /* Reset - cancel any delay, delete all windows */
      window_list = 0xFF;       /* all windows... */

      /* Delete window */
      gst_cea708dec_for_each_window (decoder, window_list, NO_CHANGE,
          "reset_window", gst_cea708dec_init_window);
      return;

    case CC_COMMAND_SPA:       /* Set pen attributes */
      decoder->output_ignore = 2;       /* 2 byte parameter = pen attributes */
      gst_cea708dec_set_pen_attributes (decoder, dtvcc_buffer, index + 1);
      return;

    case CC_COMMAND_SPC:       /* Set pen color */
      decoder->output_ignore = 3;       /* 3 byte parameter = color & opacity */
      gst_cea708dec_set_pen_color (decoder, dtvcc_buffer, index + 1);
      return;

    case CC_COMMAND_SPL:       /* Set pen location */
      /* Set pen location - row, column address within the current window */
      decoder->output_ignore = 2;       /* 2 byte parameter = row, col */
      window->pen_row = dtvcc_buffer[index + 1] & 0xF;
      window->pen_col = dtvcc_buffer[index + 2] & 0x3F;
      GST_LOG ("Pen location: row=%d col=%d", window->pen_row, window->pen_col);
      return;

    case CC_COMMAND_SWA:       /* Set window attributes */
      /* Set window attributes - color, word wrap, border, scroll effect, etc */
      decoder->output_ignore = 4;       /* 4 byte parameter = window attributes */
      gst_cea708dec_set_window_attributes (decoder, dtvcc_buffer, index + 1);
      return;

    case CC_COMMAND_DF0:       /* Define window */
    case CC_COMMAND_DF1:
    case CC_COMMAND_DF2:
    case CC_COMMAND_DF3:
    case CC_COMMAND_DF4:
    case CC_COMMAND_DF5:
    case CC_COMMAND_DF6:
    case CC_COMMAND_DF7:
    {
      window_list = 0xFF;       /* all windows... */

      /* set window - size, style, pen style, anchor position, etc. */
      decoder->output_ignore = 6;       /* 6 byte parameter = window definition */
      decoder->current_window = c & 0x7;
      gst_cea708dec_define_window (decoder, dtvcc_buffer, index + 1);
      return;
    }
  }
}

static void
get_cea708dec_bufcat (gpointer data, gpointer whole_buf)
{
  gchar *buf = whole_buf;
  strcat ((gchar *) buf, data);
  g_free (data);
}

static gboolean
gst_cea708dec_render_text (Cea708Dec * decoder, GSList ** text_list,
    gint length, guint window_id)
{
  gchar *out_str = NULL;
  PangoAlignment align_mode;
  PangoFontDescription *desc;
  gchar *font_desc;
  cea708Window *window = decoder->cc_windows[window_id];

  if (length > 0) {
    out_str = g_malloc0 (length + 1);
    memset (out_str, 0, length + 1);

    g_slist_foreach (*text_list, get_cea708dec_bufcat, out_str);
    GST_LOG ("rendering '%s'", out_str);
    g_slist_free (*text_list);
    window->layout = pango_layout_new (decoder->pango_context);
    align_mode = gst_cea708dec_get_align_mode (window->justify_mode);
    pango_layout_set_alignment (window->layout, (PangoAlignment) align_mode);
    pango_layout_set_markup (window->layout, out_str, length);
    if (!decoder->default_font_desc)
      font_desc = g_strdup_printf ("%s %s", font_names[0], pen_size_names[1]);
    else
      font_desc = g_strdup (decoder->default_font_desc);
    desc = pango_font_description_from_string (font_desc);
    if (desc) {
      GST_INFO ("font description set: %s", font_desc);
      pango_layout_set_font_description (window->layout, desc);
      gst_cea708dec_adjust_values_with_fontdesc (window, desc);
      pango_font_description_free (desc);
      gst_cea708dec_render_pangocairo (window);
    } else {
      GST_ERROR ("font description parse failed: %s", font_desc);
    }
    g_free (font_desc);
    g_free (out_str);
    /* data freed in slist loop!
     *g_slist_free_full (*text_list, g_free); */
    *text_list = NULL;
    return TRUE;
  }

  return FALSE;
}

static void
gst_cea708dec_window_add_char (Cea708Dec * decoder, gunichar c)
{
  cea708Window *window = decoder->cc_windows[decoder->current_window];
  gint16 pen_row;
  gint16 pen_col;

  /* Add one character to the current window, using current pen location.
   * Wrap pen location if necessary */
  if (c == 0)                   /* NULL */
    return;

  if (c == 0x0E) {              /* HCR,moves the pen location to the beginning of the current line and deletes its contents */
    for (pen_col = window->pen_col; pen_col >= 0; pen_col--) {
      window->text[window->pen_row][pen_col].c = ' ';
    }
    window->pen_col = 0;
    return;
  }

  if (c == 0x08) {              /* BS */
    switch (window->print_direction) {
      case PRINT_DIR_LEFT_TO_RIGHT:
        if (window->pen_col) {
          window->pen_col--;
        }
        break;

      case PRINT_DIR_RIGHT_TO_LEFT:
        window->pen_col++;
        break;

      case PRINT_DIR_TOP_TO_BOTTOM:
        if (window->pen_row) {
          window->pen_row--;
        }
        break;

      case PRINT_DIR_BOTTOM_TO_TOP:
        window->pen_row++;
        break;
    }
    pen_row = window->pen_row;
    pen_col = window->pen_col;
    window->text[pen_row][pen_col].c = ' ';
    return;
  }

  if (c == 0x0C) {              /* FF clears the screen and moves the pen location to (0,0) */
    window->pen_row = 0;
    window->pen_col = 0;
    gst_cea708dec_clear_window_text (decoder, decoder->current_window);
    return;
  }

  if (c == 0x0D) {
    GST_DEBUG
        ("carriage return, window->word_wrap=%d,window->scroll_direction=%d",
        window->word_wrap, window->scroll_direction);
    window->pen_col = 0;
    window->pen_row++;
  }

  if (window->pen_col >= window->column_count) {
    window->pen_col = 0;
    window->pen_row++;
  }
  /* Wrap row position if too large */
  if (window->pen_row >= window->row_count) {
    if (window->scroll_direction == SCROLL_DIR_BOTTOM_TO_TOP) {
      gst_cea708dec_scroll_window_up (decoder, decoder->current_window);
    }
    window->pen_row = window->row_count - 1;
    GST_WARNING ("pen row exceed window row count,scroll up");
  }

  if ((c != '\r') && (c != '\n')) {
    pen_row = window->pen_row;
    pen_col = window->pen_col;

    GST_LOG ("[text x=%d y=%d fgcolor=%d win=%d vis=%d] '%c' 0x%02X", pen_col,
        pen_row, window->pen_color.fg_color, decoder->current_window,
        window->visible, c, c);

    /* Each cell in the window should get the current pen color and
     * attributes as it is written */
    window->text[pen_row][pen_col].c = c;
    window->text[pen_row][pen_col].justify_mode = window->justify_mode;
    window->text[pen_row][pen_col].pen_color = window->pen_color;
    window->text[pen_row][pen_col].pen_attributes = window->pen_attributes;

    switch (window->print_direction) {
      case PRINT_DIR_LEFT_TO_RIGHT:
        window->pen_col++;
        break;

      case PRINT_DIR_RIGHT_TO_LEFT:
        if (window->pen_col) {
          window->pen_col--;
        }
        break;

      case PRINT_DIR_TOP_TO_BOTTOM:
        window->pen_row++;
        break;

      case PRINT_DIR_BOTTOM_TO_TOP:
        if (window->pen_row) {
          window->pen_row--;
        }
        break;
    }                           /* switch (print_direction) */
  }
}

static void
gst_cea708dec_process_c2 (Cea708Dec * decoder, guint8 * dtvcc_buffer, int index)
{
  guint8 c = dtvcc_buffer[index];
  if (c >= 0x00 && c <= 0x07) {
    decoder->output_ignore = 1;
  } else if (c >= 0x08 && c <= 0x0F) {
    decoder->output_ignore = 2;
  } else if (c >= 0x10 && c <= 0x17) {
    decoder->output_ignore = 3;
  } else if (c >= 0x18 && c <= 0x1F) {
    decoder->output_ignore = 4;
  }
}

static void
gst_cea708dec_process_g2 (Cea708Dec * decoder, guint8 * dtvcc_buffer, int index)
{
  guint8 c = dtvcc_buffer[index];
  gst_cea708dec_window_add_char (decoder, g2_table[c - 0x20]);
  decoder->output_ignore = 1;
}

static void
gst_cea708dec_process_c3 (Cea708Dec * decoder, guint8 * dtvcc_buffer, int index)
{
  guint8 c = dtvcc_buffer[index];
  int command_length = 0;
  if (c >= 0x80 && c <= 0x87) {
    decoder->output_ignore = 5;
  } else if (c >= 0x88 && c <= 0x8F) {
    decoder->output_ignore = 6;
  } else if (c >= 0x90 && c <= 0x9F) {
    command_length = dtvcc_buffer[index + 1] & 0x3F;
    decoder->output_ignore = command_length + 2;
  }
}

static void
gst_cea708dec_process_g3 (Cea708Dec * decoder, guint8 * dtvcc_buffer, int index)
{
  gst_cea708dec_window_add_char (decoder, 0x5F);
  decoder->output_ignore = 1;
}

void
gst_cea708dec_set_video_width_height (Cea708Dec * decoder, gint width,
    gint height)
{
  decoder->width = width;
  decoder->height = height;
}
