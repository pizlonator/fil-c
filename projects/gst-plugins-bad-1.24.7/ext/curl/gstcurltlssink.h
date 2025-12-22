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

#ifndef __GST_CURL_TLS_SINK__
#define __GST_CURL_TLS_SINK__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <curl/curl.h>
#include "gstcurlbasesink.h"

G_BEGIN_DECLS
#define GST_TYPE_CURL_TLS_SINK \
  (gst_curl_tls_sink_get_type())
#define GST_CURL_TLS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CURL_TLS_SINK, GstCurlTlsSink))
#define GST_CURL_TLS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CURL_TLS_SINK, GstCurlTlsSinkClass))
#define GST_CURL_TLS_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CURL_TLS_SINK, GstCurlTlsSinkClass))
#define GST_IS_CURL_TLS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CURL_TLS_SINK))
#define GST_IS_CURL_TLS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CURL_TLS_SINK))
typedef struct _GstCurlTlsSink GstCurlTlsSink;
typedef struct _GstCurlTlsSinkClass GstCurlTlsSinkClass;

struct _GstCurlTlsSink
{
  GstCurlBaseSink parent;

  /*< private > */
  gchar *ca_cert;
  gchar *ca_path;
  gchar *crypto_engine;
  gboolean insecure;
};

struct _GstCurlTlsSinkClass
{
  GstCurlBaseSinkClass parent_class;

  /* vmethods */
    gboolean (*set_options_unlocked) (GstCurlBaseSink * sink);
};

GType gst_curl_tls_sink_get_type (void);

G_END_DECLS
#endif
