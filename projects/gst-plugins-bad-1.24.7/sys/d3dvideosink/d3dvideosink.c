/* GStreamer
 * Copyright (C) 2012 Roland Krikava <info@bluedigits.com>
 * Copyright (C) 2010-2011 David Hoyt <dhoyt@hoytsoft.org>
 * Copyright (C) 2010 Andoni Morales <ylatuya@gmail.com>
 * Copyright (C) 2012 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "d3dvideosink.h"
#include "gstd3d9overlay.h"

#define ELEMENT_NAME  "d3dvideosink"

enum
{
  PROP_0,
  PROP_FORCE_ASPECT_RATIO,
  PROP_CREATE_RENDER_WINDOW,
  PROP_STREAM_STOP_ON_CLOSE,
  PROP_ENABLE_NAVIGATION_EVENTS,
  PROP_LAST
};

#define DEFAULT_FORCE_ASPECT_RATIO       TRUE
#define DEFAULT_CREATE_RENDER_WINDOW     TRUE
#define DEFAULT_STREAM_STOP_ON_CLOSE     TRUE
#define DEFAULT_ENABLE_NAVIGATION_EVENTS TRUE

#define GST_D3D9_VIDEO_FORMATS \
	"{ I420, YV12, UYVY, YUY2, NV12,  BGRx, RGBx, BGRA, RGBA, BGR, RGB16, RGB15 }"
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_D3D9_VIDEO_FORMATS) ";"
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY
            ", " GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D9_VIDEO_FORMATS)));

GST_DEBUG_CATEGORY (gst_d3dvideosink_debug);
#define GST_CAT_DEFAULT gst_d3dvideosink_debug

/* FWD DECLS */
/* GstXOverlay Interface */
static void
gst_d3dvideosink_video_overlay_interface_init (GstVideoOverlayInterface *
    iface);
static void gst_d3dvideosink_set_window_handle (GstVideoOverlay * overlay,
    guintptr window_id);
static void gst_d3dvideosink_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint width, gint height);
static void gst_d3dvideosink_expose (GstVideoOverlay * overlay);
/* GstNavigation Interface */
static void gst_d3dvideosink_navigation_interface_init (GstNavigationInterface *
    iface);
static void gst_d3dvideosink_navigation_send_event (GstNavigation * navigation,
    GstEvent * event);
/* GObject */
static void gst_d3dvideosink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3dvideosink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_d3dvideosink_finalize (GObject * gobject);
/* GstBaseSink */
static GstCaps *gst_d3dvideosink_get_caps (GstBaseSink * basesink,
    GstCaps * filter);
static gboolean gst_d3dvideosink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static gboolean gst_d3dvideosink_start (GstBaseSink * sink);
static gboolean gst_d3dvideosink_stop (GstBaseSink * sink);
static GstFlowReturn gst_d3dvideosink_prepare (GstBaseSink * bsink,
    GstBuffer * buf);
static gboolean gst_d3dvideosink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);
/* GstVideoSink */
static GstFlowReturn gst_d3dvideosink_show_frame (GstVideoSink * vsink,
    GstBuffer * buffer);

#define _do_init \
  G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION, gst_d3dvideosink_navigation_interface_init); \
  G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY, gst_d3dvideosink_video_overlay_interface_init); \
  GST_DEBUG_CATEGORY_INIT (gst_d3dvideosink_debug, ELEMENT_NAME, 0, "Direct3D Video");

G_DEFINE_TYPE_WITH_CODE (GstD3DVideoSink, gst_d3dvideosink, GST_TYPE_VIDEO_SINK,
    _do_init);

static void
gst_d3dvideosink_class_init (GstD3DVideoSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstVideoSinkClass *gstvideosink_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_d3dvideosink_finalize);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_d3dvideosink_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_d3dvideosink_get_property);

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_d3dvideosink_get_caps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_d3dvideosink_set_caps);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_d3dvideosink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_d3dvideosink_stop);
  gstbasesink_class->prepare = GST_DEBUG_FUNCPTR (gst_d3dvideosink_prepare);
  gstbasesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3dvideosink_propose_allocation);

  gstvideosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_d3dvideosink_show_frame);

  /* Add properties */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_FORCE_ASPECT_RATIO, g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_CREATE_RENDER_WINDOW, g_param_spec_boolean ("create-render-window",
          "Create render window",
          "If no window ID is given, a new render window is created",
          DEFAULT_CREATE_RENDER_WINDOW,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_STREAM_STOP_ON_CLOSE, g_param_spec_boolean ("stream-stop-on-close",
          "Stop streaming on window close",
          "If the render window is closed stop stream",
          DEFAULT_STREAM_STOP_ON_CLOSE,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_ENABLE_NAVIGATION_EVENTS,
      g_param_spec_boolean ("enable-navigation-events",
          "Enable navigation events",
          "When enabled, navigation events are sent upstream",
          DEFAULT_ENABLE_NAVIGATION_EVENTS,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "Direct3D9 video sink", "Sink/Video",
      "Display data using a Direct3D9 video renderer",
      "David Hoyt <dhoyt@hoytsoft.org>, Roland Krikava <info@bluedigits.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  g_rec_mutex_init (&klass->lock);
}

static void
gst_d3dvideosink_init (GstD3DVideoSink * sink)
{
  GST_DEBUG_OBJECT (sink, " ");

  /* Init Properties */
  sink->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  sink->create_internal_window = DEFAULT_CREATE_RENDER_WINDOW;
  sink->stream_stop_on_close = DEFAULT_STREAM_STOP_ON_CLOSE;
  sink->enable_navigation_events = DEFAULT_ENABLE_NAVIGATION_EVENTS;
  sink->d3d.surface = NULL;
  sink->d3d.overlay = NULL;
  sink->d3d.overlay_needs_resize = FALSE;
  g_rec_mutex_init (&sink->lock);
}

/* GObject Functions */

static void
gst_d3dvideosink_finalize (GObject * gobject)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (gobject);

  GST_DEBUG_OBJECT (sink, " ");

  gst_object_replace ((GstObject **) & sink->pool, NULL);
  gst_object_replace ((GstObject **) & sink->fallback_pool, NULL);

  gst_caps_replace (&sink->supported_caps, NULL);

  g_rec_mutex_clear (&sink->lock);

  G_OBJECT_CLASS (gst_d3dvideosink_parent_class)->finalize (gobject);
}

static void
gst_d3dvideosink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      sink->force_aspect_ratio = g_value_get_boolean (value);
      break;
    case PROP_CREATE_RENDER_WINDOW:
      sink->create_internal_window = g_value_get_boolean (value);
      break;
    case PROP_STREAM_STOP_ON_CLOSE:
      sink->stream_stop_on_close = g_value_get_boolean (value);
      break;
    case PROP_ENABLE_NAVIGATION_EVENTS:
      sink->enable_navigation_events = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3dvideosink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, sink->force_aspect_ratio);
      break;
    case PROP_CREATE_RENDER_WINDOW:
      g_value_set_boolean (value, sink->create_internal_window);
      break;
    case PROP_STREAM_STOP_ON_CLOSE:
      g_value_set_boolean (value, sink->stream_stop_on_close);
      break;
    case PROP_ENABLE_NAVIGATION_EVENTS:
      g_value_set_boolean (value, sink->enable_navigation_events);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseSinkClass Functions */

static GstCaps *
gst_d3dvideosink_get_caps (GstBaseSink * basesink, GstCaps * filter)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (basesink);
  GstCaps *caps;

  caps = d3d_supported_caps (sink);
  if (!caps)
    caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (sink));

  if (caps && filter) {
    GstCaps *isect;
    isect = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = isect;
  }

  return caps;
}

static gboolean
gst_d3dvideosink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstD3DVideoSink *sink;
  GstCaps *sink_caps;
  gint video_width, video_height;
  gint video_par_n, video_par_d;        /* video's PAR */
  gint display_par_n = 1, display_par_d = 1;    /* display's PAR */
  guint num, den;
  GstBufferPool *newpool, *oldpool;
  GstBufferPool *newfbpool, *oldfbpool;
  GstStructure *config;

  GST_DEBUG_OBJECT (bsink, "Caps: %" GST_PTR_FORMAT, caps);
  sink = GST_D3DVIDEOSINK (bsink);

  sink_caps = d3d_supported_caps (sink);

  if (!gst_caps_can_intersect (sink_caps, caps))
    goto incompatible_caps;
  gst_caps_replace (&sink_caps, NULL);

  memset (&sink->info, 0, sizeof (GstVideoInfo));
  if (!gst_video_info_from_caps (&sink->info, caps))
    goto invalid_format;

  sink->format = sink->info.finfo->format;
  video_width = sink->info.width;
  video_height = sink->info.height;
  video_par_n = sink->info.par_n;
  video_par_d = sink->info.par_d;

  GST_DEBUG_OBJECT (bsink, "Set Caps Format: %s",
      gst_video_format_to_string (sink->format));

  /* get aspect ratio from caps if it's present, and
   * convert video width and height to a display width and height
   * using wd / hd = wv / hv * PARv / PARd */

  /* TODO: Get display PAR */

  if (!gst_video_calculate_display_ratio (&num, &den, video_width,
          video_height, video_par_n, video_par_d, display_par_n, display_par_d))
    goto no_disp_ratio;

  GST_DEBUG_OBJECT (sink,
      "video width/height: %dx%d, calculated display ratio: %d/%d format: %u",
      video_width, video_height, num, den, sink->format);

  /* now find a width x height that respects this display ratio.
   * prefer those that have one of w/h the same as the incoming video
   * using wd / hd = num / den
   */

  /* start with same height, because of interlaced video
   * check hd / den is an integer scale factor, and scale wd with the PAR
   */
  if (video_height % den == 0) {
    GST_DEBUG_OBJECT (sink, "keeping video height");
    GST_VIDEO_SINK_WIDTH (sink) = (guint)
        gst_util_uint64_scale_int (video_height, num, den);
    GST_VIDEO_SINK_HEIGHT (sink) = video_height;
  } else if (video_width % num == 0) {
    GST_DEBUG_OBJECT (sink, "keeping video width");
    GST_VIDEO_SINK_WIDTH (sink) = video_width;
    GST_VIDEO_SINK_HEIGHT (sink) = (guint)
        gst_util_uint64_scale_int (video_width, den, num);
  } else {
    GST_DEBUG_OBJECT (sink, "approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (sink) = (guint)
        gst_util_uint64_scale_int (video_height, num, den);
    GST_VIDEO_SINK_HEIGHT (sink) = video_height;
  }
  GST_DEBUG_OBJECT (sink, "scaling to %dx%d",
      GST_VIDEO_SINK_WIDTH (sink), GST_VIDEO_SINK_HEIGHT (sink));

  if (GST_VIDEO_SINK_WIDTH (sink) <= 0 || GST_VIDEO_SINK_HEIGHT (sink) <= 0)
    goto no_display_size;

  memset (&sink->crop_rect, 0, sizeof (sink->crop_rect));
  sink->crop_rect.w = sink->info.width;
  sink->crop_rect.h = sink->info.height;

  sink->width = video_width;
  sink->height = video_height;

  GST_DEBUG_OBJECT (bsink, "Selected caps: %" GST_PTR_FORMAT, caps);

  if (!d3d_set_render_format (sink))
    goto incompatible_caps;

  /* Create a window (or start using an application-supplied one, then connect the graph */
  d3d_prepare_window (sink);

  newpool = gst_d3dsurface_buffer_pool_new (sink);
  config = gst_buffer_pool_get_config (newpool);
  /* we need at least 2 buffer because we hold on to the last one */
  gst_buffer_pool_config_set_params (config, caps, sink->info.size, 2, 0);
  if (!gst_buffer_pool_set_config (newpool, config)) {
    gst_object_unref (newpool);
    GST_ERROR_OBJECT (sink, "Failed to set buffer pool configuration");
    return FALSE;
  }

  newfbpool = gst_d3dsurface_buffer_pool_new (sink);
  config = gst_buffer_pool_get_config (newfbpool);
  /* we need at least 2 buffer because we hold on to the last one */
  gst_buffer_pool_config_set_params (config, caps, sink->info.size, 2, 0);
  /* Fallback pool must use videometa */
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (!gst_buffer_pool_set_config (newfbpool, config)) {
    gst_object_unref (newfbpool);
    GST_ERROR_OBJECT (sink, "Failed to set buffer pool configuration");
    return FALSE;
  }

  GST_OBJECT_LOCK (sink);
  oldpool = sink->pool;
  sink->pool = newpool;
  oldfbpool = sink->fallback_pool;
  sink->fallback_pool = newfbpool;
  GST_OBJECT_UNLOCK (sink);

  if (oldpool)
    gst_object_unref (oldpool);
  if (oldfbpool) {
    gst_buffer_pool_set_active (oldfbpool, FALSE);
    gst_object_unref (oldfbpool);
  }

  return TRUE;
  /* ERRORS */
incompatible_caps:
  {
    GST_ERROR_OBJECT (sink, "caps incompatible");
    gst_caps_unref (sink_caps);
    return FALSE;
  }
invalid_format:
  {
    GST_DEBUG_OBJECT (sink,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
no_disp_ratio:
  {
    GST_ELEMENT_ERROR (sink, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }
no_display_size:
  {
    GST_ELEMENT_ERROR (sink, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }
}

static gboolean
gst_d3dvideosink_start (GstBaseSink * bsink)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (bsink);

  GST_DEBUG_OBJECT (bsink, "Start() called");

  return d3d_class_init (sink);
}

static gboolean
gst_d3dvideosink_stop (GstBaseSink * bsink)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (bsink);
  GstD3DVideoSinkClass *klass = GST_D3DVIDEOSINK_GET_CLASS (sink);

  GST_DEBUG_OBJECT (bsink, "Stop() called");
  LOCK_CLASS (sink, klass);
  d3d_stop (sink);
  d3d_class_destroy (sink);
  UNLOCK_CLASS (sink, klass);

  return TRUE;
}

static GstFlowReturn
gst_d3dvideosink_prepare (GstBaseSink * bsink, GstBuffer * buf)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (bsink);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_TRACE ("preparing buffer:%p", buf);

  if (GST_VIDEO_SINK_WIDTH (sink) < 1 || GST_VIDEO_SINK_HEIGHT (sink) < 1) {
    return GST_FLOW_NOT_NEGOTIATED;
  }

  GST_OBJECT_LOCK (sink);
  ret = gst_d3d9_overlay_prepare (sink, buf);
  GST_OBJECT_UNLOCK (sink);
  return ret;
}

static gboolean
gst_d3dvideosink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (bsink);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  gboolean need_pool;
  GstStructure *allocation_meta = NULL;

  gst_query_parse_allocation (query, &caps, &need_pool);
  if (!caps) {
    GST_DEBUG_OBJECT (sink, "no caps specified");
    return FALSE;
  }
#ifdef DISABLE_BUFFER_POOL
  return TRUE;
#endif

  /* FIXME re-using buffer pool breaks renegotiation */
  GST_OBJECT_LOCK (sink);
  pool = sink->pool ? gst_object_ref (sink->pool) : NULL;
  GST_OBJECT_UNLOCK (sink);

  if (pool) {
    GstCaps *pcaps;

    /* we had a pool, check caps */
    GST_DEBUG_OBJECT (sink, "check existing pool caps");
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    if (!gst_caps_is_equal (caps, pcaps)) {
      GST_DEBUG_OBJECT (sink, "pool has different caps");
      /* different caps, we can't use this pool */
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  } else {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps)) {
      GST_ERROR_OBJECT (sink, "allocation query has invalid caps %"
          GST_PTR_FORMAT, caps);
      return FALSE;
    }

    /* the normal size of a frame */
    size = info.size;
  }

  if (pool == NULL && need_pool) {
    GST_DEBUG_OBJECT (sink, "create new pool");
    pool = gst_d3dsurface_buffer_pool_new (sink);

    config = gst_buffer_pool_get_config (pool);
    /* we need at least 2 buffer because we hold on to the last one */
    gst_buffer_pool_config_set_params (config, caps, size, 2, 0);
    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_object_unref (pool);
      GST_ERROR_OBJECT (sink, "failed to set pool configuration");
      return FALSE;
    }
  }

  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  if (sink->width != 0 && sink->height != 0) {
    allocation_meta =
        gst_structure_new ("GstVideoOverlayCompositionMeta",
        "width", G_TYPE_UINT, sink->width,
        "height", G_TYPE_UINT, sink->height, NULL);
    GST_DEBUG ("sending allocation query with size %dx%d",
        sink->width, sink->height);
  }
  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, allocation_meta);
  if (allocation_meta)
    gst_structure_free (allocation_meta);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  return TRUE;
}

/* PUBLIC FUNCTIONS */

/* Interface Registrations */

static void
gst_d3dvideosink_video_overlay_interface_init (GstVideoOverlayInterface * iface)
{
  iface->set_window_handle = gst_d3dvideosink_set_window_handle;
  iface->set_render_rectangle = gst_d3dvideosink_set_render_rectangle;
  iface->expose = gst_d3dvideosink_expose;
}

static void
gst_d3dvideosink_navigation_interface_init (GstNavigationInterface * iface)
{
  iface->send_event_simple = gst_d3dvideosink_navigation_send_event;
}

/* Video Render Code */

static void
gst_d3dvideosink_set_window_handle (GstVideoOverlay * overlay,
    guintptr window_id)
{
  d3d_set_window_handle (GST_D3DVIDEOSINK (overlay), window_id, FALSE);
}

static void
gst_d3dvideosink_set_render_rectangle (GstVideoOverlay * overlay, gint x,
    gint y, gint width, gint height)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (overlay);
  sink->render_rect.x = x;
  sink->render_rect.y = y;
  sink->render_rect.w = width;
  sink->render_rect.h = height;
  d3d_set_render_rectangle (sink);
}

static void
gst_d3dvideosink_expose (GstVideoOverlay * overlay)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (overlay);
  d3d_expose_window (sink);
}

static GstFlowReturn
gst_d3dvideosink_show_frame (GstVideoSink * vsink, GstBuffer * buffer)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (vsink);
  return d3d_render_buffer (sink, buffer);
}

/* Video Navigation Events */

static void
gst_d3dvideosink_navigation_send_event (GstNavigation * navigation,
    GstEvent * event)
{
  GstD3DVideoSink *sink = GST_D3DVIDEOSINK (navigation);

  GstPad *pad;
  if ((pad = gst_pad_get_peer (GST_VIDEO_SINK_PAD (sink)))) {
    if (!gst_pad_send_event (pad, gst_event_ref (event))) {
      /* If upstream didn't handle the event we'll post a message with it
       * for the application in case it wants to do something with it */
      gst_element_post_message (GST_ELEMENT_CAST (sink),
          gst_navigation_message_new_event (GST_OBJECT_CAST (sink), event));
    }
    gst_event_unref (event);
    gst_object_unref (pad);
  }
}

/* PRIVATE FUNCTIONS */


/* Plugin entry point */
static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, ELEMENT_NAME,
          GST_RANK_SECONDARY, GST_TYPE_D3DVIDEOSINK))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    d3d,
    "Direct3D plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
