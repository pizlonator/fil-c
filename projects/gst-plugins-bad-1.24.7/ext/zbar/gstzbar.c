/* GStreamer
 * Copyright (C) 2009 Stefan Kost <ensonic@users.sf.net>
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
 * SECTION:element-zbar
 * @title: zbar
 *
 * Detect bar codes in the video streams and send them as element messages to
 * the #GstBus if .#GstZBar:message property is %TRUE.
 * If the .#GstZBar:attach-frame property is %TRUE, the posted barcode message
 * includes a sample of the frame where the barcode was detected (Since 1.6).
 *
 * The element generate messages named`barcode`. The structure contains these fields:
 *
 * * #GstClockTime `timestamp`: the timestamp of the buffer that triggered the message.
 * * gchar * `type`: the symbol type.
 * * gchar * `symbol`: the detected bar code data.
 * * gint `quality`: an unscaled, relative quantity: larger values are better than smaller
 *   values.
 * * GstSample `frame`: the frame in which the barcode message was detected, if
 *   the .#GstZBar:attach-frame property was set to %TRUE (Since 1.6)
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -m v4l2src ! videoconvert ! zbar ! videoconvert ! xvimagesink
 * ]| This pipeline will detect barcodes and send them as messages.
 * |[
 * gst-launch-1.0 -m v4l2src ! tee name=t ! queue ! videoconvert ! zbar ! fakesink t. ! queue ! xvimagesink
 * ]| Same as above, but running the filter on a branch to keep the display in color
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstzbar.h"

#include <string.h>
#include <math.h>

#include <gst/video/video.h>


GST_DEBUG_CATEGORY_STATIC (zbar_debug);
#define GST_CAT_DEFAULT zbar_debug

/* GstZBar signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MESSAGE,
  PROP_ATTACH_FRAME,
  PROP_CACHE
};

#define DEFAULT_CACHE    FALSE
#define DEFAULT_MESSAGE  TRUE
#define DEFAULT_ATTACH_FRAME FALSE

#define ZBAR_YUV_CAPS \
    "{ Y800, I420, YV12, NV12, NV21, Y41B, Y42B, YUV9, YVU9 }"

static GstStaticPadTemplate gst_zbar_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (ZBAR_YUV_CAPS))
    );

static GstStaticPadTemplate gst_zbar_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (ZBAR_YUV_CAPS))
    );

static void gst_zbar_finalize (GObject * object);
static void gst_zbar_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_zbar_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_zbar_start (GstBaseTransform * base);
static gboolean gst_zbar_stop (GstBaseTransform * base);

static GstFlowReturn gst_zbar_transform_frame_ip (GstVideoFilter * vfilter,
    GstVideoFrame * frame);

#define gst_zbar_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstZBar, gst_zbar, GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (zbar_debug, "zbar", 0, "zbar"););
GST_ELEMENT_REGISTER_DEFINE (zbar, "zbar", GST_RANK_NONE, GST_TYPE_ZBAR);

static void
gst_zbar_class_init (GstZBarClass * g_class)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *trans_class;
  GstVideoFilterClass *vfilter_class;

  gobject_class = G_OBJECT_CLASS (g_class);
  gstelement_class = GST_ELEMENT_CLASS (g_class);
  trans_class = GST_BASE_TRANSFORM_CLASS (g_class);
  vfilter_class = GST_VIDEO_FILTER_CLASS (g_class);

  gobject_class->set_property = gst_zbar_set_property;
  gobject_class->get_property = gst_zbar_get_property;
  gobject_class->finalize = gst_zbar_finalize;

  g_object_class_install_property (gobject_class, PROP_MESSAGE,
      g_param_spec_boolean ("message", "message",
          "Post a barcode message for each detected code",
          DEFAULT_MESSAGE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstZBar::attach-frame:
   *
   * Attach the frame in which the barcode was detected to the posted
   * barcode message.
   *
   * Since: 1.6
   */
  g_object_class_install_property (gobject_class, PROP_ATTACH_FRAME,
      g_param_spec_boolean ("attach-frame", "Attach frame",
          "Attach a frame dump to each barcode message",
          DEFAULT_ATTACH_FRAME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CACHE,
      g_param_spec_boolean ("cache", "cache",
          "Enable or disable the inter-image result cache",
          DEFAULT_CACHE,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "Barcode detector",
      "Filter/Analyzer/Video",
      "Detect bar codes in the video streams",
      "Stefan Kost <ensonic@users.sf.net>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_zbar_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_zbar_src_template);

  trans_class->start = GST_DEBUG_FUNCPTR (gst_zbar_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_zbar_stop);

  vfilter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_zbar_transform_frame_ip);
}

static void
gst_zbar_init (GstZBar * zbar)
{
  zbar->cache = DEFAULT_CACHE;
  zbar->message = DEFAULT_MESSAGE;
  zbar->attach_frame = DEFAULT_ATTACH_FRAME;

  zbar->scanner = zbar_image_scanner_create ();
}

static void
gst_zbar_finalize (GObject * object)
{
  GstZBar *zbar = GST_ZBAR (object);

  zbar_image_scanner_destroy (zbar->scanner);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_zbar_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstZBar *zbar;

  g_return_if_fail (GST_IS_ZBAR (object));
  zbar = GST_ZBAR (object);

  switch (prop_id) {
    case PROP_CACHE:
      zbar->cache = g_value_get_boolean (value);
      break;
    case PROP_MESSAGE:
      zbar->message = g_value_get_boolean (value);
      break;
    case PROP_ATTACH_FRAME:
      zbar->attach_frame = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_zbar_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstZBar *zbar;

  g_return_if_fail (GST_IS_ZBAR (object));
  zbar = GST_ZBAR (object);

  switch (prop_id) {
    case PROP_CACHE:
      g_value_set_boolean (value, zbar->cache);
      break;
    case PROP_MESSAGE:
      g_value_set_boolean (value, zbar->message);
      break;
    case PROP_ATTACH_FRAME:
      g_value_set_boolean (value, zbar->attach_frame);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_zbar_transform_frame_ip (GstVideoFilter * vfilter, GstVideoFrame * frame)
{
  GstZBar *zbar = GST_ZBAR (vfilter);
  gpointer data;
  gint stride, height;
  zbar_image_t *image;
  const zbar_symbol_t *symbol;
  int n;

  image = zbar_image_create ();

  /* all formats we support start with an 8-bit Y plane. zbar doesn't need
   * to know about the chroma plane(s) */
  data = GST_VIDEO_FRAME_COMP_DATA (frame, 0);
  stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0);
  height = GST_VIDEO_FRAME_HEIGHT (frame);

  zbar_image_set_format (image, GST_MAKE_FOURCC ('Y', '8', '0', '0'));
  zbar_image_set_size (image, stride, height);
  zbar_image_set_data (image, (gpointer) data, stride * height, NULL);

  /* scan the image for barcodes */
  n = zbar_scan_image (zbar->scanner, image);
  if (G_UNLIKELY (n == -1)) {
    GST_WARNING_OBJECT (zbar, "Error trying to scan frame. Skipping");
    goto out;
  }
  if (n == 0)
    goto out;

  /* extract results */
  symbol = zbar_image_first_symbol (image);
  for (; symbol; symbol = zbar_symbol_next (symbol)) {
    zbar_symbol_type_t typ = zbar_symbol_get_type (symbol);
    const char *data = zbar_symbol_get_data (symbol);
    gint quality = zbar_symbol_get_quality (symbol);

    GST_DEBUG_OBJECT (zbar, "decoded %s symbol \"%s\" at quality %d",
        zbar_get_symbol_name (typ), data, quality);

    if (zbar->cache && zbar_symbol_get_count (symbol) != 0)
      continue;

    if (zbar->message) {
      GstMessage *m;
      GstStructure *s;
      GstSample *sample;
      GstCaps *sample_caps;
      GstClockTime timestamp, running_time, stream_time, duration;

      timestamp = GST_BUFFER_TIMESTAMP (frame->buffer);
      duration = GST_BUFFER_DURATION (frame->buffer);
      running_time =
          gst_segment_to_running_time (&GST_BASE_TRANSFORM (zbar)->segment,
          GST_FORMAT_TIME, timestamp);
      stream_time =
          gst_segment_to_stream_time (&GST_BASE_TRANSFORM (zbar)->segment,
          GST_FORMAT_TIME, timestamp);

      s = gst_structure_new ("barcode",
          "timestamp", G_TYPE_UINT64, timestamp,
          "stream-time", G_TYPE_UINT64, stream_time,
          "running-time", G_TYPE_UINT64, running_time,
          "type", G_TYPE_STRING, zbar_get_symbol_name (typ),
          "symbol", G_TYPE_STRING, data, "quality", G_TYPE_INT, quality, NULL);

      if (GST_CLOCK_TIME_IS_VALID (duration))
        gst_structure_set (s, "duration", G_TYPE_UINT64, duration, NULL);

      if (zbar->attach_frame) {
        /* create a sample from image */
        sample_caps = gst_video_info_to_caps (&frame->info);
        sample = gst_sample_new (frame->buffer, sample_caps, NULL, NULL);
        gst_caps_unref (sample_caps);
        gst_structure_set (s, "frame", GST_TYPE_SAMPLE, sample, NULL);
        gst_sample_unref (sample);
      }

      m = gst_message_new_element (GST_OBJECT (zbar), s);
      gst_element_post_message (GST_ELEMENT (zbar), m);

    } else if (zbar->attach_frame)
      GST_WARNING_OBJECT (zbar,
          "attach-frame=true has no effect if message=false");
  }

out:
  /* clean up */
  zbar_image_scanner_recycle_image (zbar->scanner, image);
  zbar_image_destroy (image);

  return GST_FLOW_OK;
}

static gboolean
gst_zbar_start (GstBaseTransform * base)
{
  GstZBar *zbar = GST_ZBAR (base);

  /* start the cache if enabled (e.g. for filtering dupes) */
  zbar_image_scanner_enable_cache (zbar->scanner, zbar->cache);

  return TRUE;
}

static gboolean
gst_zbar_stop (GstBaseTransform * base)
{
  GstZBar *zbar = GST_ZBAR (base);

  /* stop the cache if enabled (e.g. for filtering dupes) */
  zbar_image_scanner_enable_cache (zbar->scanner, zbar->cache);

  return TRUE;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (zbar, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    zbar,
    "zbar barcode scanner",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
