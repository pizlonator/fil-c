/*
 * Copyright 2007 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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
 * SECTION:element-pcapparse
 * @title: pcapparse
 *
 * Extracts payloads from Ethernet-encapsulated IP packets.
 * Use #GstPcapParse:src-ip, #GstPcapParse:dst-ip,
 * #GstPcapParse:src-port and #GstPcapParse:dst-port to restrict which packets
 * should be included.
 *
 * The supported data format is the classical
 * [libpcap file format](https://wiki.wireshark.org/Development/LibpcapFileFormat)
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 filesrc location=h264crasher.pcap ! pcapparse ! rtph264depay
 * ! ffdec_h264 ! fakesink
 * ]| Read from a pcap dump file using filesrc, extract the raw UDP packets,
 * depayload and decode them.
 *
 */

/* TODO:
 * - Implement support for timestamping the buffers.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstpcapparse.h"

#include <string.h>

#ifndef G_OS_WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#else
#include <winsock2.h>
#endif


const guint GST_PCAPPARSE_MAGIC_MILLISECOND_NO_SWAP_ENDIAN = 0xa1b2c3d4;
const guint GST_PCAPPARSE_MAGIC_NANOSECOND_NO_SWAP_ENDIAN = 0xa1b23c4d;
const guint GST_PCAPPARSE_MAGIC_MILLISECOND_SWAP_ENDIAN = 0xd4c3b2a1;
const guint GST_PCAPPARSE_MAGIC_NANOSECOND_SWAP_ENDIAN = 0x4d3cb2a1;


enum
{
  PROP_0,
  PROP_SRC_IP,
  PROP_DST_IP,
  PROP_SRC_PORT,
  PROP_DST_PORT,
  PROP_CAPS,
  PROP_TS_OFFSET
};

GST_DEBUG_CATEGORY_STATIC (gst_pcap_parse_debug);
#define GST_CAT_DEFAULT gst_pcap_parse_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("raw/x-pcap"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void gst_pcap_parse_finalize (GObject * object);
static void gst_pcap_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_pcap_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static GstStateChangeReturn
gst_pcap_parse_change_state (GstElement * element, GstStateChange transition);

static void gst_pcap_parse_reset (GstPcapParse * self);

static GstFlowReturn gst_pcap_parse_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_pcap_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);


#define parent_class gst_pcap_parse_parent_class
G_DEFINE_TYPE (GstPcapParse, gst_pcap_parse, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (pcapparse, "pcapparse", GST_RANK_NONE,
    GST_TYPE_PCAP_PARSE);

static void
gst_pcap_parse_class_init (GstPcapParseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_pcap_parse_finalize;
  gobject_class->get_property = gst_pcap_parse_get_property;
  gobject_class->set_property = gst_pcap_parse_set_property;

  g_object_class_install_property (gobject_class,
      PROP_SRC_IP, g_param_spec_string ("src-ip", "Source IP",
          "Source IP to restrict to", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_DST_IP, g_param_spec_string ("dst-ip", "Destination IP",
          "Destination IP to restrict to", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_SRC_PORT, g_param_spec_int ("src-port", "Source port",
          "Source port to restrict to", -1, G_MAXUINT16, -1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_DST_PORT, g_param_spec_int ("dst-port", "Destination port",
          "Destination port to restrict to", -1, G_MAXUINT16, -1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "The caps of the source pad", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TS_OFFSET,
      g_param_spec_int64 ("ts-offset", "Timestamp Offset",
          "Relative timestamp offset (ns) to apply (-1 = use absolute packet time)",
          -1, G_MAXINT64, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  element_class->change_state = gst_pcap_parse_change_state;

  gst_element_class_set_static_metadata (element_class, "PCapParse",
      "Raw/Parser",
      "Parses a raw pcap stream",
      "Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>");

  GST_DEBUG_CATEGORY_INIT (gst_pcap_parse_debug, "pcapparse", 0, "pcap parser");
}

static void
gst_pcap_parse_init (GstPcapParse * self)
{
  self->sink_pad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_pcap_parse_chain));
  gst_pad_use_fixed_caps (self->sink_pad);
  gst_pad_set_event_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_pcap_sink_event));
  gst_element_add_pad (GST_ELEMENT (self), self->sink_pad);

  self->src_pad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_use_fixed_caps (self->src_pad);
  gst_element_add_pad (GST_ELEMENT (self), self->src_pad);

  self->src_ip = -1;
  self->dst_ip = -1;
  self->src_port = -1;
  self->dst_port = -1;
  self->offset = -1;

  self->adapter = gst_adapter_new ();

  gst_pcap_parse_reset (self);
}

static void
gst_pcap_parse_finalize (GObject * object)
{
  GstPcapParse *self = GST_PCAP_PARSE (object);

  g_object_unref (self->adapter);
  if (self->caps)
    gst_caps_unref (self->caps);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static const gchar *
get_ip_address_as_string (gint64 ip_addr)
{
  if (ip_addr >= 0) {
    struct in_addr addr;
    addr.s_addr = ip_addr;
    return inet_ntoa (addr);
  } else {
    return "";
  }
}

static void
set_ip_address_from_string (gint64 * ip_addr, const gchar * ip_str)
{
  if (ip_str[0] != '\0') {
    gulong addr = inet_addr (ip_str);
    if (addr != INADDR_NONE)
      *ip_addr = addr;
  } else {
    *ip_addr = -1;
  }
}

static void
gst_pcap_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPcapParse *self = GST_PCAP_PARSE (object);

  switch (prop_id) {
    case PROP_SRC_IP:
      g_value_set_string (value, get_ip_address_as_string (self->src_ip));
      break;

    case PROP_DST_IP:
      g_value_set_string (value, get_ip_address_as_string (self->dst_ip));
      break;

    case PROP_SRC_PORT:
      g_value_set_int (value, self->src_port);
      break;

    case PROP_DST_PORT:
      g_value_set_int (value, self->dst_port);
      break;

    case PROP_CAPS:
      gst_value_set_caps (value, self->caps);
      break;

    case PROP_TS_OFFSET:
      g_value_set_int64 (value, self->offset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_pcap_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPcapParse *self = GST_PCAP_PARSE (object);

  switch (prop_id) {
    case PROP_SRC_IP:
      set_ip_address_from_string (&self->src_ip, g_value_get_string (value));
      break;

    case PROP_DST_IP:
      set_ip_address_from_string (&self->dst_ip, g_value_get_string (value));
      break;

    case PROP_SRC_PORT:
      self->src_port = g_value_get_int (value);
      break;

    case PROP_DST_PORT:
      self->dst_port = g_value_get_int (value);
      break;

    case PROP_CAPS:
    {
      const GstCaps *new_caps_val;
      GstCaps *new_caps, *old_caps;

      new_caps_val = gst_value_get_caps (value);
      if (new_caps_val == NULL) {
        new_caps = gst_caps_new_any ();
      } else {
        new_caps = gst_caps_copy (new_caps_val);
      }

      old_caps = self->caps;
      self->caps = new_caps;
      if (old_caps)
        gst_caps_unref (old_caps);

      gst_pad_set_caps (self->src_pad, new_caps);
      break;
    }

    case PROP_TS_OFFSET:
      self->offset = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_pcap_parse_reset (GstPcapParse * self)
{
  self->initialized = FALSE;
  self->swap_endian = FALSE;
  self->nanosecond_timestamp = FALSE;
  self->cur_packet_size = -1;
  self->cur_ts = GST_CLOCK_TIME_NONE;
  self->base_ts = GST_CLOCK_TIME_NONE;
  self->newsegment_sent = FALSE;
  self->first_packet = TRUE;

  gst_adapter_clear (self->adapter);
}

static guint32
gst_pcap_parse_read_uint32 (GstPcapParse * self, const guint8 * p)
{
  guint32 val = *((guint32 *) p);

  if (self->swap_endian) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    return GUINT32_FROM_BE (val);
#else
    return GUINT32_FROM_LE (val);
#endif
  } else {
    return val;
  }
}

#define ETH_MAC_ADDRESSES_LEN    12
#define ETH_HEADER_LEN    14
#define ETH_VLAN_HEADER_LEN    4
#define SLL_HEADER_LEN    16
#define SLL2_HEADER_LEN    20
#define IP_HEADER_MIN_LEN 20
#define UDP_HEADER_LEN     8

#define IP_PROTO_UDP      17
#define IP_PROTO_TCP      6


static gboolean
gst_pcap_parse_scan_frame (GstPcapParse * self,
    const guint8 * buf,
    gint buf_size, const guint8 ** payload, gint * payload_size)
{
  const guint8 *buf_ip = 0;
  const guint8 *buf_proto;
  guint16 eth_type;
  guint8 b;
  guint8 ip_header_size;
  guint8 flags;
  guint16 fragment_offset;
  guint8 ip_protocol;
  guint32 ip_src_addr;
  guint32 ip_dst_addr;
  guint16 src_port;
  guint16 dst_port;
  guint16 len;
  guint16 ip_packet_len;

  switch (self->linktype) {
    case LINKTYPE_ETHER:
      if (buf_size < ETH_HEADER_LEN + IP_HEADER_MIN_LEN + UDP_HEADER_LEN)
        return FALSE;
      eth_type = GUINT16_FROM_BE (*((guint16 *) (buf + ETH_MAC_ADDRESSES_LEN)));
      /* check for vlan 802.1q header (4 bytes, with first two bytes equal to 0x8100)  */
      if (eth_type == 0x8100) {
        if (buf_size <
            ETH_HEADER_LEN + ETH_VLAN_HEADER_LEN + IP_HEADER_MIN_LEN +
            UDP_HEADER_LEN)
          return FALSE;
        eth_type =
            GUINT16_FROM_BE (*((guint16 *) (buf + ETH_MAC_ADDRESSES_LEN +
                    ETH_VLAN_HEADER_LEN)));
        buf_ip = buf + ETH_HEADER_LEN + ETH_VLAN_HEADER_LEN;
      } else {
        buf_ip = buf + ETH_HEADER_LEN;
      }
      break;
    case LINKTYPE_SLL:
      if (buf_size < SLL_HEADER_LEN + IP_HEADER_MIN_LEN + UDP_HEADER_LEN)
        return FALSE;

      eth_type = GUINT16_FROM_BE (*((guint16 *) (buf + 14)));
      buf_ip = buf + SLL_HEADER_LEN;
      break;
    case LINKTYPE_SLL2:
      if (buf_size < SLL2_HEADER_LEN + IP_HEADER_MIN_LEN + UDP_HEADER_LEN)
        return FALSE;

      eth_type = GUINT16_FROM_BE (*((guint16 *) (buf)));
      buf_ip = buf + SLL2_HEADER_LEN;
      break;
    case LINKTYPE_RAW:
      if (buf_size < IP_HEADER_MIN_LEN + UDP_HEADER_LEN)
        return FALSE;

      eth_type = 0x800;         /* This is fine since IPv4/IPv6 is parse elsewhere */
      buf_ip = buf;
      break;

    default:
      return FALSE;
  }

  if (eth_type != 0x800) {
    GST_ERROR_OBJECT (self,
        "Link type %d: Ethernet type %d is not supported; only type 0x800",
        (gint) self->linktype, (gint) eth_type);
    return FALSE;
  }

  b = *buf_ip;

  /* Check that the packet is IPv4 */
  if (((b >> 4) & 0x0f) != 4)
    return FALSE;

  ip_header_size = (b & 0x0f) * 4;
  if (buf_ip + ip_header_size > buf + buf_size)
    return FALSE;

  flags = buf_ip[6] >> 5;
  fragment_offset =
      (GUINT16_FROM_BE (*((guint16 *) (buf_ip + 6))) & 0x1fff) * 8;
  if (flags & 0x1 || fragment_offset > 0) {
    GST_ERROR_OBJECT (self, "Fragmented packets are not supported");
    return FALSE;
  }

  ip_protocol = *(buf_ip + 9);
  GST_LOG_OBJECT (self, "ip proto %d", (gint) ip_protocol);

  if (ip_protocol != IP_PROTO_UDP && ip_protocol != IP_PROTO_TCP)
    return FALSE;

  /* ip info */
  memcpy (&ip_src_addr, buf_ip + 12, sizeof (ip_src_addr));
  memcpy (&ip_dst_addr, buf_ip + 16, sizeof (ip_dst_addr));
  buf_proto = buf_ip + ip_header_size;
  ip_packet_len = GUINT16_FROM_BE (*(guint16 *) (buf_ip + 2));

  /* ok for tcp and udp */
  src_port = GUINT16_FROM_BE (*((guint16 *) (buf_proto + 0)));
  dst_port = GUINT16_FROM_BE (*((guint16 *) (buf_proto + 2)));

  /* extract some params and data according to protocol */
  if (ip_protocol == IP_PROTO_UDP) {
    len = GUINT16_FROM_BE (*((guint16 *) (buf_proto + 4)));
    if (len < UDP_HEADER_LEN || buf_proto + len > buf + buf_size)
      return FALSE;

    *payload = buf_proto + UDP_HEADER_LEN;
    *payload_size = len - UDP_HEADER_LEN;
  } else {
    if (buf_proto + 12 >= buf + buf_size)
      return FALSE;
    len = (buf_proto[12] >> 4) * 4;
    if (buf_proto + len > buf + buf_size)
      return FALSE;

    /* all remaining data following tcp header is payload */
    *payload = buf_proto + len;
    *payload_size = ip_packet_len - ip_header_size - len;
  }

  /* but still filter as configured */
  if (self->src_ip >= 0 && ip_src_addr != self->src_ip)
    return FALSE;

  if (self->dst_ip >= 0 && ip_dst_addr != self->dst_ip)
    return FALSE;

  if (self->src_port >= 0 && src_port != self->src_port)
    return FALSE;

  if (self->dst_port >= 0 && dst_port != self->dst_port)
    return FALSE;

  return TRUE;
}

static GstFlowReturn
gst_pcap_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstPcapParse *self = GST_PCAP_PARSE (parent);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBufferList *list = NULL;

  gst_adapter_push (self->adapter, buffer);

  while (ret == GST_FLOW_OK) {
    gint avail;
    const guint8 *data;

    avail = gst_adapter_available (self->adapter);

    if (self->initialized) {
      if (self->cur_packet_size >= 0) {
        /* Parse the Packet Data */
        if (avail < self->cur_packet_size)
          break;

        if (self->cur_packet_size > 0) {
          const guint8 *payload_data;
          gint payload_size;

          data = gst_adapter_map (self->adapter, self->cur_packet_size);

          GST_LOG_OBJECT (self, "examining packet size %" G_GINT64_FORMAT,
              self->cur_packet_size);

          if (gst_pcap_parse_scan_frame (self, data, self->cur_packet_size,
                  &payload_data, &payload_size)) {
            GstBuffer *out_buf;
            guintptr offset = payload_data - data;

            gst_adapter_unmap (self->adapter);
            gst_adapter_flush (self->adapter, offset);
            /* we don't use _take_buffer_fast() on purpose here, we need a
             * buffer with a single memory, since the RTP depayloaders expect
             * the complete RTP header to be in the first memory if there are
             * multiple ones and we can't guarantee that with _fast() */
            if (payload_size > 0) {
              out_buf = gst_adapter_take_buffer (self->adapter, payload_size);
            } else {
              out_buf = gst_buffer_new ();
            }

            /* only first packet should have DISCONT flag */
            if (G_LIKELY (!self->first_packet)) {
              GST_BUFFER_FLAG_UNSET (out_buf, GST_BUFFER_FLAG_DISCONT);
            } else {
              GST_BUFFER_FLAG_SET (out_buf, GST_BUFFER_FLAG_DISCONT);
              self->first_packet = FALSE;
            }

            gst_adapter_flush (self->adapter,
                self->cur_packet_size - offset - payload_size);

            if (GST_CLOCK_TIME_IS_VALID (self->cur_ts)) {
              if (!GST_CLOCK_TIME_IS_VALID (self->base_ts))
                self->base_ts = self->cur_ts;
              if (self->offset >= 0) {
                self->cur_ts -= self->base_ts;
                self->cur_ts += self->offset;
              }
            }
            GST_BUFFER_DTS (out_buf) = self->cur_ts;


            if (list == NULL)
              list = gst_buffer_list_new ();
            gst_buffer_list_add (list, out_buf);
          } else {
            gst_adapter_unmap (self->adapter);
            gst_adapter_flush (self->adapter, self->cur_packet_size);
          }
        }

        self->cur_packet_size = -1;
      } else {
        /* Parse the Record (Packet) Header */
        guint32 ts_sec;
        guint32 ts_usec;
        guint32 incl_len;

        /* sizeof(pcaprec_hdr_t) == 16 */
        if (avail < 16)
          break;

        data = gst_adapter_map (self->adapter, 16);

        ts_sec = gst_pcap_parse_read_uint32 (self, data + 0);
        ts_usec = gst_pcap_parse_read_uint32 (self, data + 4);
        incl_len = gst_pcap_parse_read_uint32 (self, data + 8);
        /* orig_len = gst_pcap_parse_read_uint32 (self, data + 12); */

        gst_adapter_unmap (self->adapter);
        gst_adapter_flush (self->adapter, 16);

        self->cur_ts =
            ts_sec * GST_SECOND +
            ts_usec * (self->nanosecond_timestamp ? 1 : GST_USECOND);
        self->cur_packet_size = incl_len;
      }
    } else {
      /* Parse the Global Header */
      guint32 magic;
      guint32 linktype;
      guint16 major_version;

      /* sizeof(pcap_hdr_t) == 24 */
      if (avail < 24)
        break;

      data = gst_adapter_map (self->adapter, 24);

      magic = *((guint32 *) data);
      major_version = *((guint16 *) (data + 4));
      linktype = *((guint32 *) (data + 20));
      gst_adapter_unmap (self->adapter);

      if (magic == GST_PCAPPARSE_MAGIC_MILLISECOND_NO_SWAP_ENDIAN ||
          magic == GST_PCAPPARSE_MAGIC_NANOSECOND_NO_SWAP_ENDIAN) {
        self->swap_endian = FALSE;
        if (magic == GST_PCAPPARSE_MAGIC_NANOSECOND_NO_SWAP_ENDIAN)
          self->nanosecond_timestamp = TRUE;
      } else if (magic == GST_PCAPPARSE_MAGIC_MILLISECOND_SWAP_ENDIAN ||
          magic == GST_PCAPPARSE_MAGIC_NANOSECOND_SWAP_ENDIAN) {
        self->swap_endian = TRUE;
        if (magic == GST_PCAPPARSE_MAGIC_NANOSECOND_SWAP_ENDIAN)
          self->nanosecond_timestamp = TRUE;
        major_version = GUINT16_SWAP_LE_BE (major_version);
        linktype = GUINT32_SWAP_LE_BE (linktype);
      } else {
        GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, (NULL),
            ("File is not a libpcap file, magic is %X", magic));
        ret = GST_FLOW_ERROR;
        goto out;
      }

      if (major_version != 2) {
        GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, (NULL),
            ("File is not a libpcap major version 2, but %u", major_version));
        ret = GST_FLOW_ERROR;
        goto out;
      }

      if (linktype != LINKTYPE_ETHER && linktype != LINKTYPE_SLL &&
          linktype != LINKTYPE_RAW && linktype != LINKTYPE_SLL2) {
        GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, (NULL),
            ("Only dumps of type Ethernet, raw IP or Linux Cooked (SLL/SLL2) "
                "understood; type %d unknown", linktype));
        ret = GST_FLOW_ERROR;
        goto out;
      }

      GST_DEBUG_OBJECT (self, "linktype %u", linktype);
      self->linktype = linktype;

      gst_adapter_flush (self->adapter, 24);
      self->initialized = TRUE;
    }
  }

  if (list) {
    if (!self->newsegment_sent && GST_CLOCK_TIME_IS_VALID (self->cur_ts)) {
      GstSegment segment;

      if (self->caps)
        gst_pad_set_caps (self->src_pad, self->caps);
      gst_segment_init (&segment, GST_FORMAT_TIME);
      segment.start = self->base_ts;
      gst_pad_push_event (self->src_pad, gst_event_new_segment (&segment));
      self->newsegment_sent = TRUE;
    }

    ret = gst_pad_push_list (self->src_pad, list);
    list = NULL;
  }

out:

  if (list)
    gst_buffer_list_unref (list);

  return ret;
}

static gboolean
gst_pcap_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret = TRUE;
  GstPcapParse *self = GST_PCAP_PARSE (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      /* Drop it, we'll replace it with our own */
      gst_event_unref (event);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_pcap_parse_reset (self);
      /* Push event down the pipeline so that other elements stop flushing */
      /* fall through */
    default:
      ret = gst_pad_push_event (self->src_pad, event);
      break;
  }

  return ret;
}

static GstStateChangeReturn
gst_pcap_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstPcapParse *self = GST_PCAP_PARSE (element);
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_pcap_parse_reset (self);
      break;
    default:
      break;
  }


  return ret;
}
