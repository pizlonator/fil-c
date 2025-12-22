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

#ifndef __GST_CURL_BASE_SINK__
#define __GST_CURL_BASE_SINK__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <curl/curl.h>

G_BEGIN_DECLS
#define GST_TYPE_CURL_BASE_SINK \
  (gst_curl_base_sink_get_type())
#define GST_CURL_BASE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CURL_BASE_SINK, GstCurlBaseSink))
#define GST_CURL_BASE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CURL_BASE_SINK, GstCurlBaseSinkClass))
#define GST_CURL_BASE_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CURL_BASE_SINK, GstCurlBaseSinkClass))
#define GST_IS_CURL_BASE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CURL_BASE_SINK))
#define GST_IS_CURL_BASE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CURL_BASE_SINK))
typedef struct _GstCurlBaseSink GstCurlBaseSink;
typedef struct _GstCurlBaseSinkClass GstCurlBaseSinkClass;

typedef struct _TransferBuffer TransferBuffer;
typedef struct _TransferCondition TransferCondition;

struct _TransferBuffer
{
  guint8 *ptr;
  size_t len;
  size_t offset;
};

struct _TransferCondition
{
  GCond cond;
  gboolean data_sent;
  gboolean data_available;
  gboolean wait_for_response;
};

struct _GstCurlBaseSink
{
  GstBaseSink parent;

  /*< private > */
  CURLM *multi_handle;
  CURL *curl;
  GstPollFD fd;
  GstPoll *fdset;
  curlsocktype socket_type;
  GThread *transfer_thread;
  gchar *error;
  GstFlowReturn flow_ret;
  TransferBuffer *transfer_buf;
  TransferCondition *transfer_cond;
  gint num_buffers_per_packet;
  gint timeout;
  gchar *url;
  gchar *user;
  gchar *passwd;
  gchar *file_name;
  guint qos_dscp;
  gboolean transfer_thread_close;
  gboolean new_file;
  gboolean is_live;
};

struct _GstCurlBaseSinkClass
{
  GstBaseSinkClass parent_class;

  /* vmethods */
    gboolean (*set_protocol_dynamic_options_unlocked) (GstCurlBaseSink * sink);
    gboolean (*set_options_unlocked) (GstCurlBaseSink * sink);
  void (*set_mime_type) (GstCurlBaseSink * sink, GstCaps * caps);
  void (*transfer_prepare_poll_wait) (GstCurlBaseSink * sink);
    glong (*transfer_get_response_code) (GstCurlBaseSink * sink, glong resp);
    gboolean (*transfer_verify_response_code) (GstCurlBaseSink * sink);
    GstFlowReturn (*prepare_transfer) (GstCurlBaseSink * sink);
  void (*handle_transfer) (GstCurlBaseSink * sink);
    size_t (*transfer_read_cb) (void *curl_ptr, size_t size, size_t nmemb,
      void *stream);
    size_t (*transfer_data_buffer) (GstCurlBaseSink * sink, void *curl_ptr,
      size_t block_size, guint * last_chunk);
    size_t (*flush_data_unlocked) (GstCurlBaseSink * sink, void *curl_ptr,
      size_t block_size, gboolean new_file, gboolean close_transfer);
    gboolean (*has_buffered_data_unlocked) (GstCurlBaseSink * sink);
};

GType gst_curl_base_sink_get_type (void);

void gst_curl_base_sink_transfer_thread_notify_unlocked
    (GstCurlBaseSink * sink);
void gst_curl_base_sink_set_live (GstCurlBaseSink * sink, gboolean live);
gboolean gst_curl_base_sink_is_live (GstCurlBaseSink * sink);

G_END_DECLS
#endif
