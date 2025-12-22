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
#include <gst/audio/audio.h>

#include "gstaudiosegmentclip.h"

static GstStaticPadTemplate sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL)));

static GstStaticPadTemplate src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL)));

static void gst_audio_segment_clip_reset (GstSegmentClip * self);
static GstFlowReturn gst_audio_segment_clip_clip_buffer (GstSegmentClip * self,
    GstBuffer * buffer, GstBuffer ** outbuf);
static gboolean gst_audio_segment_clip_set_caps (GstSegmentClip * self,
    GstCaps * caps);

GST_DEBUG_CATEGORY_STATIC (gst_audio_segment_clip_debug);
#define GST_CAT_DEFAULT gst_audio_segment_clip_debug

G_DEFINE_TYPE (GstAudioSegmentClip, gst_audio_segment_clip,
    GST_TYPE_SEGMENT_CLIP);
GST_ELEMENT_REGISTER_DEFINE (audiosegmentclip, "audiosegmentclip",
    GST_RANK_NONE, GST_TYPE_AUDIO_SEGMENT_CLIP);

static void
gst_audio_segment_clip_class_init (GstAudioSegmentClipClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstSegmentClipClass *segment_clip_klass = GST_SEGMENT_CLIP_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_audio_segment_clip_debug, "audiosegmentclip", 0,
      "audiosegmentclip element");

  segment_clip_klass->reset = GST_DEBUG_FUNCPTR (gst_audio_segment_clip_reset);
  segment_clip_klass->set_caps =
      GST_DEBUG_FUNCPTR (gst_audio_segment_clip_set_caps);
  segment_clip_klass->clip_buffer =
      GST_DEBUG_FUNCPTR (gst_audio_segment_clip_clip_buffer);

  gst_element_class_set_static_metadata (element_class,
      "Audio buffer segment clipper",
      "Filter/Audio",
      "Clips audio buffers to the configured segment",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_element_class_add_static_pad_template (element_class, &sink_pad_template);
  gst_element_class_add_static_pad_template (element_class, &src_pad_template);
}

static void
gst_audio_segment_clip_init (GstAudioSegmentClip * self)
{
}

static void
gst_audio_segment_clip_reset (GstSegmentClip * base)
{
  GstAudioSegmentClip *self = GST_AUDIO_SEGMENT_CLIP (base);

  GST_DEBUG_OBJECT (self, "Resetting internal state");

  self->rate = self->framesize = 0;
}


static GstFlowReturn
gst_audio_segment_clip_clip_buffer (GstSegmentClip * base, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  GstAudioSegmentClip *self = GST_AUDIO_SEGMENT_CLIP (base);
  GstSegment *segment = &base->segment;
  GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);
  GstClockTime duration = GST_BUFFER_DURATION (buffer);
  guint64 offset = GST_BUFFER_OFFSET (buffer);
  guint64 offset_end = GST_BUFFER_OFFSET_END (buffer);
  guint size = gst_buffer_get_size (buffer);

  if (!self->rate || !self->framesize) {
    GST_ERROR_OBJECT (self, "Not negotiated yet");
    gst_buffer_unref (buffer);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (segment->format != GST_FORMAT_DEFAULT &&
      segment->format != GST_FORMAT_TIME) {
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

  *outbuf =
      gst_audio_buffer_clip (buffer, segment, self->rate, self->framesize);

  if (!*outbuf) {
    GST_DEBUG_OBJECT (self, "Buffer outside the configured segment");

    /* Now return unexpected if we're before/after the end */
    if (segment->format == GST_FORMAT_TIME) {
      if (segment->rate >= 0) {
        if (segment->stop != -1 && timestamp >= segment->stop)
          return GST_FLOW_EOS;
      } else {
        if (!GST_CLOCK_TIME_IS_VALID (duration))
          duration =
              gst_util_uint64_scale_int (size, GST_SECOND,
              self->framesize * self->rate);

        if (segment->start != -1 && timestamp + duration <= segment->start)
          return GST_FLOW_EOS;
      }
    } else {
      if (segment->rate >= 0) {
        if (segment->stop != -1 && offset != -1 && offset >= segment->stop)
          return GST_FLOW_EOS;
      } else if (offset != -1 || offset_end != -1) {
        if (offset_end == -1)
          offset_end = offset + size / self->framesize;

        if (segment->start != -1 && offset_end <= segment->start)
          return GST_FLOW_EOS;
      }
    }
  }

  return GST_FLOW_OK;
}

static gboolean
gst_audio_segment_clip_set_caps (GstSegmentClip * base, GstCaps * caps)
{
  GstAudioSegmentClip *self = GST_AUDIO_SEGMENT_CLIP (base);
  gboolean ret;
  GstAudioInfo info;
  gint rate, channels, width;

  gst_audio_info_init (&info);
  ret = gst_audio_info_from_caps (&info, caps);

  if (ret) {
    rate = GST_AUDIO_INFO_RATE (&info);
    channels = GST_AUDIO_INFO_CHANNELS (&info);
    width = GST_AUDIO_INFO_WIDTH (&info);

    GST_DEBUG_OBJECT (self, "Configured: rate %d channels %d width %d",
        rate, channels, width);
    self->rate = rate;
    self->framesize = (width / 8) * channels;
  }

  return ret;
}
