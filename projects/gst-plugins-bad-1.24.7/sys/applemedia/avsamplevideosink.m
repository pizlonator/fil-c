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
 * SECTION:element-avsamplebufferlayersink
 *
 * avsamplebufferlayersink renders video frames to a CALayer that can placed
 * inside a Core Animation render tree.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "avsamplevideosink.h"

GST_DEBUG_CATEGORY (gst_debug_av_sink);
#define GST_CAT_DEFAULT gst_debug_av_sink

static void gst_av_sample_video_sink_finalize (GObject * object);
static void gst_av_sample_video_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_av_sample_video_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static gboolean gst_av_sample_video_sink_start (GstBaseSink * bsink);
static gboolean gst_av_sample_video_sink_stop (GstBaseSink * bsink);

static void gst_av_sample_video_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_av_sample_video_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static GstCaps * gst_av_sample_video_sink_get_caps (GstBaseSink * bsink, GstCaps * filter);
static GstFlowReturn gst_av_sample_video_sink_prepare (GstBaseSink * bsink,
    GstBuffer * buf);
static GstFlowReturn gst_av_sample_video_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);
static gboolean gst_av_sample_video_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);

static GstStaticPadTemplate gst_av_sample_video_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGB, BGR, ARGB, BGRA, ABGR, RGBA, YUY2, UYVY, NV12, I420 }"))
    );

enum
{
  PROR_0,
  PROP_FORCE_ASPECT_RATIO,
  PROP_LAYER,
};

#define gst_av_sample_video_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAVSampleVideoSink, gst_av_sample_video_sink,
    GST_TYPE_VIDEO_SINK, GST_DEBUG_CATEGORY_INIT (gst_debug_av_sink, "avsamplevideosink", 0,
        "AV Sample Video Sink"));

static void
gst_av_sample_video_sink_class_init (GstAVSampleVideoSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_av_sample_video_sink_set_property;
  gobject_class->get_property = gst_av_sample_video_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LAYER,
      g_param_spec_pointer ("layer", "CALayer",
          "The CoreAnimation layer that can be placed in the render tree",
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class, "AV Sample video sink",
      "Sink/Video", "A videosink based on AVSampleBuffers",
      "Matthew Waters <matthew@centricular.com>");

  gst_element_class_add_static_pad_template (element_class, &gst_av_sample_video_sink_template);

  gobject_class->finalize = gst_av_sample_video_sink_finalize;

  gstbasesink_class->get_caps = gst_av_sample_video_sink_get_caps;
  gstbasesink_class->set_caps = gst_av_sample_video_sink_set_caps;
  gstbasesink_class->get_times = gst_av_sample_video_sink_get_times;
  gstbasesink_class->prepare = gst_av_sample_video_sink_prepare;
  gstbasesink_class->propose_allocation = gst_av_sample_video_sink_propose_allocation;
  gstbasesink_class->stop = gst_av_sample_video_sink_stop;
  gstbasesink_class->start = gst_av_sample_video_sink_start;

  gstvideosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_av_sample_video_sink_show_frame);
}

static void
gst_av_sample_video_sink_init (GstAVSampleVideoSink * av_sink)
{
  av_sink->keep_aspect_ratio = TRUE;

  g_mutex_init (&av_sink->render_lock);
}

static void
gst_av_sample_video_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAVSampleVideoSink *av_sink;

  g_return_if_fail (GST_IS_AV_SAMPLE_VIDEO_SINK (object));

  av_sink = GST_AV_SAMPLE_VIDEO_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
    {
      av_sink->keep_aspect_ratio = g_value_get_boolean (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_av_sample_video_sink_finalize (GObject * object)
{
  GstAVSampleVideoSink *av_sink = GST_AV_SAMPLE_VIDEO_SINK (object);
  __block gpointer layer = av_sink->layer;

  if (layer) {
    dispatch_async (dispatch_get_main_queue (), ^{
      CFBridgingRelease(layer);
    });
  }

  g_mutex_clear (&av_sink->render_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_av_sample_video_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAVSampleVideoSink *av_sink;

  g_return_if_fail (GST_IS_AV_SAMPLE_VIDEO_SINK (object));

  av_sink = GST_AV_SAMPLE_VIDEO_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, av_sink->keep_aspect_ratio);
      break;
    case PROP_LAYER:
      g_value_set_pointer (value, av_sink->layer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_av_sample_video_sink_start (GstBaseSink * bsink)
{
  GstAVSampleVideoSink *av_sink = GST_AV_SAMPLE_VIDEO_SINK (bsink);

  if ([NSThread isMainThread]) {
      AVSampleBufferDisplayLayer *layer = [[AVSampleBufferDisplayLayer alloc] init];
    av_sink->layer = (__bridge_retained gpointer)layer;
    if (av_sink->keep_aspect_ratio)
      layer.videoGravity = AVLayerVideoGravityResizeAspect;
    else
      layer.videoGravity = AVLayerVideoGravityResize;
    g_object_notify (G_OBJECT (av_sink), "layer");
  } else {
    dispatch_sync (dispatch_get_main_queue (), ^{
      AVSampleBufferDisplayLayer *layer = [[AVSampleBufferDisplayLayer alloc] init];
      av_sink->layer = (__bridge_retained gpointer)layer;
      if (av_sink->keep_aspect_ratio)
        layer.videoGravity = AVLayerVideoGravityResizeAspect;
      else
        layer.videoGravity = AVLayerVideoGravityResize;
      g_object_notify (G_OBJECT (av_sink), "layer");
    });
  }

  return TRUE;
}

/* with render lock */
static void
_stop_requesting_data (GstAVSampleVideoSink * av_sink)
{
  if (av_sink->layer) {
    if (av_sink->layer_requesting_data)
      [GST_AV_SAMPLE_VIDEO_SINK_LAYER(av_sink) stopRequestingMediaData];
    av_sink->layer_requesting_data = FALSE;
  }
}

static gboolean
gst_av_sample_video_sink_stop (GstBaseSink * bsink)
{
  GstAVSampleVideoSink *av_sink = GST_AV_SAMPLE_VIDEO_SINK (bsink);

  if (av_sink->pool) {
    gst_object_unref (av_sink->pool);
    av_sink->pool = NULL;
  }

  if (av_sink->layer) {
    g_mutex_lock (&av_sink->render_lock);
    _stop_requesting_data (av_sink);
    g_mutex_unlock (&av_sink->render_lock);
    [GST_AV_SAMPLE_VIDEO_SINK_LAYER(av_sink) flushAndRemoveImage];
  }

  return TRUE;
}

static void
gst_av_sample_video_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstAVSampleVideoSink *av_sink;

  av_sink = GST_AV_SAMPLE_VIDEO_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (GST_VIDEO_INFO_FPS_N (&av_sink->info) > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&av_sink->info),
            GST_VIDEO_INFO_FPS_N (&av_sink->info));
      }
    }
  }
}

static unsigned int
_cv_pixel_format_type_from_video_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_BGRA:
      return kCVPixelFormatType_32BGRA;
    case GST_VIDEO_FORMAT_ARGB:
      return kCVPixelFormatType_32ARGB;
    case GST_VIDEO_FORMAT_ABGR:
      return kCVPixelFormatType_32ABGR;
    case GST_VIDEO_FORMAT_RGBA:
      return kCVPixelFormatType_32RGBA;
    case GST_VIDEO_FORMAT_RGB:
      return kCVPixelFormatType_24RGB;
    case GST_VIDEO_FORMAT_BGR:
      return kCVPixelFormatType_24BGR;
    case GST_VIDEO_FORMAT_NV12:
      return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    case GST_VIDEO_FORMAT_I420:
      return kCVPixelFormatType_420YpCbCr8Planar;
    case GST_VIDEO_FORMAT_YUY2:
      return kCVPixelFormatType_422YpCbCr8_yuvs;
    case GST_VIDEO_FORMAT_UYVY:
      return kCVPixelFormatType_422YpCbCr8;
    default:
      return 0;
  }
}

static GstVideoFormat
_pixel_format_description_to_video_format (CFDictionaryRef attrs)
{
  CFNumberRef id_ref;
  unsigned int id;

  id_ref = (CFNumberRef) CFDictionaryGetValue (attrs, kCVPixelFormatConstant);
  CFNumberGetValue (id_ref, kCFNumberIntType, &id);

  GST_TRACE ("pixel format description id %u", id);

  CFRelease (id_ref);

  switch (id) {
    case kCVPixelFormatType_32BGRA:
      return GST_VIDEO_FORMAT_BGRA;
    case kCVPixelFormatType_32ARGB:
      return GST_VIDEO_FORMAT_ARGB;
    case kCVPixelFormatType_32ABGR:
      return GST_VIDEO_FORMAT_ABGR;
    case kCVPixelFormatType_32RGBA:
      return GST_VIDEO_FORMAT_RGBA;
    case kCVPixelFormatType_24RGB:
      return GST_VIDEO_FORMAT_RGB;
    case kCVPixelFormatType_24BGR:
      return GST_VIDEO_FORMAT_BGR;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      return GST_VIDEO_FORMAT_NV12;
    case kCVPixelFormatType_420YpCbCr8Planar:
      return GST_VIDEO_FORMAT_I420;
    case kCVPixelFormatType_422YpCbCr8_yuvs:
      return GST_VIDEO_FORMAT_YUY2;
    case kCVPixelFormatType_422YpCbCr8:
      return GST_VIDEO_FORMAT_UYVY;
    default:
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
}

static GstCaps *
gst_av_sample_video_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstAVSampleVideoSink *av_sink = GST_AV_SAMPLE_VIDEO_SINK (bsink);
  CFArrayRef formats;
  GstCaps *ret, *tmp;
  int i, n;

  formats =
      CVPixelFormatDescriptionArrayCreateWithAllPixelFormatTypes
      (kCFAllocatorDefault);

  ret = gst_caps_new_empty ();

  n = CFArrayGetCount (formats);
  for (i = 0; i < n; i++) {
    CFDictionaryRef attrs;
    CFNumberRef fourcc;
    unsigned int pixel_format;
    GstVideoFormat v_format;
    const char *format_str;
    char *caps_str;

    fourcc = (CFNumberRef)CFArrayGetValueAtIndex(formats, i);
    CFNumberGetValue (fourcc, kCFNumberIntType, &pixel_format);
    attrs = CVPixelFormatDescriptionCreateWithPixelFormatType (kCFAllocatorDefault,
        pixel_format);

    CFRelease (fourcc);

    v_format = _pixel_format_description_to_video_format (attrs);
    if (v_format != GST_VIDEO_FORMAT_UNKNOWN) {
      format_str = gst_video_format_to_string (v_format);

      caps_str = g_strdup_printf ("video/x-raw, format=%s", format_str);

      ret = gst_caps_merge (ret, gst_caps_from_string (caps_str));

      g_free (caps_str);
    }

    CFRelease (attrs);
  }

  ret = gst_caps_simplify (ret);

  gst_caps_set_simple (ret, "width", GST_TYPE_INT_RANGE, 0, G_MAXINT, "height",
      GST_TYPE_INT_RANGE, 0, G_MAXINT, "framerate", GST_TYPE_FRACTION_RANGE, 0,
      1, G_MAXINT, 1, NULL);
  GST_DEBUG_OBJECT (av_sink, "returning caps %" GST_PTR_FORMAT, ret);

  if (filter) {
    tmp = gst_caps_intersect_full (ret, filter, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = tmp;
  }

  CFRelease (formats);

  return ret;
}

static gboolean
gst_av_sample_video_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstAVSampleVideoSink *av_sink;
  gint width;
  gint height;
  gboolean ok;
  gint par_n, par_d;
  gint display_par_n, display_par_d;
  guint display_ratio_num, display_ratio_den;
  GstVideoInfo vinfo;
  GstStructure *structure;
  GstBufferPool *newpool, *oldpool;

  GST_DEBUG_OBJECT (bsink, "set caps with %" GST_PTR_FORMAT, caps);

  av_sink = GST_AV_SAMPLE_VIDEO_SINK (bsink);

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

  GST_TRACE_OBJECT (bsink, "PAR: %u/%u DAR:%u/%u", par_n, par_d, display_par_n,
      display_par_d);

  if (height % display_ratio_den == 0) {
    GST_DEBUG_OBJECT (bsink, "keeping video height");
    GST_VIDEO_SINK_WIDTH (av_sink) = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    GST_VIDEO_SINK_HEIGHT (av_sink) = height;
  } else if (width % display_ratio_num == 0) {
    GST_DEBUG_OBJECT (bsink, "keeping video width");
    GST_VIDEO_SINK_WIDTH (av_sink) = width;
    GST_VIDEO_SINK_HEIGHT (av_sink) = (guint)
        gst_util_uint64_scale_int (width, display_ratio_den, display_ratio_num);
  } else {
    GST_DEBUG_OBJECT (bsink, "approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (av_sink) = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    GST_VIDEO_SINK_HEIGHT (av_sink) = height;
  }
  GST_DEBUG_OBJECT (bsink, "scaling to %dx%d", GST_VIDEO_SINK_WIDTH (av_sink),
      GST_VIDEO_SINK_HEIGHT (av_sink));

  av_sink->info = vinfo;

  newpool = gst_video_buffer_pool_new ();
  structure = gst_buffer_pool_get_config (newpool);
  gst_buffer_pool_config_set_params (structure, caps, vinfo.size, 2, 0);
  gst_buffer_pool_set_config (newpool, structure);

  oldpool = av_sink->pool;
  /* we don't activate the pool yet, this will be done by downstream after it
   * has configured the pool. If downstream does not want our pool we will
   * activate it when we render into it */
  av_sink->pool = newpool;

  /* unref the old sink */
  if (oldpool) {
    /* we don't deactivate, some elements might still be using it, it will
     * be deactivated when the last ref is gone */
    gst_object_unref (oldpool);
  }

  return TRUE;
}

static void
_unmap_planar_frame (GstVideoFrame * v_frame, const void * data, gsize dataSize,
    gsize numberOfPlanes, const void *planeAddressed[])
{
  GST_TRACE ("freeing video frame %p", v_frame);

  gst_video_frame_unmap (v_frame);
  g_free (v_frame);
}

static void
_unmap_frame (GstVideoFrame * v_frame, const void * data)
{
  GST_TRACE ("freeing video frame %p", v_frame);

  gst_video_frame_unmap (v_frame);
  g_free (v_frame);
}

/* with render lock */
static gboolean
_enqueue_sample (GstAVSampleVideoSink * av_sink, GstBuffer *buf)
{
  CVPixelBufferRef pbuf;
  CMVideoFormatDescriptionRef v_format_desc;
  GstVideoFrame *v_frame;
  CMSampleTimingInfo sample_time;
  __block CMSampleBufferRef sample_buf;
  CFArrayRef sample_attachments;
  gsize l, r, t, b;
  gint i;

  GST_TRACE_OBJECT (av_sink, "redisplay of size:%ux%u, window size:%ux%u",
      GST_VIDEO_INFO_WIDTH (&av_sink->info),
      GST_VIDEO_INFO_HEIGHT (&av_sink->info),
      GST_VIDEO_SINK_WIDTH (av_sink),
      GST_VIDEO_SINK_HEIGHT (av_sink));

  v_frame = g_new0 (GstVideoFrame, 1);

  if (!gst_video_frame_map (v_frame, &av_sink->info, buf, GST_MAP_READ)) {
    GST_ERROR_OBJECT (av_sink, "Failed to map input video frame");
    g_free (v_frame);
    return FALSE;
  }

  if (GST_VIDEO_INFO_N_PLANES (&v_frame->info) == 1) {
    /* single plane */
    if (kCVReturnSuccess != CVPixelBufferCreateWithBytes (NULL,
        GST_VIDEO_INFO_WIDTH (&v_frame->info),
        GST_VIDEO_INFO_HEIGHT (&v_frame->info),
        _cv_pixel_format_type_from_video_format (GST_VIDEO_INFO_FORMAT (&v_frame->info)),
        v_frame->data[0], v_frame->info.stride[0],
        (CVPixelBufferReleaseBytesCallback) _unmap_frame, v_frame, NULL,
        &pbuf)) {
      GST_ERROR_OBJECT (av_sink, "Error creating Core Video pixel buffer");
      gst_video_frame_unmap (v_frame);
      g_free (v_frame);
      return FALSE;
    }
  } else {
    /* multi-planar */
    gsize widths[GST_VIDEO_MAX_PLANES] = { 0, };
    gsize heights[GST_VIDEO_MAX_PLANES] = { 0, };
    gsize strides[GST_VIDEO_MAX_PLANES] = { 0, };
    gint i;

    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&v_frame->info); i++) {
      widths[i] = GST_VIDEO_INFO_COMP_WIDTH (&v_frame->info, i);
      heights[i] = GST_VIDEO_INFO_COMP_HEIGHT (&v_frame->info, i);
      strides[i] = GST_VIDEO_INFO_COMP_STRIDE (&v_frame->info, i);
    }

    if (kCVReturnSuccess != CVPixelBufferCreateWithPlanarBytes (NULL,
        GST_VIDEO_INFO_WIDTH (&v_frame->info),
        GST_VIDEO_INFO_HEIGHT (&v_frame->info),
        _cv_pixel_format_type_from_video_format (GST_VIDEO_INFO_FORMAT (&v_frame->info)),
         /* have to put something for these two parameters otherwise
          * the callback is not called resulting in a big leak */
        v_frame, v_frame->info.size,
        GST_VIDEO_INFO_N_PLANES (&v_frame->info), v_frame->data,
        widths, heights, strides,
        (CVPixelBufferReleasePlanarBytesCallback) _unmap_planar_frame,
        v_frame, NULL, &pbuf)) {
      GST_ERROR_OBJECT (av_sink, "Error creating Core Video pixel buffer");
      gst_video_frame_unmap (v_frame);
      g_free (v_frame);
      return FALSE;
    }
  }

  CVPixelBufferLockBaseAddress (pbuf, kCVPixelBufferLock_ReadOnly);

  CVPixelBufferGetExtendedPixels (pbuf, &l, &r, &t, &b);

  GST_TRACE_OBJECT (av_sink, "CVPixelBuffer n_planes %u width %u height %u"
      " data size %" G_GSIZE_FORMAT " extra pixels l %u r %u t %u b %u",
      (guint) CVPixelBufferGetPlaneCount (pbuf),
      (guint) CVPixelBufferGetWidth (pbuf),
      (guint) CVPixelBufferGetHeight (pbuf),
      CVPixelBufferGetDataSize (pbuf),
      (guint) l, (guint) r, (guint) t, (guint) b);

  GST_TRACE_OBJECT (av_sink, "GstVideoFrame n_planes %u width %u height %u"
      " data size %"G_GSIZE_FORMAT " extra pixels l %u r %u t %u b %u",
      GST_VIDEO_INFO_N_PLANES (&v_frame->info),
      GST_VIDEO_INFO_WIDTH (&v_frame->info),
      GST_VIDEO_INFO_HEIGHT (&v_frame->info),
      v_frame->info.size, 0, 0, 0, 0);

  if (GST_VIDEO_INFO_N_PLANES (&v_frame->info) > 1) {
    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&v_frame->info); i++) {
      GST_TRACE_OBJECT (av_sink, "plane %i CVPixelBuffer width %u height %u "
          "stride %u data %p", i,
          (guint) CVPixelBufferGetWidthOfPlane (pbuf, i),
          (guint) CVPixelBufferGetHeightOfPlane (pbuf, i),
          (guint) CVPixelBufferGetBytesPerRowOfPlane (pbuf, i),
          CVPixelBufferGetBaseAddressOfPlane (pbuf, i));
      GST_TRACE_OBJECT (av_sink, "plane %i GstVideoFrame width %u height %u "
          "stride %u data %p", i,
          GST_VIDEO_INFO_COMP_WIDTH (&v_frame->info, i),
          GST_VIDEO_INFO_COMP_HEIGHT (&v_frame->info, i),
          GST_VIDEO_INFO_COMP_STRIDE (&v_frame->info, i),
          CVPixelBufferGetBaseAddressOfPlane (pbuf, i));
    }
  } else {
    GST_TRACE_OBJECT (av_sink, "CVPixelBuffer attrs stride %u data %p",
      (guint) CVPixelBufferGetBytesPerRow (pbuf),
      CVPixelBufferGetBaseAddress (pbuf));
    GST_TRACE_OBJECT (av_sink, "GstVideoFrame attrs stride %u data %p",
        v_frame->info.stride[0], v_frame->data[0]);
  }

  CVPixelBufferUnlockBaseAddress (pbuf, kCVPixelBufferLock_ReadOnly);

  if (0 != CMVideoFormatDescriptionCreateForImageBuffer (kCFAllocatorDefault,
        pbuf, &v_format_desc)) {
    GST_ERROR_OBJECT (av_sink, "Failed to retrieve video format from "
        "pixel buffer");
    CFRelease (pbuf);
    return FALSE;
  }

  sample_time.duration = CMTimeMake (GST_BUFFER_DURATION (buf), GST_SECOND);
  sample_time.presentationTimeStamp = CMTimeMake (GST_BUFFER_PTS (buf), GST_SECOND);
  sample_time.decodeTimeStamp = kCMTimeInvalid;

  if (0 != CMSampleBufferCreateForImageBuffer (kCFAllocatorDefault, pbuf, TRUE,
        NULL, NULL, v_format_desc, &sample_time, &sample_buf)) {
    GST_ERROR_OBJECT (av_sink, "Failed to create CMSampleBuffer from "
        "CVImageBuffer");
    CFRelease (v_format_desc);
    CFRelease (pbuf);
    return FALSE;
  }
  CFRelease (v_format_desc);

  sample_attachments = CMSampleBufferGetSampleAttachmentsArray (sample_buf, TRUE);
  for (i = 0; i < CFArrayGetCount (sample_attachments); i++) {
    CFMutableDictionaryRef attachments =
       (CFMutableDictionaryRef) CFArrayGetValueAtIndex (sample_attachments, i);
    /* Until we slave the CoreMedia clock, just display everything ASAP */
    CFDictionarySetValue (attachments, kCMSampleAttachmentKey_DisplayImmediately,
        kCFBooleanTrue);
  }

  AVSampleBufferDisplayLayer *layer = GST_AV_SAMPLE_VIDEO_SINK_LAYER(av_sink);
  if (av_sink->keep_aspect_ratio)
    layer.videoGravity = AVLayerVideoGravityResizeAspect;
  else
    layer.videoGravity = AVLayerVideoGravityResize;
  [layer enqueueSampleBuffer:sample_buf];

  CFRelease (pbuf);
  CFRelease (sample_buf);

  return TRUE;
}

static void
_request_data (GstAVSampleVideoSink * av_sink)
{
  av_sink->layer_requesting_data = TRUE;

  AVSampleBufferDisplayLayer *layer = GST_AV_SAMPLE_VIDEO_SINK_LAYER(av_sink);
  [layer requestMediaDataWhenReadyOnQueue:
        dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)
        usingBlock:^{
    while (TRUE) {
      /* don't needlessly fill up avsamplebufferdisplaylayer's queue.
       * This also allows us to skip displaying late frames */
      if (!layer.readyForMoreMediaData)
        break;

      g_mutex_lock (&av_sink->render_lock);

      if (!av_sink->buffer || av_sink->render_flow_return != GST_FLOW_OK) {
        _stop_requesting_data (av_sink);
        g_mutex_unlock (&av_sink->render_lock);
        break;
      }

      if (!_enqueue_sample (av_sink, av_sink->buffer)) {
        gst_buffer_unref (av_sink->buffer);
        av_sink->buffer = NULL;
        av_sink->render_flow_return = GST_FLOW_ERROR;
        g_mutex_unlock (&av_sink->render_lock);
        break;
      }

      gst_buffer_unref (av_sink->buffer);
      av_sink->buffer = NULL;
      av_sink->render_flow_return = GST_FLOW_OK;
      g_mutex_unlock (&av_sink->render_lock);
    }
  }];
}

static GstFlowReturn
gst_av_sample_video_sink_prepare (GstBaseSink * bsink, GstBuffer * buf)
{
  GstAVSampleVideoSink *av_sink;

  av_sink = GST_AV_SAMPLE_VIDEO_SINK (bsink);

  GST_LOG_OBJECT (bsink, "preparing buffer:%p", buf);

  if (GST_VIDEO_SINK_WIDTH (av_sink) < 1 ||
      GST_VIDEO_SINK_HEIGHT (av_sink) < 1) {
    return GST_FLOW_NOT_NEGOTIATED;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_av_sample_video_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstAVSampleVideoSink *av_sink;
  GstFlowReturn ret;

  GST_TRACE_OBJECT (vsink, "rendering buffer:%p", buf);

  av_sink = GST_AV_SAMPLE_VIDEO_SINK (vsink);

  g_mutex_lock (&av_sink->render_lock);
  if (av_sink->buffer)
    gst_buffer_unref (av_sink->buffer);
  av_sink->buffer = gst_buffer_ref (buf);
  ret = av_sink->render_flow_return;

  if (!av_sink->layer_requesting_data)
    _request_data (av_sink);
  g_mutex_unlock (&av_sink->render_lock);

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= 1010 && \
    defined(MAC_OS_X_VERSION_MIN_REQUIRED) && \
    MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
    AVSampleBufferDisplayLayer *layer = GST_AV_SAMPLE_VIDEO_SINK_LAYER(av_sink);
  if ([layer status] == AVQueuedSampleBufferRenderingStatusFailed) {
    GST_ERROR_OBJECT (av_sink, "failed to enqueue buffer on layer, %s",
        [[[layer error] description] UTF8String]);
    return GST_FLOW_ERROR;
  }
#endif

  return ret;
}

static gboolean
gst_av_sample_video_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstAVSampleVideoSink *av_sink = GST_AV_SAMPLE_VIDEO_SINK (bsink);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  /* FIXME re-using buffer pool breaks renegotiation */
  if ((pool = av_sink->pool))
    gst_object_ref (pool);

  if (pool != NULL) {
    GstCaps *pcaps;

    /* we had a pool, check caps */
    GST_DEBUG_OBJECT (av_sink, "check existing pool caps");
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    if (!gst_caps_is_equal (caps, pcaps)) {
      GST_DEBUG_OBJECT (av_sink, "pool has different caps");
      /* different caps, we can't use this pool */
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  } else {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    /* the normal size of a frame */
    size = info.size;
  }

  if (pool == NULL && need_pool) {
    GST_DEBUG_OBJECT (av_sink, "create new pool");
    pool = gst_video_buffer_pool_new ();

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }
  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  /* we also support various metadata */
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, 0);

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
