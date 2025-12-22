/*
 * mpegtsparse.c -
 * Copyright (C) 2007 Alessandro Decina
 *
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpegtsbase.h"
#include "mpegtsparse.h"
#include "gstmpegdesc.h"

/* latency in mseconds is maximum 100 ms between PCR */
#define TS_LATENCY 100

#define TABLE_ID_UNSET 0xFF
#define RUNNING_STATUS_RUNNING 4
#define SYNC_BYTE 0x47

GST_DEBUG_CATEGORY_STATIC (mpegts_parse_debug);
#define GST_CAT_DEFAULT mpegts_parse_debug

typedef struct _MpegTSParsePad MpegTSParsePad;

typedef struct
{
  MpegTSBaseProgram program;
  MpegTSParsePad *tspad;
} MpegTSParseProgram;

struct _MpegTSParsePad
{
  GstPad *pad;

  /* the program number that the peer wants on this pad */
  gint program_number;
  MpegTSParseProgram *program;

  /* set to FALSE before a push and TRUE after */
  gboolean pushed;

  /* the return of the latest push */
  GstFlowReturn flow_return;

  MpegTSParse2Adapter ts_adapter;
};

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

static GstStaticPadTemplate program_template =
GST_STATIC_PAD_TEMPLATE ("program_%u", GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

enum
{
  PROP_0,
  PROP_SET_TIMESTAMPS,
  PROP_SMOOTHING_LATENCY,
  PROP_PCR_PID,
  PROP_ALIGNMENT,
  PROP_SPLIT_ON_RAI,
  /* FILL ME */
};

static void mpegts_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void mpegts_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
mpegts_parse_program_started (MpegTSBase * base, MpegTSBaseProgram * program);
static void
mpegts_parse_program_stopped (MpegTSBase * base, MpegTSBaseProgram * program);

static GstFlowReturn
mpegts_parse_push (MpegTSBase * base, MpegTSPacketizerPacket * packet,
    GstMpegtsSection * section);
static void mpegts_parse_inspect_packet (MpegTSBase * base,
    MpegTSPacketizerPacket * packet);
static GstFlowReturn mpegts_parse_have_buffer (MpegTSBase * base,
    GstBuffer * buffer);

static MpegTSParsePad *mpegts_parse_create_tspad (MpegTSParse2 * parse,
    const gchar * name);
static void mpegts_parse_destroy_tspad (MpegTSParse2 * parse,
    MpegTSParsePad * tspad);

static void mpegts_parse_pad_removed (GstElement * element, GstPad * pad);
static GstPad *mpegts_parse_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void mpegts_parse_release_pad (GstElement * element, GstPad * pad);
static gboolean mpegts_parse_src_pad_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean push_event (MpegTSBase * base, GstEvent * event);

#define mpegts_parse_parent_class parent_class
G_DEFINE_TYPE (MpegTSParse2, mpegts_parse, GST_TYPE_MPEGTS_BASE);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (tsparse, "tsparse",
    GST_RANK_NONE, GST_TYPE_MPEGTS_PARSE,
    GST_DEBUG_CATEGORY_INIT (mpegts_parse_debug, "tsparse", 0,
        "MPEG transport stream parser"));

static void mpegts_parse_reset (MpegTSBase * base);
static GstFlowReturn mpegts_parse_input_done (MpegTSBase * base);
static GstFlowReturn
drain_pending_buffers (MpegTSParse2 * parse, gboolean drain_all);

static void
mpegts_parse_finalize (GObject * object)
{
  MpegTSParse2 *parse = (MpegTSParse2 *) object;

  gst_flow_combiner_free (parse->flowcombiner);

  gst_adapter_clear (parse->ts_adapter.adapter);
  g_object_unref (parse->ts_adapter.adapter);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
mpegts_parse_class_init (MpegTSParse2Class * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) (klass);
  GstElementClass *element_class;
  MpegTSBaseClass *ts_class;

  gobject_class->set_property = mpegts_parse_set_property;
  gobject_class->get_property = mpegts_parse_get_property;
  gobject_class->finalize = mpegts_parse_finalize;

  g_object_class_install_property (gobject_class, PROP_SET_TIMESTAMPS,
      g_param_spec_boolean ("set-timestamps",
          "Timestamp (or re-timestamp) the output stream",
          "If set, timestamps will be set on the output buffers using "
          "PCRs and smoothed over the smoothing-latency period", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SMOOTHING_LATENCY,
      g_param_spec_uint ("smoothing-latency", "Smoothing Latency",
          "Additional latency in microseconds for smoothing jitter in input timestamps on live capture",
          0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PCR_PID,
      g_param_spec_int ("pcr-pid", "PID containing PCR",
          "Set the PID to use for PCR values (-1 for auto)",
          -1, G_MAXINT, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ALIGNMENT,
      g_param_spec_uint ("alignment", "Alignment",
          "Number of packets per buffer (padded with dummy packets on EOS) (0 = auto)",
          0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SPLIT_ON_RAI,
      g_param_spec_boolean ("split-on-rai", "Split on RAI",
          "If set, buffers sized smaller than the alignment will be sent "
          "so that RAI packets are at the start of a new buffer", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  element_class = GST_ELEMENT_CLASS (klass);
  element_class->pad_removed = mpegts_parse_pad_removed;
  element_class->request_new_pad = mpegts_parse_request_new_pad;
  element_class->release_pad = mpegts_parse_release_pad;

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class, &program_template);

  gst_element_class_set_static_metadata (element_class,
      "MPEG transport stream parser", "Codec/Parser",
      "Parses MPEG2 transport streams",
      "Alessandro Decina <alessandro@nnva.org>, "
      "Zaheer Abbas Merali <zaheerabbas at merali dot org>");

  ts_class = GST_MPEGTS_BASE_CLASS (klass);
  ts_class->push = GST_DEBUG_FUNCPTR (mpegts_parse_push);
  ts_class->push_event = GST_DEBUG_FUNCPTR (push_event);
  ts_class->program_started = GST_DEBUG_FUNCPTR (mpegts_parse_program_started);
  ts_class->program_stopped = GST_DEBUG_FUNCPTR (mpegts_parse_program_stopped);
  ts_class->reset = GST_DEBUG_FUNCPTR (mpegts_parse_reset);
  ts_class->input_done = GST_DEBUG_FUNCPTR (mpegts_parse_input_done);
  ts_class->inspect_packet = GST_DEBUG_FUNCPTR (mpegts_parse_inspect_packet);
}

static void
mpegts_parse_init (MpegTSParse2 * parse)
{
  MpegTSBase *base = (MpegTSBase *) parse;

  base->program_size = sizeof (MpegTSParseProgram);
  base->push_data = TRUE;
  base->push_section = TRUE;
  base->push_unknown = TRUE;

  parse->user_pcr_pid = parse->pcr_pid = -1;

  parse->flowcombiner = gst_flow_combiner_new ();

  parse->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_flow_combiner_add_pad (parse->flowcombiner, parse->srcpad);
  parse->first = TRUE;
  gst_pad_set_query_function (parse->srcpad,
      GST_DEBUG_FUNCPTR (mpegts_parse_src_pad_query));
  gst_element_add_pad (GST_ELEMENT (parse), parse->srcpad);

  parse->have_group_id = FALSE;
  parse->group_id = G_MAXUINT;

  parse->ts_adapter.adapter = gst_adapter_new ();
  parse->ts_adapter.packets_in_adapter = 0;
  parse->ts_adapter.first_is_keyframe = TRUE;
  parse->alignment = 0;
  parse->is_eos = FALSE;
  parse->header = 0;
  parse->split_on_rai = FALSE;
}

static void
mpegts_parse_reset (MpegTSBase * base)
{
  MpegTSParse2 *parse = (MpegTSParse2 *) base;

  /* Set the various know PIDs we are interested in */

  /* CAT */
  MPEGTS_BIT_SET (base->known_psi, 1);
  /* NIT, ST */
  MPEGTS_BIT_SET (base->known_psi, 0x10);
  /* SDT, BAT, ST */
  MPEGTS_BIT_SET (base->known_psi, 0x11);
  /* EIT, ST, CIT (TS 102 323) */
  MPEGTS_BIT_SET (base->known_psi, 0x12);
  /* RST, ST */
  MPEGTS_BIT_SET (base->known_psi, 0x13);
  /* RNT (TS 102 323) */
  MPEGTS_BIT_SET (base->known_psi, 0x16);
  /* inband signalling */
  MPEGTS_BIT_SET (base->known_psi, 0x1c);
  /* measurement */
  MPEGTS_BIT_SET (base->known_psi, 0x1d);
  /* DIT */
  MPEGTS_BIT_SET (base->known_psi, 0x1e);
  /* SIT */
  MPEGTS_BIT_SET (base->known_psi, 0x1f);

  parse->first = TRUE;
  parse->have_group_id = FALSE;
  parse->group_id = G_MAXUINT;

  g_list_free_full (parse->pending_buffers, (GDestroyNotify) gst_buffer_unref);
  parse->pending_buffers = NULL;

  parse->current_pcr = GST_CLOCK_TIME_NONE;
  parse->previous_pcr = GST_CLOCK_TIME_NONE;
  parse->base_pcr = GST_CLOCK_TIME_NONE;
  parse->bytes_since_pcr = 0;
  parse->pcr_pid = parse->user_pcr_pid;
  parse->ts_offset = 0;

  gst_adapter_clear (parse->ts_adapter.adapter);
  parse->ts_adapter.packets_in_adapter = 0;
  parse->ts_adapter.first_is_keyframe = TRUE;
  parse->is_eos = FALSE;
  parse->header = 0;
}

static void
mpegts_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  MpegTSParse2 *parse = (MpegTSParse2 *) object;

  switch (prop_id) {
    case PROP_SET_TIMESTAMPS:
      parse->set_timestamps = g_value_get_boolean (value);
      break;
    case PROP_SMOOTHING_LATENCY:
      parse->smoothing_latency = GST_USECOND * g_value_get_uint (value);
      mpegts_packetizer_set_pcr_discont_threshold (GST_MPEGTS_BASE
          (parse)->packetizer, parse->smoothing_latency);
      break;
    case PROP_PCR_PID:
      parse->pcr_pid = parse->user_pcr_pid = g_value_get_int (value);
      break;
    case PROP_ALIGNMENT:
      parse->alignment = g_value_get_uint (value);
      break;
    case PROP_SPLIT_ON_RAI:
      parse->split_on_rai = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
mpegts_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  MpegTSParse2 *parse = (MpegTSParse2 *) object;

  switch (prop_id) {
    case PROP_SET_TIMESTAMPS:
      g_value_set_boolean (value, parse->set_timestamps);
      break;
    case PROP_SMOOTHING_LATENCY:
      g_value_set_uint (value, parse->smoothing_latency / GST_USECOND);
      break;
    case PROP_PCR_PID:
      g_value_set_int (value, parse->pcr_pid);
      break;
    case PROP_ALIGNMENT:
      g_value_set_uint (value, parse->alignment);
      break;
    case PROP_SPLIT_ON_RAI:
      g_value_set_boolean (value, parse->split_on_rai);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gboolean
prepare_src_pad (MpegTSBase * base, MpegTSParse2 * parse)
{
  GstEvent *event;
  gchar *stream_id;
  GstCaps *caps;

  if (!parse->first)
    return TRUE;

  /* If there's no packet_size yet, we can't set caps yet */
  if (G_UNLIKELY (base->packetizer->packet_size == 0))
    return FALSE;

  stream_id =
      gst_pad_create_stream_id (parse->srcpad, GST_ELEMENT_CAST (base),
      "multi-program");

  event =
      gst_pad_get_sticky_event (parse->parent.sinkpad, GST_EVENT_STREAM_START,
      0);
  if (event) {
    if (gst_event_parse_group_id (event, &parse->group_id))
      parse->have_group_id = TRUE;
    else
      parse->have_group_id = FALSE;
    gst_event_unref (event);
  } else if (!parse->have_group_id) {
    parse->have_group_id = TRUE;
    parse->group_id = gst_util_group_id_next ();
  }
  event = gst_event_new_stream_start (stream_id);
  if (parse->have_group_id)
    gst_event_set_group_id (event, parse->group_id);

  gst_pad_push_event (parse->srcpad, event);
  g_free (stream_id);

  caps = gst_caps_new_simple ("video/mpegts",
      "systemstream", G_TYPE_BOOLEAN, TRUE,
      "packetsize", G_TYPE_INT, base->packetizer->packet_size, NULL);

  gst_pad_set_caps (parse->srcpad, caps);
  gst_caps_unref (caps);

  /* If setting output timestamps, ensure that the output segment is TIME */
  if (parse->set_timestamps == FALSE || base->segment.format == GST_FORMAT_TIME)
    /* Just use the upstream segment */
    base->out_segment = base->segment;
  else {
    GstSegment *seg = &base->out_segment;
    gst_segment_init (seg, GST_FORMAT_TIME);
    GST_DEBUG_OBJECT (parse,
        "Generating time output segment %" GST_SEGMENT_FORMAT, seg);
  }
  gst_pad_push_event (parse->srcpad,
      gst_event_new_segment (&base->out_segment));

  parse->first = FALSE;

  return TRUE;
}

static gboolean
push_event (MpegTSBase * base, GstEvent * event)
{
  MpegTSParse2 *parse = (MpegTSParse2 *) base;
  GList *tmp;

  if (G_UNLIKELY (parse->first)) {
    /* We will send the segment when really starting  */
    if (G_UNLIKELY (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT)) {
      gst_event_unref (event);
      return TRUE;
    }
    prepare_src_pad (base, parse);
  }
  if (G_UNLIKELY (GST_EVENT_TYPE (event) == GST_EVENT_EOS)) {
    gsize packet_size = base->packetizer->packet_size;

    parse->is_eos = TRUE;

    if (packet_size > 0 && parse->alignment > 0 &&
        parse->ts_adapter.packets_in_adapter > 0
        && parse->ts_adapter.packets_in_adapter < parse->alignment) {
      GstBuffer *buf;
      GstMapInfo map;
      guint8 *data;
      gint missing_packets =
          parse->alignment - parse->ts_adapter.packets_in_adapter;
      gint i = missing_packets;

      GST_DEBUG_OBJECT (parse, "Adding %d dummy packets", missing_packets);

      buf = gst_buffer_new_and_alloc (missing_packets * packet_size);
      gst_buffer_map (buf, &map, GST_MAP_READWRITE);
      data = map.data;

      for (; i > 0; i--) {
        gint offset;

        if (packet_size > MPEGTS_NORMAL_PACKETSIZE) {
          parse->header++;
          GST_WRITE_UINT32_BE (data, parse->header);
          offset = 4;
        } else {
          offset = 0;
        }
        GST_WRITE_UINT8 (data + offset, SYNC_BYTE);
        /* null packet PID */
        GST_WRITE_UINT16_BE (data + offset + 1, 0x1FFF);
        /* no adaptation field exists | continuity counter undefined */
        GST_WRITE_UINT8 (data + offset + 3, 0x10);
        /* payload */
        memset (data + offset + 4, 0, MPEGTS_NORMAL_PACKETSIZE - 4);
        data += packet_size;
      }
      gst_buffer_unmap (buf, &map);
      gst_adapter_push (parse->ts_adapter.adapter, buf);
      parse->ts_adapter.packets_in_adapter += missing_packets;
    }
    drain_pending_buffers (parse, TRUE);
  }

  if (G_UNLIKELY (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT))
    parse->ts_offset = 0;

  for (tmp = parse->srcpads; tmp; tmp = tmp->next) {
    GstPad *pad = (GstPad *) tmp->data;
    if (pad) {
      gst_event_ref (event);
      gst_pad_push_event (pad, event);
    }
  }

  gst_pad_push_event (parse->srcpad, event);

  return TRUE;
}

static MpegTSParsePad *
mpegts_parse_create_tspad (MpegTSParse2 * parse, const gchar * pad_name)
{
  GstPad *pad;
  MpegTSParsePad *tspad;

  pad = gst_pad_new_from_static_template (&program_template, pad_name);
  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (mpegts_parse_src_pad_query));

  /* create our wrapper */
  tspad = g_new0 (MpegTSParsePad, 1);
  tspad->pad = pad;
  tspad->program_number = -1;
  tspad->program = NULL;
  tspad->pushed = FALSE;
  tspad->flow_return = GST_FLOW_NOT_LINKED;
  tspad->ts_adapter.adapter = gst_adapter_new ();
  tspad->ts_adapter.packets_in_adapter = 0;
  tspad->ts_adapter.first_is_keyframe = TRUE;
  gst_pad_set_element_private (pad, tspad);
  gst_flow_combiner_add_pad (parse->flowcombiner, pad);

  return tspad;
}

static void
mpegts_parse_destroy_tspad (MpegTSParse2 * parse, MpegTSParsePad * tspad)
{
  gst_adapter_clear (tspad->ts_adapter.adapter);
  g_object_unref (tspad->ts_adapter.adapter);

  /* free the wrapper */
  g_free (tspad);
}

static void
mpegts_parse_pad_removed (GstElement * element, GstPad * pad)
{
  MpegTSParsePad *tspad;
  MpegTSParse2 *parse = GST_MPEGTS_PARSE (element);

  if (gst_pad_get_direction (pad) == GST_PAD_SINK)
    return;

  tspad = (MpegTSParsePad *) gst_pad_get_element_private (pad);
  if (tspad) {
    mpegts_parse_destroy_tspad (parse, tspad);

    parse->srcpads = g_list_remove_all (parse->srcpads, pad);
  }

  if (GST_ELEMENT_CLASS (parent_class)->pad_removed)
    GST_ELEMENT_CLASS (parent_class)->pad_removed (element, pad);
}

static GstPad *
mpegts_parse_request_new_pad (GstElement * element, GstPadTemplate * template,
    const gchar * padname, const GstCaps * caps)
{
  MpegTSParse2 *parse;
  MpegTSParsePad *tspad;
  MpegTSParseProgram *parseprogram;
  GstPad *pad;
  gint program_num = -1;
  GstEvent *event;
  gchar *stream_id;

  g_return_val_if_fail (template != NULL, NULL);
  g_return_val_if_fail (GST_IS_MPEGTS_PARSE (element), NULL);
  g_return_val_if_fail (padname != NULL, NULL);

  sscanf (padname + 8, "%d", &program_num);

  GST_DEBUG_OBJECT (element, "padname:%s, program:%d", padname, program_num);

  parse = GST_MPEGTS_PARSE (element);

  tspad = mpegts_parse_create_tspad (parse, padname);
  tspad->program_number = program_num;

  /* Find if the program is already active */
  parseprogram =
      (MpegTSParseProgram *) mpegts_base_get_program (GST_MPEGTS_BASE (parse),
      program_num);
  if (parseprogram) {
    tspad->program = parseprogram;
    parseprogram->tspad = tspad;
  }

  pad = tspad->pad;
  parse->srcpads = g_list_append (parse->srcpads, pad);

  gst_pad_set_active (pad, TRUE);

  stream_id = gst_pad_create_stream_id (pad, element, padname + 8);

  event =
      gst_pad_get_sticky_event (parse->parent.sinkpad, GST_EVENT_STREAM_START,
      0);
  if (event) {
    if (gst_event_parse_group_id (event, &parse->group_id))
      parse->have_group_id = TRUE;
    else
      parse->have_group_id = FALSE;
    gst_event_unref (event);
  } else if (!parse->have_group_id) {
    parse->have_group_id = TRUE;
    parse->group_id = gst_util_group_id_next ();
  }
  event = gst_event_new_stream_start (stream_id);
  if (parse->have_group_id)
    gst_event_set_group_id (event, parse->group_id);

  gst_pad_push_event (pad, event);
  g_free (stream_id);

  gst_element_add_pad (element, pad);

  return pad;
}

static GstBuffer *
mpegts_packet_to_buffer (MpegTSPacketizerPacket * packet)
{
  GstBuffer *buf =
      gst_buffer_new_and_alloc (packet->data_end - packet->data_start);
  gst_buffer_fill (buf, 0, packet->data_start,
      packet->data_end - packet->data_start);
  return buf;
}

static void
mpegts_parse_release_pad (GstElement * element, GstPad * pad)
{
  MpegTSParse2 *parse = (MpegTSParse2 *) element;

  gst_pad_set_active (pad, FALSE);
  /* we do the cleanup in GstElement::pad-removed */
  gst_flow_combiner_remove_pad (parse->flowcombiner, pad);
  gst_element_remove_pad (element, pad);
}

static GstFlowReturn
empty_adapter_into_pad (MpegTSParse2 * parse, MpegTSParse2Adapter * ts_adapter,
    GstPad * pad)
{
  GstAdapter *adapter = ts_adapter->adapter;
  GstBuffer *buf = NULL;
  guint64 pts_dist, dts_dist;
  GstClockTime pts, dts;
  gsize avail = gst_adapter_available (adapter);
  GstFlowReturn ret = GST_FLOW_OK;
  gsize offset;

  if (avail > 0)
    buf = gst_adapter_take_buffer (adapter, avail);
  /* Find the previous PTS/DTS. We also handle un-aligned input since want to
   * use the most recent PTS/DTS if present */
  offset = MIN (GST_MPEGTS_BASE (parse)->packetizer->packet_size, 188);
  pts = gst_adapter_prev_pts_at_offset (adapter, offset, &pts_dist);
  dts = gst_adapter_prev_dts_at_offset (adapter, offset, &dts_dist);

  GST_LOG_OBJECT (pad,
      "prev pts:%" GST_TIME_FORMAT " (dist:%" G_GUINT64_FORMAT ") dts:%"
      GST_TIME_FORMAT " (dist:%" G_GUINT64_FORMAT ")",
      GST_TIME_ARGS (pts), pts_dist, GST_TIME_ARGS (dts), dts_dist);

  ts_adapter->packets_in_adapter = 0;

  if (buf) {
    GST_BUFFER_PTS (buf) = pts;
    GST_BUFFER_DTS (buf) = dts;
    if (!ts_adapter->first_is_keyframe)
      gst_buffer_set_flags (buf, GST_BUFFER_FLAG_DELTA_UNIT);
    ret = gst_pad_push (pad, buf);
  }

  return ret;
}

static GstFlowReturn
enqueue_and_maybe_push_buffer (MpegTSParse2 * parse, GstPad * pad,
    MpegTSParse2Adapter * ts_adapter, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;

  if (buffer != NULL) {
    if (parse->alignment == 1) {
      ret = gst_pad_push (pad, buffer);
      ret = gst_flow_combiner_update_flow (parse->flowcombiner, ret);
    } else {
      if (!GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)
          && parse->split_on_rai) {
        ret = empty_adapter_into_pad (parse, ts_adapter, pad);
        ret = gst_flow_combiner_update_flow (parse->flowcombiner, ret);
      }
      gst_adapter_push (ts_adapter->adapter, buffer);
      ts_adapter->packets_in_adapter++;
      if (ts_adapter->packets_in_adapter == 1 && parse->split_on_rai) {
        ts_adapter->first_is_keyframe =
            !GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      }

      if (ts_adapter->packets_in_adapter == parse->alignment
          && ts_adapter->packets_in_adapter > 0) {
        ret = empty_adapter_into_pad (parse, ts_adapter, pad);
        ret = gst_flow_combiner_update_flow (parse->flowcombiner, ret);
      }
    }
  }

  return ret;
}

static GstFlowReturn
mpegts_parse_tspad_push_section (MpegTSParse2 * parse, MpegTSParsePad * tspad,
    GstMpegtsSection * section, MpegTSPacketizerPacket * packet,
    GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean to_push = TRUE;

  if (tspad->program_number != -1) {
    if (tspad->program) {
      /* we push all sections to all pads except PMTs which we
       * only push to pads meant to receive that program number */
      if (section->table_id == 0x02) {
        /* PMT */
        if (section->subtable_extension != tspad->program_number)
          to_push = FALSE;
      }
    } else if (section->table_id != 0x00) {
      /* there's a program filter on the pad but the PMT for the program has not
       * been parsed yet, ignore the pad until we get a PMT.
       * But we always allow PAT to go through */
      to_push = FALSE;
    }
  }

  GST_DEBUG_OBJECT (parse,
      "pushing section: %d program number: %d table_id: %d", to_push,
      tspad->program_number, section->table_id);

  if (to_push) {
    ret =
        enqueue_and_maybe_push_buffer (parse, tspad->pad,
        &tspad->ts_adapter, gst_buffer_ref (buf));
  }

  GST_LOG_OBJECT (parse, "Returning %s", gst_flow_get_name (ret));
  return ret;
}

static GstFlowReturn
mpegts_parse_tspad_push (MpegTSParse2 * parse, MpegTSParsePad * tspad,
    MpegTSPacketizerPacket * packet, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  MpegTSBaseProgram *bp = NULL;

  if (tspad->program_number != -1) {
    if (tspad->program)
      bp = (MpegTSBaseProgram *) tspad->program;
    else
      bp = mpegts_base_get_program ((MpegTSBase *) parse,
          tspad->program_number);
  }

  if (bp) {
    if (packet->pid == bp->pmt_pid || bp->streams == NULL
        || bp->streams[packet->pid]) {
      /* push if there's no filter or if the pid is in the filter */
      ret = gst_pad_push (tspad->pad, gst_buffer_ref (buf));
      ret = gst_flow_combiner_update_flow (parse->flowcombiner, ret);
    }
  }
  GST_DEBUG_OBJECT (parse, "Returning %s", gst_flow_get_name (ret));

  return ret;
}

static void
pad_clear_for_push (GstPad * pad, MpegTSParse2 * parse)
{
  MpegTSParsePad *tspad = (MpegTSParsePad *) gst_pad_get_element_private (pad);

  tspad->flow_return = GST_FLOW_NOT_LINKED;
  tspad->pushed = FALSE;
}

static GstFlowReturn
mpegts_parse_push (MpegTSBase * base, MpegTSPacketizerPacket * packet,
    GstMpegtsSection * section)
{
  MpegTSParse2 *parse = (MpegTSParse2 *) base;
  guint32 pads_cookie;
  gboolean done = FALSE;
  GstPad *pad = NULL;
  MpegTSParsePad *tspad;
  GstFlowReturn ret;
  GList *srcpads;
  GstBuffer *buf;

  GST_OBJECT_LOCK (parse);
  srcpads = parse->srcpads;

  /* clear tspad->pushed on pads */
  g_list_foreach (srcpads, (GFunc) pad_clear_for_push, parse);
  if (srcpads)
    ret = GST_FLOW_NOT_LINKED;
  else
    ret = GST_FLOW_OK;

  /* Get cookie and source pads list */
  pads_cookie = GST_ELEMENT_CAST (parse)->pads_cookie;
  if (G_LIKELY (srcpads)) {
    pad = GST_PAD_CAST (srcpads->data);
    g_object_ref (pad);
  }
  GST_OBJECT_UNLOCK (parse);

  buf = mpegts_packet_to_buffer (packet);
  if (parse->split_on_rai
      && !(packet->afc_flags & MPEGTS_AFC_RANDOM_ACCESS_FLAG)) {
    gst_buffer_set_flags (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  }

  /* Copy over input PTS/DTS (if present) */
  GST_BUFFER_DTS (buf) = base->packetizer->last_dts;
  GST_BUFFER_PTS (buf) = base->packetizer->last_pts;
  ret = mpegts_parse_have_buffer (base, gst_buffer_ref (buf));

  while (pad && !done) {
    tspad = gst_pad_get_element_private (pad);

    if (G_LIKELY (!tspad->pushed)) {
      if (section) {
        tspad->flow_return =
            mpegts_parse_tspad_push_section (parse, tspad, section, packet,
            buf);
      } else {
        tspad->flow_return =
            mpegts_parse_tspad_push (parse, tspad, packet, buf);
      }
      tspad->pushed = TRUE;

      if (G_UNLIKELY (tspad->flow_return != GST_FLOW_OK
              && tspad->flow_return != GST_FLOW_NOT_LINKED)) {
        /* return the error upstream */
        ret = tspad->flow_return;
        done = TRUE;
      }

    }

    if (ret == GST_FLOW_NOT_LINKED)
      ret = tspad->flow_return;

    g_object_unref (pad);

    if (G_UNLIKELY (!done)) {
      GST_OBJECT_LOCK (parse);
      if (G_UNLIKELY (pads_cookie != GST_ELEMENT_CAST (parse)->pads_cookie)) {
        /* resync */
        GST_DEBUG ("resync");
        pads_cookie = GST_ELEMENT_CAST (parse)->pads_cookie;
        srcpads = parse->srcpads;
      } else {
        GST_DEBUG ("getting next pad");
        /* Get next pad */
        srcpads = g_list_next (srcpads);
      }

      if (srcpads) {
        pad = GST_PAD_CAST (srcpads->data);
        g_object_ref (pad);
      } else
        done = TRUE;
      GST_OBJECT_UNLOCK (parse);
    }
  }

  gst_buffer_unref (buf);
  return ret;
}

static void
mpegts_parse_inspect_packet (MpegTSBase * base, MpegTSPacketizerPacket * packet)
{
  MpegTSParse2 *parse = GST_MPEGTS_PARSE (base);
  GST_LOG ("pid 0x%04x pusi:%d, afc:%d, cont:%d, payload:%p PCR %"
      G_GUINT64_FORMAT, packet->pid, packet->payload_unit_start_indicator,
      packet->scram_afc_cc & 0x30,
      FLAGS_CONTINUITY_COUNTER (packet->scram_afc_cc), packet->payload,
      packet->pcr);

  /* Store the PCR if desired */
  if (parse->current_pcr == GST_CLOCK_TIME_NONE &&
      packet->afc_flags & MPEGTS_AFC_PCR_FLAG) {
    /* Take this as the pcr_pid if set to auto-select */
    if (parse->pcr_pid == -1)
      parse->pcr_pid = packet->pid;
    /* Check the PCR-PID matches the program we want for multiple programs */
    if (parse->pcr_pid == packet->pid) {
      parse->current_pcr = mpegts_packetizer_pts_to_ts (base->packetizer,
          PCRTIME_TO_GSTTIME (packet->pcr), parse->pcr_pid);
      GST_DEBUG ("Got new PCR %" GST_TIME_FORMAT " raw %" G_GUINT64_FORMAT,
          GST_TIME_ARGS (parse->current_pcr), packet->pcr);
      if (parse->base_pcr == GST_CLOCK_TIME_NONE) {
        parse->base_pcr = parse->current_pcr;
      }
    }
  }
}

static GstClockTime
get_pending_timestamp_diff (MpegTSParse2 * parse)
{
  GList *l;
  GstClockTime first_ts, last_ts;

  if (parse->pending_buffers == NULL)
    return GST_CLOCK_TIME_NONE;

  l = g_list_last (parse->pending_buffers);
  first_ts = GST_BUFFER_PTS (l->data);
  if (first_ts == GST_CLOCK_TIME_NONE)
    return GST_CLOCK_TIME_NONE;

  l = g_list_first (parse->pending_buffers);
  last_ts = GST_BUFFER_PTS (l->data);
  if (last_ts == GST_CLOCK_TIME_NONE)
    return GST_CLOCK_TIME_NONE;

  return last_ts - first_ts;
}

static GstFlowReturn
drain_pending_buffers (MpegTSParse2 * parse, gboolean drain_all)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime start_ts;
  GstClockTime pcr = GST_CLOCK_TIME_NONE;
  GstClockTime pcr_diff = 0;
  gsize pcr_bytes, bytes_since_pcr, pos;
  GstBuffer *buffer;
  GList *l, *end = NULL;

  if (parse->pending_buffers == NULL)
    return GST_FLOW_OK;         /* Nothing to push */

  /*
   * There are 4 cases:
   *  1 We get a buffer with no PCR -> it's the head of the list
   *      -> Do nothing, unless it's EOS
   *  2 We get a buffer with a PCR, it's the first PCR we've seen, and belongs
   *    to the buffer at the head of the list
   *    -> Push any buffers in the list except the head,
   *       using a smoothing of their timestamps to land at the PCR
   *    -> store new PCR as the previous PCR, bytes_since_pcr = sizeof (buffer);
   *  3 It's EOS (drain_all == TRUE, current_pcr == NONE)
   *    -> Push any buffers in the list using a smoothing of their timestamps
   *       starting at the previous PCR or first TS
   *  4 We get a buffer with a PCR, and have a previous PCR
   *    -> If distance > smoothing_latency,
   *       output buffers except the last in the pending queue using
   *       piecewise-linear timestamps
   *    -> store new PCR as the previous PCR, bytes_since_pcr = sizeof (buffer);
   */

  /* Case 1 */
  if (!GST_CLOCK_TIME_IS_VALID (parse->current_pcr) && !drain_all)
    return GST_FLOW_OK;

  if (GST_CLOCK_TIME_IS_VALID (parse->current_pcr)) {
    pcr = parse->current_pcr;
    parse->current_pcr = GST_CLOCK_TIME_NONE;
  }

  /* The bytes of the last buffer are after the PCR */
  buffer = GST_BUFFER (g_list_nth_data (parse->pending_buffers, 0));
  bytes_since_pcr = gst_buffer_get_size (buffer);

  pcr_bytes = parse->bytes_since_pcr - bytes_since_pcr;

  if (!drain_all)
    end = g_list_first (parse->pending_buffers);

  /* Case 2 */
  if (!GST_CLOCK_TIME_IS_VALID (parse->previous_pcr)) {
    pcr_diff = get_pending_timestamp_diff (parse);

    /* Calculate the start_ts that ends at the end timestamp */
    start_ts = GST_CLOCK_TIME_NONE;
    if (end) {
      start_ts = GST_BUFFER_PTS (GST_BUFFER (end->data));
      if (start_ts > pcr_diff)
        start_ts -= pcr_diff;
    }
  } else if (drain_all) {       /* Case 3 */
    start_ts = parse->previous_pcr;
    pcr_diff = get_pending_timestamp_diff (parse);
  } else {                      /* Case 4 */
    start_ts = parse->previous_pcr;
    if (GST_CLOCK_TIME_IS_VALID (pcr) && pcr > start_ts)
      pcr_diff = GST_CLOCK_DIFF (start_ts, pcr);

    /* Make sure PCR observations are sufficiently far apart */
    if (drain_all == FALSE && pcr_diff < parse->smoothing_latency)
      return GST_FLOW_OK;
  }

  GST_INFO_OBJECT (parse, "Pushing buffers - startTS %" GST_TIME_FORMAT
      " duration %" GST_TIME_FORMAT " %" G_GSIZE_FORMAT " bytes",
      GST_TIME_ARGS (start_ts), GST_TIME_ARGS (pcr_diff), pcr_bytes);

  /* Now, push buffers out pacing timestamps over pcr_diff time and pcr_bytes */
  pos = 0;
  l = g_list_last (parse->pending_buffers);
  while (l != end) {
    GList *p;
    GstClockTime out_ts = start_ts;

    buffer = gst_buffer_make_writable (GST_BUFFER (l->data));

    if (out_ts != GST_CLOCK_TIME_NONE && pcr_diff != GST_CLOCK_TIME_NONE &&
        pcr_bytes && pos)
      out_ts += gst_util_uint64_scale (pcr_diff, pos, pcr_bytes);

    pos += gst_buffer_get_size (buffer);

    GST_DEBUG_OBJECT (parse,
        "InputTS %" GST_TIME_FORMAT " out %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_PTS (buffer)), GST_TIME_ARGS (out_ts));

    GST_BUFFER_PTS (buffer) = out_ts + parse->ts_offset;
    GST_BUFFER_DTS (buffer) = out_ts + parse->ts_offset;
    if (ret == GST_FLOW_OK) {
      ret =
          enqueue_and_maybe_push_buffer (parse, parse->srcpad,
          &parse->ts_adapter, buffer);
    } else {
      gst_buffer_unref (buffer);
    }

    /* Free this list node and move to the next */
    p = g_list_previous (l);
    parse->pending_buffers = g_list_delete_link (parse->pending_buffers, l);
    l = p;
  }

  if (parse->is_eos) {
    empty_adapter_into_pad (parse, &parse->ts_adapter, parse->srcpad);
  }

  parse->pending_buffers = end;
  parse->bytes_since_pcr = bytes_since_pcr;
  parse->previous_pcr = pcr;
  return ret;
}

static GstFlowReturn
mpegts_parse_have_buffer (MpegTSBase * base, GstBuffer * buffer)
{
  MpegTSParse2 *parse = GST_MPEGTS_PARSE (base);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (parse, "Received buffer %" GST_PTR_FORMAT, buffer);

  /* Assume all packets have equal size */
  if (parse->alignment > 0 &&
      base->packetizer->packet_size != MPEGTS_NORMAL_PACKETSIZE) {
    GstMapInfo map;
    guint8 *data;

    gst_buffer_map (buffer, &map, GST_MAP_READ);
    data = map.data;

    parse->header = GST_READ_UINT32_BE (data);
    gst_buffer_unmap (buffer, &map);
  }

  if (parse->current_pcr != GST_CLOCK_TIME_NONE) {
    GST_DEBUG_OBJECT (parse,
        "InputTS %" GST_TIME_FORMAT " PCR %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
        GST_TIME_ARGS (parse->current_pcr));
  }

  if (parse->set_timestamps || parse->first) {
    parse->pending_buffers = g_list_prepend (parse->pending_buffers, buffer);
    parse->bytes_since_pcr += gst_buffer_get_size (buffer);
    buffer = NULL;
  }

  if (!prepare_src_pad (base, parse))
    return GST_FLOW_OK;

  if (parse->pending_buffers != NULL) {
    /* Don't keep pending_buffers if not setting output timestamps */
    gboolean drain_all = (parse->set_timestamps == FALSE);
    ret = drain_pending_buffers (parse, drain_all);
    if (ret != GST_FLOW_OK) {
      if (buffer)
        gst_buffer_unref (buffer);
      return ret;
    }
  }

  ret =
      enqueue_and_maybe_push_buffer (parse, parse->srcpad,
      &parse->ts_adapter, buffer);
  return ret;
}

static void
empty_pad (GstPad * pad, MpegTSParse2 * parse)
{
  MpegTSParsePad *tspad = (MpegTSParsePad *) gst_pad_get_element_private (pad);
  GstFlowReturn ret;
  ret = empty_adapter_into_pad (parse, &tspad->ts_adapter, tspad->pad);
  ret = gst_flow_combiner_update_flow (parse->flowcombiner, ret);
}

static GstFlowReturn
mpegts_parse_input_done (MpegTSBase * base)
{
  MpegTSParse2 *parse = GST_MPEGTS_PARSE (base);
  GstFlowReturn ret = GST_FLOW_OK;

  if (!prepare_src_pad (base, parse))
    return GST_FLOW_OK;

  if (parse->alignment == 0) {
    ret = empty_adapter_into_pad (parse, &parse->ts_adapter, parse->srcpad);
    ret = gst_flow_combiner_update_flow (parse->flowcombiner, ret);
    g_list_foreach (parse->srcpads, (GFunc) empty_pad, parse);
  }
  return ret;
}

static MpegTSParsePad *
find_pad_for_program (MpegTSParse2 * parse, guint program_number)
{
  GList *tmp;

  for (tmp = parse->srcpads; tmp; tmp = tmp->next) {
    MpegTSParsePad *tspad = gst_pad_get_element_private ((GstPad *) tmp->data);

    if (tspad->program_number == program_number)
      return tspad;
  }

  return NULL;
}

static void
mpegts_parse_program_started (MpegTSBase * base, MpegTSBaseProgram * program)
{
  MpegTSParse2 *parse = GST_MPEGTS_PARSE (base);
  MpegTSParseProgram *parseprogram = (MpegTSParseProgram *) program;
  MpegTSParsePad *tspad;

  /* If we have a request pad for that program, activate it */
  tspad = find_pad_for_program (parse, program->program_number);

  if (tspad) {
    tspad->program = parseprogram;
    parseprogram->tspad = tspad;
  }
}

static void
mpegts_parse_program_stopped (MpegTSBase * base, MpegTSBaseProgram * program)
{
  MpegTSParse2 *parse = GST_MPEGTS_PARSE (base);
  MpegTSParseProgram *parseprogram = (MpegTSParseProgram *) program;
  MpegTSParsePad *tspad;

  /* If we have a request pad for that program, activate it */
  tspad = find_pad_for_program (parse, program->program_number);

  if (tspad) {
    tspad->program = NULL;
    parseprogram->tspad = NULL;
  }

  parse->pcr_pid = -1;
  parse->ts_offset += parse->current_pcr - parse->base_pcr;
  parse->base_pcr = GST_CLOCK_TIME_NONE;
}

static gboolean
mpegts_parse_src_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  MpegTSParse2 *parse = GST_MPEGTS_PARSE (parent);
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      if ((res = gst_pad_peer_query (((MpegTSBase *) parse)->sinkpad, query))) {
        gboolean is_live;
        GstClockTime min_latency, max_latency;

        gst_query_parse_latency (query, &is_live, &min_latency, &max_latency);
        if (is_live) {
          GstClockTime extra_latency = TS_LATENCY * GST_MSECOND;
          if (parse->set_timestamps) {
            extra_latency = MAX (extra_latency, parse->smoothing_latency);
          }
          min_latency += extra_latency;
          if (max_latency != GST_CLOCK_TIME_NONE)
            max_latency += extra_latency;
        }

        gst_query_set_latency (query, is_live, min_latency, max_latency);
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
  }
  return res;
}
