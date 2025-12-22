/* GStreamer
 * Copyright (C) 2017, Collabora Ltd.
 *   Author:Justin Kim <justin.kim@collabora.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsrtelements.h"
#include "gstsrtsrc.h"
#include "gstsrtsink.h"

#ifndef GST_REMOVE_DEPRECATED

#define GST_TYPE_SRT_CLIENT_SRC gst_srt_client_src_get_type()
#define GST_TYPE_SRT_SERVER_SRC gst_srt_server_src_get_type()

#define GST_TYPE_SRT_CLIENT_SINK gst_srt_client_sink_get_type()
#define GST_TYPE_SRT_SERVER_SINK gst_srt_server_sink_get_type()

typedef GstSRTSrc GstSRTClientSrc;
typedef GstSRTSrcClass GstSRTClientSrcClass;

typedef GstSRTSrc GstSRTServerSrc;
typedef GstSRTSrcClass GstSRTServerSrcClass;

typedef GstSRTSink GstSRTClientSink;
typedef GstSRTSinkClass GstSRTClientSinkClass;

typedef GstSRTSink GstSRTServerSink;
typedef GstSRTSinkClass GstSRTServerSinkClass;

static GType gst_srt_client_src_get_type (void);
static GType gst_srt_server_src_get_type (void);
static GType gst_srt_client_sink_get_type (void);
static GType gst_srt_server_sink_get_type (void);

G_DEFINE_TYPE (GstSRTClientSrc, gst_srt_client_src, GST_TYPE_SRT_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (srtclientsrc, "srtclientsrc",
    GST_RANK_NONE, GST_TYPE_SRT_CLIENT_SRC, srt_element_init (plugin));

G_DEFINE_TYPE (GstSRTServerSrc, gst_srt_server_src, GST_TYPE_SRT_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (srtserversrc, "srtserversrc",
    GST_RANK_NONE, GST_TYPE_SRT_SERVER_SRC, srt_element_init (plugin));

G_DEFINE_TYPE (GstSRTClientSink, gst_srt_client_sink, GST_TYPE_SRT_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (srtclientsink, "srtclientsink",
    GST_RANK_NONE, GST_TYPE_SRT_CLIENT_SINK, srt_element_init (plugin));

G_DEFINE_TYPE (GstSRTServerSink, gst_srt_server_sink, GST_TYPE_SRT_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (srtserversink, "srtserversink",
    GST_RANK_NONE, GST_TYPE_SRT_SERVER_SINK, srt_element_init (plugin));

static void
gst_srt_client_src_init (GstSRTClientSrc * src)
{
}

static void
gst_srt_client_src_class_init (GstSRTClientSrcClass * klass)
{
}

static void
gst_srt_server_src_init (GstSRTServerSrc * src)
{
}

static void
gst_srt_server_src_class_init (GstSRTServerSrcClass * klass)
{
}

static void
gst_srt_client_sink_init (GstSRTClientSink * sink)
{
}

static void
gst_srt_client_sink_class_init (GstSRTClientSinkClass * klass)
{
}

static void
gst_srt_server_sink_init (GstSRTServerSink * sink)
{
}

static void
gst_srt_server_sink_class_init (GstSRTServerSinkClass * klass)
{
}

#endif
