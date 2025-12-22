/*
 * gstrtponviftimestamp.h
 *
 * Copyright (C) 2014 Axis Communications AB
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GST_RTP_ONVIF_TIMESTAMP_H__
#define __GST_RTP_ONVIF_TIMESTAMP_H__


#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GST_TYPE_RTP_ONVIF_TIMESTAMP \
  (gst_rtp_onvif_timestamp_get_type())
#define GST_RTP_ONVIF_TIMESTAMP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_ONVIF_TIMESTAMP,GstRtpOnvifTimestamp))
#define GST_RTP_ONVIF_TIMESTAMP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_ONVIF_TIMESTAMP,GstRtpOnvifTimestampClass))
#define GST_IS_RTP_ONVIF_TIMESTAMP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_ONVIF_TIMESTAMP))
#define GST_IS_RTP_ONVIF_TIMESTAMP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_ONVIF_TIMESTAMP))

typedef struct _GstRtpOnvifTimestamp GstRtpOnvifTimestamp;
typedef struct _GstRtpOnvifTimestampClass GstRtpOnvifTimestampClass;

struct _GstRtpOnvifTimestamp {
  GstElement element;

  /* pads */
  GstPad *sinkpad,*srcpad;

  GstClockTime prop_ntp_offset;
  guint prop_cseq;
  gboolean prop_set_e_bit;
  gboolean prop_set_t_bit;
  gboolean prop_drop_out_of_segment;

  /* whether reference timestamps from the buffers should be used instead
   * of the ntp offset mechanism */
  gboolean prop_use_reference_timestamps;
  GstCaps *reference_timestamp_id;

  /* currently used ntp-offset
   *(can be changed runtime with a GstOnvifTimestamp event)
   */
  GstClockTime ntp_offset;
  /* a GstOnvifTimestamp event might mark the stream as discontinued */
  gboolean set_d_bit;
  gboolean set_e_bit;
  gboolean set_t_bit;

  GstSegment segment;
  /* Buffer waiting to be handled, only used if prop_set_e_bit is TRUE */
  GQueue *event_queue;
  GstBuffer *buffer;
  GstBufferList *list;
  guint current_list_size;
};

struct _GstRtpOnvifTimestampClass {
  GstElementClass parent_class;
};

GType gst_rtp_onvif_timestamp_get_type(void);
GST_ELEMENT_REGISTER_DECLARE (rtponviftimestamp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GST_RTP_ONVIF_TIMESTAMP_H__ */
