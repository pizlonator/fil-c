/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#include "gstavtpbasepayload.h"

GST_DEBUG_CATEGORY_STATIC (avtpbasepayload_debug);
#define GST_CAT_DEFAULT (avtpbasepayload_debug)

#define DEFAULT_STREAMID 0xAABBCCDDEEFF0000
#define DEFAULT_MTT 50000000
#define DEFAULT_TU 1000000
#define DEFAULT_PROCESSING_DEADLINE (20 * GST_MSECOND)

enum
{
  PROP_0,
  PROP_STREAMID,
  PROP_MTT,
  PROP_TU,
  PROP_PROCESSING_DEADLINE,
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-avtp")
    );

static void gst_avtp_base_payload_class_init (GstAvtpBasePayloadClass * klass);
static void gst_avtp_base_payload_init (GstAvtpBasePayload * avtpbasepayload,
    gpointer g_class);

static void gst_avtp_base_payload_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_avtp_base_payload_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_avtp_base_payload_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);

GType
gst_avtp_base_payload_get_type (void)
{
  static GType avtpbasepayload_type = 0;

  if (g_once_init_enter ((gsize *) & avtpbasepayload_type)) {
    static const GTypeInfo avtpbasepayload_info = {
      sizeof (GstAvtpBasePayloadClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_avtp_base_payload_class_init,
      NULL,
      NULL,
      sizeof (GstAvtpBasePayload),
      0,
      (GInstanceInitFunc) gst_avtp_base_payload_init,
    };
    GType _type;

    _type = g_type_register_static (GST_TYPE_ELEMENT, "GstAvtpBasePayload",
        &avtpbasepayload_info, G_TYPE_FLAG_ABSTRACT);

    g_once_init_leave ((gsize *) & avtpbasepayload_type, _type);
  }
  return avtpbasepayload_type;
}

static void
gst_avtp_base_payload_class_init (GstAvtpBasePayloadClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gst_avtp_base_payload_set_property;
  object_class->get_property = gst_avtp_base_payload_get_property;

  g_object_class_install_property (object_class, PROP_STREAMID,
      g_param_spec_uint64 ("streamid", "Stream ID",
          "Stream ID associated with the AVTPDU", 0, G_MAXUINT64,
          DEFAULT_STREAMID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_MTT,
      g_param_spec_uint ("mtt", "Maximum Transit Time",
          "Maximum Transit Time (MTT) in nanoseconds", 0,
          G_MAXUINT, DEFAULT_MTT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TU,
      g_param_spec_uint ("tu", "Timing Uncertainty",
          "Timing Uncertainty (TU) in nanoseconds", 0,
          G_MAXUINT, DEFAULT_TU, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROCESSING_DEADLINE,
      g_param_spec_uint64 ("processing-deadline", "Processing deadline",
          "Maximum amount of time (in ns) the pipeline can take for processing the buffer",
          0, G_MAXUINT64, DEFAULT_PROCESSING_DEADLINE, G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  klass->chain = NULL;
  klass->sink_event = GST_DEBUG_FUNCPTR (gst_avtp_base_payload_sink_event);

  GST_DEBUG_CATEGORY_INIT (avtpbasepayload_debug, "avtpbasepayload", 0,
      "Base class for AVTP payloaders");

  gst_type_mark_as_plugin_api (GST_TYPE_AVTP_BASE_PAYLOAD, 0);
}

static void
gst_avtp_base_payload_init (GstAvtpBasePayload * avtpbasepayload,
    gpointer g_class)
{
  GstPadTemplate *templ;
  GstElement *element = GST_ELEMENT (avtpbasepayload);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstAvtpBasePayloadClass *avtpbasepayload_class =
      GST_AVTP_BASE_PAYLOAD_CLASS (g_class);

  g_assert (avtpbasepayload_class->chain != NULL);

  avtpbasepayload->srcpad = gst_pad_new_from_static_template (&src_template,
      "src");
  gst_element_add_pad (element, avtpbasepayload->srcpad);

  templ = gst_element_class_get_pad_template (element_class, "sink");
  g_assert (templ != NULL);
  avtpbasepayload->sinkpad = gst_pad_new_from_template (templ, "sink");
  gst_pad_set_chain_function (avtpbasepayload->sinkpad,
      avtpbasepayload_class->chain);
  gst_pad_set_event_function (avtpbasepayload->sinkpad,
      avtpbasepayload_class->sink_event);
  gst_element_add_pad (element, avtpbasepayload->sinkpad);

  avtpbasepayload->streamid = DEFAULT_STREAMID;
  avtpbasepayload->mtt = DEFAULT_MTT;
  avtpbasepayload->tu = DEFAULT_TU;
  avtpbasepayload->processing_deadline = DEFAULT_PROCESSING_DEADLINE;

  avtpbasepayload->latency = GST_CLOCK_TIME_NONE;
  avtpbasepayload->seqnum = 0;
  gst_segment_init (&avtpbasepayload->segment, GST_FORMAT_UNDEFINED);
}

static void
gst_avtp_base_payload_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (object);

  GST_DEBUG_OBJECT (avtpbasepayload, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_STREAMID:
      avtpbasepayload->streamid = g_value_get_uint64 (value);
      break;
    case PROP_MTT:
      avtpbasepayload->mtt = g_value_get_uint (value);
      break;
    case PROP_TU:
      avtpbasepayload->tu = g_value_get_uint (value);
      break;
    case PROP_PROCESSING_DEADLINE:
      avtpbasepayload->processing_deadline = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avtp_base_payload_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (object);

  GST_DEBUG_OBJECT (avtpbasepayload, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_STREAMID:
      g_value_set_uint64 (value, avtpbasepayload->streamid);
      break;
    case PROP_MTT:
      g_value_set_uint (value, avtpbasepayload->mtt);
      break;
    case PROP_TU:
      g_value_set_uint (value, avtpbasepayload->tu);
      break;
    case PROP_PROCESSING_DEADLINE:
      g_value_set_uint64 (value, avtpbasepayload->processing_deadline);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_avtp_base_payload_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (parent);

  GST_DEBUG_OBJECT (avtpbasepayload, "event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &avtpbasepayload->segment);
      /* Fall through */
    default:
      return gst_pad_event_default (pad, parent, event);
  }
}

GstClockTime
gst_avtp_base_payload_calc_ptime (GstAvtpBasePayload * avtpbasepayload,
    GstBuffer * buffer)
{
  GstClockTime base_time, running_time;

  g_assert (GST_BUFFER_PTS (buffer) != GST_CLOCK_TIME_NONE);

  if (G_UNLIKELY (avtpbasepayload->latency == GST_CLOCK_TIME_NONE)) {
    GstQuery *query;

    query = gst_query_new_latency ();
    if (!gst_pad_peer_query (avtpbasepayload->sinkpad, query))
      return GST_CLOCK_TIME_NONE;
    gst_query_parse_latency (query, NULL, &avtpbasepayload->latency, NULL);
    gst_query_unref (query);

    GST_DEBUG_OBJECT (avtpbasepayload, "latency %" GST_TIME_FORMAT,
        GST_TIME_ARGS (avtpbasepayload->latency));
  }

  base_time = gst_element_get_base_time (GST_ELEMENT (avtpbasepayload));

  running_time = gst_segment_to_running_time (&avtpbasepayload->segment,
      avtpbasepayload->segment.format, GST_BUFFER_PTS (buffer));

  return base_time + running_time + avtpbasepayload->latency +
      avtpbasepayload->processing_deadline + avtpbasepayload->mtt +
      avtpbasepayload->tu;
}
