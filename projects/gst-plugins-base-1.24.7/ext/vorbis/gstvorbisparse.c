/* GStreamer
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) 2006 Andy Wingo <wingo@pobox.com>
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
 * SECTION:element-vorbisparse
 * @title: vorbisparse
 * @see_also: vorbisdec, oggdemux, theoraparse
 *
 * The vorbisparse element will parse the header packets of the Vorbis
 * stream and put them as the streamheader in the caps. This is used in the
 * multifdsink case where you want to stream live vorbis streams to multiple
 * clients, each client has to receive the streamheaders first before they can
 * consume the vorbis packets.
 *
 * This element also makes sure that the buffers that it pushes out are properly
 * timestamped and that their offset and offset_end are set. The buffers that
 * vorbisparse outputs have all of the metadata that oggmux expects to receive,
 * which allows you to (for example) remux an ogg/vorbis file.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=sine.ogg ! oggdemux ! vorbisparse ! fakesink
 * ]|
 *  This pipeline shows that the streamheader is set in the caps, and that each
 * buffer has the timestamp, duration, offset, and offset_end set.
 * |[
 * gst-launch-1.0 filesrc location=sine.ogg ! oggdemux ! vorbisparse \
 *            ! oggmux ! filesink location=sine-remuxed.ogg
 * ]|
 *  This pipeline shows remuxing. sine-remuxed.ogg might not be exactly the same
 * as sine.ogg, but they should produce exactly the same decoded data.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstvorbiselements.h"
#include "gstvorbisparse.h"

GST_DEBUG_CATEGORY_STATIC (vorbisparse_debug);
#define GST_CAT_DEFAULT vorbisparse_debug

static GstStaticPadTemplate vorbis_parse_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-vorbis")
    );

static GstStaticPadTemplate vorbis_parse_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-vorbis")
    );

#define gst_vorbis_parse_parent_class parent_class
G_DEFINE_TYPE (GstVorbisParse, gst_vorbis_parse, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vorbisparse, "vorbisparse",
    GST_RANK_NONE, GST_TYPE_VORBIS_PARSE,
    GST_DEBUG_CATEGORY_INIT (vorbisparse_debug, "vorbisparse", 0,
        "vorbis parsing element");
    vorbis_element_init (plugin));

static GstFlowReturn vorbis_parse_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static GstStateChangeReturn vorbis_parse_change_state (GstElement * element,
    GstStateChange transition);
static gboolean vorbis_parse_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean vorbis_parse_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean vorbis_parse_convert (GstPad * pad, GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);
static GstFlowReturn vorbis_parse_parse_packet (GstVorbisParse * parse,
    GstBuffer * buf);

static void
gst_vorbis_parse_class_init (GstVorbisParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gstelement_class->change_state = vorbis_parse_change_state;

  gst_element_class_add_static_pad_template (gstelement_class,
      &vorbis_parse_src_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &vorbis_parse_sink_factory);
  gst_element_class_set_static_metadata (gstelement_class, "VorbisParse",
      "Codec/Parser/Audio", "parse raw vorbis streams",
      "Thomas Vander Stichele <thomas at apestaart dot org>");

  klass->parse_packet = GST_DEBUG_FUNCPTR (vorbis_parse_parse_packet);
}

static void
gst_vorbis_parse_init (GstVorbisParse * parse)
{
  parse->sinkpad =
      gst_pad_new_from_static_template (&vorbis_parse_sink_factory, "sink");
  gst_pad_set_chain_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (vorbis_parse_chain));
  gst_pad_set_event_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (vorbis_parse_sink_event));
  gst_element_add_pad (GST_ELEMENT (parse), parse->sinkpad);

  parse->srcpad =
      gst_pad_new_from_static_template (&vorbis_parse_src_factory, "src");
  gst_pad_set_query_function (parse->srcpad,
      GST_DEBUG_FUNCPTR (vorbis_parse_src_query));
  gst_element_add_pad (GST_ELEMENT (parse), parse->srcpad);
}

static void
vorbis_parse_set_header_on_caps (GstVorbisParse * parse, GstCaps * caps)
{
  GstBuffer *buf1, *buf2, *buf3;
  GstStructure *structure;
  GValue array = { 0 };
  GValue value = { 0 };

  g_assert (parse);
  g_assert (parse->streamheader);
  g_assert (parse->streamheader->next);
  g_assert (parse->streamheader->next->next);
  buf1 = parse->streamheader->data;
  g_assert (buf1);
  buf2 = parse->streamheader->next->data;
  g_assert (buf2);
  buf3 = parse->streamheader->next->next->data;
  g_assert (buf3);

  structure = gst_caps_get_structure (caps, 0);

  /* mark buffers */
  GST_BUFFER_FLAG_SET (buf1, GST_BUFFER_FLAG_HEADER);
  GST_BUFFER_FLAG_SET (buf2, GST_BUFFER_FLAG_HEADER);
  GST_BUFFER_FLAG_SET (buf3, GST_BUFFER_FLAG_HEADER);

  /* put buffers in a fixed list */
  g_value_init (&array, GST_TYPE_ARRAY);
  g_value_init (&value, GST_TYPE_BUFFER);
  gst_value_set_buffer (&value, buf1);
  gst_value_array_append_value (&array, &value);
  g_value_unset (&value);
  g_value_init (&value, GST_TYPE_BUFFER);
  gst_value_set_buffer (&value, buf2);
  gst_value_array_append_value (&array, &value);
  g_value_unset (&value);
  g_value_init (&value, GST_TYPE_BUFFER);
  gst_value_set_buffer (&value, buf3);
  gst_value_array_append_value (&array, &value);
  gst_structure_take_value (structure, "streamheader", &array);
  g_value_unset (&value);
}

static void
vorbis_parse_drain_event_queue (GstVorbisParse * parse)
{
  while (parse->event_queue->length) {
    GstEvent *event;

    event = GST_EVENT_CAST (g_queue_pop_head (parse->event_queue));
    gst_pad_event_default (parse->sinkpad, GST_OBJECT_CAST (parse), event);
  }
}

static gboolean
vorbis_parse_have_header_packet (GstVorbisParse * parse, guint8 hdr_id)
{
  guint8 hdr;
  GList *l;

  for (l = parse->streamheader; l != NULL; l = l->next) {
    if (gst_buffer_extract (l->data, 0, &hdr, 1) == 1 && hdr == hdr_id)
      return TRUE;
  }

  return FALSE;
}

static gboolean
vorbis_parse_push_headers (GstVorbisParse * parse)
{
  /* mark and put on caps */
  GstCaps *caps;
  GstBuffer *outbuf, *outbuf1, *outbuf2, *outbuf3;
  ogg_packet packet;
  GstMapInfo map;
  const gchar *hdr_name;

  /* Check we have enough header packets, and the right ones */
  hdr_name = "identification";
  if (!vorbis_parse_have_header_packet (parse, 1))
    goto missing_header;

  hdr_name = "comment";
  if (!vorbis_parse_have_header_packet (parse, 3))
    goto missing_header;

  hdr_name = "setup";
  if (!vorbis_parse_have_header_packet (parse, 5))
    goto missing_header;

  outbuf = GST_BUFFER_CAST (parse->streamheader->data);
  gst_buffer_map (outbuf, &map, GST_MAP_READ);
  packet.packet = map.data;
  packet.bytes = map.size;
  packet.granulepos = GST_BUFFER_OFFSET_END (outbuf);
  packet.packetno = 1;
  packet.e_o_s = 0;
  packet.b_o_s = 1;
  vorbis_synthesis_headerin (&parse->vi, &parse->vc, &packet);
  gst_buffer_unmap (outbuf, &map);
  parse->sample_rate = parse->vi.rate;
  parse->channels = parse->vi.channels;
  outbuf1 = outbuf;

  outbuf = GST_BUFFER_CAST (parse->streamheader->next->data);
  gst_buffer_map (outbuf, &map, GST_MAP_READ);
  packet.packet = map.data;
  packet.bytes = map.size;
  packet.granulepos = GST_BUFFER_OFFSET_END (outbuf);
  packet.packetno = 2;
  packet.e_o_s = 0;
  packet.b_o_s = 0;
  vorbis_synthesis_headerin (&parse->vi, &parse->vc, &packet);
  gst_buffer_unmap (outbuf, &map);
  outbuf2 = outbuf;

  outbuf = GST_BUFFER_CAST (parse->streamheader->next->next->data);
  gst_buffer_map (outbuf, &map, GST_MAP_READ);
  packet.packet = map.data;
  packet.bytes = map.size;
  packet.granulepos = GST_BUFFER_OFFSET_END (outbuf);
  packet.packetno = 3;
  packet.e_o_s = 0;
  packet.b_o_s = 0;
  vorbis_synthesis_headerin (&parse->vi, &parse->vc, &packet);
  gst_buffer_unmap (outbuf, &map);
  outbuf3 = outbuf;

  /* get the headers into the caps, passing them to vorbis as we go */
  caps = gst_caps_new_simple ("audio/x-vorbis",
      "rate", G_TYPE_INT, parse->sample_rate,
      "channels", G_TYPE_INT, parse->channels, NULL);
  vorbis_parse_set_header_on_caps (parse, caps);
  GST_DEBUG_OBJECT (parse, "here are the caps: %" GST_PTR_FORMAT, caps);
  gst_pad_set_caps (parse->srcpad, caps);
  gst_caps_unref (caps);

  /* first process queued events */
  vorbis_parse_drain_event_queue (parse);

  /* push out buffers, ignoring return value... */
  gst_pad_push (parse->srcpad, outbuf1);
  gst_pad_push (parse->srcpad, outbuf2);
  gst_pad_push (parse->srcpad, outbuf3);

  g_list_free (parse->streamheader);
  parse->streamheader = NULL;
  return TRUE;

/* ERRORS */
missing_header:
  {
    GST_ELEMENT_ERROR (parse, STREAM, DECODE, (NULL),
        ("Vorbis stream is missing %s header", hdr_name));
    return FALSE;
  }
}

static void
vorbis_parse_clear_queue (GstVorbisParse * parse)
{
  while (parse->buffer_queue->length) {
    GstBuffer *buf;

    buf = GST_BUFFER_CAST (g_queue_pop_head (parse->buffer_queue));
    gst_buffer_unref (buf);
  }
  while (parse->event_queue->length) {
    GstEvent *event;

    event = GST_EVENT_CAST (g_queue_pop_head (parse->event_queue));
    gst_event_unref (event);
  }
}

static GstFlowReturn
vorbis_parse_push_buffer (GstVorbisParse * parse, GstBuffer * buf,
    gint64 granulepos)
{
  guint64 samples;

  /* our hack as noted below */
  samples = GST_BUFFER_OFFSET (buf);

  GST_BUFFER_OFFSET_END (buf) = granulepos;
  GST_BUFFER_DURATION (buf) = samples * GST_SECOND / parse->sample_rate;
  GST_BUFFER_OFFSET (buf) = granulepos * GST_SECOND / parse->sample_rate;
  GST_BUFFER_TIMESTAMP (buf) =
      GST_BUFFER_OFFSET (buf) - GST_BUFFER_DURATION (buf);

  return gst_pad_push (parse->srcpad, buf);
}

static GstFlowReturn
vorbis_parse_drain_queue_prematurely (GstVorbisParse * parse)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gint64 granulepos = MAX (parse->prev_granulepos, 0);

  /* got an EOS event, make sure to push out any buffers that were in the queue
   * -- won't normally be the case, but this catches the
   * didn't-get-a-granulepos-on-the-last-packet case. Assuming a continuous
   * stream. */

  /* if we got EOS before any buffers came, go ahead and push the other events
   * first */
  vorbis_parse_drain_event_queue (parse);

  while (!g_queue_is_empty (parse->buffer_queue)) {
    GstBuffer *buf;

    buf = GST_BUFFER_CAST (g_queue_pop_head (parse->buffer_queue));

    granulepos += GST_BUFFER_OFFSET (buf);
    ret = vorbis_parse_push_buffer (parse, buf, granulepos);

    if (ret != GST_FLOW_OK)
      goto done;
  }

  parse->prev_granulepos = granulepos;

done:
  return ret;
}

static GstFlowReturn
vorbis_parse_drain_queue (GstVorbisParse * parse, gint64 granulepos)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *walk;
  gint64 cur = granulepos;
  gint64 gp;

  for (walk = parse->buffer_queue->head; walk; walk = walk->next)
    cur -= GST_BUFFER_OFFSET (walk->data);

  if (parse->prev_granulepos != -1)
    cur = MAX (cur, parse->prev_granulepos);

  while (!g_queue_is_empty (parse->buffer_queue)) {
    GstBuffer *buf;

    buf = GST_BUFFER_CAST (g_queue_pop_head (parse->buffer_queue));

    cur += GST_BUFFER_OFFSET (buf);
    gp = CLAMP (cur, 0, granulepos);

    ret = vorbis_parse_push_buffer (parse, buf, gp);

    if (ret != GST_FLOW_OK)
      goto done;
  }

  parse->prev_granulepos = granulepos;

done:
  return ret;
}

static GstFlowReturn
vorbis_parse_queue_buffer (GstVorbisParse * parse, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  long blocksize;
  ogg_packet packet;
  GstMapInfo map;

  buf = gst_buffer_make_writable (buf);

  gst_buffer_map (buf, &map, GST_MAP_READ);
  packet.packet = map.data;
  packet.bytes = map.size;
  GST_DEBUG ("%p, %" G_GSIZE_FORMAT, map.data, map.size);
  packet.granulepos = GST_BUFFER_OFFSET_END (buf);
  packet.packetno = parse->packetno + parse->buffer_queue->length;
  packet.e_o_s = 0;

  blocksize = vorbis_packet_blocksize (&parse->vi, &packet);
  gst_buffer_unmap (buf, &map);

  /* temporarily store the sample count in OFFSET -- we overwrite this later */

  if (parse->prev_blocksize < 0)
    GST_BUFFER_OFFSET (buf) = 0;
  else
    GST_BUFFER_OFFSET (buf) = (blocksize + parse->prev_blocksize) / 4;

  parse->prev_blocksize = blocksize;

  g_queue_push_tail (parse->buffer_queue, buf);

  if (GST_BUFFER_OFFSET_END_IS_VALID (buf))
    ret = vorbis_parse_drain_queue (parse, GST_BUFFER_OFFSET_END (buf));

  return ret;
}

static GstFlowReturn
vorbis_parse_parse_packet (GstVorbisParse * parse, GstBuffer * buf)
{
  GstFlowReturn ret;
  GstMapInfo map;
  gboolean have_header;

  parse->packetno++;

  have_header = FALSE;
  gst_buffer_map (buf, &map, GST_MAP_READ);
  if (map.size >= 1) {
    if (map.data[0] & 1)
      have_header = TRUE;
  }
  gst_buffer_unmap (buf, &map);

  if (have_header) {
    if (!parse->streamheader_sent) {
      /* we need to collect the headers still */
      /* so put it on the streamheader list and return */
      parse->streamheader = g_list_append (parse->streamheader, buf);
    }
    ret = GST_FLOW_OK;
  } else {
    /* data packet, push the headers we collected before */
    if (!parse->streamheader_sent) {
      if (!vorbis_parse_push_headers (parse)) {
        ret = GST_FLOW_ERROR;
        goto out;
      }
      parse->streamheader_sent = TRUE;
    }
    ret = vorbis_parse_queue_buffer (parse, buf);
  }

out:

  return ret;
}

static GstFlowReturn
vorbis_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstVorbisParseClass *klass;
  GstVorbisParse *parse;

  parse = GST_VORBIS_PARSE (parent);
  klass = GST_VORBIS_PARSE_CLASS (G_OBJECT_GET_CLASS (parse));

  g_assert (klass->parse_packet != NULL);

  return klass->parse_packet (parse, buffer);
}

static gboolean
vorbis_parse_queue_event (GstVorbisParse * parse, GstEvent * event)
{
  GstFlowReturn ret = TRUE;

  g_queue_push_tail (parse->event_queue, event);

  return ret;
}

static gboolean
vorbis_parse_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret;
  GstVorbisParse *parse;

  parse = GST_VORBIS_PARSE (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      vorbis_parse_clear_queue (parse);
      parse->prev_granulepos = -1;
      parse->prev_blocksize = -1;
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_EOS:
      vorbis_parse_drain_queue_prematurely (parse);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      if (!parse->streamheader_sent && GST_EVENT_IS_SERIALIZED (event)
          && GST_EVENT_TYPE (event) > GST_EVENT_CAPS)
        ret = vorbis_parse_queue_event (parse, event);
      else
        ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
vorbis_parse_convert (GstPad * pad,
    GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  GstVorbisParse *parse;
  guint64 scale = 1;

  parse = GST_VORBIS_PARSE (GST_PAD_PARENT (pad));

  /* fixme: assumes atomic access to lots of instance variables modified from
   * the streaming thread, including 64-bit variables */

  if (parse->packetno < 4)
    return FALSE;

  if (src_format == *dest_format) {
    *dest_value = src_value;
    return TRUE;
  }

  if (parse->sinkpad == pad &&
      (src_format == GST_FORMAT_BYTES || *dest_format == GST_FORMAT_BYTES))
    return FALSE;

  switch (src_format) {
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          scale = sizeof (float) * parse->vi.channels;
        case GST_FORMAT_DEFAULT:
          *dest_value =
              scale * gst_util_uint64_scale_int (src_value, parse->vi.rate,
              GST_SECOND);
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = src_value * sizeof (float) * parse->vi.channels;
          break;
        case GST_FORMAT_TIME:
          *dest_value =
              gst_util_uint64_scale_int (src_value, GST_SECOND, parse->vi.rate);
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          *dest_value = src_value / (sizeof (float) * parse->vi.channels);
          break;
        case GST_FORMAT_TIME:
          *dest_value = gst_util_uint64_scale_int (src_value, GST_SECOND,
              parse->vi.rate * sizeof (float) * parse->vi.channels);
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }

  return res;
}

static gboolean
vorbis_parse_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gint64 granulepos;
  GstVorbisParse *parse;
  gboolean res = FALSE;

  parse = GST_VORBIS_PARSE (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;
      gint64 value;

      granulepos = parse->prev_granulepos;

      gst_query_parse_position (query, &format, NULL);

      /* and convert to the final format */
      if (!(res =
              vorbis_parse_convert (pad, GST_FORMAT_DEFAULT, granulepos,
                  &format, &value)))
        goto error;

      /* fixme: support segments
         value = (value - parse->segment_start) + parse->segment_time;
       */

      gst_query_set_position (query, format, value);

      GST_LOG_OBJECT (parse, "query %p: peer returned granulepos: %"
          G_GUINT64_FORMAT " - we return %" G_GUINT64_FORMAT " (format %u)",
          query, granulepos, value, format);

      break;
    }
    case GST_QUERY_DURATION:
    {
      /* fixme: not threadsafe */
      /* query peer for total length */
      if (!gst_pad_is_linked (parse->sinkpad)) {
        GST_WARNING_OBJECT (parse, "sink pad %" GST_PTR_FORMAT " is not linked",
            parse->sinkpad);
        goto error;
      }
      if (!(res = gst_pad_peer_query (parse->sinkpad, query)))
        goto error;
      break;
    }
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if (!(res =
              vorbis_parse_convert (pad, src_fmt, src_val, &dest_fmt,
                  &dest_val)))
        goto error;
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }
  return res;

error:
  {
    GST_WARNING_OBJECT (parse, "error handling query");
    return res;
  }
}

static GstStateChangeReturn
vorbis_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstVorbisParse *parse = GST_VORBIS_PARSE (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      vorbis_info_init (&parse->vi);
      vorbis_comment_init (&parse->vc);
      parse->prev_granulepos = -1;
      parse->prev_blocksize = -1;
      parse->packetno = 0;
      parse->streamheader_sent = FALSE;
      parse->buffer_queue = g_queue_new ();
      parse->event_queue = g_queue_new ();
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      vorbis_info_clear (&parse->vi);
      vorbis_comment_clear (&parse->vc);
      vorbis_parse_clear_queue (parse);
      g_queue_free (parse->buffer_queue);
      parse->buffer_queue = NULL;
      g_queue_free (parse->event_queue);
      parse->event_queue = NULL;
      break;
    default:
      break;
  }

  return ret;
}
