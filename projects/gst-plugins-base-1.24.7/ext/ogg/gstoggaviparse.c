/* GStreamer
 * Copyright (C) 2006 Wim Taymans <wim@fluendo.com>
 *
 * gstoggaviparse.c: ogg avi stream parser
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

/*
 * Ogg in AVI is mostly done for vorbis audio. In the codec_data we receive the
 * first 3 packets of the raw vorbis data. On the sinkpad we receive full-blown Ogg
 * pages. 
 * Before extracting the packets out of the ogg pages, we push the raw vorbis
 * header packets to the decoder.
 * We don't use the incoming timestamps but use the ganulepos on the ogg pages
 * directly.
 * This parser only does ogg/vorbis for now.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst.h>
#include <ogg/ogg.h>
#include <string.h>

#include "gstoggelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_ogg_avi_parse_debug);
#define GST_CAT_DEFAULT gst_ogg_avi_parse_debug

#define GST_TYPE_OGG_AVI_PARSE (gst_ogg_avi_parse_get_type())
#define GST_OGG_AVI_PARSE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OGG_AVI_PARSE, GstOggAviParse))
#define GST_OGG_AVI_PARSE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OGG_AVI_PARSE, GstOggAviParse))
#define GST_IS_OGG_AVI_PARSE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OGG_AVI_PARSE))
#define GST_IS_OGG_AVI_PARSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OGG_AVI_PARSE))

static GType gst_ogg_avi_parse_get_type (void);

typedef struct _GstOggAviParse GstOggAviParse;
typedef struct _GstOggAviParseClass GstOggAviParseClass;

struct _GstOggAviParse
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  gboolean discont;
  gint serial;

  ogg_sync_state sync;
  ogg_stream_state stream;
};

struct _GstOggAviParseClass
{
  GstElementClass parent_class;
};


static GstElementClass *parent_class = NULL;

G_DEFINE_TYPE (GstOggAviParse, gst_ogg_avi_parse, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (oggaviparse, "oggaviparse",
    GST_RANK_PRIMARY, GST_TYPE_OGG_AVI_PARSE,
    GST_DEBUG_CATEGORY_INIT (gst_ogg_avi_parse_debug, "oggaviparse", 0,
        "ogg avi parser"));

enum
{
  PROP_0
};

static GstStaticPadTemplate ogg_avi_parse_src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-vorbis")
    );

static GstStaticPadTemplate ogg_avi_parse_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-ogg-avi")
    );

static void gst_ogg_avi_parse_finalize (GObject * object);
static GstStateChangeReturn gst_ogg_avi_parse_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_ogg_avi_parse_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_ogg_avi_parse_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_ogg_avi_parse_setcaps (GstPad * pad, GstCaps * caps);

static void
gst_ogg_avi_parse_class_init (GstOggAviParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_set_static_metadata (gstelement_class,
      "Ogg AVI parser", "Codec/Parser",
      "parse an ogg avi stream into pages (info about ogg: http://xiph.org)",
      "Wim Taymans <wim@fluendo.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &ogg_avi_parse_sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &ogg_avi_parse_src_template_factory);

  parent_class = g_type_class_peek_parent (klass);

  gstelement_class->change_state = gst_ogg_avi_parse_change_state;

  gobject_class->finalize = gst_ogg_avi_parse_finalize;
}

static void
gst_ogg_avi_parse_init (GstOggAviParse * ogg)
{
  /* create the sink and source pads */
  ogg->sinkpad =
      gst_pad_new_from_static_template (&ogg_avi_parse_sink_template_factory,
      "sink");
  gst_pad_set_event_function (ogg->sinkpad, gst_ogg_avi_parse_event);
  gst_pad_set_chain_function (ogg->sinkpad, gst_ogg_avi_parse_chain);
  gst_element_add_pad (GST_ELEMENT (ogg), ogg->sinkpad);

  ogg->srcpad =
      gst_pad_new_from_static_template (&ogg_avi_parse_src_template_factory,
      "src");
  gst_pad_use_fixed_caps (ogg->srcpad);
  gst_element_add_pad (GST_ELEMENT (ogg), ogg->srcpad);
}

static void
gst_ogg_avi_parse_finalize (GObject * object)
{
  GstOggAviParse *ogg = GST_OGG_AVI_PARSE (object);

  GST_LOG_OBJECT (ogg, "Disposing of object %p", ogg);

  ogg_sync_clear (&ogg->sync);
  ogg_stream_clear (&ogg->stream);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_ogg_avi_parse_setcaps (GstPad * pad, GstCaps * caps)
{
  GstOggAviParse *ogg;
  GstStructure *structure;
  const GValue *codec_data;
  GstBuffer *buffer;
  GstMapInfo map;
  guint8 *ptr;
  gsize left;
  guint32 sizes[3];
  GstCaps *outcaps;
  gint i, offs;

  ogg = GST_OGG_AVI_PARSE (GST_OBJECT_PARENT (pad));

  structure = gst_caps_get_structure (caps, 0);

  /* take codec data */
  codec_data = gst_structure_get_value (structure, "codec_data");
  if (codec_data == NULL)
    goto no_data;

  /* only buffers are valid */
  if (G_VALUE_TYPE (codec_data) != GST_TYPE_BUFFER)
    goto wrong_format;

  /* Now parse the data */
  buffer = gst_value_get_buffer (codec_data);

  /* first 22 bytes are bits_per_sample, channel_mask, GUID
   * Then we get 3 LE guint32 with the 3 header sizes
   * then we get the bytes of the 3 headers. */
  gst_buffer_map (buffer, &map, GST_MAP_READ);

  ptr = map.data;
  left = map.size;

  GST_LOG_OBJECT (ogg, "configuring codec_data of size %" G_GSIZE_FORMAT, left);

  /* skip headers */
  ptr += 22;
  left -= 22;

  /* we need at least 12 bytes for the packet sizes of the 3 headers */
  if (left < 12)
    goto buffer_too_small;

  /* read sizes of the 3 headers */
  sizes[0] = GST_READ_UINT32_LE (ptr);
  sizes[1] = GST_READ_UINT32_LE (ptr + 4);
  sizes[2] = GST_READ_UINT32_LE (ptr + 8);

  GST_DEBUG_OBJECT (ogg, "header sizes: %u %u %u", sizes[0], sizes[1],
      sizes[2]);

  left -= 12;

  /* and we need at least enough data for all the headers */
  if (left < sizes[0] + sizes[1] + sizes[2])
    goto buffer_too_small;

  /* set caps */
  outcaps = gst_caps_new_empty_simple ("audio/x-vorbis");
  gst_pad_set_caps (ogg->srcpad, outcaps);
  gst_caps_unref (outcaps);

  /* copy header data */
  offs = 34;
  for (i = 0; i < 3; i++) {
    GstBuffer *out;

    /* now output the raw vorbis header packets */
    out = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, offs, sizes[i]);
    gst_pad_push (ogg->srcpad, out);

    offs += sizes[i];
  }
  gst_buffer_unmap (buffer, &map);

  return TRUE;

  /* ERRORS */
no_data:
  {
    GST_DEBUG_OBJECT (ogg, "no codec_data found in caps");
    return FALSE;
  }
wrong_format:
  {
    GST_DEBUG_OBJECT (ogg, "codec_data is not a buffer");
    return FALSE;
  }
buffer_too_small:
  {
    GST_DEBUG_OBJECT (ogg, "codec_data is too small");
    gst_buffer_unmap (buffer, &map);
    return FALSE;
  }
}

static gboolean
gst_ogg_avi_parse_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstOggAviParse *ogg;
  gboolean ret;

  ogg = GST_OGG_AVI_PARSE (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_ogg_avi_parse_setcaps (pad, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_FLUSH_START:
      ret = gst_pad_push_event (ogg->srcpad, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      ogg_sync_reset (&ogg->sync);
      ogg_stream_reset (&ogg->stream);
      ogg->discont = TRUE;
      ret = gst_pad_push_event (ogg->srcpad, event);
      break;
    default:
      ret = gst_pad_push_event (ogg->srcpad, event);
      break;
  }
  return ret;
}

static GstFlowReturn
gst_ogg_avi_parse_push_packet (GstOggAviParse * ogg, ogg_packet * packet)
{
  GstBuffer *buffer;
  GstFlowReturn result;

  /* allocate space for header and body */
  buffer = gst_buffer_new_and_alloc (packet->bytes);
  gst_buffer_fill (buffer, 0, packet->packet, packet->bytes);

  GST_LOG_OBJECT (ogg, "created buffer %p from page", buffer);

  GST_BUFFER_OFFSET_END (buffer) = packet->granulepos;

  if (ogg->discont) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    ogg->discont = FALSE;
  }

  result = gst_pad_push (ogg->srcpad, buffer);

  return result;
}

static GstFlowReturn
gst_ogg_avi_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn result = GST_FLOW_OK;
  GstOggAviParse *ogg;
  guint size;
  gchar *oggbuf;
  gint ret = -1;

  ogg = GST_OGG_AVI_PARSE (parent);

  size = gst_buffer_get_size (buffer);

  GST_LOG_OBJECT (ogg, "Chain function received buffer of size %d", size);

  if (GST_BUFFER_IS_DISCONT (buffer)) {
    ogg_sync_reset (&ogg->sync);
    ogg->discont = TRUE;
  }

  /* write data to sync layer */
  oggbuf = ogg_sync_buffer (&ogg->sync, size);
  gst_buffer_extract (buffer, 0, oggbuf, size);
  ogg_sync_wrote (&ogg->sync, size);
  gst_buffer_unref (buffer);

  /* try to get as many packets out of the stream as possible */
  do {
    ogg_page page;

    /* try to swap out a page */
    ret = ogg_sync_pageout (&ogg->sync, &page);
    if (ret == 0) {
      GST_DEBUG_OBJECT (ogg, "need more data");
      break;
    } else if (ret == -1) {
      GST_DEBUG_OBJECT (ogg, "discont in pages");
      ogg->discont = TRUE;
    } else {
      /* new unknown stream, init the ogg stream with the serial number of the
       * page. */
      if (ogg->serial == -1) {
        ogg->serial = ogg_page_serialno (&page);
        ogg_stream_init (&ogg->stream, ogg->serial);
      }

      /* submit page */
      if (ogg_stream_pagein (&ogg->stream, &page) != 0) {
        GST_WARNING_OBJECT (ogg, "ogg stream choked on page resetting stream");
        ogg_sync_reset (&ogg->sync);
        ogg->discont = TRUE;
        continue;
      }

      /* try to get as many packets as possible out of the page */
      do {
        ogg_packet packet;

        ret = ogg_stream_packetout (&ogg->stream, &packet);
        GST_LOG_OBJECT (ogg, "packetout gave %d", ret);
        switch (ret) {
          case 0:
            break;
          case -1:
            /* out of sync, We mark a DISCONT. */
            ogg->discont = TRUE;
            break;
          case 1:
            result = gst_ogg_avi_parse_push_packet (ogg, &packet);
            if (result != GST_FLOW_OK)
              goto done;
            break;
          default:
            GST_WARNING_OBJECT (ogg,
                "invalid return value %d for ogg_stream_packetout, resetting stream",
                ret);
            break;
        }
      }
      while (ret != 0);
    }
  }
  while (ret != 0);

done:
  return result;
}

static GstStateChangeReturn
gst_ogg_avi_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstOggAviParse *ogg;
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  ogg = GST_OGG_AVI_PARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      ogg_sync_init (&ogg->sync);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ogg_sync_reset (&ogg->sync);
      ogg_stream_reset (&ogg->stream);
      ogg->serial = -1;
      ogg->discont = TRUE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  result = parent_class->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      ogg_sync_clear (&ogg->sync);
      break;
    default:
      break;
  }
  return result;
}
