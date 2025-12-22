/* GStreamer
 * Copyright (C) 2011 Axis Communications <dev-gstreamer@axis.com>
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

#ifndef __GST_CURL_SMTP_SINK__
#define __GST_CURL_SMTP_SINK__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <curl/curl.h>
#include "gstcurltlssink.h"

G_BEGIN_DECLS
#define GST_TYPE_CURL_SMTP_SINK \
  (gst_curl_smtp_sink_get_type())
#define GST_CURL_SMTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CURL_SMTP_SINK, GstCurlSmtpSink))
#define GST_CURL_SMTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CURL_SMTP_SINK, GstCurlSmtpSinkClass))
#define GST_IS_CURL_SMTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CURL_SMTP_SINK))
#define GST_IS_CURL_SMTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CURL_SMTP_SINK))
typedef struct _GstCurlSmtpSink GstCurlSmtpSink;
typedef struct _GstCurlSmtpSinkClass GstCurlSmtpSinkClass;

typedef struct _Base64Chunk Base64Chunk;

struct _Base64Chunk
{
  GByteArray *chunk_array;
  gint save;
  gint state;
};

struct _GstCurlSmtpSink
{
  GstCurlTlsSink parent;

  /*< private > */
  Base64Chunk *base64_chunk;
  GByteArray *payload_headers;
  struct curl_slist *curl_recipients;
  gchar *mail_rcpt;
  gchar *mail_from;
  gchar *subject;
  gchar *message_body;
  gchar *content_type;
  gboolean use_ssl;
  gint nbr_attachments;
  gchar *pop_user;
  gchar *pop_passwd;
  gchar *pop_location;
  CURL *pop_curl;

  gboolean transfer_end;
  GCond cond_transfer_end;

  gint curr_attachment;
  gboolean reset_transfer_options;
  gboolean final_boundary_added;
  gboolean eos;
};

struct _GstCurlSmtpSinkClass
{
  GstCurlTlsSinkClass parent_class;
};

GType gst_curl_smtp_sink_get_type (void);

G_END_DECLS
#endif
