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
 * SECTION:element-ristrtpext
 * @title: ristrtpext
 * @see_also: ristsink
 *
 * This elements adds the RTP header extension defined by the RIST profile.
 *
 * If the GstRistRtpExt::drop-null-ts-packets property is set, then it
 * will try to parse a MPEG Transport Stream inside the RTP packets
 * and look for "null" packets among the first 7 TS packets and remove
 * them, and mark their removal in the header.
 *
 * If the GstRistRtpExt::sequence-number-extension property is set, it will add
 * a RTP sequence number roll-over counter to the RTP header extension. This
 * code assumes that packets inserted to this element are never more than half
 * of the sequence number space (2^15) away from the latest. Re-transmissions
 * should therefore be done after processing with this element.
 *
 * If the GstRistRtpExt::drop-null-ts-packets and
 * GstRistRtpExt::sequence-number-extension properties are both FALSE, it is
 * pass through.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/rtp/rtp.h>

#include "gstrist.h"

GST_DEBUG_CATEGORY_STATIC (gst_rist_rtp_ext_debug);
#define GST_CAT_DEFAULT gst_rist_rtp_ext_debug

enum
{
  PROP_DROP_NULL_TS_PACKETS = 1,
  PROP_SEQUENCE_NUMBER_EXTENSION
};

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));


static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));


struct _GstRistRtpExt
{
  GstElement parent;

  GstPad *srcpad, *sinkpad;

  gboolean drop_null;
  gboolean add_seqnumext;

  guint32 extseqnum;
};

G_DEFINE_TYPE_WITH_CODE (GstRistRtpExt, gst_rist_rtp_ext, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_rist_rtp_ext_debug, "ristrtpext", 0,
        "RIST RTP Extension"));
GST_ELEMENT_REGISTER_DEFINE (ristrtpext, "ristrtpext", GST_RANK_NONE,
    GST_TYPE_RIST_RTP_EXT);

static GstFlowReturn
gst_rist_rtp_ext_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstRistRtpExt *self = GST_RIST_RTP_EXT (parent);
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  gboolean drop_null = self->drop_null;
  gboolean ts_packet_size = 0;
  guint ts_packet_count = 0;
  guint8 npd_bits = 0;
  gboolean num_packets_deleted = 0;
  guint8 *data;
  guint wordlen;

  if (!self->drop_null && !self->add_seqnumext)
    return gst_pad_push (self->srcpad, buffer);

  if (self->drop_null) {
    if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp)) {
      GST_ELEMENT_ERROR (self, STREAM, MUX, (NULL),
          ("Could not map RTP buffer"));
      goto mapping_error;
    }

    if (gst_rtp_buffer_get_payload_type (&rtp) == GST_RTP_PAYLOAD_MP2T) {
      if (gst_rtp_buffer_get_payload_len (&rtp) % 188 == 0) {
        ts_packet_size = 188;
        ts_packet_count = gst_rtp_buffer_get_payload_len (&rtp) / 188;
      } else if (gst_rtp_buffer_get_payload_len (&rtp) % 204 == 0) {
        ts_packet_size = 204;
        ts_packet_count = gst_rtp_buffer_get_payload_len (&rtp) / 204;
      } else {
        drop_null = FALSE;
      }
    }
    gst_rtp_buffer_unmap (&rtp);
  }

  buffer = gst_buffer_make_writable (buffer);

  if (!gst_rtp_buffer_map (buffer, GST_MAP_READWRITE, &rtp)) {
    GST_ELEMENT_ERROR (self, STREAM, MUX, (NULL), ("Could not map RTP buffer"));
    goto mapping_error;
  }

  if (drop_null) {
    guint8 *data = gst_rtp_buffer_get_payload (&rtp);
    guint plen = gst_rtp_buffer_get_payload_len (&rtp);
    guint i;

    if (gst_rtp_buffer_get_padding (&rtp)) {
      GST_ELEMENT_ERROR (self, STREAM, MUX, (NULL),
          ("FIXME: Can not remove null TS packets if RTP padding is present"));
      goto mapping_error;
    }

    for (i = 0; i < MIN (ts_packet_count, 7); i++) {
      guint offset = (i - num_packets_deleted) * ts_packet_size;
      guint16 pid;

      /* Look for sync byte (0x47) at the start of TS packets */
      if (data[offset] != 0x47) {
        GST_ELEMENT_ERROR (self, STREAM, MUX, (NULL),
            ("Buffer does not contain valid MP2T data,"
                " the sync byte is not present"));
        goto error_mapped;
      }

      pid = ((data[offset + 1] & 0x1F) << 8) | data[offset + 2];
      /* is NULL packet (PID == 0x1FFF means null) */
      if (pid == 0x1FFF) {
        guint remaining_plen = plen - (num_packets_deleted * ts_packet_size);

        num_packets_deleted++;
        npd_bits |= 1 << (6 - i);
        if (offset + ts_packet_size < remaining_plen)
          memmove (data + offset, data + offset + ts_packet_size,
              remaining_plen - offset - ts_packet_size);
      }
    }
  }

  if (gst_rtp_buffer_get_extension (&rtp)) {
    GST_ELEMENT_ERROR (self, STREAM, MUX, (NULL),
        ("RTP buffer already has an extension set"));
    goto error_mapped;
  }

  gst_rtp_buffer_set_extension (&rtp, TRUE);
  gst_rtp_buffer_set_extension_data (&rtp, 'R' << 8 | 'I', 1);
  gst_rtp_buffer_get_extension_data (&rtp, NULL, (void **) &data, &wordlen);

  data[0] = drop_null << 7;
  data[0] |= self->add_seqnumext << 6;
  if (ts_packet_count <= 7)
    data[0] |= (ts_packet_count & 7) << 3;      /* Size */

  data[1] = (ts_packet_size == 204) << 7;
  data[1] |= (npd_bits & 0x7F);

  if (self->add_seqnumext) {
    guint16 seqnum = gst_rtp_buffer_get_seq (&rtp);
    guint32 extseqnum;

    if (GST_BUFFER_IS_DISCONT (buffer))
      self->extseqnum = -1;

    extseqnum = gst_rist_rtp_ext_seq (&self->extseqnum, seqnum);

    GST_WRITE_UINT16_BE (data + 2, (extseqnum >> 16));
  }

  gst_rtp_buffer_unmap (&rtp);

  if (num_packets_deleted != 0)
    gst_buffer_resize (buffer, 0,
        gst_buffer_get_size (buffer) - (ts_packet_size * num_packets_deleted));

  return gst_pad_push (self->srcpad, buffer);

mapping_error:
  gst_buffer_unref (buffer);
  return GST_FLOW_ERROR;

error_mapped:
  gst_rtp_buffer_unmap (&rtp);
  gst_buffer_unref (buffer);
  return GST_FLOW_ERROR;
}

static void
gst_rist_rtp_ext_init (GstRistRtpExt * self)
{
  self->extseqnum = -1;

  self->sinkpad = gst_pad_new_from_static_template (&sink_templ,
      sink_templ.name_template);
  self->srcpad = gst_pad_new_from_static_template (&src_templ,
      src_templ.name_template);

  GST_PAD_SET_PROXY_ALLOCATION (self->sinkpad);
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);
  gst_pad_set_chain_function (self->sinkpad, gst_rist_rtp_ext_chain);

  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}

static void
gst_rist_rtp_ext_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRistRtpExt *self = GST_RIST_RTP_EXT (object);

  switch (prop_id) {
    case PROP_DROP_NULL_TS_PACKETS:
      g_value_set_boolean (value, self->drop_null);
      break;
    case PROP_SEQUENCE_NUMBER_EXTENSION:
      g_value_set_boolean (value, self->add_seqnumext);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rist_rtp_ext_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRistRtpExt *self = GST_RIST_RTP_EXT (object);

  switch (prop_id) {
    case PROP_DROP_NULL_TS_PACKETS:
      self->drop_null = g_value_get_boolean (value);
      break;
    case PROP_SEQUENCE_NUMBER_EXTENSION:
      self->add_seqnumext = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rist_rtp_ext_class_init (GstRistRtpExtClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  gst_element_class_set_metadata (element_class,
      "RIST RTP Extension adder", "Filter/Network",
      "Adds RIST TR-06-2 RTP Header extension",
      "Olivier Crete <olivier.crete@collabora.com");
  gst_element_class_add_static_pad_template (element_class, &src_templ);
  gst_element_class_add_static_pad_template (element_class, &sink_templ);

  object_class->get_property = gst_rist_rtp_ext_get_property;
  object_class->set_property = gst_rist_rtp_ext_set_property;

  g_object_class_install_property (object_class, PROP_DROP_NULL_TS_PACKETS,
      g_param_spec_boolean ("drop-null-ts-packets", "Drop null TS packets",
          "Drop null MPEG-TS packet and replace them with a custom header"
          " extension.", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_SEQUENCE_NUMBER_EXTENSION,
      g_param_spec_boolean ("sequence-number-extension",
          "Sequence Number Extension",
          "Add sequence number extension to packets.", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
}
