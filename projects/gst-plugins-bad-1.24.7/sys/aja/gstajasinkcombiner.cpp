/* GStreamer
 * Copyright (C) 2021 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstajacommon.h"
#include "gstajasinkcombiner.h"

GST_DEBUG_CATEGORY_STATIC(gst_aja_sink_combiner_debug);
#define GST_CAT_DEFAULT gst_aja_sink_combiner_debug

static GstStaticPadTemplate video_sink_template = GST_STATIC_PAD_TEMPLATE(
    "video", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw"));

static GstStaticPadTemplate audio_sink_template =
    GST_STATIC_PAD_TEMPLATE("audio", GST_PAD_SINK, GST_PAD_REQUEST,
                            GST_STATIC_CAPS("audio/x-raw, "
                                            "format = (string) S32LE, "
                                            "rate = (int) 48000, "
                                            "channels = (int) [ 1, 16 ], "
                                            "layout = (string) interleaved"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw"));

G_DEFINE_TYPE(GstAjaSinkCombiner, gst_aja_sink_combiner, GST_TYPE_AGGREGATOR);
#define parent_class gst_aja_sink_combiner_parent_class

static void gst_aja_sink_combiner_finalize(GObject *object) {
  GstAjaSinkCombiner *self = GST_AJA_SINK_COMBINER(object);

  GST_OBJECT_LOCK(self);
  gst_caps_replace(&self->audio_caps, NULL);
  gst_caps_replace(&self->video_caps, NULL);
  GST_OBJECT_UNLOCK(self);

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GstFlowReturn gst_aja_sink_combiner_aggregate(GstAggregator *aggregator,
                                                     gboolean timeout) {
  GstAjaSinkCombiner *self = GST_AJA_SINK_COMBINER(aggregator);
  GstBuffer *video_buffer, *audio_buffer;

  if (gst_aggregator_pad_is_eos(GST_AGGREGATOR_PAD_CAST(self->audio_sinkpad)) &&
      gst_aggregator_pad_is_eos(GST_AGGREGATOR_PAD_CAST(self->video_sinkpad))) {
    GST_DEBUG_OBJECT(self, "All pads EOS");
    return GST_FLOW_EOS;
  }

  // FIXME: We currently assume that upstream provides
  // - properly chunked buffers (1 buffer = 1 video frame)
  // - properly synchronized buffers (audio/video starting at the same time)
  // - no gaps
  //
  // This can be achieved externally with elements like audiobuffersplit and
  // videorate.

  video_buffer = gst_aggregator_pad_peek_buffer(
      GST_AGGREGATOR_PAD_CAST(self->video_sinkpad));
  if (!video_buffer) return GST_AGGREGATOR_FLOW_NEED_DATA;

  audio_buffer = gst_aggregator_pad_peek_buffer(
      GST_AGGREGATOR_PAD_CAST(self->audio_sinkpad));
  if (!audio_buffer && !gst_aggregator_pad_is_eos(
                           GST_AGGREGATOR_PAD_CAST(self->audio_sinkpad))) {
    gst_buffer_unref(video_buffer);
    GST_TRACE_OBJECT(self, "Audio not ready yet, waiting");
    return GST_AGGREGATOR_FLOW_NEED_DATA;
  }

  gst_aggregator_pad_drop_buffer(GST_AGGREGATOR_PAD_CAST(self->video_sinkpad));
  video_buffer = gst_buffer_make_writable(video_buffer);
  GST_TRACE_OBJECT(self,
                   "Outputting buffer with video %" GST_PTR_FORMAT
                   " and audio %" GST_PTR_FORMAT,
                   video_buffer, audio_buffer);
  if (audio_buffer) {
    gst_buffer_add_aja_audio_meta(video_buffer, audio_buffer);
    gst_buffer_unref(audio_buffer);
    gst_aggregator_pad_drop_buffer(
        GST_AGGREGATOR_PAD_CAST(self->audio_sinkpad));
  }

  if (!gst_pad_has_current_caps(GST_AGGREGATOR_SRC_PAD(self)) ||
      self->caps_changed) {
    GstCaps *caps = gst_caps_copy(self->video_caps);
    GstStructure *s;

    s = gst_caps_get_structure(caps, 0);
    if (self->audio_caps) {
      const GstStructure *s2;
      gint audio_channels;

      s2 = gst_caps_get_structure(self->audio_caps, 0);

      gst_structure_get_int(s2, "channels", &audio_channels);
      gst_structure_set(s, "audio-channels", G_TYPE_INT, audio_channels, NULL);
    } else {
      gst_structure_set(s, "audio-channels", G_TYPE_INT, 0, NULL);
    }

    GST_DEBUG_OBJECT(self, "Configuring caps %" GST_PTR_FORMAT, caps);

    gst_aggregator_set_src_caps(GST_AGGREGATOR(self), caps);
    gst_caps_unref(caps);
    self->caps_changed = FALSE;
  }

  // Update the position for synchronization purposes
  GST_AGGREGATOR_PAD_CAST(GST_AGGREGATOR_SRC_PAD(self))->segment.position =
      GST_BUFFER_PTS(video_buffer);
  if (GST_BUFFER_DURATION_IS_VALID(video_buffer))
    GST_AGGREGATOR_PAD_CAST(GST_AGGREGATOR_SRC_PAD(self))->segment.position +=
        GST_BUFFER_DURATION(video_buffer);

  return gst_aggregator_finish_buffer(GST_AGGREGATOR_CAST(self), video_buffer);
}

static gboolean gst_aja_sink_combiner_sink_event(GstAggregator *aggregator,
                                                 GstAggregatorPad *agg_pad,
                                                 GstEvent *event) {
  GstAjaSinkCombiner *self = GST_AJA_SINK_COMBINER(aggregator);

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEGMENT: {
      const GstSegment *segment;

      gst_event_parse_segment(event, &segment);
      gst_aggregator_update_segment(GST_AGGREGATOR(self), segment);
      break;
    }
    case GST_EVENT_CAPS: {
      GstCaps *caps;

      gst_event_parse_caps(event, &caps);

      if (agg_pad == GST_AGGREGATOR_PAD_CAST(self->audio_sinkpad)) {
        GST_OBJECT_LOCK(self);
        gst_caps_replace(&self->audio_caps, caps);
        self->caps_changed = TRUE;
        GST_OBJECT_UNLOCK(self);
      } else if (agg_pad == GST_AGGREGATOR_PAD_CAST(self->video_sinkpad)) {
        GST_OBJECT_LOCK(self);
        gst_caps_replace(&self->video_caps, caps);
        self->caps_changed = TRUE;
        GST_OBJECT_UNLOCK(self);
      }

      break;
    }
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS(parent_class)
      ->sink_event(aggregator, agg_pad, event);
}

static gboolean gst_aja_sink_combiner_sink_query(GstAggregator *aggregator,
                                                 GstAggregatorPad *agg_pad,
                                                 GstQuery *query) {
  GstAjaSinkCombiner *self = GST_AJA_SINK_COMBINER(aggregator);

  switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_CAPS: {
      GstCaps *filter, *caps;

      gst_query_parse_caps(query, &filter);

      if (agg_pad == GST_AGGREGATOR_PAD_CAST(self->audio_sinkpad)) {
        caps = gst_pad_get_pad_template_caps(GST_PAD(agg_pad));
      } else if (agg_pad == GST_AGGREGATOR_PAD_CAST(self->video_sinkpad)) {
        caps = gst_pad_peer_query_caps(GST_AGGREGATOR_SRC_PAD(self), NULL);
        caps = gst_caps_make_writable(caps);
        guint caps_size = gst_caps_get_size(caps);
        for (guint i = 0; i < caps_size; i++) {
          GstStructure *s = gst_caps_get_structure(caps, i);
          gst_structure_remove_field(s, "audio-channels");
        }
      } else {
        g_assert_not_reached();
      }

      if (filter) {
        GstCaps *tmp = gst_caps_intersect(filter, caps);
        gst_caps_unref(caps);
        caps = tmp;
      }

      gst_query_set_caps_result(query, caps);

      return TRUE;
    }
    case GST_QUERY_ALLOCATION: {
      // Proxy to the sink for both pads so that the AJA allocator can be
      // used upstream as needed.
      return gst_pad_peer_query(GST_AGGREGATOR_SRC_PAD(self), query);
    }
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS(parent_class)
      ->sink_query(aggregator, agg_pad, query);
}

static gboolean gst_aja_sink_combiner_negotiate(GstAggregator *aggregator) {
  return TRUE;
}

static gboolean gst_aja_sink_combiner_stop(GstAggregator *aggregator) {
  GstAjaSinkCombiner *self = GST_AJA_SINK_COMBINER(aggregator);

  GST_OBJECT_LOCK(self);
  gst_caps_replace(&self->audio_caps, NULL);
  gst_caps_replace(&self->video_caps, NULL);
  GST_OBJECT_UNLOCK(self);

  return TRUE;
}

static void gst_aja_sink_combiner_class_init(GstAjaSinkCombinerClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAggregatorClass *aggregator_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  aggregator_class = (GstAggregatorClass *)klass;

  gobject_class->finalize = gst_aja_sink_combiner_finalize;

  gst_element_class_set_static_metadata(
      gstelement_class, "AJA sink audio/video combiner", "Audio/Video/Combiner",
      "Combines corresponding audio/video frames",
      "Sebastian Dröge <sebastian@centricular.com>");

  gst_element_class_add_static_pad_template_with_gtype(
      gstelement_class, &video_sink_template, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype(
      gstelement_class, &audio_sink_template, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype(
      gstelement_class, &src_template, GST_TYPE_AGGREGATOR_PAD);

  aggregator_class->aggregate = gst_aja_sink_combiner_aggregate;
  aggregator_class->stop = gst_aja_sink_combiner_stop;
  aggregator_class->sink_event = gst_aja_sink_combiner_sink_event;
  aggregator_class->sink_query = gst_aja_sink_combiner_sink_query;
  aggregator_class->negotiate = gst_aja_sink_combiner_negotiate;
  aggregator_class->get_next_time = gst_aggregator_simple_get_next_time;

  // We don't support requesting new pads
  gstelement_class->request_new_pad = NULL;

  GST_DEBUG_CATEGORY_INIT(gst_aja_sink_combiner_debug, "ajasinkcombiner", 0,
                          "AJA sink combiner");
}

static void gst_aja_sink_combiner_init(GstAjaSinkCombiner *self) {
  GstPadTemplate *templ;

  templ = gst_static_pad_template_get(&video_sink_template);
  self->video_sinkpad =
      GST_PAD(g_object_new(GST_TYPE_AGGREGATOR_PAD, "name", "video",
                           "direction", GST_PAD_SINK, "template", templ, NULL));
  gst_object_unref(templ);
  gst_element_add_pad(GST_ELEMENT_CAST(self), self->video_sinkpad);

  templ = gst_static_pad_template_get(&audio_sink_template);
  self->audio_sinkpad =
      GST_PAD(g_object_new(GST_TYPE_AGGREGATOR_PAD, "name", "audio",
                           "direction", GST_PAD_SINK, "template", templ, NULL));
  gst_object_unref(templ);
  gst_element_add_pad(GST_ELEMENT_CAST(self), self->audio_sinkpad);
}
