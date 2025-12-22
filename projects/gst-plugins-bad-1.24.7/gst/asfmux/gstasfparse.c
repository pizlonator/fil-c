/* ASF parser plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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

#include <string.h>
#include "gstasfparse.h"

/* FIXME add this include
 * #include <glib/gi18n-lib.h> */

GST_DEBUG_CATEGORY_STATIC (asfparse_debug);
#define GST_CAT_DEFAULT asfparse_debug

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf, parsed = (boolean) true")
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf, parsed = (boolean) false")
    );

#define gst_asf_parse_parent_class parent_class
G_DEFINE_TYPE (GstAsfParse, gst_asf_parse, GST_TYPE_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE (asfparse, "asfparse",
    GST_RANK_NONE, GST_TYPE_ASF_PARSE);

static gboolean
gst_asf_parse_start (GstBaseParse * parse)
{
  GstAsfParse *asfparse = GST_ASF_PARSE_CAST (parse);
  gst_asf_file_info_reset (asfparse->asfinfo);
  asfparse->parse_state = ASF_PARSING_HEADERS;
  asfparse->parsed_packets = 0;

  /* ASF Obj header length */
  gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse),
      ASF_GUID_OBJSIZE_SIZE);

  gst_base_parse_set_syncable (GST_BASE_PARSE_CAST (asfparse), FALSE);

  return TRUE;
}

static gboolean
gst_asf_parse_stop (GstBaseParse * parse)
{
  GstAsfParse *asfparse = GST_ASF_PARSE_CAST (parse);
  gst_asf_file_info_reset (asfparse->asfinfo);

  return TRUE;
}

static GstFlowReturn
gst_asf_parse_parse_data_object (GstAsfParse * asfparse, guint8 * data,
    gsize size)
{
  GstByteReader reader;
  GstFlowReturn ret = GST_FLOW_OK;
  guint64 packet_count = 0;

  GST_DEBUG_OBJECT (asfparse, "Parsing data object");

  gst_byte_reader_init (&reader, data, size);
  /* skip to packet count */
  if (!gst_byte_reader_skip (&reader, 40))
    goto error;
  if (!gst_byte_reader_get_uint64_le (&reader, &packet_count))
    goto error;

  if (asfparse->asfinfo->packets_count != packet_count) {
    GST_WARNING_OBJECT (asfparse, "File properties object and data object have "
        "different packets count, %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT,
        asfparse->asfinfo->packets_count, packet_count);
  } else {
    GST_DEBUG_OBJECT (asfparse, "Total packets: %" G_GUINT64_FORMAT,
        packet_count);
  }

  return GST_FLOW_OK;

error:
  ret = GST_FLOW_ERROR;
  GST_ERROR_OBJECT (asfparse, "Error while parsing data object headers");
  return ret;
}

static GstFlowReturn
gst_asf_parse_parse_packet (GstAsfParse * asfparse, GstBaseParseFrame * frame,
    GstMapInfo * map)
{
  GstBuffer *buffer = frame->buffer;
  GstAsfPacketInfo *packetinfo = asfparse->packetinfo;

  /* gst_asf_parse_packet_* won't accept size larger than the packet size, so we assume
   * it will always be packet_size here */
  g_return_val_if_fail (map->size >= asfparse->asfinfo->packet_size,
      GST_FLOW_ERROR);

  if (!gst_asf_parse_packet_from_data (map->data,
          asfparse->asfinfo->packet_size, buffer, packetinfo, FALSE,
          asfparse->asfinfo->packet_size))
    goto error;

  GST_DEBUG_OBJECT (asfparse, "Received packet of length %" G_GUINT32_FORMAT
      ", padding %" G_GUINT32_FORMAT ", send time %" G_GUINT32_FORMAT
      ", duration %" G_GUINT16_FORMAT " and %s keyframe(s)",
      packetinfo->packet_size, packetinfo->padding,
      packetinfo->send_time, packetinfo->duration,
      (packetinfo->has_keyframe) ? "with" : "without");

  /* set gstbuffer fields */
  if (!packetinfo->has_keyframe) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  }
  GST_BUFFER_TIMESTAMP (buffer) = ((GstClockTime) packetinfo->send_time)
      * GST_MSECOND;
  GST_BUFFER_DURATION (buffer) = ((GstClockTime) packetinfo->duration)
      * GST_MSECOND;

  return GST_FLOW_OK;

error:
  GST_ERROR_OBJECT (asfparse, "Error while parsing data packet");
  return GST_FLOW_ERROR;
}


/* reads the next object and pushes it through without parsing */
static GstFlowReturn
gst_asf_parse_handle_frame_push_object (GstAsfParse * asfparse,
    GstBaseParseFrame * frame, gint * skipsize, const Guid * guid)
{
  GstBuffer *buffer = frame->buffer;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  if (map.size >= ASF_GUID_OBJSIZE_SIZE) {
    guint64 size;

    size = gst_asf_match_and_peek_obj_size (map.data, guid);

    if (size == 0) {
      GST_ERROR_OBJECT (asfparse, "GUID starting identifier missing");
      ret = GST_FLOW_ERROR;
      gst_buffer_unmap (buffer, &map);
      goto end;
    }

    if (size > map.size) {
      /* request all the obj data */
      gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse), size);
      gst_buffer_unmap (buffer, &map);
      goto end;
    }

    gst_buffer_unmap (buffer, &map);

    gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse),
        ASF_GUID_OBJSIZE_SIZE);
    gst_base_parse_finish_frame (GST_BASE_PARSE_CAST (asfparse), frame, size);
  } else {
    gst_buffer_unmap (buffer, &map);
    *skipsize = 0;
  }

end:
  return ret;
}

static GstFlowReturn
gst_asf_parse_handle_frame_headers (GstAsfParse * asfparse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  GstBuffer *buffer = frame->buffer;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  if (map.size >= ASF_GUID_OBJSIZE_SIZE) {
    guint64 size;

    size = gst_asf_match_and_peek_obj_size (map.data,
        &(guids[ASF_HEADER_OBJECT_INDEX]));

    if (size == 0) {
      GST_ERROR_OBJECT (asfparse, "ASF starting identifier missing");
      ret = GST_FLOW_ERROR;
      gst_buffer_unmap (buffer, &map);
      goto end;
    }

    if (size > map.size) {
      /* request all the obj data */
      gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse), size);
      gst_buffer_unmap (buffer, &map);
      goto end;
    }

    if (gst_asf_parse_headers_from_data (map.data, map.size, asfparse->asfinfo)) {
      GST_DEBUG_OBJECT (asfparse, "Successfully parsed headers");
      asfparse->parse_state = ASF_PARSING_DATA;
      gst_buffer_unmap (buffer, &map);

      GST_INFO_OBJECT (asfparse, "Broadcast mode %s",
          asfparse->asfinfo->broadcast ? "on" : "off");

      gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse),
          ASF_GUID_OBJSIZE_SIZE);

      gst_pad_push_event (GST_BASE_PARSE_SRC_PAD (asfparse),
          gst_event_new_caps (gst_caps_new_simple ("video/x-ms-asf", "parsed",
                  G_TYPE_BOOLEAN, TRUE, NULL)));
      gst_base_parse_finish_frame (GST_BASE_PARSE_CAST (asfparse), frame, size);
    } else {
      ret = GST_FLOW_ERROR;
    }
  } else {
    gst_buffer_unmap (buffer, &map);
    *skipsize = 0;
  }

end:
  return ret;
}

static GstFlowReturn
gst_asf_parse_handle_frame_data_header (GstAsfParse * asfparse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  GstBuffer *buffer = frame->buffer;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  if (map.size >= ASF_GUID_OBJSIZE_SIZE) {
    guint64 size;

    size = gst_asf_match_and_peek_obj_size (map.data,
        &(guids[ASF_DATA_OBJECT_INDEX]));

    if (size == 0) {
      GST_ERROR_OBJECT (asfparse, "ASF data object missing");
      ret = GST_FLOW_ERROR;
      gst_buffer_unmap (buffer, &map);
      goto end;
    }

    if (ASF_DATA_OBJECT_SIZE > map.size) {
      /* request all the obj data header size */
      gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse),
          ASF_DATA_OBJECT_SIZE);
      gst_buffer_unmap (buffer, &map);
      goto end;
    }

    if (gst_asf_parse_parse_data_object (asfparse, map.data,
            map.size) == GST_FLOW_OK) {
      GST_DEBUG_OBJECT (asfparse, "Successfully parsed data object");
      asfparse->parse_state = ASF_PARSING_PACKETS;
      gst_buffer_unmap (buffer, &map);

      gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse),
          asfparse->asfinfo->packet_size);

      gst_base_parse_finish_frame (GST_BASE_PARSE_CAST (asfparse), frame,
          ASF_DATA_OBJECT_SIZE);
    }
  } else {
    gst_buffer_unmap (buffer, &map);
    *skipsize = 0;
  }

end:
  return ret;
}

static GstFlowReturn
gst_asf_parse_handle_frame_packets (GstAsfParse * asfparse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  GstBuffer *buffer = frame->buffer;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (asfparse, "Packet parsing");
  gst_buffer_map (buffer, &map, GST_MAP_READ);
  if (G_LIKELY (map.size >= asfparse->asfinfo->packet_size)) {

    GST_DEBUG_OBJECT (asfparse, "Parsing packet %" G_GUINT64_FORMAT,
        asfparse->parsed_packets);

    ret = gst_asf_parse_parse_packet (asfparse, frame, &map);

    gst_buffer_unmap (buffer, &map);

    if (ret == GST_FLOW_OK) {
      asfparse->parsed_packets++;
      gst_base_parse_finish_frame (GST_BASE_PARSE_CAST (asfparse), frame,
          asfparse->asfinfo->packet_size);

      /* test if all packets have been processed */
      if (G_UNLIKELY (!asfparse->asfinfo->broadcast &&
              asfparse->parsed_packets == asfparse->asfinfo->packets_count)) {
        GST_INFO_OBJECT (asfparse,
            "All %" G_GUINT64_FORMAT " packets processed",
            asfparse->parsed_packets);
        asfparse->parse_state = ASF_PARSING_INDEXES;
        gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse),
            ASF_GUID_OBJSIZE_SIZE);
      }
    }
  } else {
    gst_base_parse_set_min_frame_size (GST_BASE_PARSE_CAST (asfparse),
        asfparse->asfinfo->packet_size);
    gst_buffer_unmap (buffer, &map);
    *skipsize = 0;
  }

  return ret;
}

static GstFlowReturn
gst_asf_parse_handle_frame_indexes (GstAsfParse * asfparse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  /* don't care about indexes, just push them forward */
  return gst_asf_parse_handle_frame_push_object (asfparse, frame, skipsize,
      NULL);
}


static GstFlowReturn
gst_asf_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  GstAsfParse *asfparse = GST_ASF_PARSE_CAST (parse);

  switch (asfparse->parse_state) {
    case ASF_PARSING_HEADERS:
      return gst_asf_parse_handle_frame_headers (asfparse, frame, skipsize);
    case ASF_PARSING_DATA:
      return gst_asf_parse_handle_frame_data_header (asfparse, frame, skipsize);
    case ASF_PARSING_PACKETS:
      return gst_asf_parse_handle_frame_packets (asfparse, frame, skipsize);
    case ASF_PARSING_INDEXES:
      return gst_asf_parse_handle_frame_indexes (asfparse, frame, skipsize);
    default:
      break;
  }

  g_assert_not_reached ();
  return GST_FLOW_ERROR;
}

static void
gst_asf_parse_finalize (GObject * object)
{
  GstAsfParse *asfparse = GST_ASF_PARSE (object);
  gst_asf_file_info_free (asfparse->asfinfo);
  g_free (asfparse->packetinfo);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_asf_parse_class_init (GstAsfParseClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseParseClass *gstbaseparse_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbaseparse_class = (GstBaseParseClass *) klass;

  gobject_class->finalize = gst_asf_parse_finalize;

  gstbaseparse_class->start = gst_asf_parse_start;
  gstbaseparse_class->stop = gst_asf_parse_stop;
  gstbaseparse_class->handle_frame = gst_asf_parse_handle_frame;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "ASF parser",
      "Parser", "Parses ASF", "Thiago Santos <thiagoss@embedded.ufcg.edu.br>");

  GST_DEBUG_CATEGORY_INIT (asfparse_debug, "asfparse", 0,
      "Parser for ASF streams");
}

static void
gst_asf_parse_init (GstAsfParse * asfparse)
{
  asfparse->asfinfo = gst_asf_file_info_new ();
  asfparse->packetinfo = g_new0 (GstAsfPacketInfo, 1);
}
