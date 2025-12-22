/* GStreamer RTMP Library
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
 * Copyright (C) 2017 Make.TV, Inc. <info@make.tv>
 *   Contact: Jan Alexander Steffens (heftig) <jsteffens@make.tv>
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

#ifndef _GST_RTMP_CONNECTION_H_
#define _GST_RTMP_CONNECTION_H_

#include <gio/gio.h>
#include <gst/gst.h>
#include "amf.h"

G_BEGIN_DECLS

#define GST_TYPE_RTMP_CONNECTION   (gst_rtmp_connection_get_type())
#define GST_RTMP_CONNECTION(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_CONNECTION,GstRtmpConnection))
#define GST_IS_RTMP_CONNECTION(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_CONNECTION))

#define GST_RTMP_DEFAULT_CHUNK_SIZE 128
#define GST_RTMP_MINIMUM_CHUNK_SIZE 1
#define GST_RTMP_MAXIMUM_CHUNK_SIZE 0x7FFFFFFF

/* Matches librtmp */
#define GST_RTMP_DEFAULT_WINDOW_ACK_SIZE 2500000

typedef struct _GstRtmpConnection GstRtmpConnection;

typedef void (*GstRtmpConnectionFunc)
    (GstRtmpConnection * connection, gpointer user_data);
typedef void (*GstRtmpConnectionMessageFunc)
    (GstRtmpConnection * connection, GstBuffer * buffer, gpointer user_data);

typedef void (*GstRtmpCommandCallback) (const gchar * command_name,
    GPtrArray * arguments, gpointer user_data);

GType gst_rtmp_connection_get_type (void);

GstRtmpConnection *gst_rtmp_connection_new (GSocketConnection * connection, GCancellable * cancellable);

GSocket *gst_rtmp_connection_get_socket (GstRtmpConnection * connection);

void gst_rtmp_connection_close (GstRtmpConnection * connection);
void gst_rtmp_connection_close_and_unref (gpointer ptr);

void gst_rtmp_connection_set_input_handler (GstRtmpConnection * connection,
    GstRtmpConnectionMessageFunc callback, gpointer user_data,
    GDestroyNotify user_data_destroy);

void gst_rtmp_connection_set_output_handler (GstRtmpConnection * connection,
    GstRtmpConnectionFunc callback, gpointer user_data,
    GDestroyNotify user_data_destroy);

void gst_rtmp_connection_queue_bytes (GstRtmpConnection *self,
    GBytes * bytes);
void gst_rtmp_connection_queue_message (GstRtmpConnection * connection,
    GstBuffer * buffer);
guint gst_rtmp_connection_get_num_queued (GstRtmpConnection * connection);

guint gst_rtmp_connection_send_command (GstRtmpConnection * connection,
    GstRtmpCommandCallback response_command, gpointer user_data,
    guint32 stream_id, const gchar * command_name, const GstAmfNode * argument,
    ...) G_GNUC_NULL_TERMINATED;

void gst_rtmp_connection_expect_command (GstRtmpConnection * connection,
    GstRtmpCommandCallback response_command, gpointer user_data,
    guint32 stream_id, const gchar * command_name);

void gst_rtmp_connection_set_chunk_size (GstRtmpConnection * connection,
    guint32 chunk_size);
void gst_rtmp_connection_request_window_size (GstRtmpConnection * connection,
    guint32 window_ack_size);

void gst_rtmp_connection_set_data_frame (GstRtmpConnection * connection,
    GstBuffer * buffer);

GstStructure * gst_rtmp_connection_get_null_stats (void);
GstStructure * gst_rtmp_connection_get_stats (GstRtmpConnection * connection);

G_END_DECLS

#endif
