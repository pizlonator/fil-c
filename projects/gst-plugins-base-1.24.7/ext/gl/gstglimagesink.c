/*
 * GStreamer
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 * Copyright (C) 2005,2006,2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-glimagesink
 * @title: glimagesink
 *
 * glimagesink renders video frames to a drawable on a local or remote
 * display using OpenGL. This element can receive a Window ID from the
 * application through the VideoOverlay interface and will then render video
 * frames in this drawable.
 * If no Window ID was provided by the application, the element will
 * create its own internal window and render into it.
 *
 * See the #GstGLDisplay documentation for a list of environment variables that
 * can override window/platform detection.
 *
 * ## Scaling
 *
 * Depends on the driver, OpenGL handles hardware accelerated
 * scaling of video frames. This means that the element will just accept
 * incoming video frames no matter their geometry and will then put them to the
 * drawable scaling them on the fly. Using the #GstGLImageSink:force-aspect-ratio
 * property it is possible to enforce scaling with a constant aspect ratio,
 * which means drawing black borders around the video frame.
 *
 * ## Events
 *
 * Through the gl thread, glimagesink handle some events coming from the drawable
 * to manage its appearance even when the data is not flowing (GST_STATE_PAUSED).
 * That means that even when the element is paused, it will receive expose events
 * from the drawable and draw the latest frame with correct borders/aspect-ratio.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 -v videotestsrc ! video/x-raw ! glimagesink
 * ]| A pipeline to test hardware scaling.
 * No special opengl extension is used in this pipeline, that's why it should work
 * with OpenGL >= 1.1. That's the case if you are using the MESA3D driver v1.3.
 * |[
 * gst-launch-1.0 -v videotestsrc ! video/x-raw,format=I420 ! glimagesink
 * ]| A pipeline to test hardware scaling and hardware colorspace conversion.
 * When your driver supports GLSL (OpenGL Shading Language needs OpenGL >= 2.1),
 * the 4 following format YUY2, UYVY, I420, YV12 and AYUV are converted to RGB32
 * through some fragment shaders and using one framebuffer (FBO extension OpenGL >= 1.4).
 * If your driver does not support GLSL but supports MESA_YCbCr extension then
 * the you can use YUY2 and UYVY. In this case the colorspace conversion is automatically
 * made when loading the texture and therefore no framebuffer is used.
 * |[
 * gst-launch-1.0 -v gltestsrc ! glimagesink
 * ]| A pipeline 100% OpenGL.
 * No special opengl extension is used in this pipeline, that's why it should work
 * with OpenGL >= 1.1. That's the case if you are using the MESA3D driver v1.3.
 * |[
 * gst-plugins-base/tests/examples/gl/generic/cube
 * ]| The graphic FPS scene can be greater than the input video FPS.
 * The graphic scene can be written from a client code through the
 * two glfilterapp properties.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/videooverlay.h>
#include <gst/video/navigation.h>
#include <gst/video/gstvideoaffinetransformationmeta.h>

#include "gstglelements.h"
#include "gstglimagesink.h"
#include "gstglsinkbin.h"
#include "gstglutils.h"

#include <gst/gl/gstglviewconvert.h>

GST_DEBUG_CATEGORY (gst_debug_glimage_sink);
#define GST_CAT_DEFAULT gst_debug_glimage_sink

#define DEFAULT_SHOW_PREROLL_FRAME  TRUE
#define DEFAULT_HANDLE_EVENTS       TRUE
#define DEFAULT_FORCE_ASPECT_RATIO  TRUE
#define DEFAULT_IGNORE_ALPHA        TRUE

#define DEFAULT_MULTIVIEW_MODE GST_VIDEO_MULTIVIEW_MODE_MONO
#define DEFAULT_MULTIVIEW_FLAGS GST_VIDEO_MULTIVIEW_FLAGS_NONE
#define DEFAULT_MULTIVIEW_DOWNMIX GST_GL_STEREO_DOWNMIX_ANAGLYPH_GREEN_MAGENTA_DUBOIS

typedef GstGLSinkBin GstGLImageSinkBin;
typedef GstGLSinkBinClass GstGLImageSinkBinClass;

G_DEFINE_TYPE (GstGLImageSinkBin, gst_gl_image_sink_bin, GST_TYPE_GL_SINK_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glimagesink, "glimagesink",
    GST_RANK_SECONDARY, gst_gl_image_sink_bin_get_type (),
    gl_element_init (plugin));

enum
{
  PROP_BIN_0,
  PROP_BIN_ROTATE_METHOD,
  PROP_BIN_FORCE_ASPECT_RATIO,
  PROP_BIN_PIXEL_ASPECT_RATIO,
  PROP_BIN_HANDLE_EVENTS,
  PROP_BIN_CONTEXT,
  PROP_BIN_IGNORE_ALPHA,
  PROP_BIN_SHOW_PREROLL_FRAME,
  PROP_BIN_OUTPUT_MULTIVIEW_LAYOUT,
  PROP_BIN_OUTPUT_MULTIVIEW_FLAGS,
  PROP_BIN_OUTPUT_MULTIVIEW_DOWNMIX_MODE,
  PROP_BIN_LAST
};

enum
{
  SIGNAL_BIN_0,
  SIGNAL_BIN_CLIENT_DRAW,
  SIGNAL_BIN_CLIENT_RESHAPE,
  SIGNAL_BIN_LAST,
};

static guint gst_gl_image_sink_bin_signals[SIGNAL_BIN_LAST] = { 0 };

static void
gst_gl_image_sink_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec)
{
  g_object_set_property (G_OBJECT (GST_GL_SINK_BIN (object)->sink),
      param_spec->name, value);
}

static void
gst_gl_image_sink_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec)
{
  g_object_get_property (G_OBJECT (GST_GL_SINK_BIN (object)->sink),
      param_spec->name, value);
}

static gboolean
_on_client_reshape (GstGLImageSink * sink, GstGLContext * context,
    guint width, guint height, gpointer data)
{
  gboolean ret;

  g_signal_emit (data, gst_gl_image_sink_bin_signals[SIGNAL_BIN_CLIENT_RESHAPE],
      0, context, width, height, &ret);

  return ret;
}

static gboolean
_on_client_draw (GstGLImageSink * sink, GstGLContext * context,
    GstSample * sample, gpointer data)
{
  gboolean ret;

  g_signal_emit (data, gst_gl_image_sink_bin_signals[SIGNAL_BIN_CLIENT_DRAW], 0,
      context, sample, &ret);

  return ret;
}

#define DEFAULT_ROTATE_METHOD GST_VIDEO_ORIENTATION_IDENTITY

#define GST_TYPE_GL_ROTATE_METHOD (gst_gl_rotate_method_get_type())

static const GEnumValue rotate_methods[] = {
  {GST_VIDEO_ORIENTATION_IDENTITY, "Identity (no rotation)", "none"},
  {GST_VIDEO_ORIENTATION_90R, "Rotate clockwise 90 degrees", "clockwise"},
  {GST_VIDEO_ORIENTATION_180, "Rotate 180 degrees", "rotate-180"},
  {GST_VIDEO_ORIENTATION_90L, "Rotate counter-clockwise 90 degrees",
      "counterclockwise"},
  {GST_VIDEO_ORIENTATION_HORIZ, "Flip horizontally", "horizontal-flip"},
  {GST_VIDEO_ORIENTATION_VERT, "Flip vertically", "vertical-flip"},
  {GST_VIDEO_ORIENTATION_UL_LR,
      "Flip across upper left/lower right diagonal", "upper-left-diagonal"},
  {GST_VIDEO_ORIENTATION_UR_LL,
      "Flip across upper right/lower left diagonal", "upper-right-diagonal"},
  {GST_VIDEO_ORIENTATION_AUTO,
      "Select rotate method based on image-orientation tag", "automatic"},
  {0, NULL, NULL},
};

static GType
gst_gl_rotate_method_get_type (void)
{
  static GType rotate_method_type = 0;

  if (!rotate_method_type) {
    rotate_method_type = g_enum_register_static ("GstGLRotateMethod",
        rotate_methods);
  }
  return rotate_method_type;
}

static void
gst_gl_image_sink_bin_init (GstGLImageSinkBin * self)
{
  GstGLImageSink *sink = g_object_new (GST_TYPE_GLIMAGE_SINK, NULL);

  g_signal_connect (sink, "client-reshape", (GCallback) _on_client_reshape,
      self);
  g_signal_connect (sink, "client-draw", (GCallback) _on_client_draw, self);

  gst_gl_sink_bin_finish_init_with_element (GST_GL_SINK_BIN (self),
      GST_ELEMENT (sink));
}

static void
gst_gl_image_sink_bin_class_init (GstGLImageSinkBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gst_gl_image_sink_bin_get_property;
  gobject_class->set_property = gst_gl_image_sink_bin_set_property;

  /* gl sink */
  g_object_class_install_property (gobject_class, PROP_BIN_ROTATE_METHOD,
      g_param_spec_enum ("rotate-method",
          "rotate method",
          "rotate method",
          GST_TYPE_GL_ROTATE_METHOD, DEFAULT_ROTATE_METHOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BIN_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BIN_HANDLE_EVENTS,
      g_param_spec_boolean ("handle-events", "Handle XEvents",
          "When enabled, XEvents will be selected and handled",
          DEFAULT_HANDLE_EVENTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BIN_IGNORE_ALPHA,
      g_param_spec_boolean ("ignore-alpha", "Ignore Alpha",
          "When enabled, alpha will be ignored and converted to black",
          DEFAULT_IGNORE_ALPHA, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BIN_CONTEXT,
      g_param_spec_object ("context", "OpenGL context", "Get OpenGL context",
          GST_TYPE_GL_CONTEXT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BIN_PIXEL_ASPECT_RATIO,
      gst_param_spec_fraction ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", 0, 1, G_MAXINT, 1, 1, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* video sink */
  g_object_class_install_property (gobject_class, PROP_BIN_SHOW_PREROLL_FRAME,
      g_param_spec_boolean ("show-preroll-frame", "Show preroll frame",
          "Whether to render video frames during preroll",
          DEFAULT_SHOW_PREROLL_FRAME,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_BIN_OUTPUT_MULTIVIEW_LAYOUT,
      g_param_spec_enum ("output-multiview-mode", "Output Multiview Mode",
          "Choose output mode for multiview/3D video",
          GST_TYPE_VIDEO_MULTIVIEW_MODE, DEFAULT_MULTIVIEW_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_BIN_OUTPUT_MULTIVIEW_FLAGS,
      g_param_spec_flags ("output-multiview-flags", "Output Multiview Flags",
          "Output multiview layout modifier flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, DEFAULT_MULTIVIEW_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_BIN_OUTPUT_MULTIVIEW_DOWNMIX_MODE,
      g_param_spec_enum ("output-multiview-downmix-mode",
          "Mode for mono downmixed output",
          "Output anaglyph type to generate when downmixing to mono",
          GST_TYPE_GL_STEREO_DOWNMIX, DEFAULT_MULTIVIEW_DOWNMIX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_video_overlay_install_properties (gobject_class, PROP_BIN_LAST);

  gst_gl_image_sink_bin_signals[SIGNAL_BIN_CLIENT_DRAW] =
      g_signal_new ("client-draw", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 2, GST_TYPE_GL_CONTEXT,
      GST_TYPE_SAMPLE);

  gst_gl_image_sink_bin_signals[SIGNAL_BIN_CLIENT_RESHAPE] =
      g_signal_new ("client-reshape", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 3,
      GST_TYPE_GL_CONTEXT, G_TYPE_UINT, G_TYPE_UINT);

  gst_type_mark_as_plugin_api (GST_TYPE_GL_ROTATE_METHOD, 0);
}

#define GST_GLIMAGE_SINK_GET_LOCK(glsink) \
  (GST_GLIMAGE_SINK(glsink)->drawing_lock)
#define GST_GLIMAGE_SINK_LOCK(glsink) \
  (g_mutex_lock(&GST_GLIMAGE_SINK_GET_LOCK (glsink)))
#define GST_GLIMAGE_SINK_UNLOCK(glsink) \
  (g_mutex_unlock(&GST_GLIMAGE_SINK_GET_LOCK (glsink)))

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

#define SUPPORTED_GL_APIS GST_GL_API_OPENGL | GST_GL_API_GLES2 | GST_GL_API_OPENGL3

static void gst_glimage_sink_thread_init_redisplay (GstGLImageSink * gl_sink);
static void gst_glimage_sink_cleanup_glthread (GstGLImageSink * gl_sink);
static void gst_glimage_sink_on_close (GstGLImageSink * gl_sink);
static void gst_glimage_sink_on_resize (GstGLImageSink * gl_sink,
    gint width, gint height);
static void gst_glimage_sink_on_draw (GstGLImageSink * gl_sink);
static gboolean gst_glimage_sink_redisplay (GstGLImageSink * gl_sink);

static void gst_glimage_sink_finalize (GObject * object);
static void gst_glimage_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_glimage_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static gboolean gst_glimage_sink_event (GstBaseSink * sink, GstEvent * event);
static gboolean gst_glimage_sink_query (GstBaseSink * bsink, GstQuery * query);
static void gst_glimage_sink_set_context (GstElement * element,
    GstContext * context);

static GstStateChangeReturn
gst_glimage_sink_change_state (GstElement * element, GstStateChange transition);

static void gst_glimage_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_glimage_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static GstCaps *gst_glimage_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static GstFlowReturn gst_glimage_sink_prepare (GstBaseSink * bsink,
    GstBuffer * buf);
static GstFlowReturn gst_glimage_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);
static gboolean gst_glimage_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);

static void gst_glimage_sink_video_overlay_init (GstVideoOverlayInterface *
    iface);
static void gst_glimage_sink_set_window_handle (GstVideoOverlay * overlay,
    guintptr id);
static void gst_glimage_sink_expose (GstVideoOverlay * overlay);
static void gst_glimage_sink_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint width, gint height);
static void gst_glimage_sink_handle_events (GstVideoOverlay * overlay,
    gboolean handle_events);
static gboolean update_output_format (GstGLImageSink * glimage_sink);

#define GST_GL_SINK_CAPS \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "              \
    "format = (string) RGBA, "                                          \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes } "                   \
    " ; "                                                               \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY ","                \
    GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION "), "           \
    "format = (string) RGBA, "                                          \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes } "

static GstStaticPadTemplate gst_glimage_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_GL_SINK_CAPS));

enum
{
  ARG_0,
  ARG_DISPLAY,
  PROP_ROTATE_METHOD,
  PROP_FORCE_ASPECT_RATIO,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_CONTEXT,
  PROP_HANDLE_EVENTS,
  PROP_IGNORE_ALPHA,
  PROP_OUTPUT_MULTIVIEW_LAYOUT,
  PROP_OUTPUT_MULTIVIEW_FLAGS,
  PROP_OUTPUT_MULTIVIEW_DOWNMIX_MODE,
  PROP_LAST
};

enum
{
  SIGNAL_0,
  CLIENT_DRAW_SIGNAL,
  CLIENT_RESHAPE_SIGNAL,
  LAST_SIGNAL
};

static guint gst_glimage_sink_signals[LAST_SIGNAL] = { 0 };

static void
_display_size_to_stream_size (GstGLImageSink * gl_sink, gdouble x,
    gdouble y, gdouble * stream_x, gdouble * stream_y)
{
  gdouble stream_width, stream_height;

  stream_width = (gdouble) GST_VIDEO_INFO_WIDTH (&gl_sink->out_info);
  stream_height = (gdouble) GST_VIDEO_INFO_HEIGHT (&gl_sink->out_info);

  /* from display coordinates to stream coordinates */
  if (gl_sink->display_rect.w > 0)
    *stream_x =
        (x - gl_sink->display_rect.x) / gl_sink->display_rect.w * stream_width;
  else
    *stream_x = 0.;

  /* clip to stream size */
  if (*stream_x < 0.)
    *stream_x = 0.;
  if (*stream_x > GST_VIDEO_INFO_WIDTH (&gl_sink->out_info))
    *stream_x = GST_VIDEO_INFO_WIDTH (&gl_sink->out_info);

  /* same for y-axis */
  if (gl_sink->display_rect.h > 0)
    *stream_y =
        (y - gl_sink->display_rect.y) / gl_sink->display_rect.h * stream_height;
  else
    *stream_y = 0.;

  if (*stream_y < 0.)
    *stream_y = 0.;
  if (*stream_y > GST_VIDEO_INFO_HEIGHT (&gl_sink->out_info))
    *stream_y = GST_VIDEO_INFO_HEIGHT (&gl_sink->out_info);

  GST_TRACE ("transform %fx%f into %fx%f", x, y, *stream_x, *stream_y);
}

static void
_display_scroll_value_to_stream_scroll_value (GstGLImageSink * gl_sink,
    gdouble delta_x, gdouble delta_y, gdouble * stream_delta_x,
    gdouble * stream_delta_y)
{
  gdouble stream_width, stream_height;

  stream_width = (gdouble) GST_VIDEO_INFO_WIDTH (&gl_sink->out_info);
  stream_height = (gdouble) GST_VIDEO_INFO_HEIGHT (&gl_sink->out_info);

  if (delta_x != 0 && gl_sink->display_rect.w > 0)
    *stream_delta_x = delta_x * (stream_width / gl_sink->display_rect.w);
  else
    *stream_delta_x = delta_x;

  if (delta_y != 0 && gl_sink->display_rect.h > 0)
    *stream_delta_y = delta_y * (stream_height / gl_sink->display_rect.h);
  else
    *stream_delta_y = delta_y;

  GST_TRACE_OBJECT (gl_sink, "transform %fx%f into %fx%f", delta_x, delta_y,
      *stream_delta_x, *stream_delta_y);
}

/* rotate 90 */
static const gfloat clockwise_matrix[] = {
  0.0f, -1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

/* rotate 180 */
static const gfloat clockwise_180_matrix[] = {
  -1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, -1.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

/* rotate 270 */
static const gfloat counterclockwise_matrix[] = {
  0.0f, 1.0f, 0.0f, 0.0f,
  -1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

/* horizontal-flip */
static const gfloat horizontal_flip_matrix[] = {
  -1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

/* vertical-flip */
static const gfloat vertical_flip_matrix[] = {
  1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, -1.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

/* upper-left-diagonal */
static const gfloat upper_left_matrix[] = {
  0.0f, -1.0f, 0.0f, 0.0f,
  -1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

/* upper-right-diagonal */
static const gfloat upper_right_matrix[] = {
  0.0f, 1.0f, 0.0f, 0.0f,
  1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

static void
gst_glimage_sink_set_rotate_method (GstGLImageSink * gl_sink,
    GstVideoOrientationMethod method, gboolean from_tag)
{
  GstVideoOrientationMethod tag_method = DEFAULT_ROTATE_METHOD;
  GST_GLIMAGE_SINK_LOCK (gl_sink);
  if (from_tag)
    tag_method = method;
  else
    gl_sink->rotate_method = method;

  if (gl_sink->rotate_method == GST_VIDEO_ORIENTATION_AUTO)
    method = tag_method;
  else
    method = gl_sink->rotate_method;

  if (method != gl_sink->current_rotate_method) {
    GST_DEBUG_OBJECT (gl_sink, "Changing method from %s to %s",
        rotate_methods[gl_sink->current_rotate_method].value_nick,
        rotate_methods[method].value_nick);

    switch (method) {
      case GST_VIDEO_ORIENTATION_IDENTITY:
        gl_sink->transform_matrix = NULL;
        gl_sink->output_mode_changed = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_90R:
        gl_sink->transform_matrix = clockwise_matrix;
        gl_sink->output_mode_changed = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_180:
        gl_sink->transform_matrix = clockwise_180_matrix;
        gl_sink->output_mode_changed = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_90L:
        gl_sink->transform_matrix = counterclockwise_matrix;
        gl_sink->output_mode_changed = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_HORIZ:
        gl_sink->transform_matrix = horizontal_flip_matrix;
        gl_sink->output_mode_changed = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_VERT:
        gl_sink->transform_matrix = vertical_flip_matrix;
        gl_sink->output_mode_changed = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_UL_LR:
        gl_sink->transform_matrix = upper_left_matrix;
        gl_sink->output_mode_changed = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_UR_LL:
        gl_sink->transform_matrix = upper_right_matrix;
        gl_sink->output_mode_changed = TRUE;
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    gl_sink->current_rotate_method = method;
  }
  GST_GLIMAGE_SINK_UNLOCK (gl_sink);
}

static void
gst_glimage_sink_navigation_send_event (GstNavigation * navigation,
    GstEvent * event)
{
  GstGLImageSink *sink = GST_GLIMAGE_SINK (navigation);
  gboolean handled = FALSE;
  GstGLWindow *window;
  guint width, height;
  gdouble x, y;

  if (!sink->context) {
    gst_event_unref (event);
    return;
  }

  event = gst_event_make_writable (event);

  window = gst_gl_context_get_window (sink->context);
  g_return_if_fail (GST_IS_GL_WINDOW (window));

  width = GST_VIDEO_SINK_WIDTH (sink);
  height = GST_VIDEO_SINK_HEIGHT (sink);
  gst_gl_window_get_surface_dimensions (window, &width, &height);

  /* Converting pointer coordinates to the non scaled geometry */
  if (width != 0 && height != 0 &&
      gst_navigation_event_get_coordinates (event, &x, &y)) {
    gdouble stream_x, stream_y;

    _display_size_to_stream_size (sink, x, y, &stream_x, &stream_y);

    gst_navigation_event_set_coordinates (event, stream_x, stream_y);
  }

  /* Converting pointer scroll coordinates to the non scaled geometry */
  if (width != 0 && height != 0 && gst_navigation_event_get_type (event)
      == GST_NAVIGATION_EVENT_MOUSE_SCROLL) {
    gdouble dx, dy, stream_dx, stream_dy;

    gst_navigation_event_parse_mouse_scroll_event (event, &x, &y, &dx, &dy);
    _display_scroll_value_to_stream_scroll_value (sink, dx, dy, &stream_dx,
        &stream_dy);

    /* FIXME: Add support for NavigationModifier */
    gst_event_replace (&event,
        gst_navigation_event_new_mouse_scroll (x, y, stream_dx, stream_dy,
            GST_NAVIGATION_MODIFIER_NONE));
  }

  gst_event_ref (event);
  handled = gst_pad_push_event (GST_VIDEO_SINK_PAD (sink), event);

  if (!handled)
    gst_element_post_message ((GstElement *) sink,
        gst_navigation_message_new_event ((GstObject *) sink, event));

  gst_event_unref (event);
  gst_object_unref (window);
}

static void
gst_glimage_sink_navigation_interface_init (GstNavigationInterface * iface)
{
  iface->send_event_simple = gst_glimage_sink_navigation_send_event;
}

#define gst_glimage_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLImageSink, gst_glimage_sink,
    GST_TYPE_VIDEO_SINK, G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_glimage_sink_video_overlay_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_glimage_sink_navigation_interface_init);
    GST_DEBUG_CATEGORY_INIT (gst_debug_glimage_sink, "glimagesink", 0,
        "OpenGL Video Sink"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glimagesinkelement, "glimagesinkelement",
    GST_RANK_NONE, gst_glimage_sink_get_type (), gl_element_init (plugin));

static void
gst_glimage_sink_class_init (GstGLImageSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_glimage_sink_set_property;
  gobject_class->get_property = gst_glimage_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_ROTATE_METHOD,
      g_param_spec_enum ("rotate-method",
          "rotate method",
          "rotate method",
          GST_TYPE_GL_ROTATE_METHOD, DEFAULT_ROTATE_METHOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      gst_param_spec_fraction ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", 0, 1, G_MAXINT, 1, 1, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONTEXT,
      g_param_spec_object ("context", "OpenGL context", "Get OpenGL context",
          GST_TYPE_GL_CONTEXT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HANDLE_EVENTS,
      g_param_spec_boolean ("handle-events", "Handle XEvents",
          "When enabled, XEvents will be selected and handled",
          DEFAULT_HANDLE_EVENTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_IGNORE_ALPHA,
      g_param_spec_boolean ("ignore-alpha", "Ignore Alpha",
          "When enabled, alpha will be ignored and converted to black",
          DEFAULT_IGNORE_ALPHA, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_OUTPUT_MULTIVIEW_LAYOUT,
      g_param_spec_enum ("output-multiview-mode",
          "Output Multiview Mode",
          "Choose output mode for multiview/3D video",
          GST_TYPE_VIDEO_MULTIVIEW_MODE, DEFAULT_MULTIVIEW_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_MULTIVIEW_FLAGS,
      g_param_spec_flags ("output-multiview-flags",
          "Output Multiview Flags",
          "Output multiview layout modifier flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, DEFAULT_MULTIVIEW_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_OUTPUT_MULTIVIEW_DOWNMIX_MODE,
      g_param_spec_enum ("output-multiview-downmix-mode",
          "Mode for mono downmixed output",
          "Output anaglyph type to generate when downmixing to mono",
          GST_TYPE_GL_STEREO_DOWNMIX, DEFAULT_MULTIVIEW_DOWNMIX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_video_overlay_install_properties (gobject_class, PROP_LAST);

  gst_element_class_set_metadata (element_class, "OpenGL video sink",
      "Sink/Video", "A videosink based on OpenGL",
      "Julien Isorce <julien.isorce@gmail.com>");

  /**
   * GstGLImageSink::client-draw:
   * @object: the #GstGLImageSink
   * @context: the #GstGLContext
   * @sample: the #GstSample
   *
   * Will be emitted before actually drawing the texture.  The client should
   * redraw the surface/contents of @sample and return %TRUE.
   *
   * Returns: whether the texture was redrawn by the signal.  If not, a
   *          default redraw will occur.
   */
  gst_glimage_sink_signals[CLIENT_DRAW_SIGNAL] =
      g_signal_new ("client-draw", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 2,
      GST_TYPE_GL_CONTEXT, GST_TYPE_SAMPLE);

  /**
   * GstGLImageSink::client-reshape:
   * @object: the #GstGLImageSink
   * @context: the #GstGLContext
   * @width: the #guint width of the texture.
   * @height: the #guint height of the texture.
   *
   * The client should resize the surface/window/viewport with the @width and
   * @height and return %TRUE.
   *
   * Returns: whether the content area was resized by the signal.  If not, a
   *          default viewport resize will occur.
   */
  gst_glimage_sink_signals[CLIENT_RESHAPE_SIGNAL] =
      g_signal_new ("client-reshape", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 3,
      GST_TYPE_GL_CONTEXT, G_TYPE_UINT, G_TYPE_UINT);

  gst_element_class_add_static_pad_template (element_class,
      &gst_glimage_sink_template);

  gobject_class->finalize = gst_glimage_sink_finalize;

  gstelement_class->change_state = gst_glimage_sink_change_state;
  gstelement_class->set_context = gst_glimage_sink_set_context;
  gstbasesink_class->event = gst_glimage_sink_event;
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_glimage_sink_query);
  gstbasesink_class->set_caps = gst_glimage_sink_set_caps;
  gstbasesink_class->get_caps = gst_glimage_sink_get_caps;
  gstbasesink_class->get_times = gst_glimage_sink_get_times;
  gstbasesink_class->prepare = gst_glimage_sink_prepare;
  gstbasesink_class->propose_allocation = gst_glimage_sink_propose_allocation;

  gstvideosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_glimage_sink_show_frame);
}

static void
gst_glimage_sink_init (GstGLImageSink * glimage_sink)
{
  glimage_sink->window_id = 0;
  glimage_sink->new_window_id = 0;
  glimage_sink->display = NULL;
  glimage_sink->keep_aspect_ratio = TRUE;
  glimage_sink->par_n = 0;
  glimage_sink->par_d = 1;
  glimage_sink->redisplay_texture = 0;
  glimage_sink->handle_events = TRUE;
  glimage_sink->ignore_alpha = TRUE;
  glimage_sink->overlay_compositor = NULL;

  glimage_sink->mview_output_mode = DEFAULT_MULTIVIEW_MODE;
  glimage_sink->mview_output_flags = DEFAULT_MULTIVIEW_FLAGS;
  glimage_sink->mview_downmix_mode = DEFAULT_MULTIVIEW_DOWNMIX;

  glimage_sink->current_rotate_method = DEFAULT_ROTATE_METHOD;
  glimage_sink->transform_matrix = NULL;

  g_mutex_init (&glimage_sink->drawing_lock);
}

static void
gst_glimage_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLImageSink *glimage_sink;

  g_return_if_fail (GST_IS_GLIMAGE_SINK (object));

  glimage_sink = GST_GLIMAGE_SINK (object);

  switch (prop_id) {
    case PROP_ROTATE_METHOD:
      gst_glimage_sink_set_rotate_method (glimage_sink,
          g_value_get_enum (value), FALSE);
      break;
    case PROP_FORCE_ASPECT_RATIO:
    {
      glimage_sink->keep_aspect_ratio = g_value_get_boolean (value);
      break;
    }
    case PROP_PIXEL_ASPECT_RATIO:
    {
      glimage_sink->par_n = gst_value_get_fraction_numerator (value);
      glimage_sink->par_d = gst_value_get_fraction_denominator (value);
      break;
    }
    case PROP_HANDLE_EVENTS:
      gst_glimage_sink_handle_events (GST_VIDEO_OVERLAY (glimage_sink),
          g_value_get_boolean (value));
      break;
    case PROP_IGNORE_ALPHA:
      glimage_sink->ignore_alpha = g_value_get_boolean (value);
      break;
    case PROP_OUTPUT_MULTIVIEW_LAYOUT:
      GST_GLIMAGE_SINK_LOCK (glimage_sink);
      glimage_sink->mview_output_mode = g_value_get_enum (value);
      glimage_sink->output_mode_changed = TRUE;
      GST_GLIMAGE_SINK_UNLOCK (glimage_sink);
      break;
    case PROP_OUTPUT_MULTIVIEW_FLAGS:
      GST_GLIMAGE_SINK_LOCK (glimage_sink);
      glimage_sink->mview_output_flags = g_value_get_flags (value);
      glimage_sink->output_mode_changed = TRUE;
      GST_GLIMAGE_SINK_UNLOCK (glimage_sink);
      break;
    case PROP_OUTPUT_MULTIVIEW_DOWNMIX_MODE:
      GST_GLIMAGE_SINK_LOCK (glimage_sink);
      glimage_sink->mview_downmix_mode = g_value_get_enum (value);
      glimage_sink->output_mode_changed = TRUE;
      GST_GLIMAGE_SINK_UNLOCK (glimage_sink);
      break;
    default:
      if (!gst_video_overlay_set_property (object, PROP_LAST, prop_id, value))
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_glimage_sink_finalize (GObject * object)
{
  GstGLImageSink *glimage_sink;

  g_return_if_fail (GST_IS_GLIMAGE_SINK (object));

  glimage_sink = GST_GLIMAGE_SINK (object);
  g_mutex_clear (&glimage_sink->drawing_lock);

  GST_DEBUG ("finalized");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_glimage_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLImageSink *glimage_sink;

  g_return_if_fail (GST_IS_GLIMAGE_SINK (object));

  glimage_sink = GST_GLIMAGE_SINK (object);

  switch (prop_id) {
    case PROP_ROTATE_METHOD:
      g_value_set_enum (value, glimage_sink->current_rotate_method);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, glimage_sink->keep_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gst_value_set_fraction (value, glimage_sink->par_n, glimage_sink->par_d);
      break;
    case PROP_CONTEXT:
      g_value_set_object (value, glimage_sink->context);
      break;
    case PROP_HANDLE_EVENTS:
      g_value_set_boolean (value, glimage_sink->handle_events);
      break;
    case PROP_IGNORE_ALPHA:
      g_value_set_boolean (value, glimage_sink->ignore_alpha);
      break;
    case PROP_OUTPUT_MULTIVIEW_LAYOUT:
      g_value_set_enum (value, glimage_sink->mview_output_mode);
      break;
    case PROP_OUTPUT_MULTIVIEW_FLAGS:
      g_value_set_flags (value, glimage_sink->mview_output_flags);
      break;
    case PROP_OUTPUT_MULTIVIEW_DOWNMIX_MODE:
      g_value_set_enum (value, glimage_sink->mview_downmix_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_glimage_sink_key_event_cb (GstGLWindow * window, char *event_name, char
    *key_string, GstGLImageSink * gl_sink)
{
  GstEvent *event = NULL;

  GST_DEBUG_OBJECT (gl_sink, "event %s key %s pressed", event_name, key_string);
  // FIXME: Add support for modifiers
  if (0 == g_strcmp0 ("key-press", event_name)) {
    event =
        gst_navigation_event_new_key_press (key_string,
        GST_NAVIGATION_MODIFIER_NONE);
  } else if (0 == g_strcmp0 ("key-release", event_name)) {
    event =
        gst_navigation_event_new_key_release (key_string,
        GST_NAVIGATION_MODIFIER_NONE);
  }

  if (event)
    gst_navigation_send_event_simple (GST_NAVIGATION (gl_sink), event);
}

static void
gst_glimage_sink_mouse_event_cb (GstGLWindow * window, char *event_name,
    int button, double posx, double posy, GstGLImageSink * gl_sink)
{
  GstEvent *event = NULL;

  GST_DEBUG_OBJECT (gl_sink, "event %s at %g, %g", event_name, posx, posy);
  // FIXME: Add support for modifiers
  if (0 == g_strcmp0 ("mouse-button-press", event_name)) {
    event =
        gst_navigation_event_new_mouse_button_press (button, posx, posy,
        GST_NAVIGATION_MODIFIER_NONE);
  } else if (0 == g_strcmp0 ("mouse-button-release", event_name)) {
    event =
        gst_navigation_event_new_mouse_button_release (button, posx, posy,
        GST_NAVIGATION_MODIFIER_NONE);
  } else if (0 == g_strcmp0 ("mouse-move", event_name)) {
    event =
        gst_navigation_event_new_mouse_move (posx, posy,
        GST_NAVIGATION_MODIFIER_NONE);
  }

  if (event)
    gst_navigation_send_event_simple (GST_NAVIGATION (gl_sink), event);
}


static void
gst_glimage_sink_mouse_scroll_event_cb (GstGLWindow * window,
    double posx, double posy, double delta_x, double delta_y,
    GstGLImageSink * gl_sink)
{
  GST_DEBUG_OBJECT (gl_sink, "event scroll at %g, %g", posx, posy);
  gst_navigation_send_event_simple (GST_NAVIGATION (gl_sink),
      gst_navigation_event_new_mouse_scroll (posx, posy, delta_x, delta_y,
          GST_NAVIGATION_MODIFIER_NONE));
}

static void
_set_context (GstGLImageSink * gl_sink, GstGLContext * context)
{
  GstGLContext *old_context;

  GST_GLIMAGE_SINK_LOCK (gl_sink);

  old_context = gl_sink->context;
  gl_sink->context = context;

  GST_GLIMAGE_SINK_UNLOCK (gl_sink);

  if (old_context)
    gst_object_unref (old_context);
}

static void
_set_other_context (GstGLImageSink * gl_sink, GstGLContext * other_context)
{
  GST_GLIMAGE_SINK_LOCK (gl_sink);
  if (gl_sink->other_context)
    gst_object_unref (gl_sink->other_context);

  gl_sink->other_context = other_context;
  GST_GLIMAGE_SINK_UNLOCK (gl_sink);
}

static void
_set_display (GstGLImageSink * gl_sink, GstGLDisplay * display)
{
  GST_GLIMAGE_SINK_LOCK (gl_sink);
  if (gl_sink->display)
    gst_object_unref (gl_sink->display);

  gl_sink->display = display;
  GST_GLIMAGE_SINK_UNLOCK (gl_sink);
}

static gboolean
_ensure_gl_setup (GstGLImageSink * gl_sink)
{
  GError *error = NULL;

  GST_TRACE_OBJECT (gl_sink, "Ensuring setup");

  if (!gl_sink->context) {
    GST_OBJECT_LOCK (gl_sink->display);
    do {
      GstGLContext *other_context = NULL;
      GstGLContext *context = NULL;
      GstGLWindow *window = NULL;

      _set_context (gl_sink, NULL);

      GST_DEBUG_OBJECT (gl_sink,
          "No current context, creating one for %" GST_PTR_FORMAT,
          gl_sink->display);

      if (gl_sink->other_context) {
        other_context = gst_object_ref (gl_sink->other_context);
      } else {
        other_context =
            gst_gl_display_get_gl_context_for_thread (gl_sink->display, NULL);
      }

      if (!gst_gl_display_create_context (gl_sink->display,
              other_context, &context, &error)) {
        if (other_context)
          gst_object_unref (other_context);
        GST_OBJECT_UNLOCK (gl_sink->display);
        goto context_error;
      }
      _set_context (gl_sink, context);
      context = NULL;

      GST_DEBUG_OBJECT (gl_sink,
          "created context %" GST_PTR_FORMAT " from other context %"
          GST_PTR_FORMAT, gl_sink->context, gl_sink->other_context);

      window = gst_gl_context_get_window (gl_sink->context);

      GST_DEBUG_OBJECT (gl_sink, "got window %" GST_PTR_FORMAT, window);

      if (!gl_sink->window_id && !gl_sink->new_window_id)
        gst_video_overlay_prepare_window_handle (GST_VIDEO_OVERLAY (gl_sink));

      GST_DEBUG_OBJECT (gl_sink,
          "window_id : %" G_GUINTPTR_FORMAT " , new_window_id : %"
          G_GUINTPTR_FORMAT, gl_sink->window_id, gl_sink->new_window_id);

      if (gl_sink->window_id != gl_sink->new_window_id) {
        gl_sink->window_id = gl_sink->new_window_id;
        GST_DEBUG_OBJECT (gl_sink, "Setting window handle on gl window");
        gst_gl_window_set_window_handle (window, gl_sink->window_id);
      }

      gst_gl_window_handle_events (window, gl_sink->handle_events);

      /* setup callbacks */
      gst_gl_window_set_resize_callback (window,
          GST_GL_WINDOW_RESIZE_CB (gst_glimage_sink_on_resize),
          gst_object_ref (gl_sink), (GDestroyNotify) gst_object_unref);
      gst_gl_window_set_draw_callback (window,
          GST_GL_WINDOW_CB (gst_glimage_sink_on_draw),
          gst_object_ref (gl_sink), (GDestroyNotify) gst_object_unref);
      gst_gl_window_set_close_callback (window,
          GST_GL_WINDOW_CB (gst_glimage_sink_on_close),
          gst_object_ref (gl_sink), (GDestroyNotify) gst_object_unref);
      gl_sink->key_sig_id = g_signal_connect (window, "key-event", G_CALLBACK
          (gst_glimage_sink_key_event_cb), gl_sink);
      gl_sink->mouse_sig_id =
          g_signal_connect (window, "mouse-event",
          G_CALLBACK (gst_glimage_sink_mouse_event_cb), gl_sink);
      gl_sink->mouse_scroll_sig_id =
          g_signal_connect (window, "scroll-event",
          G_CALLBACK (gst_glimage_sink_mouse_scroll_event_cb), gl_sink);

      gst_gl_window_set_render_rectangle (window, gl_sink->x, gl_sink->y,
          gl_sink->width, gl_sink->height);

      if (other_context)
        gst_object_unref (other_context);
      gst_object_unref (window);
    } while (!gst_gl_display_add_context (gl_sink->display, gl_sink->context));
    GST_OBJECT_UNLOCK (gl_sink->display);
  } else
    GST_TRACE_OBJECT (gl_sink, "Already have a context");

  return TRUE;

context_error:
  {
    GST_ELEMENT_ERROR (gl_sink, RESOURCE, NOT_FOUND, ("%s", error->message),
        (NULL));

    _set_context (gl_sink, NULL);

    g_clear_error (&error);

    return FALSE;
  }
}

static gboolean
gst_glimage_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstGLImageSink *gl_sink = GST_GLIMAGE_SINK (sink);
  GstTagList *taglist;
  GstVideoOrientationMethod method;
  gboolean ret;

  GST_DEBUG_OBJECT (gl_sink, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
      gst_event_parse_tag (event, &taglist);

      if (gst_video_orientation_from_tag (taglist, &method))
        gst_glimage_sink_set_rotate_method (gl_sink, method, TRUE);

      break;
    default:
      break;
  }

  ret = GST_BASE_SINK_CLASS (parent_class)->event (sink, event);

  return ret;
}

static gboolean
gst_glimage_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstGLImageSink *glimage_sink = GST_GLIMAGE_SINK (bsink);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      GstGLContext *context = NULL;
      GstGLContext *other_context = NULL;
      GstGLDisplay *display = NULL;

      GST_GLIMAGE_SINK_LOCK (bsink);
      if (glimage_sink->context)
        context = gst_object_ref (glimage_sink->context);
      if (glimage_sink->other_context)
        other_context = gst_object_ref (glimage_sink->other_context);
      if (glimage_sink->display)
        display = gst_object_ref (glimage_sink->display);
      GST_GLIMAGE_SINK_UNLOCK (bsink);

      res = gst_gl_handle_context_query ((GstElement *) glimage_sink, query,
          display, context, other_context);

      if (context)
        gst_object_unref (context);
      if (other_context)
        gst_object_unref (other_context);
      if (display)
        gst_object_unref (display);
      if (res)
        return res;
      break;
    }
    case GST_QUERY_DRAIN:
    {
      GstBuffer *buf[2];

      GST_GLIMAGE_SINK_LOCK (glimage_sink);
      glimage_sink->redisplay_texture = 0;
      buf[0] = glimage_sink->stored_buffer[0];
      buf[1] = glimage_sink->stored_buffer[1];
      glimage_sink->stored_buffer[0] = glimage_sink->stored_buffer[1] = NULL;
      glimage_sink->stored_sync_meta = glimage_sink->next_sync_meta = NULL;
      GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

      gst_buffer_replace (buf, NULL);
      gst_buffer_replace (buf + 1, NULL);

      gst_buffer_replace (&glimage_sink->input_buffer, NULL);
      gst_buffer_replace (&glimage_sink->input_buffer2, NULL);
      gst_buffer_replace (&glimage_sink->next_buffer, NULL);
      gst_buffer_replace (&glimage_sink->next_buffer2, NULL);
      gst_buffer_replace (&glimage_sink->next_sync, NULL);

      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
    }
    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }

  return res;
}

static void
gst_glimage_sink_set_context (GstElement * element, GstContext * context)
{
  GstGLImageSink *gl_sink = GST_GLIMAGE_SINK (element);
  GstGLContext *other_context = NULL;
  GstGLDisplay *display = NULL;

  gst_gl_handle_set_context (element, context, &display, &other_context);
  if (display)
    _set_display (gl_sink, display);
  if (other_context)
    _set_other_context (gl_sink, other_context);

  if (gl_sink->display)
    gst_gl_display_filter_gl_api (gl_sink->display, SUPPORTED_GL_APIS);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static GstStateChangeReturn
gst_glimage_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstGLImageSink *glimage_sink;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstGLContext *context;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  glimage_sink = GST_GLIMAGE_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_gl_ensure_element_data (glimage_sink, &glimage_sink->display,
              &glimage_sink->other_context))
        return GST_STATE_CHANGE_FAILURE;

      gst_gl_display_filter_gl_api (glimage_sink->display, SUPPORTED_GL_APIS);

      if (!_ensure_gl_setup (glimage_sink))
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_atomic_int_set (&glimage_sink->to_quit, 0);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      GstBuffer *buf[2];

      GST_GLIMAGE_SINK_LOCK (glimage_sink);
      /* mark the redisplay_texture as unavailable (=0)
       * to avoid drawing
       */
      glimage_sink->redisplay_texture = 0;
      buf[0] = glimage_sink->stored_buffer[0];
      buf[1] = glimage_sink->stored_buffer[1];
      glimage_sink->stored_buffer[0] = glimage_sink->stored_buffer[1] = NULL;
      glimage_sink->stored_sync_meta = glimage_sink->next_sync_meta = NULL;

      if (glimage_sink->stored_sync)
        gst_buffer_unref (glimage_sink->stored_sync);
      glimage_sink->stored_sync = NULL;

      GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

      gst_buffer_replace (buf, NULL);
      gst_buffer_replace (buf + 1, NULL);

      gst_object_replace ((GstObject **) & glimage_sink->convert_views, NULL);
      gst_buffer_replace (&glimage_sink->input_buffer, NULL);
      gst_buffer_replace (&glimage_sink->input_buffer2, NULL);
      gst_buffer_replace (&glimage_sink->next_buffer, NULL);
      gst_buffer_replace (&glimage_sink->next_buffer2, NULL);
      gst_buffer_replace (&glimage_sink->next_sync, NULL);

      glimage_sink->window_id = 0;
      /* but do not reset glimage_sink->new_window_id */

      GST_VIDEO_SINK_WIDTH (glimage_sink) = 1;
      GST_VIDEO_SINK_HEIGHT (glimage_sink) = 1;
      /* Clear cached caps */
      if (glimage_sink->out_caps) {
        gst_caps_unref (glimage_sink->out_caps);
        glimage_sink->out_caps = NULL;
      }
      if (glimage_sink->in_caps) {
        gst_caps_unref (glimage_sink->in_caps);
        glimage_sink->in_caps = NULL;
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (glimage_sink->overlay_compositor) {
        gst_object_unref (glimage_sink->overlay_compositor);
        glimage_sink->overlay_compositor = NULL;
      }

      context = NULL;
      GST_GLIMAGE_SINK_LOCK (glimage_sink);
      if (glimage_sink->context)
        context = gst_object_ref (glimage_sink->context);
      GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

      if (context) {
        GstGLWindow *window = gst_gl_context_get_window (context);

        gst_gl_window_send_message (window,
            GST_GL_WINDOW_CB (gst_glimage_sink_cleanup_glthread), glimage_sink);

        gst_gl_window_set_resize_callback (window, NULL, NULL, NULL);
        gst_gl_window_set_draw_callback (window, NULL, NULL, NULL);
        gst_gl_window_set_close_callback (window, NULL, NULL, NULL);

        if (glimage_sink->key_sig_id)
          g_signal_handler_disconnect (window, glimage_sink->key_sig_id);
        glimage_sink->key_sig_id = 0;
        if (glimage_sink->mouse_sig_id)
          g_signal_handler_disconnect (window, glimage_sink->mouse_sig_id);
        glimage_sink->mouse_sig_id = 0;
        if (glimage_sink->mouse_scroll_sig_id)
          g_signal_handler_disconnect (window,
              glimage_sink->mouse_scroll_sig_id);
        glimage_sink->mouse_scroll_sig_id = 0;

        gst_object_unref (window);
        _set_context (glimage_sink, NULL);

        gst_object_unref (context);
      }

      glimage_sink->window_id = 0;

      _set_other_context (glimage_sink, NULL);
      _set_display (glimage_sink, NULL);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_glimage_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstGLImageSink *glimagesink;

  glimagesink = GST_GLIMAGE_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (GST_VIDEO_INFO_FPS_N (&glimagesink->out_info) > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&glimagesink->out_info),
            GST_VIDEO_INFO_FPS_N (&glimagesink->out_info));
      }
    }
  }
}

static GstCaps *
gst_glimage_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstCaps *tmp = NULL;
  GstCaps *result = NULL;

  tmp = gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD (bsink));

  if (filter) {
    GST_DEBUG_OBJECT (bsink, "intersecting with filter caps %" GST_PTR_FORMAT,
        filter);

    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  result = gst_gl_overlay_compositor_add_caps (result);

  GST_DEBUG_OBJECT (bsink, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
configure_display_from_info (GstGLImageSink * glimage_sink,
    const GstVideoInfo * vinfo)
{
  gint width;
  gint height;
  gboolean ok;
  gint par_n, par_d;
  gint display_par_n, display_par_d;
  guint display_ratio_num, display_ratio_den;

  width = GST_VIDEO_INFO_WIDTH (vinfo);
  height = GST_VIDEO_INFO_HEIGHT (vinfo);

  par_n = GST_VIDEO_INFO_PAR_N (vinfo);
  par_d = GST_VIDEO_INFO_PAR_D (vinfo);

  if (!par_n)
    par_n = 1;

  /* get display's PAR */
  if (glimage_sink->par_n != 0 && glimage_sink->par_d != 0) {
    display_par_n = glimage_sink->par_n;
    display_par_d = glimage_sink->par_d;
  } else {
    display_par_n = 1;
    display_par_d = 1;
  }

  ok = gst_video_calculate_display_ratio (&display_ratio_num,
      &display_ratio_den, width, height, par_n, par_d, display_par_n,
      display_par_d);

  if (!ok)
    return FALSE;

  GST_TRACE ("PAR: %u/%u DAR:%u/%u", par_n, par_d, display_par_n,
      display_par_d);

  if (height % display_ratio_den == 0) {
    GST_DEBUG ("keeping video height");
    GST_VIDEO_SINK_WIDTH (glimage_sink) = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    GST_VIDEO_SINK_HEIGHT (glimage_sink) = height;
  } else if (width % display_ratio_num == 0) {
    GST_DEBUG ("keeping video width");
    GST_VIDEO_SINK_WIDTH (glimage_sink) = width;
    GST_VIDEO_SINK_HEIGHT (glimage_sink) = (guint)
        gst_util_uint64_scale_int (width, display_ratio_den, display_ratio_num);
  } else {
    GST_DEBUG ("approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (glimage_sink) = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    GST_VIDEO_SINK_HEIGHT (glimage_sink) = height;
  }
  GST_DEBUG ("scaling to %dx%d", GST_VIDEO_SINK_WIDTH (glimage_sink),
      GST_VIDEO_SINK_HEIGHT (glimage_sink));

  return TRUE;
}

static gboolean
_mview_modes_are_equal (GstVideoMultiviewMode a, GstVideoMultiviewMode b)
{
  if (a == b)
    return TRUE;
  if (a == GST_VIDEO_MULTIVIEW_MODE_NONE && b == GST_VIDEO_MULTIVIEW_MODE_MONO)
    return TRUE;
  if (a == GST_VIDEO_MULTIVIEW_MODE_MONO && b == GST_VIDEO_MULTIVIEW_MODE_NONE)
    return TRUE;

  return FALSE;
}

/* Called with GST_GLIMAGE_SINK lock held, to
 * copy in_info to out_info and update out_caps */
static gboolean
update_output_format (GstGLImageSink * glimage_sink)
{
  GstVideoInfo *out_info = &glimage_sink->out_info;
  GstVideoMultiviewMode mv_mode;
  GstGLWindow *window = NULL;
  GstGLTextureTarget previous_target;
  GstStructure *s;
  const gchar *target_str;
  GstCaps *out_caps;
  gboolean ret;

  *out_info = glimage_sink->in_info;
  previous_target = glimage_sink->texture_target;

  mv_mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&glimage_sink->in_info);

  if (!_mview_modes_are_equal (glimage_sink->mview_output_mode, mv_mode)) {
    /* Input is multiview, and output wants a conversion - configure 3d converter now,
     * otherwise defer it until either the caps or the 3D output mode changes */
    gst_video_multiview_video_info_change_mode (out_info,
        glimage_sink->mview_output_mode, glimage_sink->mview_output_flags);

    if (glimage_sink->convert_views == NULL) {
      glimage_sink->convert_views = gst_gl_view_convert_new ();
      gst_gl_view_convert_set_context (glimage_sink->convert_views,
          glimage_sink->context);
    }
  } else {
    if (glimage_sink->convert_views) {
      gst_object_unref (glimage_sink->convert_views);
      glimage_sink->convert_views = NULL;
    }
  }

  ret = configure_display_from_info (glimage_sink, out_info);

  if (glimage_sink->convert_views) {
    gint new_w, new_h;
    gint par_n, par_d;

    /* Match actual output window size for pixel-aligned output,
     * even though we can't necessarily match the starting left/right
     * view parity properly for line-by-line modes, because that
     * depends on the window being placed correctly.
     * FIXME: Should this rescaling be configurable? */
    if (glimage_sink->display_rect.w > 0 && glimage_sink->display_rect.h > 0) {
      new_w = glimage_sink->display_rect.w;
      new_h = glimage_sink->display_rect.h;
      if (new_w != out_info->width || new_h != out_info->height) {
        /* Recalculate PAR if rescaling */
        gint from_dar_n, from_dar_d;
        if (!gst_util_fraction_multiply (out_info->width, out_info->height,
                out_info->par_n, out_info->par_d, &from_dar_n,
                &from_dar_d) ||
            !gst_util_fraction_multiply (from_dar_n, from_dar_d, new_h, new_w,
                &par_n, &par_d)) {
          par_n = glimage_sink->par_n;
          par_d = glimage_sink->par_d;
        }
        out_info->par_n = par_n;
        out_info->par_d = par_d;
        out_info->width = new_w;
        out_info->height = new_h;
      }

      GST_LOG_OBJECT (glimage_sink, "Set 3D output scale to %dx%d PAR %d/%d",
          out_info->width, out_info->height, out_info->par_n, out_info->par_d);
    }
  }

  s = gst_caps_get_structure (glimage_sink->in_caps, 0);
  target_str = gst_structure_get_string (s, "texture-target");

  if (!target_str)
    target_str = GST_GL_TEXTURE_TARGET_2D_STR;

  glimage_sink->texture_target = gst_gl_texture_target_from_string (target_str);
  if (!glimage_sink->texture_target)
    return FALSE;

  out_caps = gst_video_info_to_caps (out_info);
  gst_caps_set_features (out_caps, 0,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));
  gst_caps_set_simple (out_caps, "texture-target", G_TYPE_STRING,
      target_str, NULL);

  if (glimage_sink->convert_views) {
    gst_caps_set_simple (out_caps, "texture-target", G_TYPE_STRING,
        GST_GL_TEXTURE_TARGET_2D_STR, NULL);

    glimage_sink->texture_target = GST_GL_TEXTURE_TARGET_2D;

    GST_GLIMAGE_SINK_UNLOCK (glimage_sink);
    gst_gl_view_convert_set_caps (glimage_sink->convert_views,
        glimage_sink->in_caps, out_caps);
    g_object_set (glimage_sink->convert_views, "downmix-mode",
        glimage_sink->mview_downmix_mode, NULL);
    GST_GLIMAGE_SINK_LOCK (glimage_sink);
  }

  if (glimage_sink->out_caps)
    gst_caps_unref (glimage_sink->out_caps);
  glimage_sink->out_caps = out_caps;

  if (previous_target != GST_GL_TEXTURE_TARGET_NONE &&
      glimage_sink->texture_target != previous_target) {
    /* regenerate the shader for the changed target */
    GstGLWindow *window = gst_gl_context_get_window (glimage_sink->context);
    gst_gl_window_send_message (window,
        GST_GL_WINDOW_CB (gst_glimage_sink_cleanup_glthread), glimage_sink);
    gst_object_unref (window);
  }

  glimage_sink->output_mode_changed = FALSE;

  if (glimage_sink->context)
    window = gst_gl_context_get_window (glimage_sink->context);
  if (window) {
    gst_gl_window_queue_resize (window);
    gst_object_unref (window);
  }

  return ret;
}

static gboolean
gst_glimage_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstGLImageSink *glimage_sink;
  gboolean ok;
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (bsink, "set caps with %" GST_PTR_FORMAT, caps);

  glimage_sink = GST_GLIMAGE_SINK (bsink);

  ok = gst_video_info_from_caps (&vinfo, caps);
  if (!ok)
    return FALSE;

  if (!_ensure_gl_setup (glimage_sink))
    return FALSE;

  GST_GLIMAGE_SINK_LOCK (glimage_sink);
  if (glimage_sink->in_caps)
    gst_caps_unref (glimage_sink->in_caps);
  glimage_sink->in_caps = gst_caps_ref (caps);
  glimage_sink->in_info = vinfo;
  ok = update_output_format (glimage_sink);

  GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

  return ok;
}

/* Take the input_buffer and run it through 3D conversion if needed.
 * Called with glimagesink lock, but might drop it temporarily */
static gboolean
prepare_next_buffer (GstGLImageSink * glimage_sink)
{
  GstBuffer *in_buffer, *next_buffer, *old_buffer;
  GstBuffer *in_buffer2 = NULL, *next_buffer2 = NULL, *old_buffer2;
  GstBuffer *next_sync = NULL, *old_sync;
  GstGLSyncMeta *sync_meta;
  GstVideoFrame gl_frame;
  GstGLViewConvert *convert_views = NULL;
  GstVideoInfo *info;

  if (glimage_sink->input_buffer == NULL)
    return TRUE;                /* No input buffer to process */

  if (GST_VIDEO_INFO_MULTIVIEW_MODE (&glimage_sink->in_info) ==
      GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    if (glimage_sink->input_buffer2 == NULL)
      return TRUE;              /* Need 2nd input buffer to process */
    in_buffer2 = gst_buffer_ref (glimage_sink->input_buffer2);
  }

  in_buffer = gst_buffer_ref (glimage_sink->input_buffer);
  if (glimage_sink->convert_views &&
      (GST_VIDEO_INFO_MULTIVIEW_MODE (&glimage_sink->in_info) !=
          GST_VIDEO_INFO_MULTIVIEW_MODE (&glimage_sink->out_info) ||
          GST_VIDEO_INFO_MULTIVIEW_FLAGS (&glimage_sink->in_info) !=
          GST_VIDEO_INFO_MULTIVIEW_FLAGS (&glimage_sink->out_info)))
    convert_views = gst_object_ref (glimage_sink->convert_views);

  GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

  if (convert_views) {
    info = &glimage_sink->out_info;

    if (gst_gl_view_convert_submit_input_buffer (glimage_sink->convert_views,
            GST_BUFFER_IS_DISCONT (in_buffer), in_buffer) != GST_FLOW_OK) {
      gst_buffer_replace (&in_buffer2, NULL);
      goto fail;
    }
    if (in_buffer2) {
      if (gst_gl_view_convert_submit_input_buffer (glimage_sink->convert_views,
              GST_BUFFER_IS_DISCONT (in_buffer2), in_buffer2) != GST_FLOW_OK) {
        goto fail;
      }
    }

    if (gst_gl_view_convert_get_output (glimage_sink->convert_views,
            &next_buffer) != GST_FLOW_OK)
      goto fail;
    if (GST_VIDEO_INFO_MULTIVIEW_MODE (info) ==
        GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
      if (gst_gl_view_convert_get_output (glimage_sink->convert_views,
              &next_buffer2) != GST_FLOW_OK)
        goto fail;
    }
    gst_object_unref (convert_views);
    convert_views = NULL;

    if (next_buffer == NULL) {
      /* Not ready to paint a buffer yet */
      GST_GLIMAGE_SINK_LOCK (glimage_sink);
      return TRUE;
    }
  } else {
    next_buffer = in_buffer;
    info = &glimage_sink->in_info;
  }

  if (!glimage_sink->overlay_compositor) {
    if (!(glimage_sink->overlay_compositor =
            gst_gl_overlay_compositor_new (glimage_sink->context))) {
      gst_buffer_unref (next_buffer);
      goto fail;
    }
  }

  gst_gl_overlay_compositor_upload_overlays (glimage_sink->overlay_compositor,
      next_buffer);

  sync_meta = gst_buffer_get_gl_sync_meta (next_buffer);

  if (!sync_meta) {
    next_sync = gst_buffer_new ();
    sync_meta = gst_buffer_add_gl_sync_meta (glimage_sink->context, next_sync);
    gst_gl_sync_meta_set_sync_point (sync_meta, glimage_sink->context);
  }

  /* in_buffer invalid now */
  if (!gst_video_frame_map (&gl_frame, info, next_buffer,
          GST_MAP_READ | GST_MAP_GL)) {
    gst_buffer_unref (next_buffer);
    GST_ERROR ("Failed to map video frame.");
    goto fail;
  }

  GST_GLIMAGE_SINK_LOCK (glimage_sink);
  glimage_sink->next_tex = *(guint *) gl_frame.data[0];

  old_buffer = glimage_sink->next_buffer;
  glimage_sink->next_buffer = next_buffer;
  old_buffer2 = glimage_sink->next_buffer2;
  glimage_sink->next_buffer2 = next_buffer2;

  old_sync = glimage_sink->next_sync;
  glimage_sink->next_sync = next_sync;
  glimage_sink->next_sync_meta = sync_meta;

  /* Need to drop the lock again, to avoid a deadlock if we're
   * dropping the last ref on this buffer and it goes back to our
   * allocator */
  GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

  if (old_buffer)
    gst_buffer_unref (old_buffer);
  if (old_buffer2)
    gst_buffer_unref (old_buffer2);
  if (old_sync)
    gst_buffer_unref (old_sync);
  gst_video_frame_unmap (&gl_frame);

  GST_GLIMAGE_SINK_LOCK (glimage_sink);

  return TRUE;

fail:
  if (convert_views)
    gst_object_unref (convert_views);
  GST_GLIMAGE_SINK_LOCK (glimage_sink);
  return FALSE;
}

static GstFlowReturn
gst_glimage_sink_prepare (GstBaseSink * bsink, GstBuffer * buf)
{
  GstGLImageSink *glimage_sink;
  GstGLSyncMeta *sync_meta;
  GstBuffer **target;
  GstBuffer *old_input;

  glimage_sink = GST_GLIMAGE_SINK (bsink);

  GST_TRACE ("preparing buffer:%p", buf);

  if (GST_VIDEO_SINK_WIDTH (glimage_sink) < 1 ||
      GST_VIDEO_SINK_HEIGHT (glimage_sink) < 1) {
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (!_ensure_gl_setup (glimage_sink))
    return GST_FLOW_NOT_NEGOTIATED;

  sync_meta = gst_buffer_get_gl_sync_meta (buf);
  if (sync_meta)
    gst_gl_sync_meta_wait (sync_meta, glimage_sink->context);

  GST_GLIMAGE_SINK_LOCK (glimage_sink);
  if (glimage_sink->window_resized) {
    glimage_sink->window_resized = FALSE;
    GST_GLIMAGE_SINK_UNLOCK (glimage_sink);
    GST_DEBUG_OBJECT (glimage_sink, "Sending reconfigure event on sinkpad.");
    gst_pad_push_event (GST_BASE_SINK (glimage_sink)->sinkpad,
        gst_event_new_reconfigure ());
    GST_GLIMAGE_SINK_LOCK (glimage_sink);
  }

  target = &glimage_sink->input_buffer;
  if (GST_VIDEO_INFO_MULTIVIEW_MODE (&glimage_sink->in_info) ==
      GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME &&
      !GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_FIRST_IN_BUNDLE)) {
    target = &glimage_sink->input_buffer2;
  }
  old_input = *target;
  *target = gst_buffer_ref (buf);

  if (glimage_sink->output_mode_changed)
    update_output_format (glimage_sink);

  if (!prepare_next_buffer (glimage_sink)) {
    GST_GLIMAGE_SINK_UNLOCK (glimage_sink);
    if (old_input)
      gst_buffer_unref (old_input);
    goto convert_views_failed;
  }
  GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

  if (old_input)
    gst_buffer_unref (old_input);

  if (glimage_sink->window_id != glimage_sink->new_window_id) {
    GstGLWindow *window = gst_gl_context_get_window (glimage_sink->context);

    glimage_sink->window_id = glimage_sink->new_window_id;
    gst_gl_window_set_window_handle (window, glimage_sink->window_id);

    gst_object_unref (window);
  }

  return GST_FLOW_OK;
convert_views_failed:
  {
    GST_ELEMENT_ERROR (glimage_sink, RESOURCE, NOT_FOUND,
        ("%s", "Failed to convert multiview video buffer"), (NULL));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_glimage_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstGLImageSink *glimage_sink;

  GST_TRACE ("rendering buffer:%p", buf);

  glimage_sink = GST_GLIMAGE_SINK (vsink);

  GST_TRACE ("redisplay texture:%u of size:%ux%u, window size:%ux%u",
      glimage_sink->next_tex, GST_VIDEO_INFO_WIDTH (&glimage_sink->out_info),
      GST_VIDEO_INFO_HEIGHT (&glimage_sink->out_info),
      GST_VIDEO_SINK_WIDTH (glimage_sink),
      GST_VIDEO_SINK_HEIGHT (glimage_sink));

  /* Ask the underlying window to redraw its content */
  if (!gst_glimage_sink_redisplay (glimage_sink))
    goto redisplay_failed;

  GST_TRACE ("post redisplay");

  if (g_atomic_int_get (&glimage_sink->to_quit) != 0) {
    GST_ELEMENT_ERROR (glimage_sink, RESOURCE, NOT_FOUND,
        ("%s", "Quit requested"), (NULL));
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;

/* ERRORS */
redisplay_failed:
  {
    GST_ELEMENT_ERROR (glimage_sink, RESOURCE, NOT_FOUND,
        ("%s", "Window redisplay failed"), (NULL));
    return GST_FLOW_ERROR;
  }
}

static void
gst_glimage_sink_video_overlay_init (GstVideoOverlayInterface * iface)
{
  iface->set_window_handle = gst_glimage_sink_set_window_handle;
  iface->set_render_rectangle = gst_glimage_sink_set_render_rectangle;
  iface->handle_events = gst_glimage_sink_handle_events;
  iface->expose = gst_glimage_sink_expose;
}

static void
gst_glimage_sink_set_window_handle (GstVideoOverlay * overlay, guintptr id)
{
  GstGLImageSink *glimage_sink = GST_GLIMAGE_SINK (overlay);
  guintptr window_id = (guintptr) id;

  g_return_if_fail (GST_IS_GLIMAGE_SINK (overlay));

  GST_DEBUG ("set_xwindow_id %" G_GUINT64_FORMAT, (guint64) window_id);

  glimage_sink->new_window_id = window_id;
}


static void
gst_glimage_sink_expose (GstVideoOverlay * overlay)
{
  GstGLImageSink *glimage_sink = GST_GLIMAGE_SINK (overlay);

  /* redisplay opengl scene */
  if (glimage_sink->display) {
    if (glimage_sink->window_id
        && glimage_sink->window_id != glimage_sink->new_window_id) {
      GstGLWindow *window = gst_gl_context_get_window (glimage_sink->context);

      glimage_sink->window_id = glimage_sink->new_window_id;
      gst_gl_window_set_window_handle (window, glimage_sink->window_id);

      gst_object_unref (window);
    }

    gst_glimage_sink_redisplay (glimage_sink);
  }
}

static void
gst_glimage_sink_handle_events (GstVideoOverlay * overlay,
    gboolean handle_events)
{
  GstGLImageSink *glimage_sink = GST_GLIMAGE_SINK (overlay);

  glimage_sink->handle_events = handle_events;
  if (G_LIKELY (glimage_sink->context)) {
    GstGLWindow *window;
    window = gst_gl_context_get_window (glimage_sink->context);
    gst_gl_window_handle_events (window, handle_events);
    gst_object_unref (window);
  }
}

static void
gst_glimage_sink_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint width, gint height)
{
  GstGLImageSink *glimage_sink = GST_GLIMAGE_SINK (overlay);

  if (G_LIKELY (glimage_sink->context)) {
    GstGLWindow *window;
    window = gst_gl_context_get_window (glimage_sink->context);
    gst_gl_window_set_render_rectangle (window, x, y, width, height);
    gst_object_unref (window);
  }

  glimage_sink->x = x;
  glimage_sink->y = y;
  glimage_sink->width = width;
  glimage_sink->height = height;
}

static gboolean
gst_glimage_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstGLImageSink *glimage_sink = GST_GLIMAGE_SINK (bsink);
  GstStructure *config;
  GstCaps *caps;
  GstBufferPool *pool = NULL;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;
  GstStructure *allocation_meta = NULL;
  GstGLContext *context = NULL;

  if (!_ensure_gl_setup (glimage_sink))
    return FALSE;

  GST_GLIMAGE_SINK_LOCK (glimage_sink);
  if (glimage_sink->context)
    context = gst_object_ref (glimage_sink->context);
  GST_GLIMAGE_SINK_UNLOCK (glimage_sink);

  if (!context)
    return FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GST_DEBUG_OBJECT (glimage_sink, "create new pool");

    pool = gst_gl_buffer_pool_new (context);
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);

    if (!gst_buffer_pool_set_config (pool, config)) {
      g_object_unref (pool);
      goto config_failed;
    }
  }

  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    g_object_unref (pool);

  if (context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, 0);

  if (glimage_sink->window_width != 0 && glimage_sink->window_height != 0) {
    allocation_meta =
        gst_structure_new ("GstVideoOverlayCompositionMeta",
        "width", G_TYPE_UINT, glimage_sink->window_width,
        "height", G_TYPE_UINT, glimage_sink->window_height, NULL);
    GST_DEBUG ("sending alloc query with size %dx%d",
        glimage_sink->window_width, glimage_sink->window_height);
  }

  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, allocation_meta);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_AFFINE_TRANSFORMATION_META_API_TYPE, 0);

  if (allocation_meta)
    gst_structure_free (allocation_meta);

  gst_object_unref (context);
  return TRUE;

  /* ERRORS */
no_caps:
  {
    gst_object_unref (context);
    GST_WARNING_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    gst_object_unref (context);
    GST_WARNING_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    gst_object_unref (context);
    GST_WARNING_OBJECT (bsink, "failed setting config");
    return FALSE;
  }
}

/* *INDENT-OFF* */
static const GLfloat vertices[] = {
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f
};

static const GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
/* *INDENT-ON* */

static void
_bind_buffer (GstGLImageSink * gl_sink)
{
  const GstGLFuncs *gl = gl_sink->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, gl_sink->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, gl_sink->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (gl_sink->attr_position, 3, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (gl_sink->attr_texture, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (gl_sink->attr_position);
  gl->EnableVertexAttribArray (gl_sink->attr_texture);
}

static void
_unbind_buffer (GstGLImageSink * gl_sink)
{
  const GstGLFuncs *gl = gl_sink->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (gl_sink->attr_position);
  gl->DisableVertexAttribArray (gl_sink->attr_texture);
}

/* Called in the gl thread */
static void
gst_glimage_sink_thread_init_redisplay (GstGLImageSink * gl_sink)
{
  const GstGLFuncs *gl = gl_sink->context->gl_vtable;
  GError *error = NULL;
  GstGLSLStage *frag_stage, *vert_stage;

  vert_stage = gst_glsl_stage_new_with_string (gl_sink->context,
      GL_VERTEX_SHADER, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
      gst_gl_shader_string_vertex_mat4_vertex_transform);
  if (gl_sink->texture_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
    gchar *frag_str;
    frag_str =
        gst_gl_shader_string_fragment_external_oes_get_default
        (gl_sink->context, GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);

    frag_stage = gst_glsl_stage_new_with_string (gl_sink->context,
        GL_FRAGMENT_SHADER, GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, frag_str);

    g_free (frag_str);
  } else {
    frag_stage = gst_glsl_stage_new_default_fragment (gl_sink->context);
  }
  if (!vert_stage || !frag_stage) {
    GST_ERROR_OBJECT (gl_sink, "Failed to retrieve fragment shader for "
        "texture target");
    if (vert_stage)
      gst_object_unref (vert_stage);
    if (frag_stage)
      gst_object_unref (frag_stage);
    gst_glimage_sink_cleanup_glthread (gl_sink);
    return;
  }

  if (!(gl_sink->redisplay_shader =
          gst_gl_shader_new_link_with_stages (gl_sink->context, &error,
              vert_stage, frag_stage, NULL))) {
    GST_ERROR_OBJECT (gl_sink, "Failed to link shader: %s", error->message);
    gst_glimage_sink_cleanup_glthread (gl_sink);
    return;
  }

  gl_sink->attr_position =
      gst_gl_shader_get_attribute_location (gl_sink->redisplay_shader,
      "a_position");
  gl_sink->attr_texture =
      gst_gl_shader_get_attribute_location (gl_sink->redisplay_shader,
      "a_texcoord");

  if (gl->GenVertexArrays) {
    gl->GenVertexArrays (1, &gl_sink->vao);
    gl->BindVertexArray (gl_sink->vao);
  }

  if (!gl_sink->vertex_buffer) {
    gl->GenBuffers (1, &gl_sink->vertex_buffer);
    gl->BindBuffer (GL_ARRAY_BUFFER, gl_sink->vertex_buffer);
    gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices,
        GL_STATIC_DRAW);
  }

  if (!gl_sink->vbo_indices) {
    gl->GenBuffers (1, &gl_sink->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, gl_sink->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);
  }

  if (gl->GenVertexArrays) {
    _bind_buffer (gl_sink);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
}

static void
gst_glimage_sink_cleanup_glthread (GstGLImageSink * gl_sink)
{
  const GstGLFuncs *gl = gl_sink->context->gl_vtable;

  if (gl_sink->redisplay_shader) {
    gst_object_unref (gl_sink->redisplay_shader);
    gl_sink->redisplay_shader = NULL;
  }

  if (gl_sink->vao) {
    gl->DeleteVertexArrays (1, &gl_sink->vao);
    gl_sink->vao = 0;
  }

  if (gl_sink->vertex_buffer) {
    gl->DeleteBuffers (1, &gl_sink->vertex_buffer);
    gl_sink->vertex_buffer = 0;
  }

  if (gl_sink->vbo_indices) {
    gl->DeleteBuffers (1, &gl_sink->vbo_indices);
    gl_sink->vbo_indices = 0;
  }

  if (gl_sink->overlay_compositor)
    gst_gl_overlay_compositor_free_overlays (gl_sink->overlay_compositor);
}

/* Called with object lock held */
static void
gst_glimage_sink_on_resize (GstGLImageSink * gl_sink, gint width, gint height)
{
  /* Here gl_sink members (ex:gl_sink->out_info) have a life time of set_caps.
   * It means that they cannot change between two set_caps
   */
  const GstGLFuncs *gl;
  gboolean do_reshape;

  GST_DEBUG_OBJECT (gl_sink, "GL Window resized to %ux%u", width, height);

  /* check if a client reshape callback is registered */
  g_signal_emit (gl_sink, gst_glimage_sink_signals[CLIENT_RESHAPE_SIGNAL], 0,
      gl_sink->context, width, height, &do_reshape);
  GST_GLIMAGE_SINK_LOCK (gl_sink);

  gl = gl_sink->context->gl_vtable;
  width = MAX (1, width);
  height = MAX (1, height);

  /* Check if we would suggest a different width/height now */
  gl_sink->window_resized = ((gl_sink->window_width != width)
      || (gl_sink->window_height != height))
      && (gl_sink->window_width != 0)
      && (gl_sink->window_height != 0);

  gl_sink->window_width = width;
  gl_sink->window_height = height;

  gst_gl_insert_debug_marker (gl_sink->context, "%s window resize to %ix%i",
      GST_OBJECT_NAME (gl_sink), width, height);

  /* default reshape */
  if (!do_reshape) {
    if (gl_sink->keep_aspect_ratio) {
      GstVideoRectangle src, dst, result;

      src.x = 0;
      src.y = 0;
      if (gl_sink->current_rotate_method == GST_VIDEO_ORIENTATION_90R
          || gl_sink->current_rotate_method == GST_VIDEO_ORIENTATION_90L
          || gl_sink->current_rotate_method == GST_VIDEO_ORIENTATION_UL_LR
          || gl_sink->current_rotate_method == GST_VIDEO_ORIENTATION_UR_LL) {
        src.h = GST_VIDEO_SINK_WIDTH (gl_sink);
        src.w = GST_VIDEO_SINK_HEIGHT (gl_sink);
      } else {
        src.w = GST_VIDEO_SINK_WIDTH (gl_sink);
        src.h = GST_VIDEO_SINK_HEIGHT (gl_sink);
      }

      /* The stream may not be negotiated yet */
      src.w = MAX (1, src.w);
      src.h = MAX (1, src.h);

      dst.x = 0;
      dst.y = 0;
      dst.w = width;
      dst.h = height;

      gst_video_sink_center_rect (src, dst, &result, TRUE);
      gl_sink->output_mode_changed |= (result.w != gl_sink->display_rect.w);
      gl_sink->output_mode_changed |= (result.h != gl_sink->display_rect.h);
      gl_sink->display_rect = result;
    } else {
      gl_sink->output_mode_changed |= (width != gl_sink->display_rect.w);
      gl_sink->output_mode_changed |= (height != gl_sink->display_rect.h);

      gl_sink->display_rect.x = 0;
      gl_sink->display_rect.y = 0;
      gl_sink->display_rect.w = width;
      gl_sink->display_rect.h = height;
    }

    gl->Viewport (gl_sink->display_rect.x, gl_sink->display_rect.y,
        gl_sink->display_rect.w, gl_sink->display_rect.h);
    GST_DEBUG_OBJECT (gl_sink, "GL output area now %u,%u %ux%u",
        gl_sink->display_rect.x, gl_sink->display_rect.y,
        gl_sink->display_rect.w, gl_sink->display_rect.h);
  } else {
    gint viewport_dims[4];

    /* save the viewport for use later */
    gl->GetIntegerv (GL_VIEWPORT, viewport_dims);

    gl_sink->display_rect.x = viewport_dims[0];
    gl_sink->display_rect.y = viewport_dims[1];
    gl_sink->display_rect.w = viewport_dims[2];
    gl_sink->display_rect.h = viewport_dims[3];
  }
  GST_GLIMAGE_SINK_UNLOCK (gl_sink);
}

static void
gst_glimage_sink_on_draw (GstGLImageSink * gl_sink)
{
  /* Here gl_sink members (ex:gl_sink->out_info) have a life time of set_caps.
   * It means that they cannot not change between two set_caps as well as
   * for the redisplay_texture size.
   * Whereas redisplay_texture id changes every sink_render
   */

  const GstGLFuncs *gl = NULL;
  GstGLWindow *window = NULL;
  gboolean do_redisplay = FALSE;
  GstSample *sample = NULL;
  guint gl_target = gst_gl_texture_target_to_gl (gl_sink->texture_target);

  g_return_if_fail (GST_IS_GLIMAGE_SINK (gl_sink));

  GST_GLIMAGE_SINK_LOCK (gl_sink);

  gl = gl_sink->context->gl_vtable;

  /* check if texture is ready for being drawn */
  if (!gl_sink->redisplay_texture) {
    GST_GLIMAGE_SINK_UNLOCK (gl_sink);
    return;
  }

  window = gst_gl_context_get_window (gl_sink->context);
  window->is_drawing = TRUE;

  /* opengl scene */
  gst_gl_insert_debug_marker (gl_sink->context, "%s element drawing texture %u",
      GST_OBJECT_NAME (gl_sink), gl_sink->redisplay_texture);
  GST_TRACE ("redrawing texture:%u", gl_sink->redisplay_texture);

  if (gl_sink->stored_sync_meta)
    gst_gl_sync_meta_wait (gl_sink->stored_sync_meta,
        gst_gl_context_get_current ());

  /* make sure that the environment is clean */
  gst_gl_context_clear_shader (gl_sink->context);
  gl->BindTexture (gl_target, 0);

  if (!gst_gl_window_controls_viewport (window)) {
    gl->Viewport (gl_sink->display_rect.x, gl_sink->display_rect.y,
        gl_sink->display_rect.w, gl_sink->display_rect.h);
    GST_DEBUG_OBJECT (gl_sink, "GL output area now %u,%u %ux%u",
        gl_sink->display_rect.x, gl_sink->display_rect.y,
        gl_sink->display_rect.w, gl_sink->display_rect.h);
  }

  sample = gst_sample_new (gl_sink->stored_buffer[0],
      gl_sink->out_caps, &GST_BASE_SINK (gl_sink)->segment, NULL);
  g_signal_emit (gl_sink, gst_glimage_sink_signals[CLIENT_DRAW_SIGNAL], 0,
      gl_sink->context, sample, &do_redisplay);
  gst_sample_unref (sample);

  if (gl_sink->stored_buffer[1]) {
    sample = gst_sample_new (gl_sink->stored_buffer[1],
        gl_sink->out_caps, &GST_BASE_SINK (gl_sink)->segment, NULL);
    g_signal_emit (gl_sink, gst_glimage_sink_signals[CLIENT_DRAW_SIGNAL], 0,
        gl_sink->context, sample, &do_redisplay);
    gst_sample_unref (sample);
  }

  if (!do_redisplay) {
    gfloat alpha = gl_sink->ignore_alpha ? 1.0f : 0.0f;

    gl->ClearColor (0.0, 0.0, 0.0, alpha);
    gl->Clear (GL_COLOR_BUFFER_BIT);

    if (gl_sink->ignore_alpha) {
      gl->BlendColor (0.0, 0.0, 0.0, alpha);
      if (gl->BlendFuncSeparate) {
        gl->BlendFuncSeparate (GL_SRC_ALPHA, GL_CONSTANT_COLOR, GL_ONE,
            GL_ONE_MINUS_SRC_ALPHA);
      } else {
        /* we don't have separate blend modes, perform something close to
         * correct instead */
        gl->BlendFunc (GL_SRC_ALPHA, GL_CONSTANT_COLOR);
      }
      gl->BlendEquation (GL_FUNC_ADD);
      gl->Enable (GL_BLEND);
    }

    gst_gl_shader_use (gl_sink->redisplay_shader);

    if (gl->GenVertexArrays)
      gl->BindVertexArray (gl_sink->vao);
    _bind_buffer (gl_sink);

    gl->ActiveTexture (GL_TEXTURE0);
    gl->BindTexture (gl_target, gl_sink->redisplay_texture);
    gst_gl_shader_set_uniform_1i (gl_sink->redisplay_shader, "tex", 0);
    {
      GstVideoAffineTransformationMeta *af_meta;
      gfloat matrix[16];

      af_meta =
          gst_buffer_get_video_affine_transformation_meta
          (gl_sink->stored_buffer[0]);

      if (gl_sink->transform_matrix) {
        gfloat tmp[16];

        gst_gl_get_affine_transformation_meta_as_ndc (af_meta, tmp);
        gst_gl_multiply_matrix4 (tmp, gl_sink->transform_matrix, matrix);
      } else {
        gst_gl_get_affine_transformation_meta_as_ndc (af_meta, matrix);
      }

      gst_gl_shader_set_uniform_matrix_4fv (gl_sink->redisplay_shader,
          "u_transformation", 1, FALSE, matrix);
    }

    gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    gl->BindTexture (gl_target, 0);
    gst_gl_context_clear_shader (gl_sink->context);

    if (gl->GenVertexArrays)
      gl->BindVertexArray (0);
    else
      _unbind_buffer (gl_sink);

    if (gl_sink->ignore_alpha)
      gl->Disable (GL_BLEND);

    gst_gl_overlay_compositor_draw_overlays (gl_sink->overlay_compositor);
  }
  /* end default opengl scene */
  window->is_drawing = FALSE;
  gst_object_unref (window);

  GST_GLIMAGE_SINK_UNLOCK (gl_sink);
}

static void
gst_glimage_sink_on_close (GstGLImageSink * gl_sink)
{
  GstGLWindow *window = NULL;

  GST_WARNING_OBJECT (gl_sink, "Output window was closed");

  GST_GLIMAGE_SINK_LOCK (gl_sink);
  if (gl_sink->context)
    window = gst_gl_context_get_window (gl_sink->context);
  GST_GLIMAGE_SINK_UNLOCK (gl_sink);

  if (window) {
    if (gl_sink->key_sig_id)
      g_signal_handler_disconnect (window, gl_sink->key_sig_id);
    gl_sink->key_sig_id = 0;
    if (gl_sink->mouse_sig_id)
      g_signal_handler_disconnect (window, gl_sink->mouse_sig_id);
    gl_sink->mouse_sig_id = 0;
    if (gl_sink->mouse_scroll_sig_id)
      g_signal_handler_disconnect (window, gl_sink->mouse_scroll_sig_id);
    gl_sink->mouse_scroll_sig_id = 0;

    gst_object_unref (window);
  }

  g_atomic_int_set (&gl_sink->to_quit, 1);
}

static gboolean
gst_glimage_sink_redisplay (GstGLImageSink * gl_sink)
{
  GstGLWindow *window;
  GstBuffer *old_stored_buffer[2], *old_sync;
  gulong handler_id;

  window = gst_gl_context_get_window (gl_sink->context);
  if (!window)
    return FALSE;

  handler_id =
      g_signal_handler_find (GST_ELEMENT_PARENT (gl_sink), G_SIGNAL_MATCH_ID,
      gst_gl_image_sink_bin_signals[SIGNAL_BIN_CLIENT_DRAW], 0,
      NULL, NULL, NULL);

  if (G_UNLIKELY (!gl_sink->redisplay_shader) && (!handler_id
          || !gl_sink->other_context)) {
    gst_gl_window_send_message (window,
        GST_GL_WINDOW_CB (gst_glimage_sink_thread_init_redisplay), gl_sink);

    /* if the shader is still null it means it failed to be useable */
    if (G_UNLIKELY (!gl_sink->redisplay_shader)) {
      gst_object_unref (window);
      return FALSE;
    }

    gst_gl_window_set_preferred_size (window, GST_VIDEO_SINK_WIDTH (gl_sink),
        GST_VIDEO_SINK_HEIGHT (gl_sink));
    gst_gl_window_show (window);
  }

  /* Recreate the output texture if needed */
  GST_GLIMAGE_SINK_LOCK (gl_sink);
  if (gl_sink->window_resized) {
    gl_sink->window_resized = FALSE;
    GST_GLIMAGE_SINK_UNLOCK (gl_sink);
    GST_DEBUG_OBJECT (gl_sink, "Sending reconfigure event on sinkpad.");
    gst_pad_push_event (GST_BASE_SINK (gl_sink)->sinkpad,
        gst_event_new_reconfigure ());
    GST_GLIMAGE_SINK_LOCK (gl_sink);
  }

  if (gl_sink->output_mode_changed && gl_sink->input_buffer != NULL) {
    GST_DEBUG ("Recreating output after mode/size change");
    update_output_format (gl_sink);
    prepare_next_buffer (gl_sink);
  }

  if (gl_sink->next_buffer == NULL) {
    /* Nothing to display yet */
    GST_GLIMAGE_SINK_UNLOCK (gl_sink);
    gst_object_unref (window);
    return TRUE;
  }

  /* Avoid to release the texture while drawing */
  gl_sink->redisplay_texture = gl_sink->next_tex;
  old_stored_buffer[0] = gl_sink->stored_buffer[0];
  old_stored_buffer[1] = gl_sink->stored_buffer[1];
  gl_sink->stored_buffer[0] = gst_buffer_ref (gl_sink->next_buffer);
  if (gl_sink->next_buffer2)
    gl_sink->stored_buffer[1] = gst_buffer_ref (gl_sink->next_buffer2);
  else
    gl_sink->stored_buffer[1] = NULL;

  old_sync = gl_sink->stored_sync;
  if (gl_sink->next_sync)
    gl_sink->stored_sync = gst_buffer_ref (gl_sink->next_sync);
  else
    gl_sink->stored_sync = NULL;
  gl_sink->stored_sync_meta = gl_sink->next_sync_meta;
  GST_GLIMAGE_SINK_UNLOCK (gl_sink);

  gst_buffer_replace (old_stored_buffer, NULL);
  gst_buffer_replace (old_stored_buffer + 1, NULL);
  if (old_sync)
    gst_buffer_unref (old_sync);

  /* Drawing is asynchronous: gst_gl_window_draw is not blocking
   * It means that it does not wait for stuff to be executed in other threads
   */
  gst_gl_window_draw (window);
  gst_object_unref (window);

  return TRUE;
}
