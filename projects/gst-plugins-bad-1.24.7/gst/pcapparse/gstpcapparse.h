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

#ifndef __GST_PCAP_PARSE_H__
#define __GST_PCAP_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_PCAP_PARSE \
  (gst_pcap_parse_get_type ())
#define GST_PCAP_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PCAP_PARSE, GstPcapParse))
#define GST_PCAP_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PCAP_PARSE, GstPcapParseClass))
#define GST_IS_PCAP_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PCAP_PARSE))
#define GST_IS_PCAP_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PCAP_PARSE))

typedef struct _GstPcapParse      GstPcapParse;
typedef struct _GstPcapParseClass GstPcapParseClass;

typedef enum
{
  PCAP_PARSE_STATE_CREATED,
  PCAP_PARSE_STATE_PARSING,
} GstPcapParseState;

typedef enum
{
  LINKTYPE_ETHER  = 1,
  LINKTYPE_RAW = 101,
  LINKTYPE_SLL = 113,
  LINKTYPE_SLL2 = 276
} GstPcapParseLinktype;

/**
 * GstPcapParse:
 *
 * GstPcapParse element.
 */

struct _GstPcapParse
{
  GstElement element;

  /*< private >*/
  GstPad * sink_pad;
  GstPad * src_pad;

  /* properties */
  gint64 src_ip;
  gint64 dst_ip;
  gint32 src_port;
  gint32 dst_port;
  GstCaps *caps;
  gint64 offset;

  /* state */
  GstAdapter * adapter;
  gboolean initialized;
  gboolean swap_endian;
  gboolean nanosecond_timestamp;
  gint64 cur_packet_size;
  GstClockTime cur_ts;
  GstClockTime base_ts;
  GstPcapParseLinktype linktype;

  gboolean newsegment_sent;
  gboolean first_packet;
};

struct _GstPcapParseClass
{
  GstElementClass parent_class;
};

GType gst_pcap_parse_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (pcapparse);

G_END_DECLS

#endif /* __GST_PCAP_PARSE_H__ */
