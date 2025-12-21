/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include "gstbasetextoverlay.h"
#include "gsttextoverlay.h"
#include "gsttimeoverlay.h"
#include "gstclockoverlay.h"
#include "gsttextrender.h"
#include <string.h>
#include <math.h>

/* FIXME:
 *  - use proper strides and offset for I420
 *  - if text is wider than the video picture, it does not get
 *    clipped properly during blitting (if wrapping is disabled)
 */

#define DEFAULT_PROP_TEXT 	""
#define DEFAULT_PROP_SHADING	FALSE
#define DEFAULT_PROP_VALIGNMENT	GST_BASE_TEXT_OVERLAY_VALIGN_BASELINE
#define DEFAULT_PROP_HALIGNMENT	GST_BASE_TEXT_OVERLAY_HALIGN_CENTER
#define DEFAULT_PROP_XPAD	25
#define DEFAULT_PROP_YPAD	25
#define DEFAULT_PROP_DELTAX	0
#define DEFAULT_PROP_DELTAY	0
#define DEFAULT_PROP_XPOS       0.5
#define DEFAULT_PROP_YPOS       0.5
#define DEFAULT_PROP_WRAP_MODE  GST_BASE_TEXT_OVERLAY_WRAP_MODE_WORD_CHAR
#define DEFAULT_PROP_FONT_DESC	""
#define DEFAULT_PROP_SILENT	FALSE
#define DEFAULT_PROP_LINE_ALIGNMENT GST_BASE_TEXT_OVERLAY_LINE_ALIGN_CENTER
#define DEFAULT_PROP_WAIT_TEXT	TRUE
#define DEFAULT_PROP_AUTO_ADJUST_SIZE TRUE
#define DEFAULT_PROP_VERTICAL_RENDER  FALSE
#define DEFAULT_PROP_SCALE_MODE GST_BASE_TEXT_OVERLAY_SCALE_MODE_NONE
#define DEFAULT_PROP_SCALE_PAR_N 1
#define DEFAULT_PROP_SCALE_PAR_D 1
#define DEFAULT_PROP_DRAW_SHADOW TRUE
#define DEFAULT_PROP_DRAW_OUTLINE TRUE
#define DEFAULT_PROP_COLOR      0xffffffff
#define DEFAULT_PROP_OUTLINE_COLOR 0xff000000
#define DEFAULT_PROP_SHADING_VALUE    80
#define DEFAULT_PROP_TEXT_X 0
#define DEFAULT_PROP_TEXT_Y 0
#define DEFAULT_PROP_TEXT_WIDTH 1
#define DEFAULT_PROP_TEXT_HEIGHT 1

#define MINIMUM_OUTLINE_OFFSET 1.0
#define DEFAULT_SCALE_BASIS    640

enum
{
  PROP_0,
  PROP_TEXT,
  PROP_SHADING,
  PROP_SHADING_VALUE,
  PROP_HALIGNMENT,
  PROP_VALIGNMENT,
  PROP_XPAD,
  PROP_YPAD,
  PROP_DELTAX,
  PROP_DELTAY,
  PROP_XPOS,
  PROP_YPOS,
  PROP_X_ABSOLUTE,
  PROP_Y_ABSOLUTE,
  PROP_WRAP_MODE,
  PROP_FONT_DESC,
  PROP_SILENT,
  PROP_LINE_ALIGNMENT,
  PROP_WAIT_TEXT,
  PROP_AUTO_ADJUST_SIZE,
  PROP_VERTICAL_RENDER,
  PROP_SCALE_MODE,
  PROP_SCALE_PAR,
  PROP_COLOR,
  PROP_DRAW_SHADOW,
  PROP_DRAW_OUTLINE,
  PROP_OUTLINE_COLOR,
  PROP_TEXT_X,
  PROP_TEXT_Y,
  PROP_TEXT_WIDTH,
  PROP_TEXT_HEIGHT,
  PROP_LAST
};

GST_DEBUG_CATEGORY_STATIC (base_text_overlay_debug);
#define GST_CAT_DEFAULT base_text_overlay_debug

#define VIDEO_FORMATS GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS

#define BASE_TEXT_OVERLAY_CAPS GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)

#define BASE_TEXT_OVERLAY_ALL_CAPS BASE_TEXT_OVERLAY_CAPS ";" \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ALL)

static GstStaticCaps sw_template_caps =
GST_STATIC_CAPS (BASE_TEXT_OVERLAY_CAPS);

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (BASE_TEXT_OVERLAY_ALL_CAPS)
    );

static GstStaticPadTemplate video_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (BASE_TEXT_OVERLAY_ALL_CAPS)
    );

#define GST_TYPE_BASE_TEXT_OVERLAY_VALIGN (gst_base_text_overlay_valign_get_type())
static GType
gst_base_text_overlay_valign_get_type (void)
{
  static GType base_text_overlay_valign_type = 0;
  static const GEnumValue base_text_overlay_valign[] = {
    {GST_BASE_TEXT_OVERLAY_VALIGN_BASELINE, "baseline", "baseline"},
    {GST_BASE_TEXT_OVERLAY_VALIGN_BOTTOM, "bottom", "bottom"},
    {GST_BASE_TEXT_OVERLAY_VALIGN_TOP, "top", "top"},
    {GST_BASE_TEXT_OVERLAY_VALIGN_POS, "position",
        "Absolute position clamped to canvas"},
    {GST_BASE_TEXT_OVERLAY_VALIGN_CENTER, "center", "center"},
    {GST_BASE_TEXT_OVERLAY_VALIGN_ABSOLUTE, "absolute", "Absolute position"},
    {0, NULL, NULL},
  };

  if (!base_text_overlay_valign_type) {
    base_text_overlay_valign_type =
        g_enum_register_static ("GstBaseTextOverlayVAlign",
        base_text_overlay_valign);
  }
  return base_text_overlay_valign_type;
}

#define GST_TYPE_BASE_TEXT_OVERLAY_HALIGN (gst_base_text_overlay_halign_get_type())
static GType
gst_base_text_overlay_halign_get_type (void)
{
  static GType base_text_overlay_halign_type = 0;
  static const GEnumValue base_text_overlay_halign[] = {
    {GST_BASE_TEXT_OVERLAY_HALIGN_LEFT, "left", "left"},
    {GST_BASE_TEXT_OVERLAY_HALIGN_CENTER, "center", "center"},
    {GST_BASE_TEXT_OVERLAY_HALIGN_RIGHT, "right", "right"},
    {GST_BASE_TEXT_OVERLAY_HALIGN_POS, "position",
        "Absolute position clamped to canvas"},
    {GST_BASE_TEXT_OVERLAY_HALIGN_ABSOLUTE, "absolute", "Absolute position"},
    {0, NULL, NULL},
  };

  if (!base_text_overlay_halign_type) {
    base_text_overlay_halign_type =
        g_enum_register_static ("GstBaseTextOverlayHAlign",
        base_text_overlay_halign);
  }
  return base_text_overlay_halign_type;
}


#define GST_TYPE_BASE_TEXT_OVERLAY_WRAP_MODE (gst_base_text_overlay_wrap_mode_get_type())
static GType
gst_base_text_overlay_wrap_mode_get_type (void)
{
  static GType base_text_overlay_wrap_mode_type = 0;
  static const GEnumValue base_text_overlay_wrap_mode[] = {
    {GST_BASE_TEXT_OVERLAY_WRAP_MODE_NONE, "none", "none"},
    {GST_BASE_TEXT_OVERLAY_WRAP_MODE_WORD, "word", "word"},
    {GST_BASE_TEXT_OVERLAY_WRAP_MODE_CHAR, "char", "char"},
    {GST_BASE_TEXT_OVERLAY_WRAP_MODE_WORD_CHAR, "wordchar", "wordchar"},
    {0, NULL, NULL},
  };

  if (!base_text_overlay_wrap_mode_type) {
    base_text_overlay_wrap_mode_type =
        g_enum_register_static ("GstBaseTextOverlayWrapMode",
        base_text_overlay_wrap_mode);
  }
  return base_text_overlay_wrap_mode_type;
}

#define GST_TYPE_BASE_TEXT_OVERLAY_LINE_ALIGN (gst_base_text_overlay_line_align_get_type())
static GType
gst_base_text_overlay_line_align_get_type (void)
{
  static GType base_text_overlay_line_align_type = 0;
  static const GEnumValue base_text_overlay_line_align[] = {
    {GST_BASE_TEXT_OVERLAY_LINE_ALIGN_LEFT, "left", "left"},
    {GST_BASE_TEXT_OVERLAY_LINE_ALIGN_CENTER, "center", "center"},
    {GST_BASE_TEXT_OVERLAY_LINE_ALIGN_RIGHT, "right", "right"},
    {0, NULL, NULL}
  };

  if (!base_text_overlay_line_align_type) {
    base_text_overlay_line_align_type =
        g_enum_register_static ("GstBaseTextOverlayLineAlign",
        base_text_overlay_line_align);
  }
  return base_text_overlay_line_align_type;
}

#define GST_TYPE_BASE_TEXT_OVERLAY_SCALE_MODE (gst_base_text_overlay_scale_mode_get_type())
static GType
gst_base_text_overlay_scale_mode_get_type (void)
{
  static GType base_text_overlay_scale_mode_type = 0;
  static const GEnumValue base_text_overlay_scale_mode[] = {
    {GST_BASE_TEXT_OVERLAY_SCALE_MODE_NONE, "none", "none"},
    {GST_BASE_TEXT_OVERLAY_SCALE_MODE_PAR, "par", "par"},
    {GST_BASE_TEXT_OVERLAY_SCALE_MODE_DISPLAY, "display", "display"},
    {GST_BASE_TEXT_OVERLAY_SCALE_MODE_USER, "user", "user"},
    {0, NULL, NULL}
  };

  if (!base_text_overlay_scale_mode_type) {
    base_text_overlay_scale_mode_type =
        g_enum_register_static ("GstBaseTextOverlayScaleMode",
        base_text_overlay_scale_mode);
  }
  return base_text_overlay_scale_mode_type;
}

#define GST_BASE_TEXT_OVERLAY_GET_LOCK(ov) (&GST_BASE_TEXT_OVERLAY (ov)->lock)
#define GST_BASE_TEXT_OVERLAY_GET_COND(ov) (&GST_BASE_TEXT_OVERLAY (ov)->cond)
#define GST_BASE_TEXT_OVERLAY_LOCK(ov)     (g_mutex_lock (GST_BASE_TEXT_OVERLAY_GET_LOCK (ov)))
#define GST_BASE_TEXT_OVERLAY_UNLOCK(ov)   (g_mutex_unlock (GST_BASE_TEXT_OVERLAY_GET_LOCK (ov)))
#define GST_BASE_TEXT_OVERLAY_WAIT(ov)     (g_cond_wait (GST_BASE_TEXT_OVERLAY_GET_COND (ov), GST_BASE_TEXT_OVERLAY_GET_LOCK (ov)))
#define GST_BASE_TEXT_OVERLAY_SIGNAL(ov)   (g_cond_signal (GST_BASE_TEXT_OVERLAY_GET_COND (ov)))
#define GST_BASE_TEXT_OVERLAY_BROADCAST(ov)(g_cond_broadcast (GST_BASE_TEXT_OVERLAY_GET_COND (ov)))

static GstElementClass *parent_class = NULL;
static void gst_base_text_overlay_class_init (GstBaseTextOverlayClass * klass);
static void gst_base_text_overlay_init (GstBaseTextOverlay * overlay,
    GstBaseTextOverlayClass * klass);

static GstStateChangeReturn gst_base_text_overlay_change_state (GstElement *
    element, GstStateChange transition);

static GstCaps *gst_base_text_overlay_get_videosink_caps (GstPad * pad,
    GstBaseTextOverlay * overlay, GstCaps * filter);
static GstCaps *gst_base_text_overlay_get_src_caps (GstPad * pad,
    GstBaseTextOverlay * overlay, GstCaps * filter);
static gboolean gst_base_text_overlay_setcaps (GstBaseTextOverlay * overlay,
    GstCaps * caps);
static gboolean gst_base_text_overlay_setcaps_txt (GstBaseTextOverlay * overlay,
    GstCaps * caps);
static gboolean gst_base_text_overlay_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_base_text_overlay_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static gboolean gst_base_text_overlay_video_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_base_text_overlay_video_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstFlowReturn gst_base_text_overlay_video_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static gboolean gst_base_text_overlay_text_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_base_text_overlay_text_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstPadLinkReturn gst_base_text_overlay_text_pad_link (GstPad * pad,
    GstObject * parent, GstPad * peer);
static void gst_base_text_overlay_text_pad_unlink (GstPad * pad,
    GstObject * parent);
static void gst_base_text_overlay_pop_text (GstBaseTextOverlay * overlay);

static void gst_base_text_overlay_finalize (GObject * object);
static void gst_base_text_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_base_text_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_base_text_overlay_adjust_values_with_fontdesc (GstBaseTextOverlay * overlay,
    PangoFontDescription * desc);
static gboolean gst_base_text_overlay_can_handle_caps (GstCaps * incaps);

static void
gst_base_text_overlay_update_render_size (GstBaseTextOverlay * overlay);

GType
gst_base_text_overlay_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter ((gsize *) & type)) {
    static const GTypeInfo info = {
      sizeof (GstBaseTextOverlayClass),
      (GBaseInitFunc) NULL,
      NULL,
      (GClassInitFunc) gst_base_text_overlay_class_init,
      NULL,
      NULL,
      sizeof (GstBaseTextOverlay),
      0,
      (GInstanceInitFunc) gst_base_text_overlay_init,
    };

    g_once_init_leave ((gsize *) & type,
        g_type_register_static (GST_TYPE_ELEMENT, "GstBaseTextOverlay", &info,
            0));
  }

  return type;
}

static gchar *
gst_base_text_overlay_get_text (GstBaseTextOverlay * overlay,
    GstBuffer * video_frame)
{
  return g_strdup (overlay->default_text);
}

static void
gst_base_text_overlay_class_init (GstBaseTextOverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  GST_DEBUG_CATEGORY_INIT (base_text_overlay_debug, "basetextoverlay", 0,
      "Base Text Overlay");

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_base_text_overlay_finalize;
  gobject_class->set_property = gst_base_text_overlay_set_property;
  gobject_class->get_property = gst_base_text_overlay_get_property;

  gst_element_class_add_static_pad_template (gstelement_class,
      &src_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &video_sink_template_factory);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_base_text_overlay_change_state);

  klass->get_text = gst_base_text_overlay_get_text;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TEXT,
      g_param_spec_string ("text", "text",
          "Text to be display.", DEFAULT_PROP_TEXT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SHADING,
      g_param_spec_boolean ("shaded-background", "shaded background",
          "Whether to shade the background under the text area",
          DEFAULT_PROP_SHADING, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SHADING_VALUE,
      g_param_spec_uint ("shading-value", "background shading value",
          "Shading value to apply if shaded-background is true", 1, 255,
          DEFAULT_PROP_SHADING_VALUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_DOC_SHOW_DEFAULT));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_VALIGNMENT,
      g_param_spec_enum ("valignment", "vertical alignment",
          "Vertical alignment of the text", GST_TYPE_BASE_TEXT_OVERLAY_VALIGN,
          DEFAULT_PROP_VALIGNMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HALIGNMENT,
      g_param_spec_enum ("halignment", "horizontal alignment",
          "Horizontal alignment of the text", GST_TYPE_BASE_TEXT_OVERLAY_HALIGN,
          DEFAULT_PROP_HALIGNMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_DOC_SHOW_DEFAULT));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_XPAD,
      g_param_spec_int ("xpad", "horizontal paddding",
          "Horizontal paddding when using left/right alignment", 0, G_MAXINT,
          DEFAULT_PROP_XPAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_YPAD,
      g_param_spec_int ("ypad", "vertical padding",
          "Vertical padding when using top/bottom alignment", 0, G_MAXINT,
          DEFAULT_PROP_YPAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DELTAX,
      g_param_spec_int ("deltax", "X position modifier",
          "Shift X position to the left or to the right. Unit is pixels.",
          G_MININT, G_MAXINT, DEFAULT_PROP_DELTAX,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DELTAY,
      g_param_spec_int ("deltay", "Y position modifier",
          "Shift Y position up or down. Unit is pixels.", G_MININT, G_MAXINT,
          DEFAULT_PROP_DELTAY,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseTextOverlay:text-x:
   *
   * Resulting X position of font rendering.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TEXT_X,
      g_param_spec_int ("text-x", "horizontal position.",
          "Resulting X position of font rendering.", -G_MAXINT,
          G_MAXINT, DEFAULT_PROP_TEXT_X, G_PARAM_READABLE));

  /**
   * GstBaseTextOverlay:text-y:
   *
   * Resulting Y position of font rendering.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TEXT_Y,
      g_param_spec_int ("text-y", "vertical position",
          "Resulting Y position of font rendering.", -G_MAXINT,
          G_MAXINT, DEFAULT_PROP_TEXT_Y, G_PARAM_READABLE));

  /**
   * GstBaseTextOverlay:text-width:
   *
   * Resulting width of font rendering.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TEXT_WIDTH,
      g_param_spec_uint ("text-width", "width",
          "Resulting width of font rendering",
          0, G_MAXINT, DEFAULT_PROP_TEXT_WIDTH, G_PARAM_READABLE));

  /**
   * GstBaseTextOverlay:text-height:
   *
   * Resulting height of font rendering.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TEXT_HEIGHT,
      g_param_spec_uint ("text-height", "height",
          "Resulting height of font rendering", 0,
          G_MAXINT, DEFAULT_PROP_TEXT_HEIGHT, G_PARAM_READABLE));

  /**
   * GstBaseTextOverlay:xpos:
   *
   * Horizontal position of the rendered text when using positioned alignment.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_XPOS,
      g_param_spec_double ("xpos", "horizontal position",
          "Horizontal position when using clamped position alignment", 0, 1.0,
          DEFAULT_PROP_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseTextOverlay:ypos:
   *
   * Vertical position of the rendered text when using positioned alignment.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_YPOS,
      g_param_spec_double ("ypos", "vertical position",
          "Vertical position when using clamped position alignment", 0, 1.0,
          DEFAULT_PROP_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseTextOverlay:x-absolute:
   *
   * Horizontal position of the rendered text when using absolute alignment.
   *
   * Maps the text area to be exactly inside of video canvas for [0, 0] - [1, 1]:
   *
   * [0, 0]: Top-Lefts of video and text are aligned
   * [0.5, 0.5]: Centers are aligned
   * [1, 1]: Bottom-Rights are aligned
   *
   * Values beyond [0, 0] - [1, 1] place the text outside of the video canvas.
   *
   * Since: 1.8
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_X_ABSOLUTE,
      g_param_spec_double ("x-absolute", "horizontal position",
          "Horizontal position when using absolute alignment", -G_MAXDOUBLE,
          G_MAXDOUBLE, DEFAULT_PROP_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseTextOverlay:y-absolute:
   *
   * See x-absolute.
   *
   * Vertical position of the rendered text when using absolute alignment.
   *
   * Since: 1.8
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_Y_ABSOLUTE,
      g_param_spec_double ("y-absolute", "vertical position",
          "Vertical position when using absolute alignment", -G_MAXDOUBLE,
          G_MAXDOUBLE, DEFAULT_PROP_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WRAP_MODE,
      g_param_spec_enum ("wrap-mode", "wrap mode",
          "Whether to wrap the text and if so how.",
          GST_TYPE_BASE_TEXT_OVERLAY_WRAP_MODE, DEFAULT_PROP_WRAP_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT_DESC,
      g_param_spec_string ("font-desc", "font description",
          "Pango font description of font to be used for rendering. "
          "See documentation of pango_font_description_from_string "
          "for syntax.", DEFAULT_PROP_FONT_DESC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseTextOverlay:color:
   *
   * Color of the rendered text.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_COLOR,
      g_param_spec_uint ("color", "Color",
          "Color to use for text (big-endian ARGB).", 0, G_MAXUINT32,
          DEFAULT_PROP_COLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstTextOverlay:outline-color:
   *
   * Color of the outline of the rendered text.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_OUTLINE_COLOR,
      g_param_spec_uint ("outline-color", "Text Outline Color",
          "Color to use for outline the text (big-endian ARGB).", 0,
          G_MAXUINT32, DEFAULT_PROP_OUTLINE_COLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseTextOverlay:line-alignment:
   *
   * Alignment of text lines relative to each other (for multi-line text)
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LINE_ALIGNMENT,
      g_param_spec_enum ("line-alignment", "line alignment",
          "Alignment of text lines relative to each other.",
          GST_TYPE_BASE_TEXT_OVERLAY_LINE_ALIGN, DEFAULT_PROP_LINE_ALIGNMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseTextOverlay:silent:
   *
   * If set, no text is rendered. Useful to switch off text rendering
   * temporarily without removing the textoverlay element from the pipeline.
   */
  /* FIXME 0.11: rename to "visible" or "text-visible" or "render-text" */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SILENT,
      g_param_spec_boolean ("silent", "silent",
          "Whether to render the text string",
          DEFAULT_PROP_SILENT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseTextOverlay:draw-shadow:
   *
   * If set, a text shadow is drawn.
   *
   * Since: 1.6
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DRAW_SHADOW,
      g_param_spec_boolean ("draw-shadow", "draw-shadow",
          "Whether to draw shadow",
          DEFAULT_PROP_DRAW_SHADOW,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseTextOverlay:draw-outline:
   *
   * If set, an outline is drawn.
   *
   * Since: 1.6
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DRAW_OUTLINE,
      g_param_spec_boolean ("draw-outline", "draw-outline",
          "Whether to draw outline",
          DEFAULT_PROP_DRAW_OUTLINE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseTextOverlay:wait-text:
   *
   * If set, the video will block until a subtitle is received on the text pad.
   * If video and subtitles are sent in sync, like from the same demuxer, this
   * property should be set.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WAIT_TEXT,
      g_param_spec_boolean ("wait-text", "Wait Text",
          "Whether to wait for subtitles",
          DEFAULT_PROP_WAIT_TEXT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_AUTO_ADJUST_SIZE, g_param_spec_boolean ("auto-resize", "auto resize",
          "Automatically adjust font size to screen-size.",
          DEFAULT_PROP_AUTO_ADJUST_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_VERTICAL_RENDER,
      g_param_spec_boolean ("vertical-render", "vertical render",
          "Vertical Render.", DEFAULT_PROP_VERTICAL_RENDER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseTextOverlay:scale-mode:
   *
   * Scale text to compensate for and avoid distortion by subsequent video scaling
   *
   * Since: 1.14
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SCALE_MODE,
      g_param_spec_enum ("scale-mode", "scale mode",
          "Scale text to compensate for and avoid distortion by subsequent video scaling.",
          GST_TYPE_BASE_TEXT_OVERLAY_SCALE_MODE, DEFAULT_PROP_SCALE_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseTextOverlay:scale-pixel-aspect-ratio:
   *
   * Video scaling pixel-aspect-ratio to compensate for in user scale-mode.
   *
   * Since: 1.14
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SCALE_PAR,
      gst_param_spec_fraction ("scale-pixel-aspect-ratio",
          "scale pixel aspect ratio",
          "Pixel aspect ratio of video scale to compensate for in user scale-mode",
          1, 100, 100, 1, DEFAULT_PROP_SCALE_PAR_N, DEFAULT_PROP_SCALE_PAR_D,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TEXT_OVERLAY_HALIGN, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TEXT_OVERLAY_VALIGN, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TEXT_OVERLAY_LINE_ALIGN, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TEXT_OVERLAY_SCALE_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TEXT_OVERLAY_WRAP_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_BASE_TEXT_OVERLAY, 0);
}

static void
gst_base_text_overlay_finalize (GObject * object)
{
  GstBaseTextOverlay *overlay = GST_BASE_TEXT_OVERLAY (object);

  g_free (overlay->default_text);

  if (overlay->composition) {
    gst_video_overlay_composition_unref (overlay->composition);
    overlay->composition = NULL;
  }

  if (overlay->text_image) {
    gst_buffer_unref (overlay->text_image);
    overlay->text_image = NULL;
  }

  if (overlay->layout) {
    g_object_unref (overlay->layout);
    overlay->layout = NULL;
  }

  if (overlay->text_buffer) {
    gst_buffer_unref (overlay->text_buffer);
    overlay->text_buffer = NULL;
  }

  if (overlay->pango_context) {
    g_object_unref (overlay->pango_context);
    overlay->pango_context = NULL;
  }

  g_mutex_clear (&overlay->lock);
  g_cond_clear (&overlay->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_base_text_overlay_init (GstBaseTextOverlay * overlay,
    GstBaseTextOverlayClass * klass)
{
  GstPadTemplate *template;
  PangoFontDescription *desc;
  PangoFontMap *fontmap;

  fontmap = pango_cairo_font_map_new ();
  overlay->pango_context =
      pango_font_map_create_context (PANGO_FONT_MAP (fontmap));
  g_object_unref (fontmap);
  pango_context_set_base_gravity (overlay->pango_context, PANGO_GRAVITY_SOUTH);

  /* video sink */
  template = gst_static_pad_template_get (&video_sink_template_factory);
  overlay->video_sinkpad = gst_pad_new_from_template (template, "video_sink");
  gst_object_unref (template);
  gst_pad_set_event_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_text_overlay_video_event));
  gst_pad_set_chain_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_text_overlay_video_chain));
  gst_pad_set_query_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_text_overlay_video_query));
  GST_PAD_SET_PROXY_ALLOCATION (overlay->video_sinkpad);
  gst_element_add_pad (GST_ELEMENT (overlay), overlay->video_sinkpad);

  template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
      "text_sink");
  if (template) {
    /* text sink */
    overlay->text_sinkpad = gst_pad_new_from_template (template, "text_sink");

    gst_pad_set_event_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_text_overlay_text_event));
    gst_pad_set_chain_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_text_overlay_text_chain));
    gst_pad_set_link_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_text_overlay_text_pad_link));
    gst_pad_set_unlink_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_text_overlay_text_pad_unlink));
    gst_element_add_pad (GST_ELEMENT (overlay), overlay->text_sinkpad);
  }

  /* (video) source */
  template = gst_static_pad_template_get (&src_template_factory);
  overlay->srcpad = gst_pad_new_from_template (template, "src");
  gst_object_unref (template);
  gst_pad_set_event_function (overlay->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_text_overlay_src_event));
  gst_pad_set_query_function (overlay->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_text_overlay_src_query));
  gst_element_add_pad (GST_ELEMENT (overlay), overlay->srcpad);

  overlay->layout = pango_layout_new (overlay->pango_context);
  desc = pango_context_get_font_description (overlay->pango_context);
  gst_base_text_overlay_adjust_values_with_fontdesc (overlay, desc);

  overlay->color = DEFAULT_PROP_COLOR;
  overlay->outline_color = DEFAULT_PROP_OUTLINE_COLOR;
  overlay->halign = DEFAULT_PROP_HALIGNMENT;
  overlay->valign = DEFAULT_PROP_VALIGNMENT;
  overlay->xpad = DEFAULT_PROP_XPAD;
  overlay->ypad = DEFAULT_PROP_YPAD;
  overlay->deltax = DEFAULT_PROP_DELTAX;
  overlay->deltay = DEFAULT_PROP_DELTAY;
  overlay->xpos = DEFAULT_PROP_XPOS;
  overlay->ypos = DEFAULT_PROP_YPOS;

  overlay->wrap_mode = DEFAULT_PROP_WRAP_MODE;

  overlay->want_shading = DEFAULT_PROP_SHADING;
  overlay->shading_value = DEFAULT_PROP_SHADING_VALUE;
  overlay->silent = DEFAULT_PROP_SILENT;
  overlay->draw_shadow = DEFAULT_PROP_DRAW_SHADOW;
  overlay->draw_outline = DEFAULT_PROP_DRAW_OUTLINE;
  overlay->wait_text = DEFAULT_PROP_WAIT_TEXT;
  overlay->auto_adjust_size = DEFAULT_PROP_AUTO_ADJUST_SIZE;

  overlay->default_text = g_strdup (DEFAULT_PROP_TEXT);
  overlay->need_render = TRUE;
  overlay->text_image = NULL;
  overlay->use_vertical_render = DEFAULT_PROP_VERTICAL_RENDER;
  overlay->scale_mode = DEFAULT_PROP_SCALE_MODE;
  overlay->scale_par_n = DEFAULT_PROP_SCALE_PAR_N;
  overlay->scale_par_d = DEFAULT_PROP_SCALE_PAR_D;

  overlay->line_align = DEFAULT_PROP_LINE_ALIGNMENT;
  pango_layout_set_alignment (overlay->layout,
      (PangoAlignment) overlay->line_align);

  overlay->text_buffer = NULL;
  overlay->text_buffer_running_time = GST_CLOCK_TIME_NONE;
  overlay->text_buffer_running_time_end = GST_CLOCK_TIME_NONE;
  overlay->text_linked = FALSE;

  overlay->composition = NULL;
  overlay->upstream_composition = NULL;

  overlay->width = 1;
  overlay->height = 1;

  overlay->window_width = 1;
  overlay->window_height = 1;

  overlay->text_width = DEFAULT_PROP_TEXT_WIDTH;
  overlay->text_height = DEFAULT_PROP_TEXT_HEIGHT;

  overlay->text_x = DEFAULT_PROP_TEXT_X;
  overlay->text_y = DEFAULT_PROP_TEXT_Y;

  overlay->render_width = 1;
  overlay->render_height = 1;
  overlay->render_scale = 1.0l;

  g_mutex_init (&overlay->lock);
  g_cond_init (&overlay->cond);
  gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
}

static void
gst_base_text_overlay_set_wrap_mode (GstBaseTextOverlay * overlay, gint width)
{
  if (overlay->wrap_mode == GST_BASE_TEXT_OVERLAY_WRAP_MODE_NONE) {
    GST_DEBUG_OBJECT (overlay, "Set wrap mode NONE");
    pango_layout_set_width (overlay->layout, -1);
  } else {
    width = width * PANGO_SCALE;

    GST_DEBUG_OBJECT (overlay, "Set layout width %d", width);
    GST_DEBUG_OBJECT (overlay, "Set wrap mode    %d", overlay->wrap_mode);
    pango_layout_set_width (overlay->layout, width);
  }

  pango_layout_set_wrap (overlay->layout, (PangoWrapMode) overlay->wrap_mode);
}

static gboolean
gst_base_text_overlay_setcaps_txt (GstBaseTextOverlay * overlay, GstCaps * caps)
{
  GstStructure *structure;
  const gchar *format;

  structure = gst_caps_get_structure (caps, 0);
  format = gst_structure_get_string (structure, "format");
  overlay->have_pango_markup = (strcmp (format, "pango-markup") == 0);

  return TRUE;
}

/* only negotiate/query video overlay composition support for now */
static gboolean
gst_base_text_overlay_negotiate (GstBaseTextOverlay * overlay, GstCaps * caps)
{
  gboolean upstream_has_meta = FALSE;
  gboolean caps_has_meta = FALSE;
  gboolean alloc_has_meta = FALSE;
  gboolean attach = FALSE;
  gboolean ret = TRUE;
  guint width, height;
  GstCapsFeatures *f;
  GstCaps *overlay_caps;
  GstQuery *query;
  guint alloc_index;

  GST_DEBUG_OBJECT (overlay, "performing negotiation");

  /* Clear any pending reconfigure to avoid negotiating twice */
  gst_pad_check_reconfigure (overlay->srcpad);

  if (!caps)
    caps = gst_pad_get_current_caps (overlay->video_sinkpad);
  else
    gst_caps_ref (caps);

  if (!caps || gst_caps_is_empty (caps))
    goto no_format;

  /* Check if upstream caps have meta */
  if ((f = gst_caps_get_features (caps, 0))) {
    upstream_has_meta = gst_caps_features_contains (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  }

  /* Initialize dimensions */
  width = overlay->width;
  height = overlay->height;

  if (upstream_has_meta) {
    overlay_caps = gst_caps_ref (caps);
  } else {
    GstCaps *peercaps;

    /* BaseTransform requires caps for the allocation query to work */
    overlay_caps = gst_caps_copy (caps);
    f = gst_caps_get_features (overlay_caps, 0);
    gst_caps_features_add (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

    /* Then check if downstream accept overlay composition in caps */
    /* FIXME: We should probably check if downstream *prefers* the
     * overlay meta, and only enforce usage of it if we can't handle
     * the format ourselves and thus would have to drop the overlays.
     * Otherwise we should prefer what downstream wants here.
     */
    peercaps = gst_pad_peer_query_caps (overlay->srcpad, overlay_caps);
    caps_has_meta = !gst_caps_is_empty (peercaps);
    gst_caps_unref (peercaps);

    GST_DEBUG_OBJECT (overlay, "caps have overlay meta %d", caps_has_meta);
  }

  if (upstream_has_meta || caps_has_meta) {
    /* Send caps immediately, it's needed by GstBaseTransform to get a reply
     * from allocation query */
    ret = gst_pad_set_caps (overlay->srcpad, overlay_caps);

    /* First check if the allocation meta has compositon */
    query = gst_query_new_allocation (overlay_caps, FALSE);

    if (!gst_pad_peer_query (overlay->srcpad, query)) {
      /* no problem, we use the query defaults */
      GST_DEBUG_OBJECT (overlay, "ALLOCATION query failed");

      /* In case we were flushing, mark reconfigure and fail this method,
       * will make it retry */
      if (overlay->video_flushing)
        ret = FALSE;
    }

    alloc_has_meta = gst_query_find_allocation_meta (query,
        GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, &alloc_index);

    GST_DEBUG_OBJECT (overlay, "sink alloc has overlay meta %d",
        alloc_has_meta);

    if (alloc_has_meta) {
      const GstStructure *params;

      gst_query_parse_nth_allocation_meta (query, alloc_index, &params);
      if (params) {
        if (gst_structure_get (params, "width", G_TYPE_UINT, &width,
                "height", G_TYPE_UINT, &height, NULL)) {
          GST_DEBUG_OBJECT (overlay, "received window size: %dx%d", width,
              height);
          g_assert (width != 0 && height != 0);
        }
      }
    }

    gst_query_unref (query);
  }

  /* Update render size if needed */
  overlay->window_width = width;
  overlay->window_height = height;
  gst_base_text_overlay_update_render_size (overlay);

  /* For backward compatibility, we will prefer blitting if downstream
   * allocation does not support the meta. In other case we will prefer
   * attaching, and will fail the negotiation in the unlikely case we are
   * force to blit, but format isn't supported. */

  if (upstream_has_meta) {
    attach = TRUE;
  } else if (caps_has_meta) {
    if (alloc_has_meta) {
      attach = TRUE;
    } else {
      /* Don't attach unless we cannot handle the format */
      attach = !gst_base_text_overlay_can_handle_caps (caps);
    }
  } else {
    ret = gst_base_text_overlay_can_handle_caps (caps);
  }

  /* If we attach, then pick the overlay caps */
  if (attach) {
    GST_DEBUG_OBJECT (overlay, "Using caps %" GST_PTR_FORMAT, overlay_caps);
    /* Caps where already sent */
  } else if (ret) {
    GST_DEBUG_OBJECT (overlay, "Using caps %" GST_PTR_FORMAT, caps);
    ret = gst_pad_set_caps (overlay->srcpad, caps);
  }

  overlay->attach_compo_to_buffer = attach;

  if (!ret) {
    GST_DEBUG_OBJECT (overlay, "negotiation failed, schedule reconfigure");
    gst_pad_mark_reconfigure (overlay->srcpad);
  }

  gst_caps_unref (overlay_caps);
  gst_caps_unref (caps);

  return ret;

no_format:
  {
    if (caps)
      gst_caps_unref (caps);
    gst_pad_mark_reconfigure (overlay->srcpad);
    return FALSE;
  }
}

static gboolean
gst_base_text_overlay_can_handle_caps (GstCaps * incaps)
{
  gboolean ret;
  GstCaps *caps;

  caps = gst_static_caps_get (&sw_template_caps);
  ret = gst_caps_is_subset (incaps, caps);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_base_text_overlay_setcaps (GstBaseTextOverlay * overlay, GstCaps * caps)
{
  GstVideoInfo info;
  gboolean ret = FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* Render again if size have changed */
  if (GST_VIDEO_INFO_WIDTH (&info) != GST_VIDEO_INFO_WIDTH (&overlay->info) ||
      GST_VIDEO_INFO_HEIGHT (&info) != GST_VIDEO_INFO_HEIGHT (&overlay->info))
    overlay->need_render = TRUE;

  overlay->info = info;
  overlay->format = GST_VIDEO_INFO_FORMAT (&info);
  overlay->width = GST_VIDEO_INFO_WIDTH (&info);
  overlay->height = GST_VIDEO_INFO_HEIGHT (&info);

  ret = gst_base_text_overlay_negotiate (overlay, caps);

  GST_BASE_TEXT_OVERLAY_LOCK (overlay);

  if (!overlay->attach_compo_to_buffer &&
      !gst_base_text_overlay_can_handle_caps (caps)) {
    GST_DEBUG_OBJECT (overlay, "unsupported caps %" GST_PTR_FORMAT, caps);
    ret = FALSE;
  }
  GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);

  return ret;

  /* ERRORS */
invalid_caps:
  {
    GST_DEBUG_OBJECT (overlay, "could not parse caps");
    return FALSE;
  }
}

static void
gst_base_text_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseTextOverlay *overlay = GST_BASE_TEXT_OVERLAY (object);

  GST_BASE_TEXT_OVERLAY_LOCK (overlay);
  switch (prop_id) {
    case PROP_TEXT:
      g_free (overlay->default_text);
      overlay->default_text = g_value_dup_string (value);
      break;
    case PROP_SHADING:
      overlay->want_shading = g_value_get_boolean (value);
      break;
    case PROP_XPAD:
      overlay->xpad = g_value_get_int (value);
      break;
    case PROP_YPAD:
      overlay->ypad = g_value_get_int (value);
      break;
    case PROP_DELTAX:
      overlay->deltax = g_value_get_int (value);
      break;
    case PROP_DELTAY:
      overlay->deltay = g_value_get_int (value);
      break;
    case PROP_XPOS:
      overlay->xpos = g_value_get_double (value);
      break;
    case PROP_YPOS:
      overlay->ypos = g_value_get_double (value);
      break;
    case PROP_X_ABSOLUTE:
      overlay->xpos = g_value_get_double (value);
      break;
    case PROP_Y_ABSOLUTE:
      overlay->ypos = g_value_get_double (value);
      break;
    case PROP_VALIGNMENT:
      overlay->valign = g_value_get_enum (value);
      break;
    case PROP_HALIGNMENT:
      overlay->halign = g_value_get_enum (value);
      break;
    case PROP_WRAP_MODE:
      overlay->wrap_mode = g_value_get_enum (value);
      break;
    case PROP_FONT_DESC:
    {
      PangoFontDescription *desc;
      const gchar *fontdesc_str;

      fontdesc_str = g_value_get_string (value);
      desc = pango_font_description_from_string (fontdesc_str);
      if (desc) {
        GST_LOG_OBJECT (overlay, "font description set: %s", fontdesc_str);
        pango_layout_set_font_description (overlay->layout, desc);
        gst_base_text_overlay_adjust_values_with_fontdesc (overlay, desc);
        pango_font_description_free (desc);
      } else {
        GST_WARNING_OBJECT (overlay, "font description parse failed: %s",
            fontdesc_str);
      }
      break;
    }
    case PROP_COLOR:
      overlay->color = g_value_get_uint (value);
      break;
    case PROP_OUTLINE_COLOR:
      overlay->outline_color = g_value_get_uint (value);
      break;
    case PROP_SILENT:
      overlay->silent = g_value_get_boolean (value);
      break;
    case PROP_DRAW_SHADOW:
      overlay->draw_shadow = g_value_get_boolean (value);
      break;
    case PROP_DRAW_OUTLINE:
      overlay->draw_outline = g_value_get_boolean (value);
      break;
    case PROP_LINE_ALIGNMENT:
      overlay->line_align = g_value_get_enum (value);
      pango_layout_set_alignment (overlay->layout,
          (PangoAlignment) overlay->line_align);
      break;
    case PROP_WAIT_TEXT:
      overlay->wait_text = g_value_get_boolean (value);
      break;
    case PROP_AUTO_ADJUST_SIZE:
      overlay->auto_adjust_size = g_value_get_boolean (value);
      break;
    case PROP_VERTICAL_RENDER:
      overlay->use_vertical_render = g_value_get_boolean (value);
      if (overlay->use_vertical_render) {
        overlay->valign = GST_BASE_TEXT_OVERLAY_VALIGN_TOP;
        overlay->halign = GST_BASE_TEXT_OVERLAY_HALIGN_RIGHT;
        overlay->line_align = GST_BASE_TEXT_OVERLAY_LINE_ALIGN_LEFT;
        pango_layout_set_alignment (overlay->layout,
            (PangoAlignment) overlay->line_align);
      }
      break;
    case PROP_SCALE_MODE:
      overlay->scale_mode = g_value_get_enum (value);
      break;
    case PROP_SCALE_PAR:
      overlay->scale_par_n = gst_value_get_fraction_numerator (value);
      overlay->scale_par_d = gst_value_get_fraction_denominator (value);
      break;
    case PROP_SHADING_VALUE:
      overlay->shading_value = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  overlay->need_render = TRUE;
  GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
}

static void
gst_base_text_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBaseTextOverlay *overlay = GST_BASE_TEXT_OVERLAY (object);

  GST_BASE_TEXT_OVERLAY_LOCK (overlay);
  switch (prop_id) {
    case PROP_TEXT:
      g_value_set_string (value, overlay->default_text);
      break;
    case PROP_SHADING:
      g_value_set_boolean (value, overlay->want_shading);
      break;
    case PROP_XPAD:
      g_value_set_int (value, overlay->xpad);
      break;
    case PROP_YPAD:
      g_value_set_int (value, overlay->ypad);
      break;
    case PROP_DELTAX:
      g_value_set_int (value, overlay->deltax);
      break;
    case PROP_DELTAY:
      g_value_set_int (value, overlay->deltay);
      break;
    case PROP_XPOS:
      g_value_set_double (value, overlay->xpos);
      break;
    case PROP_YPOS:
      g_value_set_double (value, overlay->ypos);
      break;
    case PROP_X_ABSOLUTE:
      g_value_set_double (value, overlay->xpos);
      break;
    case PROP_Y_ABSOLUTE:
      g_value_set_double (value, overlay->ypos);
      break;
    case PROP_VALIGNMENT:
      g_value_set_enum (value, overlay->valign);
      break;
    case PROP_HALIGNMENT:
      g_value_set_enum (value, overlay->halign);
      break;
    case PROP_WRAP_MODE:
      g_value_set_enum (value, overlay->wrap_mode);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, overlay->silent);
      break;
    case PROP_DRAW_SHADOW:
      g_value_set_boolean (value, overlay->draw_shadow);
      break;
    case PROP_DRAW_OUTLINE:
      g_value_set_boolean (value, overlay->draw_outline);
      break;
    case PROP_LINE_ALIGNMENT:
      g_value_set_enum (value, overlay->line_align);
      break;
    case PROP_WAIT_TEXT:
      g_value_set_boolean (value, overlay->wait_text);
      break;
    case PROP_AUTO_ADJUST_SIZE:
      g_value_set_boolean (value, overlay->auto_adjust_size);
      break;
    case PROP_VERTICAL_RENDER:
      g_value_set_boolean (value, overlay->use_vertical_render);
      break;
    case PROP_SCALE_MODE:
      g_value_set_enum (value, overlay->scale_mode);
      break;
    case PROP_SCALE_PAR:
      gst_value_set_fraction (value, overlay->scale_par_n,
          overlay->scale_par_d);
      break;
    case PROP_COLOR:
      g_value_set_uint (value, overlay->color);
      break;
    case PROP_OUTLINE_COLOR:
      g_value_set_uint (value, overlay->outline_color);
      break;
    case PROP_SHADING_VALUE:
      g_value_set_uint (value, overlay->shading_value);
      break;
    case PROP_FONT_DESC:
    {
      const PangoFontDescription *desc;

      desc = pango_layout_get_font_description (overlay->layout);
      if (!desc)
        g_value_set_string (value, "");
      else {
        g_value_take_string (value, pango_font_description_to_string (desc));
      }
      break;
    }
    case PROP_TEXT_X:
      g_value_set_int (value, overlay->text_x);
      break;
    case PROP_TEXT_Y:
      g_value_set_int (value, overlay->text_y);
      break;
    case PROP_TEXT_WIDTH:
      g_value_set_uint (value, overlay->text_width);
      break;
    case PROP_TEXT_HEIGHT:
      g_value_set_uint (value, overlay->text_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  overlay->need_render = TRUE;
  GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
}

static gboolean
gst_base_text_overlay_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstBaseTextOverlay *overlay;

  overlay = GST_BASE_TEXT_OVERLAY (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_base_text_overlay_get_src_caps (pad, overlay, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static void
gst_base_text_overlay_update_render_size (GstBaseTextOverlay * overlay)
{
  gdouble video_aspect = (gdouble) overlay->width / (gdouble) overlay->height;
  gdouble window_aspect = (gdouble) overlay->window_width /
      (gdouble) overlay->window_height;

  guint text_buffer_width = 0;
  guint text_buffer_height = 0;

  if (video_aspect >= window_aspect) {
    text_buffer_width = overlay->window_width;
    text_buffer_height = window_aspect * overlay->window_height / video_aspect;
  } else if (video_aspect < window_aspect) {
    text_buffer_width = video_aspect * overlay->window_width / window_aspect;
    text_buffer_height = overlay->window_height;
  }

  if ((overlay->render_width == text_buffer_width) &&
      (overlay->render_height == text_buffer_height))
    return;

  overlay->need_render = TRUE;
  overlay->render_width = text_buffer_width;
  overlay->render_height = text_buffer_height;
  overlay->render_scale = (gdouble) overlay->render_width /
      (gdouble) overlay->width;

  GST_DEBUG ("updating render dimensions %dx%d from stream %dx%d, window %dx%d "
      "and render scale %f", overlay->render_width, overlay->render_height,
      overlay->width, overlay->height, overlay->window_width,
      overlay->window_height, overlay->render_scale);
}

static gboolean
gst_base_text_overlay_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstBaseTextOverlay *overlay;
  gboolean ret;

  overlay = GST_BASE_TEXT_OVERLAY (parent);

  if (overlay->text_linked) {
    gst_event_ref (event);
    ret = gst_pad_push_event (overlay->video_sinkpad, event);
    gst_pad_push_event (overlay->text_sinkpad, event);
  } else {
    ret = gst_pad_push_event (overlay->video_sinkpad, event);
  }

  return ret;
}

/* gst_base_text_overlay_add_feature_and_intersect:
 *
 * Creates a new #GstCaps containing the (given caps +
 * given caps feature) + (given caps intersected by the
 * given filter).
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_base_text_overlay_add_feature_and_intersect (GstCaps * caps,
    const gchar * feature, GstCaps * filter)
{
  int i, caps_size;
  GstCaps *new_caps;

  new_caps = gst_caps_copy (caps);

  caps_size = gst_caps_get_size (new_caps);
  for (i = 0; i < caps_size; i++) {
    GstCapsFeatures *features = gst_caps_get_features (new_caps, i);

    if (!gst_caps_features_is_any (features)) {
      gst_caps_features_add (features, feature);
    }
  }

  gst_caps_append (new_caps, gst_caps_intersect_full (caps,
          filter, GST_CAPS_INTERSECT_FIRST));

  return new_caps;
}

/* gst_base_text_overlay_intersect_by_feature:
 *
 * Creates a new #GstCaps based on the following filtering rule.
 *
 * For each individual caps contained in given caps, if the
 * caps uses the given caps feature, keep a version of the caps
 * with the feature and an another one without. Otherwise, intersect
 * the caps with the given filter.
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_base_text_overlay_intersect_by_feature (GstCaps * caps,
    const gchar * feature, GstCaps * filter)
{
  int i, caps_size;
  GstCaps *new_caps;

  new_caps = gst_caps_new_empty ();

  caps_size = gst_caps_get_size (caps);
  for (i = 0; i < caps_size; i++) {
    GstStructure *caps_structure = gst_caps_get_structure (caps, i);
    GstCapsFeatures *caps_features =
        gst_caps_features_copy (gst_caps_get_features (caps, i));
    GstCaps *filtered_caps;
    GstCaps *simple_caps =
        gst_caps_new_full (gst_structure_copy (caps_structure), NULL);
    gst_caps_set_features (simple_caps, 0, caps_features);

    if (gst_caps_features_contains (caps_features, feature)) {
      gst_caps_append (new_caps, gst_caps_copy (simple_caps));

      gst_caps_features_remove (caps_features, feature);
      filtered_caps = gst_caps_ref (simple_caps);
    } else {
      filtered_caps = gst_caps_intersect_full (simple_caps, filter,
          GST_CAPS_INTERSECT_FIRST);
    }

    gst_caps_unref (simple_caps);
    gst_caps_append (new_caps, filtered_caps);
  }

  return new_caps;
}

static GstCaps *
gst_base_text_overlay_get_videosink_caps (GstPad * pad,
    GstBaseTextOverlay * overlay, GstCaps * filter)
{
  GstPad *srcpad = overlay->srcpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (filter) {
    /* filter caps + composition feature + filter caps
     * filtered by the software caps. */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter = gst_base_text_overlay_add_feature_and_intersect (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);

    GST_DEBUG_OBJECT (overlay, "overlay filter %" GST_PTR_FORMAT,
        overlay_filter);
  }

  peer_caps = gst_pad_peer_query_caps (srcpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (pad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {
      /* if peer returns ANY caps, return filtered src pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (srcpad));
    } else {

      /* duplicate caps which contains the composition into one version with
       * the meta and one without. Filter the other caps by the software caps */
      GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
      caps = gst_base_text_overlay_intersect_by_feature (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (pad);
  }

  if (filter) {
    GstCaps *intersection = gst_caps_intersect_full (filter, caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  GST_DEBUG_OBJECT (overlay, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_base_text_overlay_get_src_caps (GstPad * pad, GstBaseTextOverlay * overlay,
    GstCaps * filter)
{
  GstPad *sinkpad = overlay->video_sinkpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (filter) {
    /* duplicate filter caps which contains the composition into one version
     * with the meta and one without. Filter the other caps by the software
     * caps */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter =
        gst_base_text_overlay_intersect_by_feature (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);
  }

  peer_caps = gst_pad_peer_query_caps (sinkpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (pad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {

      /* if peer returns ANY caps, return filtered sink pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (sinkpad));

    } else {

      /* return upstream caps + composition feature + upstream caps
       * filtered by the software caps. */
      GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
      caps = gst_base_text_overlay_add_feature_and_intersect (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (pad);
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }
  GST_DEBUG_OBJECT (overlay, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static void
gst_base_text_overlay_adjust_values_with_fontdesc (GstBaseTextOverlay * overlay,
    PangoFontDescription * desc)
{
  gint font_size = pango_font_description_get_size (desc) / PANGO_SCALE;
  overlay->shadow_offset = (double) (font_size) / 13.0;
  overlay->outline_offset = (double) (font_size) / 15.0;
  if (overlay->outline_offset < MINIMUM_OUTLINE_OFFSET)
    overlay->outline_offset = MINIMUM_OUTLINE_OFFSET;
}

static void
gst_base_text_overlay_get_pos (GstBaseTextOverlay * overlay,
    gint * xpos, gint * ypos)
{
  gint width, height;

  width = overlay->logical_rect.width;
  height = overlay->logical_rect.height;

  *xpos = overlay->ink_rect.x - overlay->logical_rect.x;
  switch (overlay->halign) {
    case GST_BASE_TEXT_OVERLAY_HALIGN_LEFT:
      *xpos += overlay->xpad;
      *xpos = MAX (0, *xpos);
      break;
    case GST_BASE_TEXT_OVERLAY_HALIGN_CENTER:
      *xpos += (overlay->width - width) / 2;
      break;
    case GST_BASE_TEXT_OVERLAY_HALIGN_RIGHT:
      *xpos += overlay->width - width - overlay->xpad;
      *xpos = MIN (overlay->width - overlay->ink_rect.width, *xpos);
      break;
    case GST_BASE_TEXT_OVERLAY_HALIGN_POS:
      *xpos += (gint) (overlay->width * overlay->xpos) - width / 2;
      *xpos = CLAMP (*xpos, 0, overlay->width - overlay->ink_rect.width);
      if (*xpos < 0)
        *xpos = 0;
      break;
    case GST_BASE_TEXT_OVERLAY_HALIGN_ABSOLUTE:
      *xpos = (overlay->width - overlay->text_width) * overlay->xpos;
      break;
    default:
      *xpos = 0;
  }
  *xpos += overlay->deltax;

  *ypos = overlay->ink_rect.y - overlay->logical_rect.y;
  switch (overlay->valign) {
    case GST_BASE_TEXT_OVERLAY_VALIGN_BOTTOM:
      /* This will be the same as baseline, if there is enough padding,
       * otherwise it will avoid clipping the text */
      *ypos += overlay->height - height - overlay->ypad;
      *ypos = MIN (overlay->height - overlay->ink_rect.height, *ypos);
      break;
    case GST_BASE_TEXT_OVERLAY_VALIGN_BASELINE:
      *ypos += overlay->height - height - overlay->ypad;
      /* Don't clip, this would not respect the base line */
      break;
    case GST_BASE_TEXT_OVERLAY_VALIGN_TOP:
      *ypos += overlay->ypad;
      *ypos = MAX (0.0, *ypos);
      break;
    case GST_BASE_TEXT_OVERLAY_VALIGN_POS:
      *ypos = (gint) (overlay->height * overlay->ypos) - height / 2;
      *ypos = CLAMP (*ypos, 0, overlay->height - overlay->ink_rect.height);
      break;
    case GST_BASE_TEXT_OVERLAY_VALIGN_ABSOLUTE:
      *ypos = (overlay->height - overlay->text_height) * overlay->ypos;
      break;
    case GST_BASE_TEXT_OVERLAY_VALIGN_CENTER:
      *ypos = (overlay->height - height) / 2;
      break;
    default:
      *ypos = overlay->ypad;
      break;
  }
  *ypos += overlay->deltay;

  overlay->text_x = *xpos;
  overlay->text_y = *ypos;

  GST_DEBUG_OBJECT (overlay, "Placing overlay at (%d, %d)", *xpos, *ypos);
}

static inline void
gst_base_text_overlay_set_composition (GstBaseTextOverlay * overlay)
{
  gint xpos, ypos;
  GstVideoOverlayRectangle *rectangle;

  if (overlay->text_image) {
    gint render_width, render_height;

    gst_base_text_overlay_get_pos (overlay, &xpos, &ypos);

    render_width = overlay->ink_rect.width;
    render_height = overlay->ink_rect.height;

    GST_DEBUG ("updating composition for '%s' with window size %dx%d, "
        "buffer size %dx%d, render size %dx%d and position (%d, %d)",
        overlay->default_text, overlay->window_width, overlay->window_height,
        overlay->text_width, overlay->text_height, render_width,
        render_height, xpos, ypos);

    gst_buffer_add_video_meta (overlay->text_image, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
        overlay->text_width, overlay->text_height);

    rectangle = gst_video_overlay_rectangle_new_raw (overlay->text_image,
        xpos, ypos, render_width, render_height,
        GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

    if (overlay->composition)
      gst_video_overlay_composition_unref (overlay->composition);

    if (overlay->upstream_composition) {
      overlay->composition =
          gst_video_overlay_composition_copy (overlay->upstream_composition);
      gst_video_overlay_composition_add_rectangle (overlay->composition,
          rectangle);
    } else {
      overlay->composition = gst_video_overlay_composition_new (rectangle);
    }

    gst_video_overlay_rectangle_unref (rectangle);

  } else if (overlay->composition) {
    gst_video_overlay_composition_unref (overlay->composition);
    overlay->composition = NULL;
  }
}

static gboolean
gst_text_overlay_filter_foreground_attr (PangoAttribute * attr, gpointer data)
{
  if (attr->klass->type == PANGO_ATTR_FOREGROUND) {
    return FALSE;
  } else {
    return TRUE;
  }
}

static void
gst_base_text_overlay_render_pangocairo (GstBaseTextOverlay * overlay,
    const gchar * string, gint textlen)
{
  cairo_t *cr;
  cairo_surface_t *surface;
  PangoRectangle ink_rect, logical_rect;
  cairo_matrix_t cairo_matrix;
  gint unscaled_width, unscaled_height;
  gint width, height;
  gboolean full_width = FALSE;
  double scalef_x = 1.0, scalef_y = 1.0;
  double a, r, g, b;
  gdouble shadow_offset = 0.0;
  gdouble outline_offset = 0.0;
  gint xpad = 0, ypad = 0;
  GstBuffer *buffer;
  GstMapInfo map;

  if (overlay->auto_adjust_size) {
    /* 640 pixel is default */
    scalef_x = scalef_y = (double) (overlay->width) / DEFAULT_SCALE_BASIS;
  }

  if (overlay->scale_mode != GST_BASE_TEXT_OVERLAY_SCALE_MODE_NONE) {
    gint par_n = 1, par_d = 1;

    switch (overlay->scale_mode) {
      case GST_BASE_TEXT_OVERLAY_SCALE_MODE_PAR:
        par_n = overlay->info.par_n;
        par_d = overlay->info.par_d;
        break;
      case GST_BASE_TEXT_OVERLAY_SCALE_MODE_DISPLAY:
        /* (width * par_n) / (height * par_d) = (display_w / display_h) */
        if (!gst_util_fraction_multiply (overlay->window_width,
                overlay->window_height, overlay->height, overlay->width,
                &par_n, &par_d)) {
          GST_WARNING_OBJECT (overlay,
              "Can't figure out display ratio, defaulting to 1:1");
          par_n = par_d = 1;
        }
        break;
      case GST_BASE_TEXT_OVERLAY_SCALE_MODE_USER:
        par_n = overlay->scale_par_n;
        par_d = overlay->scale_par_d;
        break;
      default:
        break;
    }
    /* sanitize */
    if (!par_n || !par_d)
      par_n = par_d = 1;
    /* compensate later scaling as would be done for a par_n / par_d p-a-r;
     * apply all scaling to y so as to allow for predictable text width
     * layout independent of the presentation aspect scaling */
    if (overlay->use_vertical_render) {
      scalef_y *= ((gdouble) par_d) / ((gdouble) par_n);
    } else {
      scalef_y *= ((gdouble) par_n) / ((gdouble) par_d);
    }
    GST_DEBUG_OBJECT (overlay,
        "compensate scaling mode %d par %d/%d, scale %f, %f",
        overlay->scale_mode, par_n, par_d, scalef_x, scalef_y);
  }

  if (overlay->draw_shadow)
    shadow_offset = ceil (overlay->shadow_offset);

  /* This value is uses as cairo line width, which is the diameter of a pen
   * that is circular. That's why only half of it is used to offset */
  if (overlay->draw_outline)
    outline_offset = ceil (overlay->outline_offset);

  if (overlay->halign == GST_BASE_TEXT_OVERLAY_HALIGN_LEFT ||
      overlay->halign == GST_BASE_TEXT_OVERLAY_HALIGN_RIGHT)
    xpad = overlay->xpad;

  if (overlay->valign == GST_BASE_TEXT_OVERLAY_VALIGN_TOP ||
      overlay->valign == GST_BASE_TEXT_OVERLAY_VALIGN_BOTTOM)
    ypad = overlay->ypad;

  pango_layout_set_width (overlay->layout, -1);
  /* set text on pango layout */
  pango_layout_set_markup (overlay->layout, string, textlen);

  /* get subtitle image size */
  pango_layout_get_pixel_extents (overlay->layout, &ink_rect, &logical_rect);

  unscaled_width = ink_rect.width + shadow_offset + outline_offset;
  width = ceil (unscaled_width * scalef_x);

  /*
   * subtitle image width can be larger then overlay width
   * so rearrange overlay wrap mode.
   */
  if (overlay->use_vertical_render) {
    if (width + ypad > overlay->height) {
      width = overlay->height - ypad;
      full_width = TRUE;
    }
  } else if (width + xpad > overlay->width) {
    width = overlay->width - xpad;
    full_width = TRUE;
  }

  if (full_width) {
    unscaled_width = width / scalef_x;
    gst_base_text_overlay_set_wrap_mode (overlay,
        unscaled_width - shadow_offset - outline_offset);
    pango_layout_get_pixel_extents (overlay->layout, &ink_rect, &logical_rect);

    unscaled_width = ink_rect.width + shadow_offset + outline_offset;
    width = ceil (unscaled_width * scalef_x);
  }

  unscaled_height = ink_rect.height + shadow_offset + outline_offset;
  height = ceil (unscaled_height * scalef_y);

  if (overlay->use_vertical_render) {
    if (height + xpad > overlay->width) {
      height = overlay->width - xpad;
      unscaled_height = width / scalef_y;
    }
  } else if (height + ypad > overlay->height) {
    height = overlay->height - ypad;
    unscaled_height = height / scalef_y;
  }

  GST_DEBUG_OBJECT (overlay, "Rendering with ink rect (%d, %d) %dx%d and "
      "logical rect (%d, %d) %dx%d", ink_rect.x, ink_rect.y, ink_rect.width,
      ink_rect.height, logical_rect.x, logical_rect.y, logical_rect.width,
      logical_rect.height);
  GST_DEBUG_OBJECT (overlay, "Rendering with width %d and height %d "
      "(shadow %f, outline %f)", unscaled_width, unscaled_height,
      shadow_offset, outline_offset);


  /* Save and scale the rectangles so get_pos() can place the text */
  overlay->ink_rect.x =
      ceil ((ink_rect.x - ceil (outline_offset / 2.0l)) * scalef_x);
  overlay->ink_rect.y =
      ceil ((ink_rect.y - ceil (outline_offset / 2.0l)) * scalef_y);
  overlay->ink_rect.width = width;
  overlay->ink_rect.height = height;

  overlay->logical_rect.x =
      ceil ((logical_rect.x - ceil (outline_offset / 2.0l)) * scalef_x);
  overlay->logical_rect.y =
      ceil ((logical_rect.y - ceil (outline_offset / 2.0l)) * scalef_y);
  overlay->logical_rect.width =
      ceil ((logical_rect.width + shadow_offset + outline_offset) * scalef_x);
  overlay->logical_rect.height =
      ceil ((logical_rect.height + shadow_offset + outline_offset) * scalef_y);

  /* flip the rectangle if doing vertical render */
  if (overlay->use_vertical_render) {
    PangoRectangle tmp = overlay->ink_rect;

    overlay->ink_rect.x = tmp.y;
    overlay->ink_rect.y = tmp.x;
    overlay->ink_rect.width = tmp.height;
    overlay->ink_rect.height = tmp.width;
    /* We want the top left correct, but we now have the top right */
    overlay->ink_rect.x += overlay->ink_rect.width;

    tmp = overlay->logical_rect;
    overlay->logical_rect.x = tmp.y;
    overlay->logical_rect.y = tmp.x;
    overlay->logical_rect.width = tmp.height;
    overlay->logical_rect.height = tmp.width;
    overlay->logical_rect.x += overlay->logical_rect.width;
  }

  /* scale to reported window size */
  width = ceil (width * overlay->render_scale);
  height = ceil (height * overlay->render_scale);
  scalef_x *= overlay->render_scale;
  scalef_y *= overlay->render_scale;

  if (width <= 0 || height <= 0) {
    GST_DEBUG_OBJECT (overlay,
        "Overlay is outside video frame. Skipping text rendering");
    return;
  }

  if (unscaled_height <= 0 || unscaled_width <= 0) {
    GST_DEBUG_OBJECT (overlay,
        "Overlay is outside video frame. Skipping text rendering");
    return;
  }
  /* Prepare the transformation matrix. Note that the transformation happens
   * in reverse order. So for horizontal text, we will translate and then
   * scale. This is important to understand which scale shall be used. */
  /* So, as this init'ed scale happens last, when the rectangle has already
   * been rotated, the scaling applied to text height (up to now),
   * has to be applied along the x-axis */
  if (overlay->use_vertical_render) {
    double tmp;

    tmp = scalef_x;
    scalef_x = scalef_y;
    scalef_y = tmp;
  }
  cairo_matrix_init_scale (&cairo_matrix, scalef_x, scalef_y);

  if (overlay->use_vertical_render) {
    gint tmp;

    /* translate to the center of the image, rotate, and translate the rotated
     * image back to the right place */
    cairo_matrix_translate (&cairo_matrix, unscaled_height / 2.0l,
        unscaled_width / 2.0l);
    /* 90 degree clockwise rotation which is PI / 2 in radiants */
    cairo_matrix_rotate (&cairo_matrix, G_PI_2);
    cairo_matrix_translate (&cairo_matrix, -(unscaled_width / 2.0l),
        -(unscaled_height / 2.0l));

    /* Swap width and height */
    tmp = height;
    height = width;
    width = tmp;
  }

  cairo_matrix_translate (&cairo_matrix,
      ceil (outline_offset / 2.0l) - ink_rect.x,
      ceil (outline_offset / 2.0l) - ink_rect.y);

  /* reallocate overlay buffer */
  buffer = gst_buffer_new_and_alloc (4 * width * height);
  gst_buffer_replace (&overlay->text_image, buffer);
  gst_buffer_unref (buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  surface = cairo_image_surface_create_for_data (map.data,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cr = cairo_create (surface);

  /* clear surface */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* apply transformations */
  cairo_set_matrix (cr, &cairo_matrix);

  /* FIXME: We use show_layout everywhere except for the surface
   * because it's really faster and internally does all kinds of
   * caching. Unfortunately we have to paint to a cairo path for
   * the outline and this is slow. Once Pango supports user fonts
   * we should use them, see
   * https://bugzilla.gnome.org/show_bug.cgi?id=598695
   *
   * Idea would the be, to create a cairo user font that
   * does shadow, outline, text painting in the
   * render_glyph function.
   */

  /* draw shadow text */
  if (overlay->draw_shadow) {
    PangoAttrList *origin_attr, *filtered_attr, *temp_attr;

    /* Store a ref on the original attributes for later restoration */
    origin_attr =
        pango_attr_list_ref (pango_layout_get_attributes (overlay->layout));
    /* Take a copy of the original attributes, because pango_attr_list_filter
     * modifies the passed list */
    temp_attr = pango_attr_list_copy (origin_attr);
    filtered_attr =
        pango_attr_list_filter (temp_attr,
        gst_text_overlay_filter_foreground_attr, NULL);
    pango_attr_list_unref (temp_attr);

    cairo_save (cr);
    cairo_translate (cr, overlay->shadow_offset, overlay->shadow_offset);
    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
    pango_layout_set_attributes (overlay->layout, filtered_attr);
    pango_cairo_show_layout (cr, overlay->layout);
    pango_layout_set_attributes (overlay->layout, origin_attr);
    pango_attr_list_unref (filtered_attr);
    pango_attr_list_unref (origin_attr);
    cairo_restore (cr);
  }

  /* draw outline text */
  if (overlay->draw_outline) {
    a = (overlay->outline_color >> 24) & 0xff;
    r = (overlay->outline_color >> 16) & 0xff;
    g = (overlay->outline_color >> 8) & 0xff;
    b = (overlay->outline_color >> 0) & 0xff;

    cairo_save (cr);
    cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
    cairo_set_line_width (cr, overlay->outline_offset);
    pango_cairo_layout_path (cr, overlay->layout);
    cairo_stroke (cr);
    cairo_restore (cr);
  }

  a = (overlay->color >> 24) & 0xff;
  r = (overlay->color >> 16) & 0xff;
  g = (overlay->color >> 8) & 0xff;
  b = (overlay->color >> 0) & 0xff;

  /* draw text */
  cairo_save (cr);
  cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
  pango_cairo_show_layout (cr, overlay->layout);
  cairo_restore (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (buffer, &map);
  if (width != 0)
    overlay->text_width = width;
  if (height != 0)
    overlay->text_height = height;

  gst_base_text_overlay_set_composition (overlay);
}

static inline void
gst_base_text_overlay_shade_planar_Y (GstBaseTextOverlay * overlay,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j, dest_stride;
  guint8 *dest_ptr;

  dest_stride = dest->info.stride[0];
  dest_ptr = dest->data[0];

  for (i = y0; i < y1; ++i) {
    for (j = x0; j < x1; ++j) {
      gint y = dest_ptr[(i * dest_stride) + j] - overlay->shading_value;

      dest_ptr[(i * dest_stride) + j] = CLAMP (y, 0, 255);
    }
  }
}

static inline void
gst_base_text_overlay_shade_packed_Y (GstBaseTextOverlay * overlay,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j;
  guint dest_stride, pixel_stride;
  guint8 *dest_ptr;

  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (dest, 0);
  dest_ptr = GST_VIDEO_FRAME_COMP_DATA (dest, 0);
  pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (dest, 0);

  if (x0 != 0)
    x0 = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (dest->info.finfo, 0, x0);
  if (x1 != 0)
    x1 = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (dest->info.finfo, 0, x1);

  if (y0 != 0)
    y0 = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (dest->info.finfo, 0, y0);
  if (y1 != 0)
    y1 = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (dest->info.finfo, 0, y1);

  for (i = y0; i < y1; i++) {
    for (j = x0; j < x1; j++) {
      gint y;
      gint y_pos;

      y_pos = (i * dest_stride) + j * pixel_stride;
      y = dest_ptr[y_pos] - overlay->shading_value;

      dest_ptr[y_pos] = CLAMP (y, 0, 255);
    }
  }
}

#define gst_base_text_overlay_shade_BGRx gst_base_text_overlay_shade_xRGB
#define gst_base_text_overlay_shade_RGBx gst_base_text_overlay_shade_xRGB
#define gst_base_text_overlay_shade_xBGR gst_base_text_overlay_shade_xRGB
static inline void
gst_base_text_overlay_shade_xRGB (GstBaseTextOverlay * overlay,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j;
  guint8 *dest_ptr;

  dest_ptr = dest->data[0];

  for (i = y0; i < y1; i++) {
    for (j = x0; j < x1; j++) {
      gint y, y_pos, k;

      y_pos = (i * 4 * overlay->width) + j * 4;
      for (k = 0; k < 4; k++) {
        y = dest_ptr[y_pos + k] - overlay->shading_value;
        dest_ptr[y_pos + k] = CLAMP (y, 0, 255);
      }
    }
  }
}

/* FIXME: orcify */
static void
gst_base_text_overlay_shade_rgb24 (GstBaseTextOverlay * overlay,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  const int pstride = 3;
  gint y, x, stride, shading_val, tmp;
  guint8 *p;

  shading_val = -overlay->shading_value;
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  for (y = y0; y < y1; ++y) {
    p = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
    p += (y * stride) + (x0 * pstride);
    for (x = x0; x < x1; ++x) {
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
    }
  }
}

static void
gst_base_text_overlay_shade_IYU1 (GstBaseTextOverlay * overlay,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  gint y, x, stride, shading_val, tmp;
  guint8 *p;

  shading_val = -overlay->shading_value;
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  /* IYU1: packed 4:1:1 YUV (Cb-Y0-Y1-Cr-Y2-Y3 ...) */
  for (y = y0; y < y1; ++y) {
    p = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
    /* move to Y0 or Y1 (we pretend the chroma is the last of the 3 bytes) */
    /* FIXME: we're not pixel-exact here if x0 is an odd number, but it's
     * unlikely anyone will notice.. */
    p += (y * stride) + ((x0 / 2) * 3) + 1;
    for (x = x0; x < x1; x += 2) {
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      /* skip chroma */
      p++;
    }
  }
}

#define ARGB_SHADE_FUNCTION(name, OFFSET)	\
static inline void \
gst_base_text_overlay_shade_##name (GstBaseTextOverlay * overlay, GstVideoFrame * dest, \
gint x0, gint x1, gint y0, gint y1) \
{ \
  gint i, j;\
  guint8 *dest_ptr;\
  \
  dest_ptr = dest->data[0];\
  \
  for (i = y0; i < y1; i++) {\
    for (j = x0; j < x1; j++) {\
      gint y, y_pos, k;\
      y_pos = (i * 4 * overlay->width) + j * 4;\
      for (k = OFFSET; k < 3+OFFSET; k++) {\
        y = dest_ptr[y_pos + k] - overlay->shading_value;\
        dest_ptr[y_pos + k] = CLAMP (y, 0, 255);\
      }\
    }\
  }\
}
ARGB_SHADE_FUNCTION (ARGB, 1);
ARGB_SHADE_FUNCTION (ABGR, 1);
ARGB_SHADE_FUNCTION (RGBA, 0);
ARGB_SHADE_FUNCTION (BGRA, 0);

static void
gst_base_text_overlay_render_text (GstBaseTextOverlay * overlay,
    const gchar * text, gint textlen)
{
  gchar *string;

  if (!overlay->need_render) {
    GST_DEBUG ("Using previously rendered text.");
    return;
  }

  /* -1 is the whole string */
  if (text != NULL && textlen < 0) {
    textlen = strlen (text);
  }

  if (text != NULL) {
    string = g_strndup (text, textlen);
  } else {                      /* empty string */
    string = g_strdup (" ");
  }
  g_strdelimit (string, "\r\t", ' ');
  textlen = strlen (string);

  /* FIXME: should we check for UTF-8 here? */

  GST_DEBUG ("Rendering '%s'", string);
  gst_base_text_overlay_render_pangocairo (overlay, string, textlen);

  g_free (string);

  overlay->need_render = FALSE;
}

/* FIXME: should probably be relative to width/height (adjusted for PAR) */
#define BOX_XPAD  6
#define BOX_YPAD  6

static void
gst_base_text_overlay_shade_background (GstBaseTextOverlay * overlay,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  x0 = CLAMP (x0 - BOX_XPAD, 0, overlay->width);
  x1 = CLAMP (x1 + BOX_XPAD, 0, overlay->width);

  y0 = CLAMP (y0 - BOX_YPAD, 0, overlay->height);
  y1 = CLAMP (y1 + BOX_YPAD, 0, overlay->height);

  switch (overlay->format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_YUV9:
    case GST_VIDEO_FORMAT_YVU9:
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_A420:
      gst_base_text_overlay_shade_planar_Y (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_VYUY:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_v308:
    case GST_VIDEO_FORMAT_IYU2:
      gst_base_text_overlay_shade_packed_Y (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_xRGB:
      gst_base_text_overlay_shade_xRGB (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_xBGR:
      gst_base_text_overlay_shade_xBGR (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGRx:
      gst_base_text_overlay_shade_BGRx (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_RGBx:
      gst_base_text_overlay_shade_RGBx (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_ARGB:
      gst_base_text_overlay_shade_ARGB (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_ABGR:
      gst_base_text_overlay_shade_ABGR (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_RGBA:
      gst_base_text_overlay_shade_RGBA (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGRA:
      gst_base_text_overlay_shade_BGRA (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_RGB:
      gst_base_text_overlay_shade_rgb24 (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_IYU1:
      gst_base_text_overlay_shade_IYU1 (overlay, frame, x0, x1, y0, y1);
      break;
    default:
      GST_FIXME_OBJECT (overlay, "implement background shading for format %s",
          gst_video_format_to_string (GST_VIDEO_FRAME_FORMAT (frame)));
      break;
  }
}

static GstFlowReturn
gst_base_text_overlay_push_frame (GstBaseTextOverlay * overlay,
    GstBuffer * video_frame)
{
  GstVideoFrame frame;

  if (overlay->composition == NULL)
    goto done;

  if (gst_pad_check_reconfigure (overlay->srcpad)) {
    if (!gst_base_text_overlay_negotiate (overlay, NULL)) {
      gst_pad_mark_reconfigure (overlay->srcpad);
      gst_buffer_unref (video_frame);
      if (GST_PAD_IS_FLUSHING (overlay->srcpad))
        return GST_FLOW_FLUSHING;
      else
        return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  video_frame = gst_buffer_make_writable (video_frame);

  if (overlay->attach_compo_to_buffer) {
    GST_DEBUG_OBJECT (overlay, "Attaching text overlay image to video buffer");
    gst_buffer_add_video_overlay_composition_meta (video_frame,
        overlay->composition);
    /* FIXME: emulate shaded background box if want_shading=true */
    goto done;
  }

  if (!gst_video_frame_map (&frame, &overlay->info, video_frame,
          GST_MAP_READWRITE))
    goto invalid_frame;

  /* shaded background box */
  if (overlay->want_shading) {
    gint xpos, ypos;

    gst_base_text_overlay_get_pos (overlay, &xpos, &ypos);

    gst_base_text_overlay_shade_background (overlay, &frame,
        xpos, xpos + overlay->text_width, ypos, ypos + overlay->text_height);
  }

  gst_video_overlay_composition_blend (overlay->composition, &frame);

  gst_video_frame_unmap (&frame);

done:

  return gst_pad_push (overlay->srcpad, video_frame);

  /* ERRORS */
invalid_frame:
  {
    gst_buffer_unref (video_frame);
    GST_DEBUG_OBJECT (overlay, "received invalid buffer");
    return GST_FLOW_OK;
  }
}

static GstPadLinkReturn
gst_base_text_overlay_text_pad_link (GstPad * pad, GstObject * parent,
    GstPad * peer)
{
  GstBaseTextOverlay *overlay;

  overlay = GST_BASE_TEXT_OVERLAY (parent);
  if (G_UNLIKELY (!overlay))
    return GST_PAD_LINK_REFUSED;

  GST_DEBUG_OBJECT (overlay, "Text pad linked");

  overlay->text_linked = TRUE;

  return GST_PAD_LINK_OK;
}

static void
gst_base_text_overlay_text_pad_unlink (GstPad * pad, GstObject * parent)
{
  GstBaseTextOverlay *overlay;

  /* don't use gst_pad_get_parent() here, will deadlock */
  overlay = GST_BASE_TEXT_OVERLAY (parent);

  GST_DEBUG_OBJECT (overlay, "Text pad unlinked");

  overlay->text_linked = FALSE;

  gst_segment_init (&overlay->text_segment, GST_FORMAT_UNDEFINED);
}

static gboolean
gst_base_text_overlay_text_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstBaseTextOverlay *overlay = NULL;

  overlay = GST_BASE_TEXT_OVERLAY (parent);

  GST_LOG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
      /* Clear any pending EOS and segment on a new stream start */
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "text stream-start");
      overlay->text_flushing = FALSE;
      overlay->text_eos = FALSE;
      gst_base_text_overlay_pop_text (overlay);
      gst_segment_init (&overlay->text_segment, GST_FORMAT_TIME);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_base_text_overlay_setcaps_txt (overlay, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      overlay->text_eos = FALSE;

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_BASE_TEXT_OVERLAY_LOCK (overlay);
        gst_segment_copy_into (segment, &overlay->text_segment);
        GST_DEBUG_OBJECT (overlay, "TEXT SEGMENT now: %" GST_SEGMENT_FORMAT,
            &overlay->text_segment);
        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      } else {
        GST_ELEMENT_WARNING (overlay, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on text input"));
      }

      gst_event_unref (event);
      ret = TRUE;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_BASE_TEXT_OVERLAY_BROADCAST (overlay);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      break;
    }
    case GST_EVENT_GAP:
    {
      GstClockTime start, duration;

      gst_event_parse_gap (event, &start, &duration);
      if (GST_CLOCK_TIME_IS_VALID (duration))
        start += duration;
      /* we do not expect another buffer until after gap,
       * so that is our position now */
      overlay->text_segment.position = start;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_BASE_TEXT_OVERLAY_BROADCAST (overlay);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);

      gst_event_unref (event);
      ret = TRUE;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "text flush stop");
      overlay->text_flushing = FALSE;
      overlay->text_eos = FALSE;
      gst_base_text_overlay_pop_text (overlay);
      gst_segment_init (&overlay->text_segment, GST_FORMAT_TIME);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_FLUSH_START:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "text flush start");
      overlay->text_flushing = TRUE;
      GST_BASE_TEXT_OVERLAY_BROADCAST (overlay);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_EOS:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      overlay->text_eos = TRUE;
      GST_INFO_OBJECT (overlay, "text EOS");
      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_BASE_TEXT_OVERLAY_BROADCAST (overlay);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      gst_event_unref (event);
      ret = TRUE;
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_base_text_overlay_video_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstBaseTextOverlay *overlay = NULL;

  overlay = GST_BASE_TEXT_OVERLAY (parent);

  GST_DEBUG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
      /* Clear any EOS and segment on a new stream */
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video stream-start");
      overlay->video_flushing = FALSE;
      overlay->video_eos = FALSE;
      gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_base_text_overlay_setcaps (overlay, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      GST_DEBUG_OBJECT (overlay, "received new segment");

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        gst_segment_copy_into (segment, &overlay->segment);
        GST_DEBUG_OBJECT (overlay, "VIDEO SEGMENT now: %" GST_SEGMENT_FORMAT,
            &overlay->segment);
      } else {
        GST_ELEMENT_WARNING (overlay, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on video input"));
      }

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video EOS");
      overlay->video_eos = TRUE;
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_START:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video flush start");
      overlay->video_flushing = TRUE;
      GST_BASE_TEXT_OVERLAY_BROADCAST (overlay);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video flush stop");
      overlay->video_flushing = FALSE;
      overlay->video_eos = FALSE;
      gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_base_text_overlay_video_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstBaseTextOverlay *overlay;

  overlay = GST_BASE_TEXT_OVERLAY (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_base_text_overlay_get_videosink_caps (pad, overlay, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

/* Called with lock held */
static void
gst_base_text_overlay_pop_text (GstBaseTextOverlay * overlay)
{
  g_return_if_fail (GST_IS_BASE_TEXT_OVERLAY (overlay));

  if (overlay->text_buffer) {
    GST_DEBUG_OBJECT (overlay, "releasing text buffer %p",
        overlay->text_buffer);
    gst_buffer_unref (overlay->text_buffer);
    overlay->text_buffer = NULL;
    overlay->text_buffer_running_time = overlay->text_buffer_running_time_end =
        GST_CLOCK_TIME_NONE;
  }

  /* Let the text task know we used that buffer */
  GST_BASE_TEXT_OVERLAY_BROADCAST (overlay);
}

/* We receive text buffers here. If they are out of segment we just ignore them.
   If the buffer is in our segment we keep it internally except if another one
   is already waiting here, in that case we wait that it gets kicked out */
static GstFlowReturn
gst_base_text_overlay_text_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBaseTextOverlay *overlay = NULL;
  gboolean in_seg = FALSE;
  guint64 clip_start = 0, clip_stop = 0;

  overlay = GST_BASE_TEXT_OVERLAY (parent);

  GST_BASE_TEXT_OVERLAY_LOCK (overlay);

  if (overlay->text_flushing) {
    GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
    ret = GST_FLOW_FLUSHING;
    GST_LOG_OBJECT (overlay, "text flushing");
    goto beach;
  }

  if (overlay->text_eos) {
    GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
    ret = GST_FLOW_EOS;
    GST_LOG_OBJECT (overlay, "text EOS");
    goto beach;
  }

  GST_LOG_OBJECT (overlay, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &overlay->segment,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer) +
          GST_BUFFER_DURATION (buffer)));

  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
    GstClockTime stop;

    if (G_LIKELY (GST_BUFFER_DURATION_IS_VALID (buffer)))
      stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    else
      stop = GST_CLOCK_TIME_NONE;

    in_seg = gst_segment_clip (&overlay->text_segment, GST_FORMAT_TIME,
        GST_BUFFER_TIMESTAMP (buffer), stop, &clip_start, &clip_stop);
  } else {
    in_seg = TRUE;
  }

  if (in_seg) {
    /* about to change metadata */
    buffer = gst_buffer_make_writable (buffer);
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    if (GST_BUFFER_DURATION_IS_VALID (buffer))
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;

    /* Wait for the previous buffer to go away */
    while (overlay->text_buffer != NULL) {
      GST_DEBUG ("Pad %s:%s has a buffer queued, waiting",
          GST_DEBUG_PAD_NAME (pad));
      GST_BASE_TEXT_OVERLAY_WAIT (overlay);
      GST_DEBUG ("Pad %s:%s resuming", GST_DEBUG_PAD_NAME (pad));
      if (overlay->text_flushing) {
        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
        ret = GST_FLOW_FLUSHING;
        goto beach;
      }
    }

    /* Calculate and store the running time for this text buffer in 
     * the current segment. We might receive a new text pad segment
     * event while this buffer is still active, and that would
     * lead to incorrect running time calculations if we did it later.
     */
    overlay->text_buffer_running_time = overlay->text_buffer_running_time_end =
        GST_CLOCK_TIME_NONE;

    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
      GstClockTime text_start = GST_BUFFER_TIMESTAMP (buffer);

      overlay->text_segment.position = clip_start;
      overlay->text_buffer_running_time =
          gst_segment_to_running_time (&overlay->text_segment,
          GST_FORMAT_TIME, text_start);

      if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
        GstClockTime text_end = text_start + GST_BUFFER_DURATION (buffer);
        overlay->text_buffer_running_time_end =
            gst_segment_to_running_time (&overlay->text_segment,
            GST_FORMAT_TIME, text_end);
      }
    }

    overlay->text_buffer = buffer;      /* pass ownership of @buffer */
    buffer = NULL;

    /* That's a new text buffer we need to render */
    overlay->need_render = TRUE;

    /* in case the video chain is waiting for a text buffer, wake it up */
    GST_BASE_TEXT_OVERLAY_BROADCAST (overlay);
  }

  GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);

beach:
  if (buffer)
    gst_buffer_unref (buffer);

  return ret;
}

static GstFlowReturn
gst_base_text_overlay_video_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstBaseTextOverlayClass *klass;
  GstBaseTextOverlay *overlay;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean in_seg = FALSE;
  guint64 start, stop, clip_start = 0, clip_stop = 0;
  gchar *text = NULL;
  GstVideoOverlayCompositionMeta *composition_meta;

  overlay = GST_BASE_TEXT_OVERLAY (parent);

  composition_meta = gst_buffer_get_video_overlay_composition_meta (buffer);
  if (composition_meta) {
    if (overlay->upstream_composition != composition_meta->overlay) {
      GST_DEBUG ("GstVideoOverlayCompositionMeta found.");
      overlay->upstream_composition = composition_meta->overlay;
      overlay->need_render = TRUE;
    }
  } else if (overlay->upstream_composition != NULL) {
    overlay->upstream_composition = NULL;
    overlay->need_render = TRUE;
  }

  klass = GST_BASE_TEXT_OVERLAY_GET_CLASS (overlay);

  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    goto missing_timestamp;

  /* ignore buffers that are outside of the current segment */
  start = GST_BUFFER_TIMESTAMP (buffer);

  if (!GST_BUFFER_DURATION_IS_VALID (buffer)) {
    stop = GST_CLOCK_TIME_NONE;
  } else {
    stop = start + GST_BUFFER_DURATION (buffer);
  }

  GST_LOG_OBJECT (overlay, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &overlay->segment,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* segment_clip() will adjust start unconditionally to segment_start if
   * no stop time is provided, so handle this ourselves */
  if (stop == GST_CLOCK_TIME_NONE && start < overlay->segment.start)
    goto out_of_segment;

  in_seg = gst_segment_clip (&overlay->segment, GST_FORMAT_TIME, start, stop,
      &clip_start, &clip_stop);

  if (!in_seg)
    goto out_of_segment;

  /* if the buffer is only partially in the segment, fix up stamps */
  if (clip_start != start || (stop != -1 && clip_stop != stop)) {
    GST_DEBUG_OBJECT (overlay, "clipping buffer timestamp/duration to segment");
    buffer = gst_buffer_make_writable (buffer);
    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    if (stop != -1)
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;
  }

  /* now, after we've done the clipping, fix up end time if there's no
   * duration (we only use those estimated values internally though, we
   * don't want to set bogus values on the buffer itself) */
  if (stop == -1) {
    if (overlay->info.fps_n && overlay->info.fps_d) {
      GST_DEBUG_OBJECT (overlay, "estimating duration based on framerate");
      stop = start + gst_util_uint64_scale_int (GST_SECOND,
          overlay->info.fps_d, overlay->info.fps_n);
    } else {
      GST_LOG_OBJECT (overlay, "no duration, assuming minimal duration");
      stop = start + 1;         /* we need to assume some interval */
    }
  }

  gst_object_sync_values (GST_OBJECT (overlay), GST_BUFFER_TIMESTAMP (buffer));

wait_for_text_buf:

  GST_BASE_TEXT_OVERLAY_LOCK (overlay);

  if (overlay->video_flushing)
    goto flushing;

  if (overlay->video_eos)
    goto have_eos;

  if (overlay->silent) {
    GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
    ret = gst_pad_push (overlay->srcpad, buffer);

    /* Update position */
    overlay->segment.position = clip_start;

    return ret;
  }

  /* Text pad not linked, rendering internal text */
  if (!overlay->text_linked) {
    if (klass->get_text) {
      text = klass->get_text (overlay, buffer);
    } else {
      text = g_strdup (overlay->default_text);
    }

    GST_LOG_OBJECT (overlay, "Text pad not linked, rendering default "
        "text: '%s'", GST_STR_NULL (text));

    GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);

    if (text != NULL && *text != '\0') {
      /* Render and push */
      gst_base_text_overlay_render_text (overlay, text, -1);
      ret = gst_base_text_overlay_push_frame (overlay, buffer);
    } else {
      /* Invalid or empty string */
      ret = gst_pad_push (overlay->srcpad, buffer);
    }
  } else {
    /* Text pad linked, check if we have a text buffer queued */
    if (overlay->text_buffer) {
      gboolean pop_text = FALSE, valid_text_time = TRUE;
      GstClockTime text_running_time = overlay->text_buffer_running_time;
      GstClockTime text_running_time_end =
          overlay->text_buffer_running_time_end;
      GstClockTime vid_running_time, vid_running_time_end;

      /* if the text buffer isn't stamped right, pop it off the
       * queue and display it for the current video frame only */
      if (!GST_CLOCK_TIME_IS_VALID (overlay->text_buffer_running_time) ||
          !GST_CLOCK_TIME_IS_VALID (overlay->text_buffer_running_time_end)) {
        GST_WARNING_OBJECT (overlay,
            "Got text buffer with invalid timestamp or duration");
        pop_text = TRUE;
        valid_text_time = FALSE;
      }

      vid_running_time =
          gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
          start);
      vid_running_time_end =
          gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
          stop);

      GST_LOG_OBJECT (overlay, "T: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (text_running_time),
          GST_TIME_ARGS (text_running_time_end));
      GST_LOG_OBJECT (overlay, "V: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (vid_running_time),
          GST_TIME_ARGS (vid_running_time_end));

      /* Text too old or in the future */
      if (valid_text_time && text_running_time_end <= vid_running_time) {
        /* text buffer too old, get rid of it and do nothing  */
        GST_LOG_OBJECT (overlay, "text buffer too old, popping");
        pop_text = FALSE;
        gst_base_text_overlay_pop_text (overlay);
        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
        goto wait_for_text_buf;
      } else if (valid_text_time && vid_running_time_end <= text_running_time) {
        GST_LOG_OBJECT (overlay, "text in future, pushing video buf");
        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
        /* Push the video frame */
        ret = gst_pad_push (overlay->srcpad, buffer);
      } else {
        GstMapInfo map;
        gchar *in_text;
        gsize in_size;

        gst_buffer_map (overlay->text_buffer, &map, GST_MAP_READ);
        in_text = (gchar *) map.data;
        in_size = map.size;

        if (in_size > 0) {
          /* g_markup_escape_text() absolutely requires valid UTF8 input, it
           * might crash otherwise. We don't fall back on GST_SUBTITLE_ENCODING
           * here on purpose, this is something that needs fixing upstream */
          if (!g_utf8_validate (in_text, in_size, NULL)) {
            const gchar *end = NULL;

            GST_WARNING_OBJECT (overlay, "received invalid UTF-8");
            in_text = g_strndup (in_text, in_size);
            while (!g_utf8_validate (in_text, in_size, &end) && end)
              *((gchar *) end) = '*';
          }

          /* Get the string */
          if (overlay->have_pango_markup) {
            text = g_strndup (in_text, in_size);
          } else {
            text = g_markup_escape_text (in_text, in_size);
          }

          if (text != NULL && *text != '\0') {
            gint text_len = strlen (text);

            while (text_len > 0 && (text[text_len - 1] == '\n' ||
                    text[text_len - 1] == '\r')) {
              --text_len;
            }
            GST_DEBUG_OBJECT (overlay, "Rendering text '%*s'", text_len, text);
            gst_base_text_overlay_render_text (overlay, text, text_len);
          } else {
            GST_DEBUG_OBJECT (overlay, "No text to render (empty buffer)");
            gst_base_text_overlay_render_text (overlay, " ", 1);
          }
          if (in_text != (gchar *) map.data)
            g_free (in_text);
        } else {
          GST_DEBUG_OBJECT (overlay, "No text to render (empty buffer)");
          gst_base_text_overlay_render_text (overlay, " ", 1);
        }

        gst_buffer_unmap (overlay->text_buffer, &map);

        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
        ret = gst_base_text_overlay_push_frame (overlay, buffer);

        if (valid_text_time && text_running_time_end <= vid_running_time_end) {
          GST_LOG_OBJECT (overlay, "text buffer not needed any longer");
          pop_text = TRUE;
        }
      }
      if (pop_text) {
        GST_BASE_TEXT_OVERLAY_LOCK (overlay);
        gst_base_text_overlay_pop_text (overlay);
        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      }
    } else {
      gboolean wait_for_text_buf = TRUE;

      if (overlay->text_eos)
        wait_for_text_buf = FALSE;

      if (!overlay->wait_text)
        wait_for_text_buf = FALSE;

      /* Text pad linked, but no text buffer available - what now? */
      if (overlay->text_segment.format == GST_FORMAT_TIME) {
        GstClockTime text_start_running_time, text_position_running_time;
        GstClockTime vid_running_time;

        vid_running_time =
            gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
            GST_BUFFER_TIMESTAMP (buffer));
        text_start_running_time =
            gst_segment_to_running_time (&overlay->text_segment,
            GST_FORMAT_TIME, overlay->text_segment.start);
        text_position_running_time =
            gst_segment_to_running_time (&overlay->text_segment,
            GST_FORMAT_TIME, overlay->text_segment.position);

        if ((GST_CLOCK_TIME_IS_VALID (text_start_running_time) &&
                vid_running_time < text_start_running_time) ||
            (GST_CLOCK_TIME_IS_VALID (text_position_running_time) &&
                vid_running_time < text_position_running_time)) {
          wait_for_text_buf = FALSE;
        }
      }

      if (wait_for_text_buf) {
        GST_DEBUG_OBJECT (overlay, "no text buffer, need to wait for one");
        GST_BASE_TEXT_OVERLAY_WAIT (overlay);
        GST_DEBUG_OBJECT (overlay, "resuming");
        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
        goto wait_for_text_buf;
      } else {
        GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
        GST_LOG_OBJECT (overlay, "no need to wait for a text buffer");
        ret = gst_pad_push (overlay->srcpad, buffer);
      }
    }
  }

  g_free (text);

  /* Update position */
  overlay->segment.position = clip_start;

  return ret;

missing_timestamp:
  {
    GST_WARNING_OBJECT (overlay, "buffer without timestamp, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

flushing:
  {
    GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
    GST_DEBUG_OBJECT (overlay, "flushing, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_FLUSHING;
  }
have_eos:
  {
    GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
    GST_DEBUG_OBJECT (overlay, "eos, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_EOS;
  }
out_of_segment:
  {
    GST_DEBUG_OBJECT (overlay, "buffer out of segment, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

static GstStateChangeReturn
gst_base_text_overlay_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstBaseTextOverlay *overlay = GST_BASE_TEXT_OVERLAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      overlay->text_flushing = TRUE;
      overlay->video_flushing = TRUE;
      /* pop_text will broadcast on the GCond and thus also make the video
       * chain exit if it's waiting for a text buffer */
      gst_base_text_overlay_pop_text (overlay);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      break;
    default:
      break;
  }

  ret = parent_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_BASE_TEXT_OVERLAY_LOCK (overlay);
      overlay->text_flushing = FALSE;
      overlay->video_flushing = FALSE;
      overlay->video_eos = FALSE;
      overlay->text_eos = FALSE;
      gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
      gst_segment_init (&overlay->text_segment, GST_FORMAT_TIME);
      GST_BASE_TEXT_OVERLAY_UNLOCK (overlay);
      break;
    default:
      break;
  }

  return ret;
}
