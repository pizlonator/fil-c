/* GStreamer
 * Copyright (C) <2021> Collabora Ltd.
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

/**
 * SECTION:element-codecalphademux
 * @title: CODEC Alpha Demuxer
 *
 * Extracts the CODEC (typically VP8/VP9) alpha stream stored as meta and
 * exposes it as a stream. This element allow using single stream VP8/9
 * decoders in order to decode both streams.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=transparency.webm ! matroskademux ! 
 *     codecalphademux name=d
 *     d.video ! queue ! vp9dec ! autovideosink
 *     d.alpha ! queue ! vp9dec ! autovideosink
 * ]| This pipeline splits and decode the video and the alpha stream, showing
 *    the result on seperate windows.
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>
#include <gst/base/gstflowcombiner.h>

#include "gstcodecalphademux.h"

GST_DEBUG_CATEGORY_STATIC (codecalphademux_debug);
#define GST_CAT_DEFAULT (codecalphademux_debug)

struct _GstCodecAlphaDemux
{
  GstElement parent;

  GstPad *sink_pad;
  GstPad *src_pad;
  GstPad *alpha_pad;

  GstFlowCombiner *flow_combiner;
};

#define gst_codec_alpha_demux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstCodecAlphaDemux, gst_codec_alpha_demux,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (codecalphademux_debug, "codecalphademux", 0,
        "codecalphademux"));

GST_ELEMENT_REGISTER_DEFINE (codec_alpha_demux, "codecalphademux",
    GST_RANK_NONE, GST_TYPE_CODEC_ALPHA_DEMUX);

static GstStaticPadTemplate gst_codec_alpha_demux_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate gst_codec_alpha_demux_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate gst_codec_alpha_demux_alpha_template =
GST_STATIC_PAD_TEMPLATE ("alpha",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstFlowReturn
gst_codec_alpha_demux_chain (GstPad * pad, GstObject * object,
    GstBuffer * buffer)
{
  GstCodecAlphaDemux *self = GST_CODEC_ALPHA_DEMUX (object);
  GstVideoCodecAlphaMeta *alpha_meta =
      gst_buffer_get_video_codec_alpha_meta (buffer);
  GstBuffer *alpha_buffer = NULL;
  GstClockTime pts = GST_BUFFER_PTS (buffer);
  GstClockTime duration = GST_BUFFER_DURATION (buffer);
  GstFlowReturn ret = GST_FLOW_EOS;

  if (alpha_meta)
    alpha_buffer = gst_buffer_ref (alpha_meta->buffer);

  ret = gst_pad_push (self->src_pad, buffer);
  ret = gst_flow_combiner_update_flow (self->flow_combiner, ret);

  /* we lost ownership here */
  buffer = NULL;
  alpha_meta = NULL;

  if (alpha_buffer) {
    ret = gst_pad_push (self->alpha_pad, alpha_buffer);
  } else {
    gst_pad_push_event (self->alpha_pad, gst_event_new_gap (pts, duration));
    ret = GST_PAD_LAST_FLOW_RETURN (self->alpha_pad);
  }
  ret = gst_flow_combiner_update_flow (self->flow_combiner, ret);

  return ret;
}

static GstCaps *
gst_codec_alpha_demux_transform_caps (GstCaps * caps, gboolean codec_alpha)
{
  if (!caps)
    return NULL;

  caps = gst_caps_copy (caps);
  gst_caps_set_simple (caps, "codec-alpha", G_TYPE_BOOLEAN, codec_alpha, NULL);

  return caps;
}

static GstEvent *
gst_codec_alpha_demux_transform_caps_event (GstEvent * src_event)
{
  GstEvent *dst_event;
  GstCaps *caps;

  gst_event_parse_caps (src_event, &caps);

  caps = gst_codec_alpha_demux_transform_caps (caps, FALSE);
  dst_event = gst_event_new_caps (caps);
  gst_event_set_seqnum (dst_event, gst_event_get_seqnum (src_event));

  gst_caps_unref (caps);
  gst_event_unref (src_event);
  return dst_event;
}

static gboolean
gst_codec_alpha_demux_sink_event (GstPad * sink_pad, GstObject * parent,
    GstEvent * event)
{
  GstCodecAlphaDemux *self = GST_CODEC_ALPHA_DEMUX (parent);

  switch (event->type) {
    case GST_EVENT_FLUSH_STOP:
      gst_flow_combiner_reset (self->flow_combiner);
      break;
    case GST_EVENT_CAPS:
      event = gst_codec_alpha_demux_transform_caps_event (event);
      break;
    default:
      break;
  }

  return gst_pad_event_default (sink_pad, parent, event);
}

static gboolean
gst_codec_alpha_demux_sink_query (GstPad * sink_pad, GstObject * parent,
    GstQuery * query)
{
  GstQuery *peer_query;
  GstCaps *caps;
  gboolean ret;

  switch (query->type) {
    case GST_QUERY_CAPS:
      gst_query_parse_caps (query, &caps);
      caps = gst_codec_alpha_demux_transform_caps (caps, FALSE);
      peer_query = gst_query_new_caps (caps);
      gst_clear_caps (&caps);
      break;
    case GST_QUERY_ACCEPT_CAPS:
      gst_query_parse_accept_caps (query, &caps);
      caps = gst_codec_alpha_demux_transform_caps (caps, FALSE);
      peer_query = gst_query_new_accept_caps (caps);
      gst_clear_caps (&caps);
      break;
    default:
      peer_query = query;
      break;
  }

  ret = gst_pad_query_default (sink_pad, parent, peer_query);
  if (!ret) {
    if (peer_query != query)
      gst_query_unref (peer_query);
    return FALSE;
  }

  switch (query->type) {
    case GST_QUERY_CAPS:
      gst_query_parse_caps_result (peer_query, &caps);
      caps = gst_caps_copy (caps);
      caps = gst_codec_alpha_demux_transform_caps (caps, TRUE);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      gst_query_unref (peer_query);
      break;
    case GST_QUERY_ACCEPT_CAPS:
    {
      gboolean result;
      gst_query_parse_accept_caps_result (peer_query, &result);
      gst_query_set_accept_caps_result (query, result);
      gst_query_unref (peer_query);
      break;
    }
    default:
      break;
  }


  return ret;
}

static void
gst_codec_alpha_demux_start (GstCodecAlphaDemux * self)
{
  gst_flow_combiner_reset (self->flow_combiner);
}

static GstStateChangeReturn
gst_codec_alpha_demux_change_state (GstElement * element,
    GstStateChange transition)
{
  GstCodecAlphaDemux *self = GST_CODEC_ALPHA_DEMUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_codec_alpha_demux_start (self);
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_codec_alpha_demux_dispose (GObject * object)
{
  GstCodecAlphaDemux *self = GST_CODEC_ALPHA_DEMUX (object);

  g_clear_object (&self->sink_pad);
  g_clear_object (&self->src_pad);
  g_clear_object (&self->alpha_pad);
  g_clear_pointer (&self->flow_combiner, gst_flow_combiner_unref);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_codec_alpha_demux_class_init (GstCodecAlphaDemuxClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  gst_element_class_set_static_metadata (element_class,
      "CODEC Alpha Demuxer", "Codec/Demuxer",
      "Extract and expose as a stream the CODEC alpha.",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_codec_alpha_demux_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_codec_alpha_demux_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_codec_alpha_demux_alpha_template);

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_codec_alpha_demux_change_state);

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_codec_alpha_demux_dispose);
}

static void
gst_codec_alpha_demux_init (GstCodecAlphaDemux * self)
{
  gst_element_create_all_pads (GST_ELEMENT (self));
  self->sink_pad = gst_element_get_static_pad (GST_ELEMENT (self), "sink");
  self->src_pad = gst_element_get_static_pad (GST_ELEMENT (self), "src");
  self->alpha_pad = gst_element_get_static_pad (GST_ELEMENT (self), "alpha");

  self->flow_combiner = gst_flow_combiner_new ();
  gst_flow_combiner_add_pad (self->flow_combiner, self->src_pad);
  gst_flow_combiner_add_pad (self->flow_combiner, self->alpha_pad);

  GST_PAD_SET_PROXY_CAPS (self->sink_pad);
  GST_PAD_SET_PROXY_CAPS (self->src_pad);
  GST_PAD_SET_PROXY_CAPS (self->alpha_pad);

  GST_PAD_SET_PROXY_SCHEDULING (self->sink_pad);
  GST_PAD_SET_PROXY_SCHEDULING (self->src_pad);
  GST_PAD_SET_PROXY_SCHEDULING (self->alpha_pad);

  gst_pad_set_chain_function (self->sink_pad, gst_codec_alpha_demux_chain);
  gst_pad_set_event_function (self->sink_pad, gst_codec_alpha_demux_sink_event);
  gst_pad_set_query_function (self->sink_pad, gst_codec_alpha_demux_sink_query);
}
