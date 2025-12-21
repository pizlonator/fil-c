/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
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


/**
 * SECTION:element-textrender
 * @title: textrender
 * @see_also: #GstTextOverlay
 *
 * This plugin renders text received on the text sink pad to a video
 * buffer (retaining the alpha channel), so it can later be overlayed
 * on top of video streams using other elements.
 *
 * The text can contain newline characters. (FIXME: What about text
 * wrapping? It does not make sense in this context)
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v filesrc location=subtitles.srt ! subparse ! textrender ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gsttextrender.h"
#include "gstpangoelements.h"


#if G_BYTE_ORDER == G_LITTLE_ENDIAN
# define CAIRO_ARGB_A 3
# define CAIRO_ARGB_R 2
# define CAIRO_ARGB_G 1
# define CAIRO_ARGB_B 0
#else
# define CAIRO_ARGB_A 0
# define CAIRO_ARGB_R 1
# define CAIRO_ARGB_G 2
# define CAIRO_ARGB_B 3
#endif

GST_DEBUG_CATEGORY_EXTERN (pango_debug);
#define GST_CAT_DEFAULT pango_debug

#define MINIMUM_OUTLINE_OFFSET 1.0

#define DEFAULT_PROP_VALIGNMENT GST_TEXT_RENDER_VALIGN_BASELINE
#define DEFAULT_PROP_HALIGNMENT GST_TEXT_RENDER_HALIGN_CENTER
#define DEFAULT_PROP_LINE_ALIGNMENT GST_TEXT_RENDER_LINE_ALIGN_CENTER
#define DEFAULT_PROP_XPAD       25
#define DEFAULT_PROP_YPAD       25

#define DEFAULT_RENDER_WIDTH 720
#define DEFAULT_RENDER_HEIGHT 576

enum
{
  PROP_0,
  PROP_HALIGNMENT,
  PROP_VALIGNMENT,
  PROP_LINE_ALIGNMENT,
  PROP_XPAD,
  PROP_YPAD,
  PROP_FONT_DESC
};

#define VIDEO_FORMATS "{ AYUV, ARGB } "

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS))
    );

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format = { pango-markup, utf8 }")
    );

#define GST_TYPE_TEXT_RENDER_VALIGN (gst_text_render_valign_get_type())
static GType
gst_text_render_valign_get_type (void)
{
  static GType text_render_valign_type = 0;
  static const GEnumValue text_render_valign[] = {
    {GST_TEXT_RENDER_VALIGN_BASELINE, "baseline", "baseline"},
    {GST_TEXT_RENDER_VALIGN_BOTTOM, "bottom", "bottom"},
    {GST_TEXT_RENDER_VALIGN_TOP, "top", "top"},
    {0, NULL, NULL},
  };

  if (!text_render_valign_type) {
    text_render_valign_type =
        g_enum_register_static ("GstTextRenderVAlign", text_render_valign);
  }
  return text_render_valign_type;
}

#define GST_TYPE_TEXT_RENDER_HALIGN (gst_text_render_halign_get_type())
static GType
gst_text_render_halign_get_type (void)
{
  static GType text_render_halign_type = 0;
  static const GEnumValue text_render_halign[] = {
    {GST_TEXT_RENDER_HALIGN_LEFT, "left", "left"},
    {GST_TEXT_RENDER_HALIGN_CENTER, "center", "center"},
    {GST_TEXT_RENDER_HALIGN_RIGHT, "right", "right"},
    {0, NULL, NULL},
  };

  if (!text_render_halign_type) {
    text_render_halign_type =
        g_enum_register_static ("GstTextRenderHAlign", text_render_halign);
  }
  return text_render_halign_type;
}

#define GST_TYPE_TEXT_RENDER_LINE_ALIGN (gst_text_render_line_align_get_type())
static GType
gst_text_render_line_align_get_type (void)
{
  static GType text_render_line_align_type = 0;
  static const GEnumValue text_render_line_align[] = {
    {GST_TEXT_RENDER_LINE_ALIGN_LEFT, "left", "left"},
    {GST_TEXT_RENDER_LINE_ALIGN_CENTER, "center", "center"},
    {GST_TEXT_RENDER_LINE_ALIGN_RIGHT, "right", "right"},
    {0, NULL, NULL}
  };

  if (!text_render_line_align_type) {
    text_render_line_align_type =
        g_enum_register_static ("GstTextRenderLineAlign",
        text_render_line_align);
  }
  return text_render_line_align_type;
}

static void gst_text_render_adjust_values_with_fontdesc (GstTextRender *
    render, PangoFontDescription * desc);

#define gst_text_render_parent_class parent_class
G_DEFINE_TYPE (GstTextRender, gst_text_render, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (textrender, "textrender",
    GST_RANK_NONE, GST_TYPE_TEXT_RENDER, pango_element_init (plugin));

static void gst_text_render_finalize (GObject * object);
static void gst_text_render_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_text_render_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void
gst_text_render_class_init (GstTextRenderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_text_render_finalize;
  gobject_class->set_property = gst_text_render_set_property;
  gobject_class->get_property = gst_text_render_get_property;

  gst_element_class_add_static_pad_template (gstelement_class,
      &src_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &sink_template_factory);

  gst_element_class_set_static_metadata (gstelement_class, "Text renderer",
      "Filter/Editor/Video",
      "Renders a text string to an image bitmap",
      "David Schleef <ds@schleef.org>, "
      "GStreamer maintainers <gstreamer-devel@lists.freedesktop.org>");

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT_DESC,
      g_param_spec_string ("font-desc", "font description",
          "Pango font description of font "
          "to be used for rendering. "
          "See documentation of "
          "pango_font_description_from_string"
          " for syntax.", "", G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_VALIGNMENT,
      g_param_spec_enum ("valignment", "vertical alignment",
          "Vertical alignment of the text", GST_TYPE_TEXT_RENDER_VALIGN,
          DEFAULT_PROP_VALIGNMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HALIGNMENT,
      g_param_spec_enum ("halignment", "horizontal alignment",
          "Horizontal alignment of the text", GST_TYPE_TEXT_RENDER_HALIGN,
          DEFAULT_PROP_HALIGNMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_XPAD,
      g_param_spec_int ("xpad", "horizontal paddding",
          "Horizontal paddding when using left/right alignment", 0, G_MAXINT,
          DEFAULT_PROP_XPAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_YPAD,
      g_param_spec_int ("ypad", "vertical padding",
          "Vertical padding when using top/bottom alignment", 0, G_MAXINT,
          DEFAULT_PROP_YPAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LINE_ALIGNMENT,
      g_param_spec_enum ("line-alignment", "line alignment",
          "Alignment of text lines relative to each other.",
          GST_TYPE_TEXT_RENDER_LINE_ALIGN, DEFAULT_PROP_LINE_ALIGNMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_TEXT_RENDER_HALIGN, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_TEXT_RENDER_VALIGN, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_TEXT_RENDER_LINE_ALIGN, 0);
}

static void
gst_text_render_adjust_values_with_fontdesc (GstTextRender * render,
    PangoFontDescription * desc)
{
  gint font_size = pango_font_description_get_size (desc) / PANGO_SCALE;

  render->shadow_offset = (double) (font_size) / 13.0;
  render->outline_offset = (double) (font_size) / 15.0;
  if (render->outline_offset < MINIMUM_OUTLINE_OFFSET)
    render->outline_offset = MINIMUM_OUTLINE_OFFSET;
}

static void
gst_text_render_render_pangocairo (GstTextRender * render)
{
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_t *cr_shadow;
  cairo_surface_t *surface_shadow;
  PangoRectangle ink_rect, logical_rect;
  gint width, height;

  pango_layout_get_pixel_extents (render->layout, &ink_rect, &logical_rect);

  width = logical_rect.width + render->shadow_offset;
  height = logical_rect.height + logical_rect.y + render->shadow_offset;

  surface_shadow = cairo_image_surface_create (CAIRO_FORMAT_A8, width, height);
  cr_shadow = cairo_create (surface_shadow);

  /* clear shadow surface */
  cairo_set_operator (cr_shadow, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr_shadow);
  cairo_set_operator (cr_shadow, CAIRO_OPERATOR_OVER);

  /* draw shadow text */
  cairo_save (cr_shadow);
  cairo_set_source_rgba (cr_shadow, 0.0, 0.0, 0.0, 0.5);
  cairo_translate (cr_shadow, render->shadow_offset, render->shadow_offset);
  pango_cairo_show_layout (cr_shadow, render->layout);
  cairo_restore (cr_shadow);

  /* draw outline text */
  cairo_save (cr_shadow);
  cairo_set_source_rgb (cr_shadow, 0.0, 0.0, 0.0);
  cairo_set_line_width (cr_shadow, render->outline_offset);
  pango_cairo_layout_path (cr_shadow, render->layout);
  cairo_stroke (cr_shadow);
  cairo_restore (cr_shadow);

  cairo_destroy (cr_shadow);

  render->text_image = g_realloc (render->text_image, 4 * width * height);

  surface = cairo_image_surface_create_for_data (render->text_image,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* set default color */
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

  cairo_save (cr);
  /* draw text */
  pango_cairo_show_layout (cr, render->layout);
  cairo_restore (cr);

  /* composite shadow with offset */
  cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
  cairo_set_source_surface (cr, surface_shadow, 0.0, 0.0);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface_shadow);
  cairo_surface_destroy (surface);
  render->image_width = width;
  render->image_height = height;
}

static void
gst_text_render_check_argb (GstTextRender * render)
{
  GstCaps *peer_caps;
  peer_caps = gst_pad_get_allowed_caps (render->srcpad);
  if (G_LIKELY (peer_caps)) {
    guint i = 0, n = 0;

    n = gst_caps_get_size (peer_caps);
    GST_DEBUG_OBJECT (render, "peer allowed caps (%u structure(s)) are %"
        GST_PTR_FORMAT, n, peer_caps);

    /* Check if AYUV or ARGB is first */
    for (i = 0; i < n; i++) {
      GstStructure *s;
      GstVideoFormat vformat;
      const GstVideoFormatInfo *info;
      const gchar *fmt;

      s = gst_caps_get_structure (peer_caps, i);
      if (!gst_structure_has_name (s, "video/x-raw"))
        continue;

      fmt = gst_structure_get_string (s, "format");
      if (fmt == NULL)
        continue;

      vformat = gst_video_format_from_string (fmt);
      info = gst_video_format_get_info (vformat);
      if (info == NULL)
        continue;

      render->use_ARGB = GST_VIDEO_FORMAT_INFO_IS_RGB (info);
    }
    gst_caps_unref (peer_caps);
  }
}

static gboolean
gst_text_render_src_setcaps (GstTextRender * render, GstCaps * caps)
{
  GstStructure *structure;
  gboolean ret;
  gint width = 0, height = 0;

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);

  GST_DEBUG_OBJECT (render, "Got caps %" GST_PTR_FORMAT, caps);

  if (width >= render->image_width && height >= render->image_height) {
    render->width = width;
    render->height = height;
  }

  gst_text_render_check_argb (render);

  ret = gst_pad_set_caps (render->srcpad, caps);

  return ret;
}

static GstCaps *
gst_text_render_fixate_caps (GstTextRender * render, GstCaps * caps)
{
  GstStructure *s;

  caps = gst_caps_truncate (caps);

  caps = gst_caps_make_writable (caps);
  s = gst_caps_get_structure (caps, 0);

  GST_DEBUG ("Fixating caps %" GST_PTR_FORMAT, caps);
  gst_structure_fixate_field_nearest_int (s, "width", MAX (render->image_width,
          DEFAULT_RENDER_WIDTH));
  gst_structure_fixate_field_nearest_int (s, "height",
      MAX (render->image_height + render->ypad, DEFAULT_RENDER_HEIGHT));
  caps = gst_caps_fixate (caps);
  GST_DEBUG ("Fixated to    %" GST_PTR_FORMAT, caps);

  return caps;
}

#define CAIRO_UNPREMULTIPLY(a,r,g,b) G_STMT_START { \
  b = (a > 0) ? MIN ((b * 255 + a / 2) / a, 255) : 0; \
  g = (a > 0) ? MIN ((g * 255 + a / 2) / a, 255) : 0; \
  r = (a > 0) ? MIN ((r * 255 + a / 2) / a, 255) : 0; \
} G_STMT_END

static void
gst_text_renderer_image_to_ayuv (GstTextRender * render, guchar * pixbuf,
    int xpos, int ypos, int stride)
{
  int y;                        /* text bitmap coordinates */
  guchar *p, *bitp;
  guchar a, r, g, b;
  int width, height;

  width = render->image_width;
  height = render->image_height;

  for (y = 0; y < height && ypos + y < render->height; y++) {
    int n;
    p = pixbuf + (ypos + y) * stride + xpos * 4;
    bitp = render->text_image + y * width * 4;
    for (n = 0; n < width && n < render->width; n++) {
      b = bitp[CAIRO_ARGB_B];
      g = bitp[CAIRO_ARGB_G];
      r = bitp[CAIRO_ARGB_R];
      a = bitp[CAIRO_ARGB_A];
      bitp += 4;

      /* Cairo uses pre-multiplied ARGB, unpremultiply it */
      CAIRO_UNPREMULTIPLY (a, r, g, b);

      *p++ = a;
      *p++ = CLAMP ((int) (((19595 * r) >> 16) + ((38470 * g) >> 16) +
              ((7471 * b) >> 16)), 0, 255);
      *p++ = CLAMP ((int) (-((11059 * r) >> 16) - ((21709 * g) >> 16) +
              ((32768 * b) >> 16) + 128), 0, 255);
      *p++ = CLAMP ((int) (((32768 * r) >> 16) - ((27439 * g) >> 16) -
              ((5329 * b) >> 16) + 128), 0, 255);
    }
  }
}

static void
gst_text_renderer_image_to_argb (GstTextRender * render, guchar * pixbuf,
    int xpos, int ypos, int stride)
{
  int i, j;
  guchar *p, *bitp;
  int width, height;

  width = render->image_width;
  height = render->image_height;

  for (i = 0; i < height && ypos + i < render->height; i++) {
    p = pixbuf + (ypos + i) * stride + xpos * 4;
    bitp = render->text_image + i * width * 4;
    for (j = 0; j < width && j < render->width; j++) {
      p[0] = bitp[CAIRO_ARGB_A];
      p[1] = bitp[CAIRO_ARGB_R];
      p[2] = bitp[CAIRO_ARGB_G];
      p[3] = bitp[CAIRO_ARGB_B];

      /* Cairo uses pre-multiplied ARGB, unpremultiply it */
      CAIRO_UNPREMULTIPLY (p[0], p[1], p[2], p[3]);

      bitp += 4;
      p += 4;
    }
  }
}

static GstFlowReturn
gst_text_render_renegotiate (GstTextRender * render)
{
  GstCaps *caps = NULL, *padcaps;
  GstFlowReturn ret = GST_FLOW_OK;

  gst_text_render_check_argb (render);

  padcaps = gst_pad_query_caps (render->srcpad, NULL);
  caps = gst_pad_peer_query_caps (render->srcpad, padcaps);
  gst_caps_unref (padcaps);

  if (!caps || gst_caps_is_empty (caps)) {
    GST_ELEMENT_ERROR (render, CORE, NEGOTIATION, (NULL), (NULL));
    ret = GST_FLOW_ERROR;
    goto done;
  }

  caps = gst_text_render_fixate_caps (render, caps);

  if (!gst_text_render_src_setcaps (render, caps)) {
    GST_ELEMENT_ERROR (render, CORE, NEGOTIATION, (NULL), (NULL));
    ret = GST_FLOW_ERROR;
    goto done;
  }

done:
  if (caps)
    gst_caps_unref (caps);
  return ret;
}

static GstFlowReturn
gst_text_render_chain (GstPad * pad, GstObject * parent, GstBuffer * inbuf)
{
  GstTextRender *render;
  GstFlowReturn ret;
  GstBuffer *outbuf;
  GstMapInfo map;
  guint8 *data;
  gsize size;
  gint n;
  gint xpos, ypos;

  render = GST_TEXT_RENDER (parent);

  gst_buffer_map (inbuf, &map, GST_MAP_READ);
  data = map.data;
  size = map.size;

  /* somehow pango barfs over "\0" buffers... */
  while (size > 0 &&
      (data[size - 1] == '\r' ||
          data[size - 1] == '\n' || data[size - 1] == '\0')) {
    size--;
  }

  /* render text */
  GST_DEBUG ("rendering '%*s'", (gint) size, data);
  if (render->have_pango_markup)
    pango_layout_set_markup (render->layout, (gchar *) data, size);
  else
    pango_layout_set_text (render->layout, (gchar *) data, size);
  gst_text_render_render_pangocairo (render);
  gst_buffer_unmap (inbuf, &map);

  if (gst_pad_check_reconfigure (render->srcpad)
      || !gst_pad_has_current_caps (render->srcpad)) {
    ret = gst_text_render_renegotiate (render);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  if (render->segment_event) {
    gst_pad_push_event (render->srcpad, render->segment_event);
    render->segment_event = NULL;
  }

  GST_DEBUG ("Allocating buffer WxH = %dx%d", render->width, render->height);
  outbuf = gst_buffer_new_and_alloc (render->width * render->height * 4);

  gst_buffer_copy_into (outbuf, inbuf, GST_BUFFER_COPY_TIMESTAMPS, 0, -1);

  gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
  data = map.data;
  size = map.size;

  if (render->use_ARGB) {
    memset (data, 0, render->width * render->height * 4);
  } else {
    for (n = 0; n < render->width * render->height; n++) {
      data[n * 4] = data[n * 4 + 1] = 0;
      data[n * 4 + 2] = data[n * 4 + 3] = 128;
    }
  }

  switch (render->halign) {
    case GST_TEXT_RENDER_HALIGN_LEFT:
      xpos = render->xpad;
      break;
    case GST_TEXT_RENDER_HALIGN_CENTER:
      xpos = (render->width - render->image_width) / 2;
      break;
    case GST_TEXT_RENDER_HALIGN_RIGHT:
      xpos = render->width - render->image_width - render->xpad;
      break;
    default:
      xpos = 0;
  }

  switch (render->valign) {
    case GST_TEXT_RENDER_VALIGN_BOTTOM:
      ypos = render->height - render->image_height - render->ypad;
      break;
    case GST_TEXT_RENDER_VALIGN_BASELINE:
      ypos = render->height - (render->image_height + render->ypad);
      break;
    case GST_TEXT_RENDER_VALIGN_TOP:
      ypos = render->ypad;
      break;
    default:
      ypos = render->ypad;
      break;
  }

  if (render->text_image) {
    if (render->use_ARGB) {
      gst_text_renderer_image_to_argb (render, data, xpos, ypos,
          render->width * 4);
    } else {
      gst_text_renderer_image_to_ayuv (render, data, xpos, ypos,
          render->width * 4);
    }
  }
  gst_buffer_unmap (outbuf, &map);

  ret = gst_pad_push (render->srcpad, outbuf);

done:
  gst_buffer_unref (inbuf);

  return ret;
}

static gboolean
gst_text_render_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTextRender *render = GST_TEXT_RENDER (parent);
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      if (gst_pad_has_current_caps (render->srcpad)) {
        ret = gst_pad_push_event (render->srcpad, event);
      } else {
        gst_event_replace (&render->segment_event, event);
        gst_event_unref (event);
      }
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      GstStructure *structure;
      const gchar *format;

      gst_event_parse_caps (event, &caps);

      structure = gst_caps_get_structure (caps, 0);
      format = gst_structure_get_string (structure, "format");
      render->have_pango_markup = (strcmp (format, "pango-markup") == 0);

      gst_event_unref (event);
      ret = TRUE;

      break;
    }
    case GST_EVENT_GAP:
      /* Negotiate caps first if we negotiated none so far as otherwise
       * downstream wouldn't have received a segment event either and
       * wouldn't know what to do with the gap event */
      if (!gst_pad_has_current_caps (render->srcpad)) {
        if (gst_text_render_renegotiate (render) != GST_FLOW_OK) {
          gst_event_unref (event);
          ret = FALSE;
          break;
        }
      }

      if (render->segment_event) {
        gst_pad_push_event (render->srcpad, render->segment_event);
        render->segment_event = NULL;
      }

      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static void
gst_text_render_finalize (GObject * object)
{
  GstTextRender *render = GST_TEXT_RENDER (object);

  gst_event_replace (&render->segment_event, NULL);

  g_free (render->text_image);

  if (render->layout)
    g_object_unref (render->layout);

  if (render->pango_context)
    g_object_unref (render->pango_context);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_text_render_init (GstTextRender * render)
{
  GstPadTemplate *template;
  PangoFontMap *fontmap;

  /* sink */
  template = gst_static_pad_template_get (&sink_template_factory);
  render->sinkpad = gst_pad_new_from_template (template, "sink");
  gst_object_unref (template);
  gst_pad_set_chain_function (render->sinkpad,
      GST_DEBUG_FUNCPTR (gst_text_render_chain));
  gst_pad_set_event_function (render->sinkpad,
      GST_DEBUG_FUNCPTR (gst_text_render_event));

  gst_element_add_pad (GST_ELEMENT (render), render->sinkpad);

  /* source */
  template = gst_static_pad_template_get (&src_template_factory);
  render->srcpad = gst_pad_new_from_template (template, "src");
  gst_object_unref (template);

  gst_element_add_pad (GST_ELEMENT (render), render->srcpad);

  fontmap = pango_cairo_font_map_new ();
  render->pango_context =
      pango_font_map_create_context (PANGO_FONT_MAP (fontmap));
  g_object_unref (fontmap);

  render->line_align = DEFAULT_PROP_LINE_ALIGNMENT;
  render->layout = pango_layout_new (render->pango_context);
  pango_layout_set_alignment (render->layout,
      (PangoAlignment) render->line_align);

  render->halign = DEFAULT_PROP_HALIGNMENT;
  render->valign = DEFAULT_PROP_VALIGNMENT;
  render->xpad = DEFAULT_PROP_XPAD;
  render->ypad = DEFAULT_PROP_YPAD;

  render->width = DEFAULT_RENDER_WIDTH;
  render->height = DEFAULT_RENDER_HEIGHT;

  render->use_ARGB = FALSE;
}

static void
gst_text_render_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTextRender *render = GST_TEXT_RENDER (object);

  switch (prop_id) {
    case PROP_VALIGNMENT:
      render->valign = g_value_get_enum (value);
      break;
    case PROP_HALIGNMENT:
      render->halign = g_value_get_enum (value);
      break;
    case PROP_LINE_ALIGNMENT:
      render->line_align = g_value_get_enum (value);
      pango_layout_set_alignment (render->layout,
          (PangoAlignment) render->line_align);
      break;
    case PROP_XPAD:
      render->xpad = g_value_get_int (value);
      break;
    case PROP_YPAD:
      render->ypad = g_value_get_int (value);
      break;
    case PROP_FONT_DESC:
    {
      PangoFontDescription *desc;

      desc = pango_font_description_from_string (g_value_get_string (value));
      if (desc) {
        GST_LOG ("font description set: %s", g_value_get_string (value));
        GST_OBJECT_LOCK (render);
        pango_layout_set_font_description (render->layout, desc);
        gst_text_render_adjust_values_with_fontdesc (render, desc);
        pango_font_description_free (desc);
        gst_text_render_render_pangocairo (render);
        GST_OBJECT_UNLOCK (render);
      } else {
        GST_WARNING ("font description parse failed: %s",
            g_value_get_string (value));
      }
      break;
    }

    default:
      break;
  }
}

static void
gst_text_render_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTextRender *render = GST_TEXT_RENDER (object);

  switch (prop_id) {
    case PROP_VALIGNMENT:
      g_value_set_enum (value, render->valign);
      break;
    case PROP_HALIGNMENT:
      g_value_set_enum (value, render->halign);
      break;
    case PROP_LINE_ALIGNMENT:
      g_value_set_enum (value, render->line_align);
      break;
    case PROP_XPAD:
      g_value_set_int (value, render->xpad);
      break;
    case PROP_YPAD:
      g_value_set_int (value, render->ypad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
