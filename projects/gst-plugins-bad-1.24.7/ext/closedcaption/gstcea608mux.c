/*
 * GStreamer
 * Copyright (C) 2023 Mathieu Duponchelle <mathieu@centricular.com>
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
 * SECTION:element-cea608mux
 * @title: cea608mux
 * @short_description: Combine CC1 and CC3 raw 608 streams
 *
 * ```
 * gst-launch-1.0 cea608mux name=mux ! fakesink dump=true \
 * filesrc location=one.scc ! sccparse ! closedcaption/x-cea-608 ! ccconverter ! mux. \
 * filesrc location=two.scc ! sccparse ! ccconverter ! closedcaption/x-cea-608, format=raw, field=0 ! \
 *     capssetter caps="closedcaption/x-cea-608, format=raw, field=1" ! mux.
 * ```
 *
 * Since: 1.24
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <string.h>

#include "ccutils.h"
#include "gstcea608mux.h"

GST_DEBUG_CATEGORY_STATIC (gst_cea608_mux_debug);
#define GST_CAT_DEFAULT gst_cea608_mux_debug

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("closedcaption/x-cea-608, format=s334-1a, "
        "framerate=(fraction){60/1, 60000/1001, 50/1, 30/1, 30000/1001, 25/1, 24/1, 24000/1001}"));

static GstStaticPadTemplate cc1_template = GST_STATIC_PAD_TEMPLATE ("cc1",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("closedcaption/x-cea-608,format=raw,field=0"));

static GstStaticPadTemplate cc3_template = GST_STATIC_PAD_TEMPLATE ("cc3",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("closedcaption/x-cea-608,format=raw,field=1"));

#define parent_class gst_cea608_mux_parent_class
G_DEFINE_TYPE (GstCea608Mux, gst_cea608_mux, GST_TYPE_AGGREGATOR);
GST_ELEMENT_REGISTER_DEFINE (cea608mux, "cea608mux",
    GST_RANK_NONE, GST_TYPE_CEA608MUX);

enum
{
  PROP_0,
};

static void
gst_cea608_mux_finalize (GObject * object)
{
  GstCea608Mux *self = GST_CEA608MUX (object);

  gst_clear_object (&self->cc_buffer);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#define GST_FLOW_NEED_DATA GST_FLOW_CUSTOM_SUCCESS

static GstAggregatorPad *
find_best_pad (GstAggregator * aggregator, GstClockTime * ts, gboolean timeout)
{
  GstAggregatorPad *best = NULL;
  GstClockTime best_ts = GST_CLOCK_TIME_NONE;
  GstIterator *pads;
  GValue padptr = { 0, };
  gboolean done = FALSE;

  pads = gst_element_iterate_sink_pads (GST_ELEMENT (aggregator));

  while (!done) {
    switch (gst_iterator_next (pads, &padptr)) {
      case GST_ITERATOR_OK:{
        GstAggregatorPad *apad = g_value_get_object (&padptr);
        GstClockTime t = GST_CLOCK_TIME_NONE;
        GstBuffer *buffer;

        buffer = gst_aggregator_pad_peek_buffer (apad);
        if (!buffer) {
          if (!timeout && !GST_PAD_IS_EOS (apad)) {
            gst_object_replace ((GstObject **) & best, NULL);
            best_ts = GST_CLOCK_TIME_NONE;
            done = TRUE;
          }
          break;
        }

        if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DTS_OR_PTS (buffer))) {
          t = gst_segment_to_running_time (&apad->segment, GST_FORMAT_TIME,
              GST_BUFFER_PTS (buffer));
        }

        if (!GST_CLOCK_TIME_IS_VALID (best_ts) ||
            (GST_CLOCK_TIME_IS_VALID (t) && t < best_ts)) {
          gst_object_replace ((GstObject **) & best, GST_OBJECT (apad));
          best_ts = t;
        }
        gst_buffer_unref (buffer);
        break;
      }
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (pads);
        /* Clear the best pad and start again. It might have disappeared */
        gst_object_replace ((GstObject **) & best, NULL);
        best_ts = GST_CLOCK_TIME_NONE;
        break;
      case GST_ITERATOR_ERROR:
        /* This can't happen if the parameters to gst_iterator_next() are valid */
        g_assert_not_reached ();
        break;
    }
    g_value_reset (&padptr);
  }
  g_value_unset (&padptr);
  gst_iterator_free (pads);

  if (best) {
    GST_LOG_OBJECT (aggregator,
        "Best pad found with TS %" GST_TIME_FORMAT ": %" GST_PTR_FORMAT,
        GST_TIME_ARGS (best_ts), best);
  } else {
    GST_LOG_OBJECT (aggregator, "Best pad not found");
  }

  if (ts && GST_CLOCK_TIME_IS_VALID (best_ts))
    *ts = best_ts;

  return best;
}

static gboolean
all_pads_eos (GstAggregator * agg)
{
  GList *l;
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (agg);
  for (l = GST_ELEMENT_CAST (agg)->sinkpads; l; l = l->next) {
    GstAggregatorPad *pad = GST_AGGREGATOR_PAD (l->data);

    if (!gst_aggregator_pad_is_eos (pad)) {
      ret = FALSE;
      break;
    }
  }
  GST_OBJECT_UNLOCK (agg);

  return ret;
}

static void
take_s334_both_fields (GstCea608Mux * self, GstBuffer * buffer)
{
  GstMapInfo out = GST_MAP_INFO_INIT;
  guint s334_len, cc_data_len, i;

  gst_buffer_map (buffer, &out, GST_MAP_READWRITE);

  cc_data_len = out.size;
  cc_buffer_take_cc_data (self->cc_buffer, self->cdp_fps_entry, FALSE, out.data,
      &cc_data_len);
  s334_len = drop_ccp_from_cc_data (out.data, cc_data_len);
  if (s334_len < 0) {
    s334_len = 0;
    goto out;
  }

  for (i = 0; i < s334_len / 3; i++) {
    guint byte = out.data[i * 3];
    /* We have to assume a line offset of 0 */
    out.data[i * 3] = (byte == 0xfc || byte == 0xf8) ? 0x80 : 0x00;
  }

out:
  gst_buffer_unmap (buffer, &out);
  gst_buffer_set_size (buffer, s334_len);
}

static GstFlowReturn
finish_s334_both_fields (GstCea608Mux * self)
{
  GstClockTime output_pts = gst_util_uint64_scale_int (GST_SECOND,
      self->cdp_fps_entry->fps_d * self->n_output_buffers,
      self->cdp_fps_entry->fps_n);
  GstClockTime output_duration =
      gst_util_uint64_scale_int (GST_SECOND, self->cdp_fps_entry->fps_d,
      self->cdp_fps_entry->fps_n);
  GstBuffer *output = gst_buffer_new_allocate (NULL, MAX_CDP_PACKET_LEN, NULL);
  GstSegment *agg_segment =
      &GST_AGGREGATOR_PAD (GST_AGGREGATOR (self)->srcpad)->segment;

  output_pts += self->start_time;

  take_s334_both_fields (self, output);
  GST_BUFFER_PTS (output) = output_pts;
  GST_BUFFER_DURATION (output) = output_duration;
  GST_DEBUG_OBJECT (self, "Finishing %" GST_PTR_FORMAT, output);
  self->n_output_buffers += 1;
  agg_segment->position = output_pts + output_duration;

  return gst_aggregator_finish_buffer (GST_AGGREGATOR (self), output);
}

static GstFlowReturn
gst_cea608_mux_aggregate (GstAggregator * aggregator, gboolean timeout)
{
  GstCea608Mux *self = GST_CEA608MUX (aggregator);
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstAggregatorPad *best_pad = NULL;
  GstClockTime output_duration =
      gst_util_uint64_scale_int (GST_SECOND, self->cdp_fps_entry->fps_d,
      self->cdp_fps_entry->fps_n);
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (aggregator->srcpad)->segment;
  GstClockTime output_start_time = agg_segment->position;
  GstClockTime output_end_running_time;

  if (agg_segment->position == -1 || agg_segment->position < agg_segment->start)
    output_start_time = agg_segment->start;

  if (!GST_CLOCK_TIME_IS_VALID (self->start_time)) {
    self->start_time = output_start_time;
    GST_DEBUG_OBJECT (self, "Start time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (self->start_time));
  }

  best_pad =
      find_best_pad (aggregator, &self->earliest_input_running_time, timeout);

  output_end_running_time =
      gst_segment_to_running_time (agg_segment, GST_FORMAT_TIME,
      output_start_time + output_duration);

  GST_LOG_OBJECT (self, "best-pad: %s, timeout: %d, "
      "earliest input running time: %"
      GST_TIME_FORMAT ", output running time: %" GST_TIME_FORMAT,
      best_pad ? GST_OBJECT_NAME (best_pad) : "NULL", timeout,
      GST_TIME_ARGS (self->earliest_input_running_time),
      GST_TIME_ARGS (output_end_running_time));

  if (GST_CLOCK_TIME_IS_VALID (self->earliest_input_running_time)
      && self->earliest_input_running_time > output_end_running_time) {
    /* Nothing to consume, earliest pad is not ready yet */
    GST_LOG_OBJECT (self, "Nothing to consume");
  } else if (best_pad) {
    GstBuffer *buffer;

    buffer = gst_aggregator_pad_pop_buffer (GST_AGGREGATOR_PAD (best_pad));

    if (buffer) {
      GstMapInfo map;

      gst_buffer_map (buffer, &map, GST_MAP_READ);

      if (g_strcmp0 (GST_PAD_NAME (best_pad), "cc1") == 0) {
        GST_DEBUG_OBJECT (self, "Consuming CC1 %" GST_PTR_FORMAT, buffer);
        cc_buffer_push_separated (self->cc_buffer, map.data, map.size, NULL, 0,
            NULL, 0);
      } else {
        GST_DEBUG_OBJECT (self, "Consuming CC3 %" GST_PTR_FORMAT, buffer);
        cc_buffer_push_separated (self->cc_buffer, NULL, 0, map.data, map.size,
            NULL, 0);
      }

      gst_buffer_unmap (buffer, &map);
      gst_buffer_unref (buffer);
    } else {
      /* We got flushed */
      flow_ret = GST_AGGREGATOR_FLOW_NEED_DATA;
    }
  } else if (all_pads_eos (aggregator)) {
    GST_INFO_OBJECT (self, "EOS!");
    flow_ret = GST_FLOW_EOS;
  } else {
    GST_LOG_OBJECT (self, "Need more data");
    flow_ret = GST_AGGREGATOR_FLOW_NEED_DATA;
  }

  if (flow_ret == GST_FLOW_OK) {
    if (timeout || output_end_running_time < self->earliest_input_running_time) {
      flow_ret = finish_s334_both_fields (self);
    }
  } else if (flow_ret == GST_FLOW_EOS && !cc_buffer_is_empty (self->cc_buffer)) {
    flow_ret = finish_s334_both_fields (self);
  }

  g_clear_pointer (&best_pad, gst_object_unref);

  return flow_ret;
}

static gboolean
gst_cea608_mux_stop (GstAggregator * aggregator)
{
  GstCea608Mux *self = GST_CEA608MUX (aggregator);

  cc_buffer_discard (self->cc_buffer);
  self->n_output_buffers = 0;
  self->earliest_input_running_time = 0;
  self->start_time = GST_CLOCK_TIME_NONE;

  return TRUE;
}

static GstFlowReturn
gst_cea608_mux_flush (GstAggregator * aggregator)
{
  GstCea608Mux *self = GST_CEA608MUX (aggregator);
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD (aggregator->srcpad)->segment;

  GST_DEBUG_OBJECT (self, "Flush");

  cc_buffer_discard (self->cc_buffer);
  self->n_output_buffers = 0;
  self->earliest_input_running_time = 0;
  self->start_time = GST_CLOCK_TIME_NONE;
  agg_segment->position = -1;

  return GST_FLOW_OK;
}

static gboolean
gst_cea608_mux_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  GstStructure *s = gst_caps_get_structure (caps, 0);
  gint fps_n, fps_d;
  GstCea608Mux *self = GST_CEA608MUX (agg);
  GstClockTime latency;

  GST_INFO_OBJECT (agg->srcpad, "set src caps: %" GST_PTR_FORMAT, caps);

  g_assert (gst_structure_get_fraction (s, "framerate", &fps_n,
          &fps_d) == TRUE);
  self->cdp_fps_entry = cdp_fps_entry_from_fps (fps_n, fps_d);
  g_assert (self->cdp_fps_entry != NULL && self->cdp_fps_entry->fps_n != 0);

  latency =
      gst_util_uint64_scale (GST_SECOND, self->cdp_fps_entry->fps_d,
      self->cdp_fps_entry->fps_n);
  gst_aggregator_set_latency (agg, latency, latency);

  return TRUE;
}

static GstBuffer *
gst_cea608_mux_clip (GstAggregator * aggregator, GstAggregatorPad * pad,
    GstBuffer * buffer)
{
  GstClockTime time;

  if (!GST_BUFFER_PTS_IS_VALID (buffer))
    return buffer;

  time = gst_segment_to_running_time (&pad->segment, GST_FORMAT_TIME,
      GST_BUFFER_PTS (buffer));
  if (!GST_CLOCK_TIME_IS_VALID (time)) {
    GST_DEBUG_OBJECT (pad, "Dropping buffer on pad outside segment %"
        GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_PTS (buffer)));
    gst_buffer_unref (buffer);
    return NULL;
  }

  return buffer;
}

static void
gst_cea608_mux_class_init (GstCea608MuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAggregatorClass *aggregator_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  aggregator_class = (GstAggregatorClass *) klass;

  gobject_class->finalize = gst_cea608_mux_finalize;

  gst_element_class_set_static_metadata (gstelement_class,
      "Closed Caption Muxer",
      "Aggregator",
      "Combines raw 608 streams",
      "Mathieu Duponchelle <mathieu@centricular.com>");

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &srctemplate, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &cc1_template, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &cc3_template, GST_TYPE_AGGREGATOR_PAD);

  aggregator_class->aggregate = gst_cea608_mux_aggregate;
  aggregator_class->stop = gst_cea608_mux_stop;
  aggregator_class->flush = gst_cea608_mux_flush;
  aggregator_class->negotiated_src_caps = gst_cea608_mux_negotiated_src_caps;
  aggregator_class->get_next_time = gst_aggregator_simple_get_next_time;
  aggregator_class->clip = gst_cea608_mux_clip;

  GST_DEBUG_CATEGORY_INIT (gst_cea608_mux_debug, "cea608mux",
      0, "Closed Caption muxer");
}

static void
gst_cea608_mux_init (GstCea608Mux * self)
{
  self->cc_buffer = cc_buffer_new ();
  cc_buffer_set_max_buffer_time (self->cc_buffer, GST_CLOCK_TIME_NONE);
  cc_buffer_set_output_padding (self->cc_buffer, TRUE, FALSE);
  self->cdp_fps_entry = &null_fps_entry;
  self->start_time = GST_CLOCK_TIME_NONE;
}
