/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstspacescope.c: simple stereo visualizer
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
 * SECTION:element-spacescope
 * @title: spacescope
 * @see_also: goom
 *
 * Spacescope is a simple audio visualisation element. It maps the left and
 * right channel to x and y coordinates.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! spacescope ! ximagesink
 * ]|
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "gstspacescope.h"

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define RGB_ORDER "xRGB"
#else
#define RGB_ORDER "BGRx"
#endif

static GstStaticPadTemplate gst_space_scope_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (RGB_ORDER))
    );

static GstStaticPadTemplate gst_space_scope_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 8000, 96000 ], "
        "channels = (int) 2, " "channel-mask = (bitmask) 0x3")
    );


GST_DEBUG_CATEGORY_STATIC (space_scope_debug);
#define GST_CAT_DEFAULT space_scope_debug

enum
{
  PROP_0,
  PROP_STYLE
};

enum
{
  STYLE_DOTS = 0,
  STYLE_LINES,
  STYLE_COLOR_DOTS,
  STYLE_COLOR_LINES,
  NUM_STYLES
};

#define GST_TYPE_SPACE_SCOPE_STYLE (gst_space_scope_style_get_type ())
static GType
gst_space_scope_style_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {STYLE_DOTS, "draw dots (default)", "dots"},
      {STYLE_LINES, "draw lines", "lines"},
      {STYLE_COLOR_DOTS, "draw color dots", "color-dots"},
      {STYLE_COLOR_LINES, "draw color lines", "color-lines"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstSpaceScopeStyle", values);
  }
  return gtype;
}

static void gst_space_scope_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_space_scope_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void render_dots (GstAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples);
static void render_lines (GstAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples);
static void render_color_dots (GstAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples);
static void render_color_lines (GstAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples);

static gboolean gst_space_scope_render (GstAudioVisualizer * scope,
    GstBuffer * audio, GstVideoFrame * video);


G_DEFINE_TYPE_WITH_CODE (GstSpaceScope, gst_space_scope,
    GST_TYPE_AUDIO_VISUALIZER, GST_DEBUG_CATEGORY_INIT (space_scope_debug,
        "spacescope", 0, "spacescope"););
GST_ELEMENT_REGISTER_DEFINE (spacescope, "spacescope", GST_RANK_NONE,
    GST_TYPE_SPACE_SCOPE);

static void
gst_space_scope_class_init (GstSpaceScopeClass * g_class)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstElementClass *element_class = (GstElementClass *) g_class;
  GstAudioVisualizerClass *scope_class = (GstAudioVisualizerClass *) g_class;

  gst_element_class_set_static_metadata (element_class, "Stereo visualizer",
      "Visualization",
      "Simple stereo visualizer", "Stefan Kost <ensonic@users.sf.net>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_space_scope_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_space_scope_sink_template);

  gobject_class->set_property = gst_space_scope_set_property;
  gobject_class->get_property = gst_space_scope_get_property;

  scope_class->render = GST_DEBUG_FUNCPTR (gst_space_scope_render);

  g_object_class_install_property (gobject_class, PROP_STYLE,
      g_param_spec_enum ("style", "drawing style",
          "Drawing styles for the space scope display.",
          GST_TYPE_SPACE_SCOPE_STYLE, STYLE_DOTS,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_SPACE_SCOPE_STYLE, 0);
}

static void
gst_space_scope_init (GstSpaceScope * scope)
{
  /* do nothing */
}

static void
gst_space_scope_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSpaceScope *scope = GST_SPACE_SCOPE (object);

  switch (prop_id) {
    case PROP_STYLE:
      scope->style = g_value_get_enum (value);
      switch (scope->style) {
        case STYLE_DOTS:
          scope->process = render_dots;
          break;
        case STYLE_LINES:
          scope->process = render_lines;
          break;
        case STYLE_COLOR_DOTS:
          scope->process = render_color_dots;
          break;
        case STYLE_COLOR_LINES:
          scope->process = render_color_lines;
          break;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_space_scope_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSpaceScope *scope = GST_SPACE_SCOPE (object);

  switch (prop_id) {
    case PROP_STYLE:
      g_value_set_enum (value, scope->style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#include "gstdrawhelpers.h"

static void
render_dots (GstAudioVisualizer * base, guint32 * vdata, gint16 * adata,
    guint num_samples)
{
  guint i, s, x, y, ox, oy;
  gfloat dx, dy;
  guint w = GST_VIDEO_INFO_WIDTH (&base->vinfo);
  guint h = GST_VIDEO_INFO_HEIGHT (&base->vinfo);

  /* draw dots 1st channel x, 2nd channel y */
  dx = w / 65536.0;
  ox = w / 2;
  dy = h / 65536.0;
  oy = h / 2;
  s = 0;
  for (i = 0; i < num_samples; i++) {
    x = (guint) (ox + (gfloat) adata[s++] * dx);
    y = (guint) (oy + (gfloat) adata[s++] * dy);
    draw_dot (vdata, x, y, w, 0x00FFFFFF);
  }
}

static void
render_lines (GstAudioVisualizer * base, guint32 * vdata, gint16 * adata,
    guint num_samples)
{
  guint i, s, x, y, ox, oy;
  gfloat dx, dy;
  guint w = GST_VIDEO_INFO_WIDTH (&base->vinfo);
  guint h = GST_VIDEO_INFO_HEIGHT (&base->vinfo);
  gint x2, y2;

  /* draw lines 1st channel x, 2nd channel y */
  dx = (w - 1) / 65536.0;
  ox = (w - 1) / 2;
  dy = (h - 1) / 65536.0;
  oy = (h - 1) / 2;
  s = 0;
  x2 = (guint) (ox + (gfloat) adata[s++] * dx);
  y2 = (guint) (oy + (gfloat) adata[s++] * dy);
  for (i = 1; i < num_samples; i++) {
    x = (guint) (ox + (gfloat) adata[s++] * dx);
    y = (guint) (oy + (gfloat) adata[s++] * dy);
    draw_line_aa (vdata, x2, x, y2, y, w, 0x00FFFFFF);
    x2 = x;
    y2 = y;
  }
}

#define CUTOFF_1 0.15
#define CUTOFF_2 0.45
#define RESONANCE (1.0/0.5)

#define filter(il, ir) G_STMT_START {                                          \
  f1l_h = il - (f1l_m * RESONANCE) - f1l_l;                                    \
  f1l_m += (f1l_h * CUTOFF_1);                                                 \
  f1l_l += (f1l_m * CUTOFF_1);                                                 \
                                                                               \
  f2l_h = (f1l_m + f1l_h) - (f2l_m * RESONANCE) - f2l_l;                       \
  f2l_m += (f2l_h * CUTOFF_2);                                                 \
  f2l_l += (f2l_m * CUTOFF_2);                                                 \
                                                                               \
  f1r_h = ir - (f1r_m * RESONANCE) - f1r_l;                                    \
  f1r_m += (f1r_h * CUTOFF_1);                                                 \
  f1r_l += (f1r_m * CUTOFF_1);                                                 \
                                                                               \
  f2r_h = (f1r_m + f1r_h) - (f2r_m * RESONANCE) - f2r_l;                       \
  f2r_m += (f2r_h * CUTOFF_2);                                                 \
  f2r_l += (f2r_m * CUTOFF_2);                                                 \
} G_STMT_END

static void
render_color_dots (GstAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples)
{
  GstSpaceScope *scope = (GstSpaceScope *) base;
  guint i, s;
  gint x, y, ox, oy;
  gfloat dx, dy;
  gint w = GST_VIDEO_INFO_WIDTH (&base->vinfo), w1 = w - 2;
  gint h = GST_VIDEO_INFO_HEIGHT (&base->vinfo), h1 = h - 2;
  gdouble il, ir;
  gdouble f1l_l = scope->f1l_l, f1l_m = scope->f1l_m, f1l_h = scope->f1l_h;
  gdouble f1r_l = scope->f1r_l, f1r_m = scope->f1r_m, f1r_h = scope->f1r_h;
  gdouble f2l_l = scope->f2l_l, f2l_m = scope->f2l_m, f2l_h = scope->f2l_h;
  gdouble f2r_l = scope->f2r_l, f2r_m = scope->f2r_m, f2r_h = scope->f2r_h;

  /* draw dots 1st channel x, 2nd channel y */
  ox = w / 2;
  oy = h / 2;
  dx = w / 65536.0;
  dy = h / 65536.0;
  s = 0;
  for (i = 0; i < num_samples; i++) {
    il = (gdouble) adata[s++];
    ir = (gdouble) adata[s++];

    filter (il, ir);

    x = (gint) (ox + f1l_l * dx);
    y = (gint) (oy + f1r_l * dy);
    x = CLAMP (x, 0, w1);
    y = CLAMP (y, 0, h1);
    draw_dot_c (vdata, x, y, w, 0x00FF0000);

    x = (gint) (ox + f2l_l * dx);
    y = (gint) (oy + f2r_l * dy);
    x = CLAMP (x, 0, w1);
    y = CLAMP (y, 0, h1);
    draw_dot_c (vdata, x, y, w, 0x0000FF00);

    x = (gint) (ox + (f2l_m + f2l_h) * dx);
    y = (gint) (oy + (f2r_m + f2r_h) * dy);
    x = CLAMP (x, 0, w1);
    y = CLAMP (y, 0, h1);
    draw_dot_c (vdata, x, y, w, 0x000000FF);
  }

  scope->f1l_l = f1l_l;
  scope->f1l_m = f1l_m;
  scope->f1l_h = f1l_h;
  scope->f1r_l = f1r_l;
  scope->f1r_m = f1r_m;
  scope->f1r_h = f1r_h;
  scope->f2l_l = f2l_l;
  scope->f2l_m = f2l_m;
  scope->f2l_h = f2l_h;
  scope->f2r_l = f2r_l;
  scope->f2r_m = f2r_m;
  scope->f2r_h = f2r_h;
}

static void
render_color_lines (GstAudioVisualizer * base, guint32 * vdata,
    gint16 * adata, guint num_samples)
{
  GstSpaceScope *scope = (GstSpaceScope *) base;
  guint i, s;
  gint x, y, ox, oy;
  gfloat dx, dy;
  gint w = GST_VIDEO_INFO_WIDTH (&base->vinfo), w1 = w - 2;
  gint h = GST_VIDEO_INFO_HEIGHT (&base->vinfo), h1 = h - 2;
  gdouble il, ir;
  gdouble f1l_l = scope->f1l_l, f1l_m = scope->f1l_m, f1l_h = scope->f1l_h;
  gdouble f1r_l = scope->f1r_l, f1r_m = scope->f1r_m, f1r_h = scope->f1r_h;
  gdouble f2l_l = scope->f2l_l, f2l_m = scope->f2l_m, f2l_h = scope->f2l_h;
  gdouble f2r_l = scope->f2r_l, f2r_m = scope->f2r_m, f2r_h = scope->f2r_h;
  gint x2, y2, x3, y3, x4, y4;

  /* draw lines 1st channel x, 2nd channel y */
  ox = w / 2;
  oy = h / 2;
  dx = w / 65536.0;
  dy = h / 65536.0;
  s = 0;

  /* do first pixels */
  il = (gdouble) adata[s++];
  ir = (gdouble) adata[s++];

  filter (il, ir);

  x = (gint) (ox + f1l_l * dx);
  y = (gint) (oy + f1r_l * dy);
  x2 = CLAMP (x, 0, w1);
  y2 = CLAMP (y, 0, h1);

  x = (gint) (ox + f2l_l * dx);
  y = (gint) (oy + f2r_l * dy);
  x3 = CLAMP (x, 0, w1);
  y3 = CLAMP (y, 0, h1);

  x = (gint) (ox + (f2l_m + f2l_h) * dx);
  y = (gint) (oy + (f2r_m + f2r_h) * dy);
  x4 = CLAMP (x, 0, w1);
  y4 = CLAMP (y, 0, h1);

  for (i = 1; i < num_samples; i++) {
    il = (gdouble) adata[s++];
    ir = (gdouble) adata[s++];

    filter (il, ir);

    x = (gint) (ox + f1l_l * dx);
    y = (gint) (oy + f1r_l * dy);
    x = CLAMP (x, 0, w1);
    y = CLAMP (y, 0, h1);
    draw_line_aa (vdata, x2, x, y2, y, w, 0x00FF0000);
    x2 = x;
    y2 = y;

    x = (gint) (ox + f2l_l * dx);
    y = (gint) (oy + f2r_l * dy);
    x = CLAMP (x, 0, w1);
    y = CLAMP (y, 0, h1);
    draw_line_aa (vdata, x3, x, y3, y, w, 0x0000FF00);
    x3 = x;
    y3 = y;

    x = (gint) (ox + (f2l_m + f2l_h) * dx);
    y = (gint) (oy + (f2r_m + f2r_h) * dy);
    x = CLAMP (x, 0, w1);
    y = CLAMP (y, 0, h1);
    draw_line_aa (vdata, x4, x, y4, y, w, 0x000000FF);
    x4 = x;
    y4 = y;
  }

  scope->f1l_l = f1l_l;
  scope->f1l_m = f1l_m;
  scope->f1l_h = f1l_h;
  scope->f1r_l = f1r_l;
  scope->f1r_m = f1r_m;
  scope->f1r_h = f1r_h;
  scope->f2l_l = f2l_l;
  scope->f2l_m = f2l_m;
  scope->f2l_h = f2l_h;
  scope->f2r_l = f2r_l;
  scope->f2r_m = f2r_m;
  scope->f2r_h = f2r_h;
}

static gboolean
gst_space_scope_render (GstAudioVisualizer * base, GstBuffer * audio,
    GstVideoFrame * video)
{
  GstSpaceScope *scope = GST_SPACE_SCOPE (base);
  GstMapInfo amap;
  guint num_samples;

  gst_buffer_map (audio, &amap, GST_MAP_READ);

  num_samples =
      amap.size / (GST_AUDIO_INFO_CHANNELS (&base->ainfo) * sizeof (gint16));
  scope->process (base, (guint32 *) GST_VIDEO_FRAME_PLANE_DATA (video, 0),
      (gint16 *) amap.data, num_samples);
  gst_buffer_unmap (audio, &amap);
  return TRUE;
}
