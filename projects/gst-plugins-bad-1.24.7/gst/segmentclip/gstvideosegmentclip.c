/* GStreamer
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include "gstvideosegmentclip.h"

static GstStaticPadTemplate sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, framerate=" GST_VIDEO_FPS_RANGE));

static GstStaticPadTemplate src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, framerate=" GST_VIDEO_FPS_RANGE));

static void gst_video_segment_clip_reset (GstSegmentClip * self);
static GstFlowReturn gst_video_segment_clip_clip_buffer (GstSegmentClip * self,
    GstBuffer * buffer, GstBuffer ** outbuf);
static gboolean gst_video_segment_clip_set_caps (GstSegmentClip * self,
    GstCaps * caps);

GST_DEBUG_CATEGORY_STATIC (gst_video_segment_clip_debug);
#define GST_CAT_DEFAULT gst_video_segment_clip_debug

G_DEFINE_TYPE (GstVideoSegmentClip, gst_video_segment_clip,
    GST_TYPE_SEGMENT_CLIP);
GST_ELEMENT_REGISTER_DEFINE (videosegmentclip, "videosegmentclip",
    GST_RANK_NONE, GST_TYPE_VIDEO_SEGMENT_CLIP);

static void
gst_video_segment_clip_class_init (GstVideoSegmentClipClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstSegmentClipClass *segment_clip_klass = GST_SEGMENT_CLIP_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_video_segment_clip_debug, "videosegmentclip", 0,
      "videosegmentclip element");

  gst_element_class_set_static_metadata (element_class,
      "Video buffer segment clipper",
      "Filter/Video",
      "Clips video buffers to the configured segment",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_element_class_add_static_pad_template (element_class, &sink_pad_template);
  gst_element_class_add_static_pad_template (element_class, &src_pad_template);

  segment_clip_klass->reset = GST_DEBUG_FUNCPTR (gst_video_segment_clip_reset);
  segment_clip_klass->set_caps =
      GST_DEBUG_FUNCPTR (gst_video_segment_clip_set_caps);
  segment_clip_klass->clip_buffer =
      GST_DEBUG_FUNCPTR (gst_video_segment_clip_clip_buffer);
}

static void
gst_video_segment_clip_init (GstVideoSegmentClip * self)
{
}

static void
gst_video_segment_clip_reset (GstSegmentClip * base)
{
  GstVideoSegmentClip *self = GST_VIDEO_SEGMENT_CLIP (base);

  GST_DEBUG_OBJECT (self, "Resetting internal state");

  self->fps_n = self->fps_d = 0;
}


static GstFlowReturn
gst_video_segment_clip_clip_buffer (GstSegmentClip * base, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  GstVideoSegmentClip *self = GST_VIDEO_SEGMENT_CLIP (base);
  GstSegment *segment = &base->segment;
  GstClockTime timestamp, duration;
  guint64 cstart, cstop;
  gboolean in_seg;

  if (!self->fps_d) {
    GST_ERROR_OBJECT (self, "Not negotiated yet");
    gst_buffer_unref (buffer);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (segment->format != GST_FORMAT_TIME) {
    GST_DEBUG_OBJECT (self, "Unsupported segment format %s",
        gst_format_get_name (segment->format));
    *outbuf = buffer;
    return GST_FLOW_OK;
  }

  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    GST_WARNING_OBJECT (self, "Buffer without valid timestamp");
    *outbuf = buffer;
    return GST_FLOW_OK;
  }

  if (self->fps_n == 0) {
    *outbuf = buffer;
    return GST_FLOW_OK;
  }

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);
  if (!GST_CLOCK_TIME_IS_VALID (duration))
    duration = gst_util_uint64_scale (GST_SECOND, self->fps_d, self->fps_n);

  in_seg =
      gst_segment_clip (segment, GST_FORMAT_TIME, timestamp,
      timestamp + duration, &cstart, &cstop);
  if (in_seg) {
    if (timestamp != cstart || timestamp + duration != cstop) {
      *outbuf = gst_buffer_make_writable (buffer);

      GST_BUFFER_TIMESTAMP (*outbuf) = cstart;
      GST_BUFFER_DURATION (*outbuf) = cstop - cstart;
    } else {
      *outbuf = buffer;
    }
  } else {
    GST_DEBUG_OBJECT (self, "Buffer outside the configured segment");

    gst_buffer_unref (buffer);
    if (segment->rate >= 0) {
      if (segment->stop != -1 && timestamp >= segment->stop)
        return GST_FLOW_EOS;
    } else {
      if (segment->start != -1 && timestamp + duration <= segment->start)
        return GST_FLOW_EOS;
    }
  }


  return GST_FLOW_OK;
}

static gboolean
gst_video_segment_clip_set_caps (GstSegmentClip * base, GstCaps * caps)
{
  GstVideoSegmentClip *self = GST_VIDEO_SEGMENT_CLIP (base);
  gboolean ret;
  GstStructure *s;
  gint fps_n, fps_d;

  s = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d)
      && (fps_d != 0);

  if (ret) {
    GST_DEBUG_OBJECT (self, "Configured framerate %d/%d", fps_n, fps_d);
    self->fps_n = fps_n;
    self->fps_d = fps_d;
  }

  return ret;
}
