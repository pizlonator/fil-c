/* Copyright (C) <2018> Philippe Normand <philn@igalia.com>
 * Copyright (C) <2018> Žan Doberšek <zdobersek@igalia.com>
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
 * SECTION:element-wpevideosrc
 * @title: wpevideosrc
 *
 * The wpevideosrc element is used to produce a video texture representing a web page
 * rendered off-screen by WPE.
 *
 * Starting from WPEBackend-FDO 1.6.x, software rendering support is available. This
 * features allows wpevideosrc to be used on machines without GPU, and/or for testing
 * purpose. To enable it, set the `LIBGL_ALWAYS_SOFTWARE=true` environment
 * variable and make sure `video/x-raw, format=BGRA` caps are negotiated by the
 * wpevideosrc element.
 *
 * As the webview loading is usually not instantaneous, the wpevideosrc element emits
 * messages indicating the load progress, in percent. The value is an estimate
 * based on the total number of bytes expected to be received for a document,
 * including all its possible subresources and child documents. The application
 * can handle these `element` messages synchronously for instance, in order to
 * display a progress bar or other visual load indicator. The load percent value
 * is stored in the message structure as a double value named
 * `estimated-load-progress` and the structure name is `wpe-stats`.
 *
 * ## Example launch lines
 *
 * ```shell
 * gst-launch-1.0 -v wpevideosrc location="https://gstreamer.freedesktop.org" ! queue ! glimagesink
 * ```
 * Shows the GStreamer website homepage
 *
 * ```shell
 * LIBGL_ALWAYS_SOFTWARE=true gst-launch-1.0 -v wpevideosrc num-buffers=50 location="https://gstreamer.freedesktop.org" \
 *   videoconvert ! pngenc ! multifilesink location=/tmp/snapshot-%05d.png
 * ```
 * Saves the first 50 video frames generated for the GStreamer website as PNG files in /tmp.
 *
 * ```shell
 * gst-play-1.0 --videosink gtkglsink web+https://gstreamer.freedesktop.org
 * ```
 * Shows the GStreamer website homepage as played with GstPlayer in a GTK+ window.
 *
 * ```shell
 * gst-launch-1.0  glvideomixer name=m sink_1::zorder=0 ! glimagesink wpevideosrc location="file:///tmp/asset.html" draw-background=0 \
 *   ! m. videotestsrc ! queue ! glupload ! glcolorconvert ! m.
 * ```
 * Composite WPE with a video stream in a single OpenGL scene.
 *
 * ```shell
 * gst-launch-1.0 glvideomixer name=m sink_1::zorder=0 sink_0::height=818 sink_0::width=1920 ! gtkglsink \
 *    wpevideosrc location="file:///tmp/asset.html" draw-background=0 ! m.
 *    uridecodebin uri="http://example.com/Sintel.2010.1080p.mkv" name=d d. ! queue ! glupload ! glcolorconvert ! m.
 * ```
 * Composite WPE with a video stream, sink_0 pad properties have to match the video dimensions.
 *
 * ```shell
 * weston -S $HOME/weston-sock -B headless-backend.so --use-gl &
 * WAYLAND_DISPLAY=$HOME/weston-sock gst-launch-1.0 wpevideosrc location=https://google.com ! queue ! fakevideosink
 * ```
 * Render Google.com with WPE in a headless Weston compositor. This can be useful for server-side WPE video processing.
 *
 * Since: 1.16
 */

/*
 * TODO:
 * - DMABuf support (requires changes in WPEBackend-fdo to expose DMABuf planes and fds)
 * - Custom EGLMemory allocator
 * - Better navigation events handling (would require a new GstNavigation API)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwpe.h"
#include "gstwpevideosrc.h"
#include <gst/gl/gl.h>
#include <gst/gl/egl/gstglmemoryegl.h>
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#include <gst/video/video.h>
#include <xkbcommon/xkbcommon.h>

#include "gstwpethreadedview.h"

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080
#define DEFAULT_FPS_N 30
#define DEFAULT_FPS_D 1
#define DEFAULT_DRAW_BACKGROUND TRUE

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_DRAW_BACKGROUND
};

enum
{
  SIGNAL_CONFIGURE_WEB_VIEW,
  SIGNAL_LOAD_BYTES,
  SIGNAL_RUN_JAVASCRIPT,
  LAST_SIGNAL
};
static guint gst_wpe_video_src_signals[LAST_SIGNAL] = { 0 };

struct _GstWpeVideoSrc
{
  GstGLBaseSrc parent;

  /* properties */
  gchar *location;
  gboolean draw_background;

  GBytes *bytes;
  gboolean gl_enabled;

  gint64 n_frames;              /* total frames sent */

  GstWPEThreadedView *view;

  GArray *touch_points;
  struct wpe_input_touch_event_raw *last_touch;

  GMutex lock;
};

#define WPE_LOCK(o) g_mutex_lock(&(o)->lock)
#define WPE_UNLOCK(o) g_mutex_unlock(&(o)->lock)

GST_DEBUG_CATEGORY_EXTERN (wpe_video_src_debug);
#define GST_CAT_DEFAULT wpe_video_src_debug

#define gst_wpe_video_src_parent_class parent_class
G_DEFINE_TYPE (GstWpeVideoSrc, gst_wpe_video_src, GST_TYPE_GL_BASE_SRC);

#define WPE_RAW_CAPS "video/x-raw, "            \
  "format = (string) BGRA, "                    \
  "width = " GST_VIDEO_SIZE_RANGE ", "          \
  "height = " GST_VIDEO_SIZE_RANGE ", "         \
  "framerate = " GST_VIDEO_FPS_RANGE ", "       \
  "pixel-aspect-ratio = (fraction)1/1"

#define WPE_GL_CAPS "video/x-raw(memory:GLMemory), "    \
  "format = (string) RGBA, "                            \
  "width = " GST_VIDEO_SIZE_RANGE ", "                  \
  "height = " GST_VIDEO_SIZE_RANGE ", "                 \
  "framerate = " GST_VIDEO_FPS_RANGE ", "               \
  "pixel-aspect-ratio = (fraction)1/1, texture-target = (string)2D"

#define WPE_VIDEO_SRC_CAPS WPE_GL_CAPS "; " WPE_RAW_CAPS
#define WPE_VIDEO_SRC_DOC_CAPS WPE_GL_CAPS "; video/x-raw, format = (string) BGRA"

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (WPE_VIDEO_SRC_CAPS));

static GstFlowReturn
gst_wpe_video_src_create (GstBaseSrc * bsrc, guint64 offset, guint length,
    GstBuffer ** buf)
{
  GstGLBaseSrc *gl_src = GST_GL_BASE_SRC (bsrc);
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (bsrc);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstBuffer *locked_buffer;
  GstClockTime next_time;
  gint64 ts_offset = 0;

  WPE_LOCK (src);
  if (src->gl_enabled) {
    WPE_UNLOCK (src);
    return GST_CALL_PARENT_WITH_DEFAULT (GST_BASE_SRC_CLASS, create, (bsrc,
            offset, length, buf), ret);
  }

  locked_buffer = src->view->buffer ();
  if (locked_buffer == NULL) {
    WPE_UNLOCK (src);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("WPE View did not render a buffer"), (NULL));
    return ret;
  }
  *buf = gst_buffer_copy_deep (locked_buffer);

  g_object_get (gl_src, "timestamp-offset", &ts_offset, NULL);

  /* The following code mimics the behaviour of GLBaseSrc::fill */
  GST_BUFFER_TIMESTAMP (*buf) = ts_offset + gl_src->running_time;
  GST_BUFFER_OFFSET (*buf) = src->n_frames;
  src->n_frames++;
  GST_BUFFER_OFFSET_END (*buf) = src->n_frames;
  if (gl_src->out_info.fps_n) {
    next_time = gst_util_uint64_scale_int (src->n_frames * GST_SECOND,
        gl_src->out_info.fps_d, gl_src->out_info.fps_n);
    GST_BUFFER_DURATION (*buf) = next_time - gl_src->running_time;
  } else {
    next_time = ts_offset;
    GST_BUFFER_DURATION (*buf) = GST_CLOCK_TIME_NONE;
  }

  GST_LOG_OBJECT (src, "Created buffer from SHM %" GST_PTR_FORMAT, *buf);

  gl_src->running_time = next_time;

  ret = GST_FLOW_OK;
  WPE_UNLOCK (src);
  return ret;
}

static GQuark
_egl_image_quark (void)
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string ("GstWPEEGLImage");
  return quark;
}

static gboolean
gst_wpe_video_src_fill_memory (GstGLBaseSrc * bsrc, GstGLMemory * memory)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (bsrc);
  const GstGLFuncs *gl;
  guint tex_id;
  GstEGLImage *locked_image;

  if (!gst_gl_context_check_feature (GST_GL_CONTEXT (bsrc->context),
          "EGL_KHR_image_base")) {
    GST_ERROR_OBJECT (src, "EGL_KHR_image_base is not supported");
    return FALSE;
  }

  WPE_LOCK (src);

  gl = bsrc->context->gl_vtable;
  tex_id = gst_gl_memory_get_texture_id (memory);
  locked_image = src->view->image ();

  if (!locked_image) {
    WPE_UNLOCK (src);
    return TRUE;
  }

  // The EGLImage is implicitely associated with the memory we're filling, so we
  // need to ensure their life cycles are tied.
  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (memory), _egl_image_quark (),
      gst_egl_image_ref (locked_image), (GDestroyNotify) gst_egl_image_unref);

  gl->ActiveTexture (GL_TEXTURE0 + memory->plane);
  gl->BindTexture (GL_TEXTURE_2D, tex_id);
  gl->EGLImageTargetTexture2D (GL_TEXTURE_2D,
      gst_egl_image_get_image (locked_image));
  gl->Flush ();
  WPE_UNLOCK (src);
  return TRUE;
}

static gboolean
gst_wpe_video_src_start (GstWpeVideoSrc * src)
{
  GstGLContext *context = NULL;
  GstGLDisplay *display = NULL;
  GstGLBaseSrc *base_src = GST_GL_BASE_SRC (src);
  gboolean created_view = FALSE;
  GBytes *bytes;

  GST_INFO_OBJECT (src, "Starting up");
  WPE_LOCK (src);

  if (src->gl_enabled) {
    context = base_src->context;
    display = base_src->display;
  }

  GST_DEBUG_OBJECT (src, "Will %sfill GLMemories",
      src->gl_enabled ? "" : "NOT ");

  auto & thread = GstWPEContextThread::singleton ();

  if (!src->view) {
    src->view = thread.createWPEView (src, context, display,
        GST_VIDEO_INFO_WIDTH (&base_src->out_info),
        GST_VIDEO_INFO_HEIGHT (&base_src->out_info));
    created_view = TRUE;
    GST_DEBUG_OBJECT (src, "created view %p", src->view);
  }

  if (!src->view) {
    WPE_UNLOCK (src);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("WPEBackend-FDO EGL display initialisation failed"), (NULL));
    return FALSE;
  }

  GST_OBJECT_LOCK (src);
  bytes = src->bytes;
  src->bytes = NULL;
  GST_OBJECT_UNLOCK (src);

  if (bytes != NULL) {
    src->view->loadData (bytes);
    g_bytes_unref (bytes);
  }

  if (created_view) {
    src->n_frames = 0;
  }
  WPE_UNLOCK (src);
  return TRUE;
}

static gboolean
gst_wpe_video_src_decide_allocation (GstBaseSrc * base_src, GstQuery * query)
{
  GstGLBaseSrc *gl_src = GST_GL_BASE_SRC (base_src);
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (base_src);
  GstCapsFeatures *caps_features;

  WPE_LOCK (src);
  caps_features = gst_caps_get_features (gl_src->out_caps, 0);
  if (caps_features != NULL
      && gst_caps_features_contains (caps_features,
          GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
    src->gl_enabled = TRUE;
  } else {
    src->gl_enabled = FALSE;
  }

  if (src->gl_enabled) {
    WPE_UNLOCK (src);
    return GST_CALL_PARENT_WITH_DEFAULT (GST_BASE_SRC_CLASS, decide_allocation,
        (base_src, query), FALSE);
  }
  WPE_UNLOCK (src);
  return gst_wpe_video_src_start (src);
}

static gboolean
gst_wpe_video_src_gl_start (GstGLBaseSrc * base_src)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (base_src);
  return gst_wpe_video_src_start (src);
}

static void
gst_wpe_video_src_stop_unlocked (GstWpeVideoSrc * src)
{
  if (src->view) {
    GST_DEBUG_OBJECT (src, "deleting view %p", src->view);
    delete src->view;
    src->view = NULL;
  }
}

static void
gst_wpe_video_src_gl_stop (GstGLBaseSrc * base_src)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (base_src);

  WPE_LOCK (src);
  gst_wpe_video_src_stop_unlocked (src);
  WPE_UNLOCK (src);
}

static gboolean
gst_wpe_video_src_stop (GstBaseSrc * base_src)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (base_src);

  /* we can call this always, GstGLBaseSrc is smart enough to not crash if
   * gst_gl_base_src_gl_start() has not been called from chaining up
   * gst_wpe_video_src_decide_allocation() */
  if (!GST_CALL_PARENT_WITH_DEFAULT (GST_BASE_SRC_CLASS, stop, (base_src),
          FALSE))
    return FALSE;

  WPE_LOCK (src);

  /* if gl-enabled, gst_wpe_video_src_stop_unlocked() would have already been called
   * inside gst_wpe_video_src_gl_stop() from the base class stopping the OpenGL
   * context */
  if (!src->gl_enabled)
    gst_wpe_video_src_stop_unlocked (src);

  WPE_UNLOCK (src);
  return TRUE;
}

static GstCaps *
gst_wpe_video_src_fixate (GstBaseSrc * base_src, GstCaps * combined_caps)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (base_src);
  GstStructure *structure;
  gint width, height;
  GstCaps *caps;

  /* In situation where software GL support is explicitly requested, select raw
   * caps, otherwise perform default caps negotiation. Unfortunately at this
   * point we don't know yet if a GL context will be usable or not, so we can't
   * check the element GstContext.
   */
  if (!g_strcmp0 (g_getenv ("LIBGL_ALWAYS_SOFTWARE"), "true")) {
    caps = gst_caps_from_string (WPE_RAW_CAPS);
  } else {
    caps = gst_caps_make_writable (combined_caps);
  }

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", DEFAULT_WIDTH);
  gst_structure_fixate_field_nearest_int (structure, "height", DEFAULT_HEIGHT);

  if (gst_structure_has_field (structure, "framerate"))
    gst_structure_fixate_field_nearest_fraction (structure, "framerate",
        DEFAULT_FPS_N, DEFAULT_FPS_D);
  else
    gst_structure_set (structure, "framerate", GST_TYPE_FRACTION, DEFAULT_FPS_N,
        DEFAULT_FPS_D, NULL);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (base_src, caps);
  GST_INFO_OBJECT (base_src, "Fixated caps to %" GST_PTR_FORMAT, caps);

  if (src->view) {
    gst_structure_get (structure, "width", G_TYPE_INT, &width, "height",
        G_TYPE_INT, &height, NULL);
    src->view->resize (width, height);
  }
  return caps;
}

void
gst_wpe_video_src_configure_web_view (GstWpeVideoSrc * src,
    WebKitWebView * webview)
{
  GValue args[2] = { {0}, {0} };

  g_value_init (&args[0], GST_TYPE_ELEMENT);
  g_value_set_object (&args[0], src);
  g_value_init (&args[1], G_TYPE_OBJECT);
  g_value_set_object (&args[1], webview);

  g_signal_emitv (args, gst_wpe_video_src_signals[SIGNAL_CONFIGURE_WEB_VIEW], 0,
      NULL);

  g_value_unset (&args[0]);
  g_value_unset (&args[1]);
}

static void
gst_wpe_video_src_run_javascript (GstWpeVideoSrc * src, const gchar * script)
{
  if (src->view && GST_STATE (GST_ELEMENT_CAST (src)) > GST_STATE_NULL) {
    GST_INFO_OBJECT (src, "running javascript");
    src->view->runJavascript (script);
  }
}

static void
gst_wpe_video_src_load_bytes (GstWpeVideoSrc * src, GBytes * bytes)
{
  if (src->view && GST_STATE (GST_ELEMENT_CAST (src)) > GST_STATE_NULL) {
    src->view->loadData (bytes);
  } else {
    GST_OBJECT_LOCK (src);
    if (src->bytes)
      g_bytes_unref (src->bytes);
    src->bytes = g_bytes_ref (bytes);
    GST_OBJECT_UNLOCK (src);
  }
}

static gboolean
gst_wpe_video_src_set_location (GstWpeVideoSrc * src, const gchar * location,
    GError ** error)
{
  GST_OBJECT_LOCK (src);
  g_free (src->location);
  src->location = g_strdup (location);
  GST_OBJECT_UNLOCK (src);

  if (src->view)
    src->view->loadUri (location);

  return TRUE;
}

static void
gst_wpe_video_src_set_draw_background (GstWpeVideoSrc * src,
    gboolean draw_background)
{
  GST_OBJECT_LOCK (src);
  src->draw_background = draw_background;
  GST_OBJECT_UNLOCK (src);

  if (src->view)
    src->view->setDrawBackground (draw_background);
}

static void
gst_wpe_video_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
    {
      const gchar *location;

      location = g_value_get_string (value);
      if (location == NULL) {
        GST_WARNING_OBJECT (src, "location property cannot be NULL");
        return;
      }

      if (!gst_wpe_video_src_set_location (src, location, NULL)) {
        GST_WARNING_OBJECT (src, "badly formatted location");
        return;
      }
      break;
    }
    case PROP_DRAW_BACKGROUND:
      gst_wpe_video_src_set_draw_background (src, g_value_get_boolean (value));
      break;
    default:
      break;
  }
}

static void
gst_wpe_video_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      GST_OBJECT_LOCK (src);
      g_value_set_string (value, src->location);
      GST_OBJECT_UNLOCK (src);
      break;
    case PROP_DRAW_BACKGROUND:
      GST_OBJECT_LOCK (src);
      g_value_set_boolean (value, src->draw_background);
      GST_OBJECT_UNLOCK (src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
_set_touch_point (struct wpe_input_touch_event_raw * point,
    GstEvent * event, enum wpe_input_touch_event_type type, guint id, gdouble x,
    gdouble y)
{

  point->time = GST_TIME_AS_MSECONDS (GST_EVENT_TIMESTAMP (event));
  point->type = type;
  point->id = (int) id;
  point->x = (int32_t) x;
  point->y = (int32_t) y;
}

static uint32_t
_gst_modifiers_to_wpe (GstEvent * ev)
{
  GstNavigationModifierType modifier_state;
  uint32_t modifiers = 0;

  if (gst_navigation_event_parse_modifier_state (ev, &modifier_state)) {
    if (modifier_state & GST_NAVIGATION_MODIFIER_CONTROL_MASK)
      modifiers |= wpe_input_keyboard_modifier_control;
    if (modifier_state & GST_NAVIGATION_MODIFIER_SHIFT_MASK)
      modifiers |= wpe_input_keyboard_modifier_shift;
    if (modifier_state & GST_NAVIGATION_MODIFIER_MOD1_MASK)
      modifiers |= wpe_input_keyboard_modifier_alt;
    if (modifier_state & GST_NAVIGATION_MODIFIER_META_MASK)
      modifiers |= wpe_input_keyboard_modifier_meta;
    if (modifier_state & GST_NAVIGATION_MODIFIER_BUTTON1_MASK)
      modifiers |= wpe_input_pointer_modifier_button1;
    if (modifier_state & GST_NAVIGATION_MODIFIER_BUTTON2_MASK)
      modifiers |= wpe_input_pointer_modifier_button2;
    if (modifier_state & GST_NAVIGATION_MODIFIER_BUTTON3_MASK)
      modifiers |= wpe_input_pointer_modifier_button3;
    if (modifier_state & GST_NAVIGATION_MODIFIER_BUTTON4_MASK)
      modifiers |= wpe_input_pointer_modifier_button4;
    if (modifier_state & GST_NAVIGATION_MODIFIER_BUTTON5_MASK)
      modifiers |= wpe_input_pointer_modifier_button5;
  }

  return modifiers;
}

static gboolean
gst_wpe_video_src_event (GstBaseSrc * base_src, GstEvent * event)
{
  gboolean ret = FALSE;
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (base_src);

  if (src->view && GST_EVENT_TYPE (event) == GST_EVENT_NAVIGATION) {
    const gchar *key;
    gint button;
    guint touch_id;
    gdouble x, y, delta_x, delta_y;

    GST_DEBUG_OBJECT (src, "Processing event %" GST_PTR_FORMAT, event);
    switch (gst_navigation_event_get_type (event)) {
      case GST_NAVIGATION_EVENT_KEY_PRESS:
      case GST_NAVIGATION_EVENT_KEY_RELEASE:
        if (gst_navigation_event_parse_key_event (event, &key)) {
          /* FIXME: This is wrong... The GstNavigation API should pass
             hardware-level information, not high-level keysym strings */
          gunichar *unichar;
          glong items_written;
          uint32_t keysym;
          struct wpe_input_keyboard_event wpe_event = { 0 };

          unichar = g_utf8_to_ucs4_fast (key, -1, &items_written);
          if (items_written == 1)
            keysym = (uint32_t) xkb_utf32_to_keysym (*unichar);
          else
            keysym =
              (uint32_t) xkb_keysym_from_name (key, XKB_KEYSYM_NO_FLAGS);

          wpe_event.key_code = keysym;
          wpe_event.pressed =
              gst_navigation_event_get_type (event) ==
              GST_NAVIGATION_EVENT_KEY_PRESS;
          wpe_event.modifiers = _gst_modifiers_to_wpe (event);
          src->view->dispatchKeyboardEvent (wpe_event);
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
      case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:
        if (gst_navigation_event_parse_mouse_button_event (event, &button, &x,
                &y)) {
          struct wpe_input_pointer_event wpe_event;
          wpe_event.time = GST_TIME_AS_MSECONDS (GST_EVENT_TIMESTAMP (event));
          wpe_event.type = wpe_input_pointer_event_type_button;
          wpe_event.x = (int) x;
          wpe_event.y = (int) y;
          wpe_event.modifiers = _gst_modifiers_to_wpe (event);
          wpe_event.button = button;
          wpe_event.state =
              gst_navigation_event_get_type (event) ==
              GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS;
          src->view->dispatchPointerEvent (wpe_event);
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_MOUSE_MOVE:
        if (gst_navigation_event_parse_mouse_move_event (event, &x, &y)) {
          struct wpe_input_pointer_event wpe_event;
          wpe_event.time = GST_TIME_AS_MSECONDS (GST_EVENT_TIMESTAMP (event));
          wpe_event.type = wpe_input_pointer_event_type_motion;
          wpe_event.x = (int) x;
          wpe_event.y = (int) y;
          wpe_event.modifiers = _gst_modifiers_to_wpe (event);
          src->view->dispatchPointerEvent (wpe_event);
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_MOUSE_SCROLL:
        if (gst_navigation_event_parse_mouse_scroll_event (event, &x, &y,
                &delta_x, &delta_y)) {
          struct wpe_input_axis_event wpe_event;
          if (delta_x) {
            wpe_event.axis = 1;
            wpe_event.value = delta_x;
          } else {
            wpe_event.axis = 0;
            wpe_event.value = delta_y;
          }
          wpe_event.time = GST_TIME_AS_MSECONDS (GST_EVENT_TIMESTAMP (event));
          wpe_event.type = wpe_input_axis_event_type_motion;
          wpe_event.x = (int) x;
          wpe_event.y = (int) y;
          src->view->dispatchAxisEvent (wpe_event);
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_TOUCH_DOWN:
        if (gst_navigation_event_parse_touch_event (event, &touch_id, &x, &y,
              NULL)) {
          struct wpe_input_touch_event_raw *point = g_new
              (struct wpe_input_touch_event_raw, 1);

          _set_touch_point (point, event, wpe_input_touch_event_type_down,
              touch_id, x, y);
          src->touch_points = g_array_append_vals(src->touch_points, point, 1);
          src->last_touch = point;
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_TOUCH_MOTION:
        if (gst_navigation_event_parse_touch_event (event, &touch_id, &x, &y,
              NULL)) {
          struct wpe_input_touch_event_raw *touch_points;
          guint idx;

          touch_points = (struct wpe_input_touch_event_raw *)
              src->touch_points->data;
          for (idx = 0; idx < src->touch_points->len; idx++) {
            if (touch_points[idx].id == (int32_t) touch_id) {
              _set_touch_point (&touch_points[idx], event,
                  wpe_input_touch_event_type_motion, touch_id, x, y);
              src->last_touch = &touch_points[idx];
              break;
            }
          }
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_TOUCH_UP:
        if (gst_navigation_event_parse_touch_up_event (event, &touch_id, &x,
              &y)) {
          struct wpe_input_touch_event_raw *touch_points;
          guint idx;

          touch_points = (struct wpe_input_touch_event_raw *)
              src->touch_points->data;
          for (idx = 0; idx < src->touch_points->len; idx++) {
            if (touch_points[idx].id == (int32_t) touch_id) {
              _set_touch_point (&touch_points[idx], event,
                  wpe_input_touch_event_type_up, touch_id, x, y);
              src->last_touch = &touch_points[idx];
              break;
            }
          }
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_TOUCH_FRAME:
        if (src->last_touch && src->touch_points->len > 0)
        {
          struct wpe_input_touch_event_raw *touch_points;
          struct wpe_input_touch_event wpe_event;
          guint idx;

          wpe_event.touchpoints =
              (struct wpe_input_touch_event_raw *) src->touch_points->data;
          wpe_event.touchpoints_length = src->touch_points->len;
          wpe_event.type = src->last_touch->type;
          wpe_event.id = src->last_touch->id;
          wpe_event.modifiers = _gst_modifiers_to_wpe (event);
          wpe_event.time = src->last_touch->time;
          src->view->dispatchTouchEvent (wpe_event);

          touch_points = (struct wpe_input_touch_event_raw *)
              src->touch_points->data;
          for (idx = 0; idx < src->touch_points->len; idx++) {
            if (touch_points[idx].type == wpe_input_touch_event_type_up ||
                touch_points[idx].type == wpe_input_touch_event_type_null) {
              g_array_remove_index_fast(src->touch_points, idx);
              idx--;
            }
          }
          src->last_touch = NULL;
          ret = TRUE;
        }
        break;
      case GST_NAVIGATION_EVENT_TOUCH_CANCEL:
        break;
      default:
        break;
    }
  }

  if (!ret) {
    ret =
        GST_CALL_PARENT_WITH_DEFAULT (GST_BASE_SRC_CLASS, event, (base_src,
            event), FALSE);
  }
  return ret;
}

static void
gst_wpe_video_src_init (GstWpeVideoSrc * src)
{
  src->draw_background = DEFAULT_DRAW_BACKGROUND;
  src->location = g_strdup (DEFAULT_LOCATION);

  gst_base_src_set_live (GST_BASE_SRC_CAST (src), TRUE);

  src->touch_points = g_array_sized_new (FALSE, FALSE, sizeof (void *), 10);
  g_mutex_init (&src->lock);
}

static void
gst_wpe_video_src_finalize (GObject * object)
{
  GstWpeVideoSrc *src = GST_WPE_VIDEO_SRC (object);

  g_free (src->location);
  g_clear_pointer (&src->bytes, g_bytes_unref);
  g_array_unref (src->touch_points);
  g_mutex_clear (&src->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wpe_video_src_class_init (GstWpeVideoSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstGLBaseSrcClass *gl_base_src_class = GST_GL_BASE_SRC_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPadTemplate *tmpl;
  GstCaps *doc_caps;

  gobject_class->set_property = gst_wpe_video_src_set_property;
  gobject_class->get_property = gst_wpe_video_src_get_property;
  gobject_class->finalize = gst_wpe_video_src_finalize;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "location",
          "The URL to display",
          DEFAULT_LOCATION, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_DRAW_BACKGROUND,
      g_param_spec_boolean ("draw-background", "Draws the background",
          "Whether to draw the WebView background", DEFAULT_DRAW_BACKGROUND,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (gstelement_class,
      "WPE source", "Source/Video",
      "Creates a video stream from a WPE browser",
      "Philippe Normand <philn@igalia.com>, Žan Doberšek <zdobersek@igalia.com>");

  tmpl = gst_static_pad_template_get (&src_factory);
  gst_element_class_add_pad_template (gstelement_class, tmpl);

  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_wpe_video_src_fixate);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_wpe_video_src_create);
  base_src_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_wpe_video_src_decide_allocation);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_wpe_video_src_stop);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_wpe_video_src_event);

  gl_base_src_class->supported_gl_api =
      static_cast < GstGLAPI >
      (GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2);
  gl_base_src_class->gl_start = GST_DEBUG_FUNCPTR (gst_wpe_video_src_gl_start);
  gl_base_src_class->gl_stop = GST_DEBUG_FUNCPTR (gst_wpe_video_src_gl_stop);
  gl_base_src_class->fill_gl_memory =
      GST_DEBUG_FUNCPTR (gst_wpe_video_src_fill_memory);

  doc_caps = gst_caps_from_string (WPE_VIDEO_SRC_DOC_CAPS);
  gst_pad_template_set_documentation_caps (tmpl, doc_caps);
  gst_clear_caps (&doc_caps);

  /**
   * GstWpeVideoSrc::configure-web-view:
   * @src: the object which received the signal
   * @webview: the webView
   *
   * Allow application to configure the webView settings.
   */
  gst_wpe_video_src_signals[SIGNAL_CONFIGURE_WEB_VIEW] =
      g_signal_new ("configure-web-view", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_OBJECT);

  /**
   * GstWpeVideoSrc::load-bytes:
   * @src: the object which received the signal
   * @bytes: the GBytes data to load
   *
   * Load the specified bytes into the internal webView.
   */
  gst_wpe_video_src_signals[SIGNAL_LOAD_BYTES] =
      g_signal_new_class_handler ("load-bytes", G_TYPE_FROM_CLASS (klass),
      static_cast < GSignalFlags > (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
      G_CALLBACK (gst_wpe_video_src_load_bytes), NULL, NULL, NULL,
      G_TYPE_NONE, 1, G_TYPE_BYTES);

  /**
   * GstWpeSrc::run-javascript:
   * @src: the object which received the signal
   * @script: the script to run
   *
   * Asynchronously run script in the context of the current page on the
   * internal webView.
   *
   * Since: 1.22
   */
    gst_wpe_video_src_signals[SIGNAL_RUN_JAVASCRIPT] =
      g_signal_new_class_handler ("run-javascript", G_TYPE_FROM_CLASS (klass),
      static_cast < GSignalFlags > (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
      G_CALLBACK (gst_wpe_video_src_run_javascript), NULL, NULL, NULL,
      G_TYPE_NONE, 1, G_TYPE_STRING);
}
