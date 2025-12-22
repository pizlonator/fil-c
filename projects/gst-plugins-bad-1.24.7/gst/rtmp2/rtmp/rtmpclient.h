/* GStreamer RTMP Library
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

#ifndef _GST_RTMP_CLIENT_H_
#define _GST_RTMP_CLIENT_H_

#include "rtmpconnection.h"

G_BEGIN_DECLS

#define GST_TYPE_RTMP_SCHEME (gst_rtmp_scheme_get_type ())

typedef enum
{
  GST_RTMP_SCHEME_RTMP = 0,
  GST_RTMP_SCHEME_RTMPS,
} GstRtmpScheme;

GType gst_rtmp_scheme_get_type (void);

GstRtmpScheme gst_rtmp_scheme_from_string (const gchar * string);
GstRtmpScheme gst_rtmp_scheme_from_uri (const GstUri * uri);
const gchar * gst_rtmp_scheme_to_string (GstRtmpScheme scheme);
const gchar * const * gst_rtmp_scheme_get_strings (void);
guint gst_rtmp_scheme_get_default_port (GstRtmpScheme scheme);



#define GST_TYPE_RTMP_AUTHMOD (gst_rtmp_authmod_get_type ())

typedef enum
{
  GST_RTMP_AUTHMOD_NONE = 0,
  GST_RTMP_AUTHMOD_AUTO,
  GST_RTMP_AUTHMOD_ADOBE,
} GstRtmpAuthmod;

GType gst_rtmp_authmod_get_type (void);



#define GST_TYPE_RTMP_STOP_COMMANDS (gst_rtmp_stop_commands_get_type ())
#define GST_RTMP_DEFAULT_STOP_COMMANDS (GST_RTMP_STOP_COMMANDS_FCUNPUBLISH | \
    GST_RTMP_STOP_COMMANDS_DELETE_STREAM) /* FCUnpublish + deleteStream */

/**
 * GstRtmpStopCommands:
 * @GST_RTMP_STOP_COMMANDS_NONE: Don't send any commands
 * @GST_RTMP_STOP_COMMANDS_FCUNPUBLISH: Send FCUnpublish command
 * @GST_RTMP_STOP_COMMANDS_CLOSE_STREAM: Send closeStream command
 * @GST_RTMP_STOP_COMMANDS_DELETE_STREAM: Send deleteStream command
 *
 * Since: 1.18
 */
typedef enum
{
  GST_RTMP_STOP_COMMANDS_NONE = 0,
  GST_RTMP_STOP_COMMANDS_FCUNPUBLISH = (1 << 0),
  GST_RTMP_STOP_COMMANDS_CLOSE_STREAM = (1 << 1),
  GST_RTMP_STOP_COMMANDS_DELETE_STREAM = (1 << 2)
} GstRtmpStopCommands;

GType gst_rtmp_stop_commands_get_type (void);



typedef struct _GstRtmpLocation
{
  GstRtmpScheme scheme;
  gchar *host;
  guint port;
  gchar *application;
  gchar *stream;
  gchar *username;
  gchar *password;
  gchar *secure_token;
  GstRtmpAuthmod authmod;
  gint timeout;
  GTlsCertificateFlags tls_flags;
  gchar *flash_ver;
  gboolean publish;
} GstRtmpLocation;

void gst_rtmp_location_copy (GstRtmpLocation * dest,
    const GstRtmpLocation * src);
void gst_rtmp_location_clear (GstRtmpLocation * uri);
gchar *gst_rtmp_location_get_string (const GstRtmpLocation * location,
    gboolean with_stream);



void gst_rtmp_client_connect_async (const GstRtmpLocation * location,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
GstRtmpConnection *gst_rtmp_client_connect_finish (GAsyncResult * result,
    GError ** error);
void gst_rtmp_client_start_publish_async (GstRtmpConnection * connection,
    const gchar * stream, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
gboolean gst_rtmp_client_start_publish_finish (GstRtmpConnection * connection,
    GAsyncResult * result, guint * stream_id, GError ** error);

void gst_rtmp_client_start_play_async (GstRtmpConnection * connection,
    const gchar * stream, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
gboolean gst_rtmp_client_start_play_finish (GstRtmpConnection * connection,
    GAsyncResult * result, guint * stream_id, GError ** error);

void gst_rtmp_client_stop_publish (GstRtmpConnection * connection,
    const gchar * stream, const GstRtmpStopCommands stop_commands);

G_END_DECLS
#endif
