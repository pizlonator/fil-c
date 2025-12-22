/* GStreamer
 * Copyright (C) 2021 Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gio/gio.h>

#include "gstdebugutilsbadelements.h"
#include "gstvideocodectestsink.h"

/**
 * SECTION:videocodectestsink
 *
 * An element that computes the checksum of a video stream and/or writes back its
 * raw I420 data ignoring the padding introduced by GStreamer. This element is
 * meant to be used for CODEC conformance testing. It also supports producing an I420
 * checksum and and can write out a file in I420 layout directly from NV12 input
 * data.
 *
 * The checksum is communicated back to the application just before EOS
 * message with an element message of type `conformance/checksum` with the
 * following fields:
 *
 * * "checksum-type"  G_TYPE_STRING The checksum type (only MD5 is supported)
 * * "checksum"       G_TYPE_STRING The checksum as a string
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 videotestsrc num-buffers=2 ! videocodectestsink location=true-raw.yuv -m
 * ]|
 *
 * Since: 1.20
 */

enum
{
  PROP_0,
  PROP_LOCATION,
};

struct _GstVideoCodecTestSink
{
  GstBaseSink parent;
  GChecksumType hash;

  /* protect with stream lock */
  GstVideoInfo vinfo;
    GstFlowReturn (*process) (GstVideoCodecTestSink * self,
      GstVideoFrame * frame);
  GOutputStream *ostream;
  GChecksum *checksum;

  /* protect with object lock */
  gchar *location;
};

static GstStaticPadTemplate gst_video_codec_test_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format = { "
        "Y444_12LE, I422_12LE, I420_12LE,"
        "Y444_10LE, I422_10LE, I420_10LE, Y444, Y42B, I420, NV12 }"));

#define gst_video_codec_test_sink_parent_class parent_class
G_DEFINE_TYPE (GstVideoCodecTestSink, gst_video_codec_test_sink,
    GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE (videocodectestsink, "videocodectestsink",
    GST_RANK_NONE, gst_video_codec_test_sink_get_type ());

static void
gst_video_codec_test_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_LOCATION:
      g_free (self->location);
      self->location = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_video_codec_test_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_video_codec_test_sink_start (GstBaseSink * sink)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (sink);
  GError *error = NULL;
  GFile *file = NULL;
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (self);

  self->checksum = g_checksum_new (self->hash);
  if (self->location)
    file = g_file_new_for_path (self->location);

  GST_OBJECT_UNLOCK (self);

  if (file) {
    self->ostream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE,
            G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error));
    if (!self->ostream) {
      GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
          ("Failed to open '%s' for writing.", self->location),
          ("Open failed failed: %s", error->message));
      g_error_free (error);
      ret = FALSE;
    }

    g_object_unref (file);
  }

  return ret;
}

static gboolean
gst_video_codec_test_sink_stop (GstBaseSink * sink)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (sink);

  g_checksum_free (self->checksum);
  self->checksum = NULL;

  if (self->ostream) {
    GError *error = NULL;

    if (!g_output_stream_close (self->ostream, NULL, &error)) {
      GST_ELEMENT_WARNING (self, RESOURCE, CLOSE,
          ("Did not close '%s' properly", self->location),
          ("Failed to close stream: %s", error->message));
    }

    g_clear_object (&self->ostream);
  }

  return TRUE;
}

static GstFlowReturn
gst_video_codec_test_sink_process_data (GstVideoCodecTestSink * self,
    const guchar * data, gssize length)
{
  GError *error = NULL;

  g_checksum_update (self->checksum, data, length);

  if (!self->ostream)
    return GST_FLOW_OK;

  if (!g_output_stream_write_all (self->ostream, data, length, NULL, NULL,
          &error)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Failed to write video data into '%s'", self->location),
        ("Writing %" G_GSIZE_FORMAT " bytes failed: %s", length,
            error->message));
    g_error_free (error);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_video_codec_test_sink_process_i42x (GstVideoCodecTestSink * self,
    GstVideoFrame * frame)
{
  guint plane;

  for (plane = 0; plane < 3; plane++) {
    gint y;
    guint stride;
    const guchar *data;

    stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, plane);
    data = GST_VIDEO_FRAME_PLANE_DATA (frame, plane);

    for (y = 0; y < GST_VIDEO_INFO_COMP_HEIGHT (&self->vinfo, plane); y++) {
      gsize length = GST_VIDEO_INFO_COMP_WIDTH (&self->vinfo, plane) *
          GST_VIDEO_INFO_COMP_PSTRIDE (&self->vinfo, plane);
      GstFlowReturn ret;

      ret = gst_video_codec_test_sink_process_data (self, data, length);
      if (ret != GST_FLOW_OK)
        return ret;

      data += stride;
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_video_codec_test_sink_process_nv12 (GstVideoCodecTestSink * self,
    GstVideoFrame * frame)
{
  gint x, y, comp;
  guint stride;
  const guchar *data;

  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);
  data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);

  for (y = 0; y < GST_VIDEO_INFO_HEIGHT (&self->vinfo); y++) {
    gsize length = GST_VIDEO_INFO_WIDTH (&self->vinfo);
    GstFlowReturn ret;

    ret = gst_video_codec_test_sink_process_data (self, data, length);
    if (ret != GST_FLOW_OK)
      return ret;

    data += stride;
  }

  /* Deinterleave the UV plane */
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 1);

  for (comp = 0; comp < 2; comp++) {
    data = GST_VIDEO_FRAME_PLANE_DATA (frame, 1);

    for (y = 0; y < GST_VIDEO_INFO_COMP_HEIGHT (&self->vinfo, 1); y++) {
      guint width = GST_ROUND_UP_2 (GST_VIDEO_INFO_WIDTH (&self->vinfo)) / 2;

      for (x = 0; x < width; x++) {
        GstFlowReturn ret;

        ret = gst_video_codec_test_sink_process_data (self,
            data + 2 * x + comp, 1);

        if (ret != GST_FLOW_OK)
          return ret;
      }

      data += stride;
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_video_codec_test_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (sink);
  GstVideoFrame frame;

  if (!gst_video_frame_map (&frame, &self->vinfo, buffer, GST_MAP_READ))
    return GST_FLOW_ERROR;

  self->process (self, &frame);

  gst_video_frame_unmap (&frame);
  return GST_FLOW_OK;
}

static gboolean
gst_video_codec_test_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (sink);

  if (!gst_video_info_from_caps (&self->vinfo, caps))
    return FALSE;

  switch (GST_VIDEO_INFO_FORMAT (&self->vinfo)) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I422_12LE:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_12LE:
      self->process = gst_video_codec_test_sink_process_i42x;
      break;
    case GST_VIDEO_FORMAT_NV12:
      self->process = gst_video_codec_test_sink_process_nv12;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static gboolean
gst_video_codec_test_sink_propose_allocation (GstBaseSink * sink,
    GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  return TRUE;
}

static gboolean
gst_video_codec_test_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (sink);

  if (event->type == GST_EVENT_EOS) {
    const gchar *checksum_type = "UNKNOWN";

    switch (self->hash) {
      case G_CHECKSUM_MD5:
        checksum_type = "MD5";
        break;
      case G_CHECKSUM_SHA1:
        checksum_type = "SHA1";
        break;
      case G_CHECKSUM_SHA256:
        checksum_type = "SHA256";
        break;
      case G_CHECKSUM_SHA512:
        checksum_type = "SHA512";
        break;
      case G_CHECKSUM_SHA384:
        checksum_type = "SHA384";
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_element (GST_OBJECT (self),
            gst_structure_new ("conformance/checksum", "checksum-type",
                G_TYPE_STRING, checksum_type, "checksum", G_TYPE_STRING,
                g_checksum_get_string (self->checksum), NULL)));
    g_checksum_reset (self->checksum);
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}

static void
gst_video_codec_test_sink_init (GstVideoCodecTestSink * sink)
{
  gst_base_sink_set_sync (GST_BASE_SINK (sink), FALSE);
  sink->hash = G_CHECKSUM_MD5;
}

static void
gst_video_codec_test_sink_finalize (GObject * object)
{
  GstVideoCodecTestSink *self = GST_VIDEO_CODEC_TEST_SINK (object);

  g_free (self->location);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_video_codec_test_sink_class_init (GstVideoCodecTestSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->set_property = gst_video_codec_test_sink_set_property;
  gobject_class->get_property = gst_video_codec_test_sink_get_property;
  gobject_class->finalize = gst_video_codec_test_sink_finalize;

  base_sink_class->start = GST_DEBUG_FUNCPTR (gst_video_codec_test_sink_start);
  base_sink_class->stop = GST_DEBUG_FUNCPTR (gst_video_codec_test_sink_stop);
  base_sink_class->render =
      GST_DEBUG_FUNCPTR (gst_video_codec_test_sink_render);
  base_sink_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_video_codec_test_sink_set_caps);
  base_sink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_video_codec_test_sink_propose_allocation);
  base_sink_class->event = GST_DEBUG_FUNCPTR (gst_video_codec_test_sink_event);

  gst_element_class_add_static_pad_template (element_class,
      &gst_video_codec_test_sink_template);

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "File path to store non-padded I420 stream (optional).", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Video CODEC Test Sink", "Debug/video/Sink",
      "Sink to test video CODEC conformance",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com");
}
