/*
 * GStreamer
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
 * SECTION:element-caopengllayersink
 *
 * caopengllayersink renders incoming video frames to CAOpenGLLayer that
 * can be retrieved through the layer property and placed in the Core
 * Animation render tree.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "caopengllayersink.h"
#include "gstglsinkbin.h"
#include <QuartzCore/QuartzCore.h>

GST_DEBUG_CATEGORY (gst_debug_ca_sink);
#define GST_CAT_DEFAULT gst_debug_ca_sink

typedef GstGLSinkBin GstCAOpenGLLayerSinkBin;
typedef GstGLSinkBinClass GstCAOpenGLLayerSinkBinClass;

G_DEFINE_TYPE (GstCAOpenGLLayerSinkBin, gst_ca_opengl_layer_sink_bin,
    GST_TYPE_GL_SINK_BIN);

#define _do_init \
    gl_element_init (plugin);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (caopengllayersink, "caopengllayersink",
    GST_RANK_NONE, GST_TYPE_CA_OPENGL_LAYER_SINK, _do_init);

enum
{
  PROP_BIN_0,
  PROP_BIN_QOS,
  PROP_BIN_FORCE_ASPECT_RATIO,
  PROP_BIN_LAST_SAMPLE,
  PROP_BIN_LAYER,
};

static void
_on_notify_layer (GObject * object, GParamSpec *pspec, gpointer user_data)
{
  GstCAOpenGLLayerSinkBin *self = user_data;

  g_object_notify (G_OBJECT (self), "layer");
}

static void
gst_ca_opengl_layer_sink_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec)
{
  g_object_set_property (G_OBJECT (GST_GL_SINK_BIN (object)->sink),
      param_spec->name, value);
}

static void
gst_ca_opengl_layer_sink_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec)
{
  g_object_get_property (G_OBJECT (GST_GL_SINK_BIN (object)->sink),
      param_spec->name, value);
}

static void
gst_ca_opengl_layer_sink_bin_init (GstCAOpenGLLayerSinkBin * self)
{
  gpointer *sink = g_object_new (GST_TYPE_CA_OPENGL_LAYER_SINK, NULL);

  g_signal_connect (sink, "notify::layer", G_CALLBACK (_on_notify_layer), self);

  gst_gl_sink_bin_finish_init_with_element (GST_GL_SINK_BIN (self),
      GST_ELEMENT (sink));
}

static void
gst_ca_opengl_layer_sink_bin_class_init (GstCAOpenGLLayerSinkBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gst_ca_opengl_layer_sink_bin_get_property;
  gobject_class->set_property = gst_ca_opengl_layer_sink_bin_set_property;

  g_object_class_install_property (gobject_class, PROP_BIN_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BIN_LAST_SAMPLE,
      g_param_spec_boxed ("last-sample", "Last Sample",
          "The last sample received in the sink", GST_TYPE_SAMPLE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BIN_LAYER,
      g_param_spec_pointer ("layer", "CAOpenGLLayer",
          "OpenGL Core Animation layer",
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BIN_QOS,
      g_param_spec_boolean ("qos", "Quality of Service",
          "Generate Quality-of-Service events upstream", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

#define GST_CA_OPENGL_LAYER_SINK_GET_LOCK(glsink) \
  (GST_CA_OPENGL_LAYER_SINK(glsink)->drawing_lock)
#define GST_CA_OPENGL_LAYER_SINK_LOCK(glsink) \
  (g_mutex_lock(&GST_CA_OPENGL_LAYER_SINK_GET_LOCK (glsink)))
#define GST_CA_OPENGL_LAYER_SINK_UNLOCK(glsink) \
  (g_mutex_unlock(&GST_CA_OPENGL_LAYER_SINK_GET_LOCK (glsink)))

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

#define SUPPORTED_GL_APIS GST_GL_API_OPENGL | GST_GL_API_GLES2 | GST_GL_API_OPENGL3

static void gst_ca_opengl_layer_sink_thread_init_redisplay (GstCAOpenGLLayerSink * ca_sink);
static void gst_ca_opengl_layer_sink_cleanup_glthread (GstCAOpenGLLayerSink * ca_sink);
static void gst_ca_opengl_layer_sink_on_resize (GstCAOpenGLLayerSink * ca_sink,
    gint width, gint height);
static void gst_ca_opengl_layer_sink_on_draw (GstCAOpenGLLayerSink * ca_sink);

static void gst_ca_opengl_layer_sink_finalize (GObject * object);
static void gst_ca_opengl_layer_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_ca_opengl_layer_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static gboolean gst_ca_opengl_layer_sink_stop (GstBaseSink * bsink);

static gboolean gst_ca_opengl_layer_sink_query (GstBaseSink * bsink, GstQuery * query);
static void gst_ca_opengl_layer_sink_set_context (GstElement * element,
    GstContext * context);

static GstStateChangeReturn gst_ca_opengl_layer_sink_change_state (GstElement *
    element, GstStateChange transition);

static void gst_ca_opengl_layer_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_ca_opengl_layer_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static GstFlowReturn gst_ca_opengl_layer_sink_prepare (GstBaseSink * bsink,
    GstBuffer * buf);
static GstFlowReturn gst_ca_opengl_layer_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);
static gboolean gst_ca_opengl_layer_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);

static GstStaticPadTemplate gst_ca_opengl_layer_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
      "format = (string) RGBA, "
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", "
      "framerate = " GST_VIDEO_FPS_RANGE ","
      "texture-target = (string) 2D")
    );

enum
{
  PROP_0,
  PROP_FORCE_ASPECT_RATIO,
  PROP_CONTEXT,
  PROP_LAYER,
};

#define gst_ca_opengl_layer_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstCAOpenGLLayerSink, gst_ca_opengl_layer_sink,
    GST_TYPE_VIDEO_SINK, GST_DEBUG_CATEGORY_INIT (gst_debug_ca_sink,
        "caopengllayersink", 0, "CAOpenGLLayer Video Sink"));

static void
gst_ca_opengl_layer_sink_class_init (GstCAOpenGLLayerSinkClass * klass)
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

  gobject_class->set_property = gst_ca_opengl_layer_sink_set_property;
  gobject_class->get_property = gst_ca_opengl_layer_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONTEXT,
      g_param_spec_object ("context",
          "OpenGL context",
          "Get OpenGL context",
          GST_TYPE_GL_CONTEXT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LAYER,
      g_param_spec_pointer ("layer", "CAOpenGLLayer",
          "OpenGL Core Animation layer",
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class, "CAOpenGLLayer video sink",
      "Sink/Video", "A video sink based on CAOpenGLLayer",
      "Matthew Waters <matthew@centricular.com>");

  gst_element_class_add_static_pad_template (element_class, &gst_ca_opengl_layer_sink_template);

  gobject_class->finalize = gst_ca_opengl_layer_sink_finalize;

  gstelement_class->change_state = gst_ca_opengl_layer_sink_change_state;
  gstelement_class->set_context = gst_ca_opengl_layer_sink_set_context;
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_ca_opengl_layer_sink_query);
  gstbasesink_class->set_caps = gst_ca_opengl_layer_sink_set_caps;
  gstbasesink_class->get_times = gst_ca_opengl_layer_sink_get_times;
  gstbasesink_class->prepare = gst_ca_opengl_layer_sink_prepare;
  gstbasesink_class->propose_allocation = gst_ca_opengl_layer_sink_propose_allocation;
  gstbasesink_class->stop = gst_ca_opengl_layer_sink_stop;

  gstvideosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_ca_opengl_layer_sink_show_frame);
}

static void
gst_ca_opengl_layer_sink_init (GstCAOpenGLLayerSink * ca_sink)
{
  ca_sink->display = NULL;
  ca_sink->keep_aspect_ratio = TRUE;
  ca_sink->stored_buffer = NULL;
  ca_sink->redisplay_texture = 0;

  g_mutex_init (&ca_sink->drawing_lock);
}

static void
gst_ca_opengl_layer_sink_finalize (GObject * object)
{
  GstCAOpenGLLayerSink *ca_sink;

  g_return_if_fail (GST_IS_CA_OPENGL_LAYER_SINK (object));

  ca_sink = GST_CA_OPENGL_LAYER_SINK (object);

  g_mutex_clear (&ca_sink->drawing_lock);

  if (ca_sink->layer) {
    CFRelease(ca_sink->layer);
    ca_sink->layer = NULL;
  }

  GST_DEBUG ("finalized");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ca_opengl_layer_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCAOpenGLLayerSink *ca_sink;

  g_return_if_fail (GST_IS_CA_OPENGL_LAYER_SINK (object));

  ca_sink = GST_CA_OPENGL_LAYER_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
    {
      ca_sink->keep_aspect_ratio = g_value_get_boolean (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ca_opengl_layer_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCAOpenGLLayerSink *ca_sink;

  g_return_if_fail (GST_IS_CA_OPENGL_LAYER_SINK (object));

  ca_sink = GST_CA_OPENGL_LAYER_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, ca_sink->keep_aspect_ratio);
      break;
    case PROP_CONTEXT:
      g_value_set_object (value, ca_sink->context);
      break;
    case PROP_LAYER:
      g_value_set_pointer (value, ca_sink->layer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
_create_layer (gpointer data)
{
  GstCAOpenGLLayerSink *ca_sink = data;
  id layer;

  if (!ca_sink->layer) {
    layer = [[NSClassFromString(@"GstGLCAOpenGLLayer") alloc]
        initWithGstGLContext:ca_sink->context];

    ca_sink->layer = (__bridge_retained gpointer)layer;
    [layer setDrawCallback:(GstGLWindowCB)gst_ca_opengl_layer_sink_on_draw
        data:ca_sink notify:NULL];
    [layer setResizeCallback:(GstGLWindowResizeCB)gst_ca_opengl_layer_sink_on_resize
        data:ca_sink notify:NULL];
    g_object_notify (G_OBJECT (ca_sink), "layer");
  }
}

static void
_invoke_on_main (GstGLWindowCB func, gpointer data)
{
  if ([NSThread isMainThread]) {
    func (data);
  } else {
    dispatch_sync (dispatch_get_main_queue (), ^{
      func (data);
    });
  }
}

static gboolean
_ensure_gl_setup (GstCAOpenGLLayerSink * ca_sink)
{
  GError *error = NULL;

  if (!gst_gl_ensure_element_data (ca_sink, &ca_sink->display,
          &ca_sink->other_context))
    return FALSE;

  gst_gl_display_filter_gl_api (ca_sink->display, SUPPORTED_GL_APIS);

  if (!ca_sink->context) {
    if (!gst_gl_display_create_context (ca_sink->display,
            ca_sink->other_context, &ca_sink->context, &error)) {
      goto context_error;
    }
  }

  if (!ca_sink->layer)
    _invoke_on_main ((GstGLWindowCB) _create_layer, ca_sink);

  return TRUE;

context_error:
  {
    GST_ELEMENT_ERROR (ca_sink, RESOURCE, NOT_FOUND, ("%s", error->message),
        (NULL));
    gst_object_unref (ca_sink->context);
    ca_sink->context = NULL;
    return FALSE;
  }
}

static gboolean
gst_ca_opengl_layer_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstCAOpenGLLayerSink *ca_sink = GST_CA_OPENGL_LAYER_SINK (bsink);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      if (gst_gl_handle_context_query ((GstElement *) ca_sink, query,
          ca_sink->display, ca_sink->context, ca_sink->other_context))
        return TRUE;
      break;
    }
    case GST_QUERY_DRAIN:
    {
      GstBuffer *buf = NULL;

      GST_CA_OPENGL_LAYER_SINK_LOCK (ca_sink);
      ca_sink->redisplay_texture = 0;
      buf = ca_sink->stored_buffer;
      ca_sink->stored_buffer = NULL;
      GST_CA_OPENGL_LAYER_SINK_UNLOCK (ca_sink);

      if (buf)
        gst_buffer_unref (buf);

      gst_buffer_replace (&ca_sink->next_buffer, NULL);
      gst_buffer_replace (&ca_sink->next_sync, NULL);

      break;
    }
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
}

static gboolean
gst_ca_opengl_layer_sink_stop (GstBaseSink * bsink)
{
  GstCAOpenGLLayerSink *ca_sink = GST_CA_OPENGL_LAYER_SINK (bsink);

  if (ca_sink->gl_caps) {
    gst_caps_unref (ca_sink->gl_caps);
    ca_sink->gl_caps = NULL;
  }

  return TRUE;
}

static void
gst_ca_opengl_layer_sink_set_context (GstElement * element, GstContext * context)
{
  GstCAOpenGLLayerSink *ca_sink = GST_CA_OPENGL_LAYER_SINK (element);

  gst_gl_handle_set_context (element, context, &ca_sink->display,
      &ca_sink->other_context);

  if (ca_sink->display)
    gst_gl_display_filter_gl_api (ca_sink->display, SUPPORTED_GL_APIS);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static GstStateChangeReturn
gst_ca_opengl_layer_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstCAOpenGLLayerSink *ca_sink;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  ca_sink = GST_CA_OPENGL_LAYER_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      _ensure_gl_setup (ca_sink);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
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
      /* mark the redisplay_texture as unavailable (=0)
       * to avoid drawing
       */
      GST_CA_OPENGL_LAYER_SINK_LOCK (ca_sink);
      ca_sink->redisplay_texture = 0;

      gst_buffer_replace (&ca_sink->stored_sync, NULL);

      if (ca_sink->stored_buffer) {
        gst_buffer_unref (ca_sink->stored_buffer);
        ca_sink->stored_buffer = NULL;
      }
      gst_buffer_replace (&ca_sink->next_buffer, NULL);
      gst_buffer_replace (&ca_sink->next_sync, NULL);
      GST_CA_OPENGL_LAYER_SINK_UNLOCK (ca_sink);

      GST_VIDEO_SINK_WIDTH (ca_sink) = 1;
      GST_VIDEO_SINK_HEIGHT (ca_sink) = 1;
      if (ca_sink->context) {
        gst_object_unref (ca_sink->context);
        ca_sink->context = NULL;
      }

      if (ca_sink->display) {
        gst_object_unref (ca_sink->display);
        ca_sink->display = NULL;
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (ca_sink->layer) {
        CFRelease(ca_sink->layer);
        ca_sink->layer = NULL;
      }
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_ca_opengl_layer_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstCAOpenGLLayerSink *ca_sink;

  ca_sink = GST_CA_OPENGL_LAYER_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (GST_VIDEO_INFO_FPS_N (&ca_sink->info) > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&ca_sink->info),
            GST_VIDEO_INFO_FPS_N (&ca_sink->info));
      }
    }
  }
}

static gboolean
gst_ca_opengl_layer_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstCAOpenGLLayerSink *ca_sink;
  gint width;
  gint height;
  gboolean ok;
  gint par_n, par_d;
  gint display_par_n, display_par_d;
  guint display_ratio_num, display_ratio_den;
  GstVideoInfo vinfo;

  GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);

  ca_sink = GST_CA_OPENGL_LAYER_SINK (bsink);

  ok = gst_video_info_from_caps (&vinfo, caps);
  if (!ok)
    return FALSE;

  width = GST_VIDEO_INFO_WIDTH (&vinfo);
  height = GST_VIDEO_INFO_HEIGHT (&vinfo);

  par_n = GST_VIDEO_INFO_PAR_N (&vinfo);
  par_d = GST_VIDEO_INFO_PAR_D (&vinfo);

  if (!par_n)
    par_n = 1;

  display_par_n = 1;
  display_par_d = 1;

  ok = gst_video_calculate_display_ratio (&display_ratio_num,
      &display_ratio_den, width, height, par_n, par_d, display_par_n,
      display_par_d);

  if (!ok)
    return FALSE;

  GST_TRACE ("PAR: %u/%u DAR:%u/%u", par_n, par_d, display_par_n,
      display_par_d);

  if (height % display_ratio_den == 0) {
    GST_DEBUG ("keeping video height");
    GST_VIDEO_SINK_WIDTH (ca_sink) = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    GST_VIDEO_SINK_HEIGHT (ca_sink) = height;
  } else if (width % display_ratio_num == 0) {
    GST_DEBUG ("keeping video width");
    GST_VIDEO_SINK_WIDTH (ca_sink) = width;
    GST_VIDEO_SINK_HEIGHT (ca_sink) = (guint)
        gst_util_uint64_scale_int (width, display_ratio_den, display_ratio_num);
  } else {
    GST_DEBUG ("approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (ca_sink) = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    GST_VIDEO_SINK_HEIGHT (ca_sink) = height;
  }
  GST_DEBUG ("scaling to %dx%d", GST_VIDEO_SINK_WIDTH (ca_sink),
      GST_VIDEO_SINK_HEIGHT (ca_sink));

  ca_sink->info = vinfo;
  if (!_ensure_gl_setup (ca_sink))
    return FALSE;

  ca_sink->caps_change = TRUE;

  return TRUE;
}

static GstFlowReturn
gst_ca_opengl_layer_sink_prepare (GstBaseSink * bsink, GstBuffer * buf)
{
  GstCAOpenGLLayerSink *ca_sink;
  GstBuffer *next_sync, *old_sync, *old_buffer;
  GstVideoFrame gl_frame;
  GstGLSyncMeta *sync_meta;

  ca_sink = GST_CA_OPENGL_LAYER_SINK (bsink);

  GST_TRACE ("preparing buffer:%p", buf);

  if (GST_VIDEO_SINK_WIDTH (ca_sink) < 1 ||
      GST_VIDEO_SINK_HEIGHT (ca_sink) < 1) {
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (!_ensure_gl_setup (ca_sink))
    return GST_FLOW_NOT_NEGOTIATED;

  if (!gst_video_frame_map (&gl_frame, &ca_sink->info, buf,
          GST_MAP_READ | GST_MAP_GL)) {
    goto upload_failed;
  }

  ca_sink->next_tex = *(guint *) gl_frame.data[0];

  next_sync = gst_buffer_new ();
  sync_meta = gst_buffer_add_gl_sync_meta (ca_sink->context, next_sync);
  gst_gl_sync_meta_set_sync_point (sync_meta, ca_sink->context);

  GST_CA_OPENGL_LAYER_SINK_LOCK (ca_sink);
  ca_sink->next_tex = *(guint *) gl_frame.data[0];

  old_buffer = ca_sink->next_buffer;
  ca_sink->next_buffer = gst_buffer_ref (buf);

  old_sync = ca_sink->next_sync;
  ca_sink->next_sync = next_sync;
  GST_CA_OPENGL_LAYER_SINK_UNLOCK (ca_sink);

  if (old_buffer)
    gst_buffer_unref (old_buffer);
  if (old_sync)
    gst_buffer_unref (old_sync);

  gst_video_frame_unmap (&gl_frame);

  return GST_FLOW_OK;

upload_failed:
  {
    GST_ELEMENT_ERROR (ca_sink, RESOURCE, NOT_FOUND,
        ("%s", "Failed to upload buffer"), (NULL));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_ca_opengl_layer_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstCAOpenGLLayerSink *ca_sink;
  GstBuffer *stored_buffer, *old_sync;

  GST_TRACE ("rendering buffer:%p", buf);

  ca_sink = GST_CA_OPENGL_LAYER_SINK (vsink);

  GST_TRACE ("redisplay texture:%u of size:%ux%u, window size:%ux%u",
      ca_sink->next_tex, GST_VIDEO_INFO_WIDTH (&ca_sink->info),
      GST_VIDEO_INFO_HEIGHT (&ca_sink->info),
      GST_VIDEO_SINK_WIDTH (ca_sink),
      GST_VIDEO_SINK_HEIGHT (ca_sink));

  /* Avoid to release the texture while drawing */
  GST_CA_OPENGL_LAYER_SINK_LOCK (ca_sink);
  ca_sink->redisplay_texture = ca_sink->next_tex;

  stored_buffer = ca_sink->stored_buffer;
  ca_sink->stored_buffer = gst_buffer_ref (ca_sink->next_buffer);

  old_sync = ca_sink->stored_sync;
  ca_sink->stored_sync = gst_buffer_ref (ca_sink->next_sync);
  GST_CA_OPENGL_LAYER_SINK_UNLOCK (ca_sink);

  /* The layer will automatically call the draw callback to draw the new
   * content */
  [CATransaction begin];
  [(__bridge GstGLCAOpenGLLayer *)(ca_sink->layer) setNeedsDisplay];
  [CATransaction commit];

  GST_TRACE ("post redisplay");

  if (stored_buffer)
    gst_buffer_unref (stored_buffer);
  if (old_sync)
    gst_buffer_unref (old_sync);

  return GST_FLOW_OK;
}

static gboolean
gst_ca_opengl_layer_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstCAOpenGLLayerSink *ca_sink = GST_CA_OPENGL_LAYER_SINK (bsink);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;

  if (!_ensure_gl_setup (ca_sink))
    return FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GST_DEBUG_OBJECT (ca_sink, "create new pool");

    pool = gst_gl_buffer_pool_new (ca_sink->context);
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config))
        goto config_failed;
  }

  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  if (ca_sink->context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, 0);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
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
_bind_buffer (GstCAOpenGLLayerSink * ca_sink)
{
  const GstGLFuncs *gl = ca_sink->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, ca_sink->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, ca_sink->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (ca_sink->attr_position, 3, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (ca_sink->attr_texture, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (ca_sink->attr_position);
  gl->EnableVertexAttribArray (ca_sink->attr_texture);
}

static void
_unbind_buffer (GstCAOpenGLLayerSink * ca_sink)
{
  const GstGLFuncs *gl = ca_sink->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (ca_sink->attr_position);
  gl->DisableVertexAttribArray (ca_sink->attr_texture);
}

/* Called in the gl thread */
static void
gst_ca_opengl_layer_sink_thread_init_redisplay (GstCAOpenGLLayerSink * ca_sink)
{
  const GstGLFuncs *gl = ca_sink->context->gl_vtable;
  GError *error = NULL;

  if (!(ca_sink->redisplay_shader = gst_gl_shader_new_default (ca_sink->context, &error))) {
    GST_ERROR_OBJECT (ca_sink, "Failed to link shader: %s", error->message);
    gst_ca_opengl_layer_sink_cleanup_glthread (ca_sink);
    return;
  }

  ca_sink->attr_position =
      gst_gl_shader_get_attribute_location (ca_sink->redisplay_shader,
      "a_position");
  ca_sink->attr_texture =
      gst_gl_shader_get_attribute_location (ca_sink->redisplay_shader,
      "a_texcoord");

  if (gl->GenVertexArrays) {
    gl->GenVertexArrays (1, &ca_sink->vao);
    gl->BindVertexArray (ca_sink->vao);
  }

  if (!ca_sink->vertex_buffer) {
    gl->GenBuffers (1, &ca_sink->vertex_buffer);
    gl->BindBuffer (GL_ARRAY_BUFFER, ca_sink->vertex_buffer);
    gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices,
        GL_STATIC_DRAW);
  }

  if (!ca_sink->vbo_indices) {
    gl->GenBuffers (1, &ca_sink->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, ca_sink->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);
  }

  if (gl->GenVertexArrays) {
    _bind_buffer (ca_sink);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
}

static void
gst_ca_opengl_layer_sink_cleanup_glthread (GstCAOpenGLLayerSink * ca_sink)
{
  const GstGLFuncs *gl = ca_sink->context->gl_vtable;

  if (ca_sink->redisplay_shader) {
    gst_object_unref (ca_sink->redisplay_shader);
    ca_sink->redisplay_shader = NULL;
  }

  if (ca_sink->vao) {
    gl->DeleteVertexArrays (1, &ca_sink->vao);
    ca_sink->vao = 0;
  }

  if (ca_sink->vbo_indices) {
    gl->DeleteBuffers (1, &ca_sink->vbo_indices);
    ca_sink->vbo_indices = 0;
  }

  if (ca_sink->vertex_buffer) {
    gl->DeleteBuffers (1, &ca_sink->vertex_buffer);
    ca_sink->vertex_buffer = 0;
  }
}

static void
gst_ca_opengl_layer_sink_on_resize (GstCAOpenGLLayerSink * ca_sink, gint width, gint height)
{
  /* Here ca_sink members (ex:ca_sink->info) have a life time of set_caps.
   * It means that they cannot not change between two set_caps
   */
  const GstGLFuncs *gl = ca_sink->context->gl_vtable;

  GST_TRACE ("GL Window resized to %ux%u", width, height);

  width = MAX (1, width);
  height = MAX (1, height);

  ca_sink->window_width = width;
  ca_sink->window_height = height;

  /* default reshape */
  if (ca_sink->keep_aspect_ratio) {
    GstVideoRectangle src, dst, result;

    src.x = 0;
    src.y = 0;
    src.w = GST_VIDEO_SINK_WIDTH (ca_sink);
    src.h = GST_VIDEO_SINK_HEIGHT (ca_sink);

    dst.x = 0;
    dst.y = 0;
    dst.w = width;
    dst.h = height;

    gst_video_sink_center_rect (src, dst, &result, TRUE);
    gl->Viewport (result.x, result.y, result.w, result.h);
  } else {
    gl->Viewport (0, 0, width, height);
  }
}

static void
gst_ca_opengl_layer_sink_on_draw (GstCAOpenGLLayerSink * ca_sink)
{
  /* Here ca_sink members (ex:ca_sink->info) have a life time of set_caps.
   * It means that they cannot not change between two set_caps as well as
   * for the redisplay_texture size.
   * Whereas redisplay_texture id changes every sink_render
   */

  const GstGLFuncs *gl = NULL;
  GstGLSyncMeta *sync_meta;

  g_return_if_fail (GST_IS_CA_OPENGL_LAYER_SINK (ca_sink));

  gl = ca_sink->context->gl_vtable;

  GST_CA_OPENGL_LAYER_SINK_LOCK (ca_sink);

  if (G_UNLIKELY (!ca_sink->redisplay_shader)) {
    gst_ca_opengl_layer_sink_thread_init_redisplay (ca_sink);
  }

  /* check if texture is ready for being drawn */
  if (!ca_sink->redisplay_texture) {
    gl->ClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    gl->Clear (GL_COLOR_BUFFER_BIT);
    GST_CA_OPENGL_LAYER_SINK_UNLOCK (ca_sink);
    return;
  }

  /* opengl scene */
  GST_TRACE ("redrawing texture:%u", ca_sink->redisplay_texture);

  if (ca_sink->caps_change) {
    GST_CA_OPENGL_LAYER_SINK_UNLOCK (ca_sink);
    gst_ca_opengl_layer_sink_on_resize (ca_sink, ca_sink->window_width,
        ca_sink->window_height);
    GST_CA_OPENGL_LAYER_SINK_LOCK (ca_sink);
    ca_sink->caps_change = FALSE;
  }

  sync_meta = gst_buffer_get_gl_sync_meta (ca_sink->stored_sync);
  if (sync_meta)
    gst_gl_sync_meta_wait (sync_meta, gst_gl_context_get_current ());

  gl->BindTexture (GL_TEXTURE_2D, 0);

  gl->ClearColor (0.0, 0.0, 0.0, 0.0);
  gl->Clear (GL_COLOR_BUFFER_BIT);

  gst_gl_shader_use (ca_sink->redisplay_shader);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (ca_sink->vao);
  _bind_buffer (ca_sink);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, ca_sink->redisplay_texture);
  gst_gl_shader_set_uniform_1i (ca_sink->redisplay_shader, "tex", 0);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (0);
  else
    _unbind_buffer (ca_sink);

  /* end default opengl scene */
  GST_CA_OPENGL_LAYER_SINK_UNLOCK (ca_sink);
}
