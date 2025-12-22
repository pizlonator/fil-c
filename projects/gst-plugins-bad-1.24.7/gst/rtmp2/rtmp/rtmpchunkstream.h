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

#ifndef _GST_RTMP_CHUNK_STREAM_H_
#define _GST_RTMP_CHUNK_STREAM_H_

#include "rtmpmessage.h"

G_BEGIN_DECLS

#define GST_RTMP_CHUNK_STREAM_PROTOCOL 2

typedef struct _GstRtmpChunkStream GstRtmpChunkStream;
typedef struct _GstRtmpChunkStreams GstRtmpChunkStreams;

void gst_rtmp_chunk_stream_clear (GstRtmpChunkStream * cstream);

guint32 gst_rtmp_chunk_stream_parse_id (const guint8 * data, gsize size);
guint32 gst_rtmp_chunk_stream_parse_header (GstRtmpChunkStream * cstream,
    const guint8 * data, gsize size);
guint32 gst_rtmp_chunk_stream_parse_payload (GstRtmpChunkStream * cstream,
    guint32 chunk_size, guint8 ** data);
guint32 gst_rtmp_chunk_stream_wrote_payload (GstRtmpChunkStream * cstream,
    guint32 chunk_size);
GstBuffer * gst_rtmp_chunk_stream_parse_finish (GstRtmpChunkStream * cstream);

GstBuffer * gst_rtmp_chunk_stream_serialize_start (GstRtmpChunkStream * cstream,
    GstBuffer * buffer, guint32 chunk_size);
GstBuffer * gst_rtmp_chunk_stream_serialize_next (GstRtmpChunkStream * cstream,
    guint32 chunk_size);
GstBuffer * gst_rtmp_chunk_stream_serialize_all (GstRtmpChunkStream * cstream,
    GstBuffer * buffer, guint32 chunk_size);

GstRtmpChunkStreams * gst_rtmp_chunk_streams_new (void);
void gst_rtmp_chunk_streams_free (gpointer ptr);

GstRtmpChunkStream *
gst_rtmp_chunk_streams_get (GstRtmpChunkStreams * cstreams, guint32 id);


G_END_DECLS

#endif
