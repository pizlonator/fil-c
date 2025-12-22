/* GStreamer RIST plugin
 * Copyright (C) 2019 Net Insight AB
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#include <gst/gst.h>

#ifndef __GST_RIST_H__
#define __GST_RIST_H__

#define GST_TYPE_RIST_RTX_RECEIVE (gst_rist_rtx_receive_get_type())
#define GST_RIST_RTX_RECEIVE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RIST_RTX_RECEIVE, GstRistRtxReceive))
typedef struct _GstRistRtxReceive GstRistRtxReceive;
typedef struct {
  GstElementClass parent_class;
} GstRistRtxReceiveClass;
GType gst_rist_rtx_receive_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ristrtxreceive);

#define GST_TYPE_RIST_RTX_SEND (gst_rist_rtx_send_get_type())
#define GST_RIST_RTX_SEND(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RIST_RTX_SEND, GstRistRtxSend))
typedef struct _GstRistRtxSend GstRistRtxSend;
typedef struct {
  GstElementClass parent_class;
} GstRistRtxSendClass;
GType gst_rist_rtx_send_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ristrtxsend);

#define GST_TYPE_RIST_SRC          (gst_rist_src_get_type())
#define GST_RIST_SRC(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RIST_SRC,GstRistSrc))
typedef struct _GstRistSrc GstRistSrc;
typedef struct {
  GstBinClass parent;
} GstRistSrcClass;
GType gst_rist_src_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ristsrc);

#define GST_TYPE_RIST_SINK          (gst_rist_sink_get_type())
#define GST_RIST_SINK(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RIST_SINK,GstRistSink))
typedef struct _GstRistSink GstRistSink;
typedef struct {
  GstBinClass parent;
} GstRistSinkClass;
GType gst_rist_sink_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ristsink);

#define GST_TYPE_RIST_RTP_EXT      (gst_rist_rtp_ext_get_type())
#define GST_RIST_RTP_EXT(obj)      (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RIST_RTP_EXT,GstRistRtpExt))
typedef struct _GstRistRtpExt GstRistRtpExt;
typedef struct {
  GstElementClass parent;
} GstRistRtpExtClass;
GType gst_rist_rtp_ext_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ristrtpext);

#define GST_TYPE_RIST_RTP_DEEXT      (gst_rist_rtp_deext_get_type())
#define GST_RIST_RTP_DEEXT(obj)      (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RIST_RTP_DEEXT,GstRistRtpDeext))
typedef struct _GstRistRtpDeext GstRistRtpDeext;
typedef struct {
  GstElementClass parent;
} GstRistRtpDeextClass;
GType gst_rist_rtp_deext_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ristrtpdeext);

guint32 gst_rist_rtp_ext_seq (guint32 * extseqnum, guint16 seqnum);

void gst_rist_rtx_send_set_extseqnum (GstRistRtxSend *self, guint32 ssrc,
    guint16 seqnum_ext);
void gst_rist_rtx_send_clear_extseqnum (GstRistRtxSend *self, guint32 ssrc);

#endif
