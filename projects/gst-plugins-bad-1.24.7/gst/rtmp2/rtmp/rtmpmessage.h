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

#ifndef _GST_RTMP_MESSAGE_H_
#define _GST_RTMP_MESSAGE_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_RTMP_MAXIMUM_MESSAGE_SIZE 0xFFFFFF

typedef enum {
  GST_RTMP_MESSAGE_TYPE_INVALID = 0,
  GST_RTMP_MESSAGE_TYPE_SET_CHUNK_SIZE = 1,
  GST_RTMP_MESSAGE_TYPE_ABORT_MESSAGE = 2,
  GST_RTMP_MESSAGE_TYPE_ACKNOWLEDGEMENT = 3,
  GST_RTMP_MESSAGE_TYPE_USER_CONTROL = 4,
  GST_RTMP_MESSAGE_TYPE_WINDOW_ACK_SIZE = 5,
  GST_RTMP_MESSAGE_TYPE_SET_PEER_BANDWIDTH = 6,
  GST_RTMP_MESSAGE_TYPE_AUDIO = 8,
  GST_RTMP_MESSAGE_TYPE_VIDEO = 9,
  GST_RTMP_MESSAGE_TYPE_DATA_AMF3 = 15,
  GST_RTMP_MESSAGE_TYPE_SHARED_OBJECT_AMF3 = 16,
  GST_RTMP_MESSAGE_TYPE_COMMAND_AMF3 = 17,
  GST_RTMP_MESSAGE_TYPE_DATA_AMF0 = 18,
  GST_RTMP_MESSAGE_TYPE_SHARED_OBJECT_AMF0 = 19,
  GST_RTMP_MESSAGE_TYPE_COMMAND_AMF0 = 20,
  GST_RTMP_MESSAGE_TYPE_AGGREGATE = 22,
} GstRtmpMessageType;

gboolean gst_rtmp_message_type_is_valid (GstRtmpMessageType type);
gboolean gst_rtmp_message_type_is_protocol_control (GstRtmpMessageType type);
const gchar * gst_rtmp_message_type_get_nick (GstRtmpMessageType type);

typedef enum
{
  GST_RTMP_USER_CONTROL_TYPE_STREAM_BEGIN = 0,
  GST_RTMP_USER_CONTROL_TYPE_STREAM_EOF = 1,
  GST_RTMP_USER_CONTROL_TYPE_STREAM_DRY = 2,
  GST_RTMP_USER_CONTROL_TYPE_SET_BUFFER_LENGTH = 3,
  GST_RTMP_USER_CONTROL_TYPE_STREAM_IS_RECORDED = 4,
  GST_RTMP_USER_CONTROL_TYPE_PING_REQUEST = 6,
  GST_RTMP_USER_CONTROL_TYPE_PING_RESPONSE = 7,

  /* undocumented */
  GST_RTMP_USER_CONTROL_TYPE_SWF_VERIFICATION_REQUEST = 26,
  GST_RTMP_USER_CONTROL_TYPE_SWF_VERIFICATION_RESPONSE = 27,
  GST_RTMP_USER_CONTROL_TYPE_BUFFER_EMPTY = 31,
  GST_RTMP_USER_CONTROL_TYPE_BUFFER_READY = 32,
} GstRtmpUserControlType;

const gchar * gst_rtmp_user_control_type_get_nick (
    GstRtmpUserControlType type);

#define GST_RTMP_META_API_TYPE (gst_rtmp_meta_api_get_type())
#define GST_RTMP_META_INFO (gst_rtmp_meta_get_info())
typedef struct _GstRtmpMeta GstRtmpMeta;

struct _GstRtmpMeta {
  GstMeta meta;
  guint32 cstream;
  guint32 ts_delta;
  guint32 size;
  GstRtmpMessageType type;
  guint32 mstream;
};

GType gst_rtmp_meta_api_get_type (void);
const GstMetaInfo * gst_rtmp_meta_get_info (void);

GstRtmpMeta * gst_buffer_add_rtmp_meta (GstBuffer * buffer);

static inline GstRtmpMeta *
gst_buffer_get_rtmp_meta (GstBuffer * buffer)
{
  return (GstRtmpMeta *) gst_buffer_get_meta (buffer, GST_RTMP_META_API_TYPE);
}

GstBuffer * gst_rtmp_message_new (GstRtmpMessageType type, guint32 cstream,
    guint32 mstream);
GstBuffer * gst_rtmp_message_new_wrapped (GstRtmpMessageType type, guint32 cstream,
    guint32 mstream, guint8 * data, gsize size);

void gst_rtmp_buffer_dump (GstBuffer * buffer, const gchar * prefix);

GstRtmpMessageType gst_rtmp_message_get_type (GstBuffer * buffer);
gboolean gst_rtmp_message_is_protocol_control (GstBuffer * buffer);
gboolean gst_rtmp_message_is_user_control (GstBuffer * buffer);

typedef struct {
  GstRtmpMessageType type;

  /* for SET_CHUNK_SIZE: chunk size */
  /* for ABORT_MESSAGE: chunk stream ID */
  /* for ACKNOWLEDGEMENT: acknowledged byte count */
  /* for WINDOW_ACK_SIZE and SET_PEER_BANDWIDTH: acknowledgement window size */
  guint32 param;

  /* for SET_PEER_BANDWIDTH: limit type */
  guint8 param2;
} GstRtmpProtocolControl;

gboolean gst_rtmp_message_parse_protocol_control (GstBuffer * buffer,
    GstRtmpProtocolControl * out);

GstBuffer * gst_rtmp_message_new_protocol_control (GstRtmpProtocolControl * pc);

typedef struct {
  GstRtmpUserControlType type;

  /* for STREAM_BEGIN to STREAM_IS_RECORDED: message stream ID */
  /* for PING_REQUEST and PING_RESPONSE: timestamp of request */
  guint32 param;

  /* for SET_BUFFER_LENGTH: buffer length in ms */
  guint32 param2;
} GstRtmpUserControl;

gboolean gst_rtmp_message_parse_user_control (GstBuffer * buffer,
    GstRtmpUserControl * out);

GstBuffer * gst_rtmp_message_new_user_control (GstRtmpUserControl * uc);

gboolean gst_rtmp_message_is_metadata (GstBuffer * buffer);

G_END_DECLS

#endif
