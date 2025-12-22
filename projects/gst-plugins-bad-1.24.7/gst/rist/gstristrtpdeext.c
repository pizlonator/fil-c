/* GStreamer RIST plugin
 * Copyright (C) 2019 Net Insight AB
 *     Author: Olivier Crete <olivier.crete@collabora.com>
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
 * SECTION:element-ristrtpdeext
 * @title: ristrtpdeext
 * @see_also: ristsrc
 *
 * This element removes the RTP header extension. If the RTP header extension
 * contained information about remove MPEG Transport Stream null packets, it
 * re-inserts them.
 *
 * If, according to the RTP sequence number and the sequence number
 * extension in the RTP header extension, the packet is more than 2^16
 * packets before the latest received, it will also drop it because it
 * is too old for the jitterbuffer to handle properly.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/rtp/rtp.h>

#include "gstrist.h"

GST_DEBUG_CATEGORY_STATIC (gst_rist_rtp_deext_debug);
#define GST_CAT_DEFAULT gst_rist_rtp_deext_debug

enum
{
  PROP_0 = 0,
  PROP_MAX_EXT_SEQNUM,
  PROP_HAVE_EXT_SEQNUM
};

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));


static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));


struct _GstRistRtpDeext
{
  GstElement parent;

  GstPad *srcpad, *sinkpad;

  gboolean have_extseqnum;
  guint32 max_extseqnum;
};

G_DEFINE_TYPE_WITH_CODE (GstRistRtpDeext, gst_rist_rtp_deext, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_rist_rtp_deext_debug, "ristrtpdeext", 0,
        "RIST RTP De-extension"));
GST_ELEMENT_REGISTER_DEFINE (ristrtpdeext, "ristrtpdeext", GST_RANK_NONE,
    GST_TYPE_RIST_RTP_DEEXT);

static guint8
bit_count (guint8 value)
{
  guint8 count = 0;

  while (value > 0) {           /* until all bits are zero */
    if ((value & 1) == 1)       /* check lower bit */
      count++;
    value >>= 1;                /* shift bits, removing lower bit */
  }
  return count;
}

static GstFlowReturn
gst_rist_rtp_deext_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstRistRtpDeext *self = GST_RIST_RTP_DEEXT (parent);
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *outbuf;
  gboolean has_seqnum_ext;
  gboolean has_drop_null;
  gboolean ts_packet_size;
  guint orig_ts_packet_count;
  guint16 bits;
  guint8 npd_bits;
  guint8 num_packets_deleted;
  guint extlen;
  gpointer extdata = NULL;
  guint8 *data = NULL;
  guint8 *payload;
  guint plen;
  guint i;
  GstMemory *mem = NULL;
  GstMapInfo map;
  guint num_restored = 0;
  guint orig_payload_offset;
  guint hdrlen;


  if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp)) {
    GST_ELEMENT_ERROR (self, STREAM, MUX, (NULL), ("Could not map RTP buffer"));
    goto mapping_error;
  }

  if (!gst_rtp_buffer_get_extension_data (&rtp, &bits, &extdata, &extlen)) {
    /* Has no extension, let's push out without modifying */
    gst_rtp_buffer_unmap (&rtp);
    return gst_pad_push (self->srcpad, buffer);
  }

  if (bits != ('R' << 8 | 'I')) {
    gst_rtp_buffer_unmap (&rtp);
    GST_LOG_OBJECT (self, "Buffer %" GST_PTR_FORMAT
        " has an extension that's not the RIST one, ignoring", buffer);
    return gst_pad_push (self->srcpad, buffer);
  }

  if (extlen != 1) {
    gst_rtp_buffer_unmap (&rtp);
    GST_LOG_OBJECT (self, "Buffer %" GST_PTR_FORMAT
        " has a RIST extension that's not of length 1, ignoring", buffer);
    return gst_pad_push (self->srcpad, buffer);
  }

  data = extdata;

  has_drop_null = (data[0] >> 7) & 1;   /* N */
  has_seqnum_ext = (data[0] >> 6) & 1;  /* E */
  orig_ts_packet_count = (data[0] >> 3) & 7;    /* Size */
  ts_packet_size = ((data[1] >> 7) & 1) ? 204 : 188;
  npd_bits = data[1] & 0x7F;

  num_packets_deleted = bit_count (npd_bits);

  self->have_extseqnum = has_seqnum_ext;

  if (has_seqnum_ext) {
    guint16 seqnumext_val = GST_READ_UINT16_BE (data + 2);
    guint32 extseqnum = seqnumext_val << 16 | gst_rtp_buffer_get_seq (&rtp);

    if (extseqnum < self->max_extseqnum &&
        self->max_extseqnum - extseqnum > G_MAXINT16) {
      gst_rtp_buffer_unmap (&rtp);
      gst_buffer_unref (buffer);
      GST_WARNING_OBJECT (self, "Buffer with extended seqnum %u is more than"
          " G_MAXINT16 (%u) before the higher received seqnum %u, dropping to"
          " avoid confusing downstream elements.",
          extseqnum, G_MAXINT16, self->max_extseqnum);
      return GST_FLOW_OK;
    }
    self->max_extseqnum = MAX (self->max_extseqnum, extseqnum);
  }

  if (!has_drop_null || num_packets_deleted == 0)
    goto no_restore;

  payload = gst_rtp_buffer_get_payload (&rtp);
  plen = gst_rtp_buffer_get_payload_len (&rtp);

  if (plen != 0) {
    if (plen % 188 == 0) {
      if (ts_packet_size != 188) {
        GST_WARNING_OBJECT (self, "RTP Header extension says packet size is"
            " 204, but payload length is divisible by 188, ignoring header");
        ts_packet_size = 188;
      }
    } else if (plen % 204 == 0) {
      if (ts_packet_size != 204) {
        GST_WARNING_OBJECT (self, "RTP Header extension says packet size is"
            " 188, but payload length is divisible by 204, ignoring header");
        ts_packet_size = 204;
      }
    } else {
      GST_WARNING_OBJECT (self, "Payload length (%u) is not divisible by 188"
          " or 204, taking TS packet size from header (%u), not restoring"
          " null packets", plen, ts_packet_size);
      goto no_restore;
    }
  }

  if ((plen / ts_packet_size) + num_packets_deleted != orig_ts_packet_count) {
    if (orig_ts_packet_count == 0)
      GST_LOG_OBJECT (self, "Original number of packet is 0, using NPD bits to"
          " restore packet size to %d",
          (plen / ts_packet_size) + num_packets_deleted);
    else
      GST_WARNING_OBJECT (self, "The number of deleted packets (%u) + the"
          " number of transmitted packets (%d) is not equal to the declared"
          " original packet count (%d), ignoring it", num_packets_deleted,
          (plen / ts_packet_size), orig_ts_packet_count);
    orig_ts_packet_count = (plen / ts_packet_size) + num_packets_deleted;
  }

  GST_LOG_OBJECT (self, "Restoring %u null TS packets for a total"
      " of %u packets", num_packets_deleted, orig_ts_packet_count);

  mem = gst_allocator_alloc (NULL, orig_ts_packet_count * ts_packet_size, NULL);
  gst_memory_map (mem, &map, GST_MAP_READWRITE);

  /* Re-create the null packets */
  for (i = 0; i < orig_ts_packet_count; i++) {
    gboolean was_deleted = (npd_bits & (1 << (6 - i))) != 0;
    guint8 *pktdst = map.data + (i * ts_packet_size);

    if (was_deleted) {
      memset (pktdst, 0, ts_packet_size);
      pktdst[0] = 0x47;
      pktdst[1] = 0x1F;
      pktdst[2] = 0xFF;
      pktdst[3] = 0x10;
      num_restored++;
    } else {
      guint src_offset = (i - num_restored) * ts_packet_size;

      if (src_offset + ts_packet_size > plen) {
        GST_WARNING_OBJECT (self, "Invalid NPD bits (0x%x), not enough data in"
            " the original RTP packet, not restoring TS packet %d", npd_bits,
            i);
      } else {
        memcpy (pktdst, payload + src_offset, ts_packet_size);
      }
    }
  }

  gst_memory_unmap (mem, &map);

no_restore:

  orig_payload_offset = gst_rtp_buffer_get_header_len (&rtp);
  hdrlen = orig_payload_offset - (4 + (extlen * 4));

  gst_rtp_buffer_unmap (&rtp);

  /* Create a new buffer without the header extension */
  outbuf = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, 0, hdrlen);

  /* Unset extension flag, can't use the GstRTPBuffer function as they will
   * try to look for the extension itself which isn't there if the flag is set.
   */
  gst_buffer_map (outbuf, &map, GST_MAP_READWRITE);
  map.data[0] &= ~0x10;
  gst_buffer_unmap (outbuf, &map);

  if (mem)
    gst_buffer_append_memory (outbuf, mem);
  else
    gst_buffer_copy_into (outbuf, buffer, GST_BUFFER_COPY_MEMORY,
        orig_payload_offset, -1);

  gst_buffer_unref (buffer);

  return gst_pad_push (self->srcpad, outbuf);

mapping_error:
  gst_buffer_unref (buffer);
  return GST_FLOW_ERROR;
}

static void
gst_rist_rtp_deext_init (GstRistRtpDeext * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_templ,
      sink_templ.name_template);
  self->srcpad = gst_pad_new_from_static_template (&src_templ,
      src_templ.name_template);

  GST_PAD_SET_PROXY_ALLOCATION (self->sinkpad);
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);
  gst_pad_set_chain_function (self->sinkpad, gst_rist_rtp_deext_chain);

  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}

static void
gst_rist_rtp_deext_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRistRtpDeext *self = GST_RIST_RTP_DEEXT (object);

  switch (prop_id) {
    case PROP_MAX_EXT_SEQNUM:
      g_value_set_uint (value, self->max_extseqnum);
      break;
    case PROP_HAVE_EXT_SEQNUM:
      g_value_set_boolean (value, self->have_extseqnum);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rist_rtp_deext_class_init (GstRistRtpDeextClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  gst_element_class_set_metadata (element_class,
      "RIST RTP Extension remover", "Filter/Network",
      "Removes RIST TR-06-2 RTP Header extension",
      "Olivier Crete <olivier.crete@collabora.com");
  gst_element_class_add_static_pad_template (element_class, &src_templ);
  gst_element_class_add_static_pad_template (element_class, &sink_templ);

  object_class->get_property = gst_rist_rtp_deext_get_property;

  g_object_class_install_property (object_class, PROP_MAX_EXT_SEQNUM,
      g_param_spec_uint ("max-ext-seqnum",
          "Maximum Extended Sequence Number",
          "Largest extended sequence number received", 0, G_MAXUINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_HAVE_EXT_SEQNUM,
      g_param_spec_boolean ("have-ext-seqnum",
          "Have extended seqnum",
          "Has an extended sequence number extension been seen", FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}
