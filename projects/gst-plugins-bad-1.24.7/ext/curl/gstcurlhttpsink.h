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

#ifndef __GST_CURL_HTTP_SINK__
#define __GST_CURL_HTTP_SINK__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <curl/curl.h>
#include "gstcurltlssink.h"

G_BEGIN_DECLS
#define GST_TYPE_CURL_HTTP_SINK \
  (gst_curl_http_sink_get_type())
#define GST_CURL_HTTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CURL_HTTP_SINK, GstCurlHttpSink))
#define GST_CURL_HTTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CURL_HTTP_SINK, GstCurlHttpSinkClass))
#define GST_IS_CURL_HTTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CURL_HTTP_SINK))
#define GST_IS_CURL_HTTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CURL_HTTP_SINK))
typedef struct _GstCurlHttpSink GstCurlHttpSink;
typedef struct _GstCurlHttpSinkClass GstCurlHttpSinkClass;

struct _GstCurlHttpSink
{
  GstCurlTlsSink parent;

  /*< private > */
  struct curl_slist *header_list;
  gchar *proxy;
  guint proxy_port;
  gchar *proxy_user;
  gchar *proxy_passwd;
  gboolean use_content_length;
  gchar *content_type;
  gchar *discovered_content_type;
  gboolean use_proxy;
  gboolean proxy_headers_set;
  gboolean proxy_auth;
  gboolean proxy_conn_established;
  glong proxy_resp;
};

struct _GstCurlHttpSinkClass
{
  GstCurlTlsSinkClass parent_class;
};

GType gst_curl_http_sink_get_type (void);

G_END_DECLS
#endif
