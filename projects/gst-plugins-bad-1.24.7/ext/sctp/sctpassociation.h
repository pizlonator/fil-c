/*
 * Copyright (c) 2015, Collabora Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef __GST_SCTP_ASSOCIATION_H__
#define __GST_SCTP_ASSOCIATION_H__

#include <glib-object.h>
#include <gst/gst.h>
#define INET
#define INET6
#include <usrsctp.h>

G_BEGIN_DECLS

#define GST_SCTP_TYPE_ASSOCIATION                  (gst_sctp_association_get_type ())
#define GST_SCTP_ASSOCIATION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_SCTP_TYPE_ASSOCIATION, GstSctpAssociation))
#define GST_SCTP_IS_ASSOCIATION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_SCTP_TYPE_ASSOCIATION))
#define GST_SCTP_ASSOCIATION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GST_SCTP_TYPE_ASSOCIATION, GstSctpAssociationClass))
#define GST_SCTP_IS_ASSOCIATION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_SCTP_TYPE_ASSOCIATION))
#define GST_SCTP_ASSOCIATION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_SCTP_TYPE_ASSOCIATION, GstSctpAssociationClass))

typedef struct _GstSctpAssociation GstSctpAssociation;
typedef struct _GstSctpAssociationClass GstSctpAssociationClass;

typedef enum
{
  GST_SCTP_ASSOCIATION_STATE_NEW,
  GST_SCTP_ASSOCIATION_STATE_READY,
  GST_SCTP_ASSOCIATION_STATE_CONNECTING,
  GST_SCTP_ASSOCIATION_STATE_CONNECTED,
  GST_SCTP_ASSOCIATION_STATE_DISCONNECTING,
  GST_SCTP_ASSOCIATION_STATE_DISCONNECTED,
  GST_SCTP_ASSOCIATION_STATE_ERROR
} GstSctpAssociationState;

typedef enum
{
  GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_NONE = 0x0000,
  GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_TTL = 0x0001,
  GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_BUF = 0x0002,
  GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_RTX = 0x0003
} GstSctpAssociationPartialReliability;

typedef void (*GstSctpAssociationPacketReceivedCb) (GstSctpAssociation *
    sctp_association, guint8 * data, gsize length, guint16 stream_id,
    guint ppid, gpointer user_data);
typedef void (*GstSctpAssociationPacketOutCb) (GstSctpAssociation *
    sctp_association, const guint8 * data, gsize length, gpointer user_data);

struct _GstSctpAssociation
{
  GObject parent_instance;

  guint32 association_id;
  guint16 local_port;
  guint16 remote_port;
  gboolean use_sock_stream;
  struct socket *sctp_ass_sock;

  GMutex association_mutex;

  GstSctpAssociationState state;

  GstSctpAssociationPacketReceivedCb packet_received_cb;
  gpointer packet_received_user_data;
  GDestroyNotify packet_received_destroy_notify;

  GstSctpAssociationPacketOutCb packet_out_cb;
  gpointer packet_out_user_data;
  GDestroyNotify packet_out_destroy_notify;
};

struct _GstSctpAssociationClass
{
  GObjectClass parent_class;

  void (*on_sctp_stream_reset) (GstSctpAssociation * sctp_association,
      guint16 stream_id);
};

GType gst_sctp_association_get_type (void);

GstSctpAssociation *gst_sctp_association_get (guint32 association_id);

gboolean gst_sctp_association_start (GstSctpAssociation * self);
void gst_sctp_association_set_on_packet_out (GstSctpAssociation * self,
    GstSctpAssociationPacketOutCb packet_out_cb, gpointer user_data, GDestroyNotify destroy_notify);
void gst_sctp_association_set_on_packet_received (GstSctpAssociation * self,
    GstSctpAssociationPacketReceivedCb packet_received_cb, gpointer user_data, GDestroyNotify destroy_notify);
void gst_sctp_association_incoming_packet (GstSctpAssociation * self,
    const guint8 * buf, guint32 length);
GstFlowReturn gst_sctp_association_send_data (GstSctpAssociation * self,
    const guint8 * buf, guint32 length, guint16 stream_id, guint32 ppid,
    gboolean ordered, GstSctpAssociationPartialReliability pr,
    guint32 reliability_param, guint32 *bytes_sent);
void gst_sctp_association_reset_stream (GstSctpAssociation * self,
    guint16 stream_id);
void gst_sctp_association_force_close (GstSctpAssociation * self);

G_END_DECLS

#endif /* __GST_SCTP_ASSOCIATION_H__ */
