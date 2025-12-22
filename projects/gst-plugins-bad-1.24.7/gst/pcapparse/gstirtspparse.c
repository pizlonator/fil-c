/* GStreamer Interleaved RTSP parser
 * Copyright (C) 2011 Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
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
 * SECTION:element-irtspparse
 * @title: irtspparse
 * @short_description: Interleaved RTSP parser
 * @see_also: #GstPcapParse
 *
 * This is an interleaved RTSP parser that allows extracting specific
 * so-called "channels" from received interleaved (TCP) RTSP data
 * (typically extracted from some network capture).
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 filesrc location=h264crasher.pcap ! pcapparse ! irtspparse
 * ! rtph264depay ! ffdec_h264 ! fakesink
 * ]| Read from a pcap dump file using filesrc, extract the raw TCP packets,
 * depayload and decode them.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstirtspparse.h"
#include <gst/base/gstbytereader.h>

GST_DEBUG_CATEGORY_STATIC (irtsp_parse_debug);
#define GST_CAT_DEFAULT irtsp_parse_debug

enum
{
  PROP_0,
  PROP_CHANNEL_ID
};


static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp ; application/x-rtcp"));

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void gst_irtsp_parse_finalize (GObject * object);

static gboolean gst_irtsp_parse_start (GstBaseParse * parse);
static gboolean gst_irtsp_parse_stop (GstBaseParse * parse);
static GstFlowReturn gst_irtsp_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);

static void gst_irtsp_parse_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_irtsp_parse_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

#define parent_class gst_irtsp_parse_parent_class
G_DEFINE_TYPE (GstIRTSPParse, gst_irtsp_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE (irtspparse, "irtspparse", GST_RANK_NONE,
    GST_TYPE_IRTSP_PARSE);

static void
gst_irtsp_parse_class_init (GstIRTSPParseClass * klass)
{
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (irtsp_parse_debug, "irtspparse", 0,
      "Interleaved RTSP stream parser");

  object_class->finalize = gst_irtsp_parse_finalize;

  object_class->set_property = gst_irtsp_parse_set_property;
  object_class->get_property = gst_irtsp_parse_get_property;

  g_object_class_install_property (object_class, PROP_CHANNEL_ID,
      g_param_spec_int ("channel-id", "channel-id",
          "Channel Identifier", 0, 255,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  parse_class->start = GST_DEBUG_FUNCPTR (gst_irtsp_parse_start);
  parse_class->stop = GST_DEBUG_FUNCPTR (gst_irtsp_parse_stop);
  parse_class->handle_frame = GST_DEBUG_FUNCPTR (gst_irtsp_parse_handle_frame);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class, "IRTSPParse",
      "Raw/Parser",
      "Parses a raw interleaved RTSP stream",
      "Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>");
}

static void
gst_irtsp_parse_reset (GstIRTSPParse * IRTSPParse)
{
  IRTSPParse->state = IRTSP_SEARCH_FRAME;
  IRTSPParse->current_offset = 0;
  IRTSPParse->discont = FALSE;
}

static void
gst_irtsp_parse_init (GstIRTSPParse * IRTSPParse)
{
  gst_base_parse_set_min_frame_size (GST_BASE_PARSE (IRTSPParse), 1);
  gst_irtsp_parse_reset (IRTSPParse);
}

static void
gst_irtsp_parse_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_irtsp_parse_start (GstBaseParse * parse)
{
  GstIRTSPParse *IRTSPParse = GST_IRTSP_PARSE (parse);

  GST_DEBUG_OBJECT (parse, "starting");

  gst_irtsp_parse_reset (IRTSPParse);

  return TRUE;
}

static gboolean
gst_irtsp_parse_stop (GstBaseParse * parse)
{
  GST_DEBUG_OBJECT (parse, "stopping");

  return TRUE;
}

static void
gst_irtsp_set_caps_once (GstBaseParse * parse)
{
  if (!gst_pad_has_current_caps (GST_BASE_PARSE_SRC_PAD (parse))) {
    GstCaps *caps = gst_caps_new_empty_simple ("application/x-rtp");
    gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (parse), caps);
    gst_caps_unref (caps);
  }
}

static GstFlowReturn
gst_irtsp_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  static const guint frame_header_size = sizeof (guint8) * 4;
  static const guint8 frame_header_magic = 0x24;
  GstIRTSPParse *IRTSPParse = GST_IRTSP_PARSE (parse);
  GstBuffer *buf = frame->buffer;
  GstMapInfo map;
  const guint8 *frame_start;
  guint8 current_channel_id;
  const guint8 *data;
  guint data_size;
  guint flushed_size;

  if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (frame->buffer,
              GST_BUFFER_FLAG_DISCONT))) {
    IRTSPParse->discont = TRUE;
  }

  gst_buffer_map (buf, &map, GST_MAP_READ);

start:
  g_assert (map.size >= IRTSPParse->current_offset);
  data = &map.data[IRTSPParse->current_offset];
  data_size = map.size - IRTSPParse->current_offset;

  switch (IRTSPParse->state) {
    case IRTSP_SEARCH_FRAME:
      /* Use the first occurrence of 0x24 as a start of interleaved frames.
       * This 'trick' allows us to parse a dump that doesn't contain RTSP
       * handshake. It's up to user to provide the data where the first 0x24
       * is an RTSP frame */
      frame_start = memchr (data, frame_header_magic, data_size);
      if (frame_start) {
        IRTSPParse->state = IRTSP_PARSE_FRAME;
        IRTSPParse->current_offset += frame_start - data;
        goto start;
      } else {
        IRTSPParse->current_offset += data_size;
      }
      break;
    case IRTSP_PARSE_FRAME:
      if (data_size > 0 && data[0] != frame_header_magic) {
        IRTSPParse->state = IRTSP_SEARCH_FRAME;
        goto start;
      }

      if (data_size >= frame_header_size) {
        IRTSPParse->current_offset += frame_header_size;
        current_channel_id = data[1];
        IRTSPParse->frame_size = GST_READ_UINT16_BE (&data[2]);
        if (current_channel_id != IRTSPParse->target_channel_id) {
          IRTSPParse->state = IRTSP_SKIP_FRAME;
        } else {
          IRTSPParse->state = IRTSP_FLUSH_FRAME;
        }
        goto start;
      }
      break;
    case IRTSP_SKIP_FRAME:
      if (data_size >= IRTSPParse->frame_size) {
        IRTSPParse->current_offset += IRTSPParse->frame_size;
        IRTSPParse->state = IRTSP_PARSE_FRAME;
        goto start;
      }
      break;
    case IRTSP_FLUSH_FRAME:
      if (data_size >= IRTSPParse->frame_size) {
        gst_irtsp_set_caps_once (parse);
        gst_buffer_unmap (buf, &map);

        frame->out_buffer = gst_buffer_copy (frame->buffer);
        gst_buffer_resize (frame->out_buffer, IRTSPParse->current_offset,
            IRTSPParse->frame_size);

        if (G_UNLIKELY (IRTSPParse->discont)) {
          GST_BUFFER_FLAG_SET (frame->out_buffer, GST_BUFFER_FLAG_DISCONT);
          IRTSPParse->discont = FALSE;
        }

        flushed_size = IRTSPParse->current_offset + IRTSPParse->frame_size;
        IRTSPParse->current_offset = 0;
        IRTSPParse->state = IRTSP_PARSE_FRAME;

        return gst_base_parse_finish_frame (parse, frame, flushed_size);
      }

      break;
    default:
      g_assert_not_reached ();
      break;
  }

  gst_buffer_unmap (buf, &map);
  return GST_FLOW_OK;
}

static void
gst_irtsp_parse_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstIRTSPParse *IRTSPParse = GST_IRTSP_PARSE (object);

  switch (prop_id) {
    case PROP_CHANNEL_ID:
      IRTSPParse->target_channel_id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_irtsp_parse_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstIRTSPParse *IRTSPParse = GST_IRTSP_PARSE (object);

  switch (prop_id) {
    case PROP_CHANNEL_ID:
      g_value_set_int (value, IRTSPParse->target_channel_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
