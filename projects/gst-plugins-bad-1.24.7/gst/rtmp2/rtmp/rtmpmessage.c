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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "amf.h"
#include "rtmpmessage.h"
#include "rtmpchunkstream.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_message_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_message_debug_category

gboolean
gst_rtmp_message_type_is_valid (GstRtmpMessageType type)
{
  switch (type) {
    case GST_RTMP_MESSAGE_TYPE_INVALID:
    case GST_RTMP_MESSAGE_TYPE_SET_CHUNK_SIZE:
    case GST_RTMP_MESSAGE_TYPE_ABORT_MESSAGE:
    case GST_RTMP_MESSAGE_TYPE_ACKNOWLEDGEMENT:
    case GST_RTMP_MESSAGE_TYPE_USER_CONTROL:
    case GST_RTMP_MESSAGE_TYPE_WINDOW_ACK_SIZE:
    case GST_RTMP_MESSAGE_TYPE_SET_PEER_BANDWIDTH:
    case GST_RTMP_MESSAGE_TYPE_AUDIO:
    case GST_RTMP_MESSAGE_TYPE_VIDEO:
    case GST_RTMP_MESSAGE_TYPE_DATA_AMF3:
    case GST_RTMP_MESSAGE_TYPE_SHARED_OBJECT_AMF3:
    case GST_RTMP_MESSAGE_TYPE_COMMAND_AMF3:
    case GST_RTMP_MESSAGE_TYPE_DATA_AMF0:
    case GST_RTMP_MESSAGE_TYPE_SHARED_OBJECT_AMF0:
    case GST_RTMP_MESSAGE_TYPE_COMMAND_AMF0:
    case GST_RTMP_MESSAGE_TYPE_AGGREGATE:
      return TRUE;
    default:
      return FALSE;
  }
}

gboolean
gst_rtmp_message_type_is_protocol_control (GstRtmpMessageType type)
{
  switch (type) {
    case GST_RTMP_MESSAGE_TYPE_SET_CHUNK_SIZE:
    case GST_RTMP_MESSAGE_TYPE_ABORT_MESSAGE:
    case GST_RTMP_MESSAGE_TYPE_ACKNOWLEDGEMENT:
    case GST_RTMP_MESSAGE_TYPE_WINDOW_ACK_SIZE:
    case GST_RTMP_MESSAGE_TYPE_SET_PEER_BANDWIDTH:
      return TRUE;

    default:
      return FALSE;
  }
}

const gchar *
gst_rtmp_message_type_get_nick (GstRtmpMessageType type)
{
  switch (type) {
    case GST_RTMP_MESSAGE_TYPE_INVALID:
      return "invalid";
    case GST_RTMP_MESSAGE_TYPE_SET_CHUNK_SIZE:
      return "set-chunk-size";
    case GST_RTMP_MESSAGE_TYPE_ABORT_MESSAGE:
      return "abort-message";
    case GST_RTMP_MESSAGE_TYPE_ACKNOWLEDGEMENT:
      return "acknowledgement";
    case GST_RTMP_MESSAGE_TYPE_USER_CONTROL:
      return "user-control";
    case GST_RTMP_MESSAGE_TYPE_WINDOW_ACK_SIZE:
      return "window-ack-size";
    case GST_RTMP_MESSAGE_TYPE_SET_PEER_BANDWIDTH:
      return "set-peer-bandwidth";
    case GST_RTMP_MESSAGE_TYPE_AUDIO:
      return "audio";
    case GST_RTMP_MESSAGE_TYPE_VIDEO:
      return "video";
    case GST_RTMP_MESSAGE_TYPE_DATA_AMF3:
      return "data-amf3";
    case GST_RTMP_MESSAGE_TYPE_SHARED_OBJECT_AMF3:
      return "shared-object-amf3";
    case GST_RTMP_MESSAGE_TYPE_COMMAND_AMF3:
      return "command-amf3";
    case GST_RTMP_MESSAGE_TYPE_DATA_AMF0:
      return "data-amf0";
    case GST_RTMP_MESSAGE_TYPE_SHARED_OBJECT_AMF0:
      return "shared-object-amf0";
    case GST_RTMP_MESSAGE_TYPE_COMMAND_AMF0:
      return "command-amf0";
    case GST_RTMP_MESSAGE_TYPE_AGGREGATE:
      return "aggregate";
    default:
      return "unknown";
  }
}

const gchar *
gst_rtmp_user_control_type_get_nick (GstRtmpUserControlType type)
{
  switch (type) {
    case GST_RTMP_USER_CONTROL_TYPE_STREAM_BEGIN:
      return "stream-begin";
    case GST_RTMP_USER_CONTROL_TYPE_STREAM_EOF:
      return "stream-eof";
    case GST_RTMP_USER_CONTROL_TYPE_STREAM_DRY:
      return "stream-dry";
    case GST_RTMP_USER_CONTROL_TYPE_SET_BUFFER_LENGTH:
      return "set-buffer-length";
    case GST_RTMP_USER_CONTROL_TYPE_STREAM_IS_RECORDED:
      return "stream-is-recorded";
    case GST_RTMP_USER_CONTROL_TYPE_PING_REQUEST:
      return "ping-request";
    case GST_RTMP_USER_CONTROL_TYPE_PING_RESPONSE:
      return "ping-response";
    case GST_RTMP_USER_CONTROL_TYPE_SWF_VERIFICATION_REQUEST:
      return "swf-verification-request";
    case GST_RTMP_USER_CONTROL_TYPE_SWF_VERIFICATION_RESPONSE:
      return "swf-verification-response";
    case GST_RTMP_USER_CONTROL_TYPE_BUFFER_EMPTY:
      return "buffer-empty";
    case GST_RTMP_USER_CONTROL_TYPE_BUFFER_READY:
      return "buffer-ready";
    default:
      return "unknown";
  }
}

GType
gst_rtmp_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = {
    NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstRtmpMetaAPI", tags);
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_message_debug_category,
        "rtmpmessage", 0, "debug category for rtmp messages");
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_rtmp_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstRtmpMeta *emeta = (GstRtmpMeta *) meta;

  emeta->cstream = 0;
  emeta->ts_delta = 0;
  emeta->size = 0;
  emeta->type = GST_RTMP_MESSAGE_TYPE_INVALID;
  emeta->mstream = 0;

  return TRUE;
}

static gboolean
gst_rtmp_meta_transform (GstBuffer * dest, GstMeta * meta, GstBuffer * buffer,
    GQuark type, gpointer data)
{
  GstRtmpMeta *smeta, *dmeta;

  if (!GST_META_TRANSFORM_IS_COPY (type)) {
    /* We only support copy transforms */
    return FALSE;
  }

  smeta = (GstRtmpMeta *) meta;
  dmeta = gst_buffer_get_rtmp_meta (dest);
  if (!dmeta) {
    dmeta = gst_buffer_add_rtmp_meta (dest);
  }

  dmeta->cstream = smeta->cstream;
  dmeta->ts_delta = smeta->ts_delta;
  dmeta->size = smeta->size;
  dmeta->type = smeta->type;
  dmeta->mstream = smeta->mstream;

  return TRUE;
}

const GstMetaInfo *
gst_rtmp_meta_get_info (void)
{
  static const GstMetaInfo *rtmp_meta_info = NULL;

  if (g_once_init_enter (&rtmp_meta_info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_RTMP_META_API_TYPE,
        "GstRtmpMeta", sizeof (GstRtmpMeta), gst_rtmp_meta_init, NULL,
        gst_rtmp_meta_transform);
    g_once_init_leave (&rtmp_meta_info, meta);
  }
  return rtmp_meta_info;
}

GstRtmpMeta *
gst_buffer_add_rtmp_meta (GstBuffer * buffer)
{
  GstRtmpMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  meta = (GstRtmpMeta *) gst_buffer_add_meta (buffer, GST_RTMP_META_INFO, NULL);
  g_assert (meta != NULL);

  return meta;
}

GstBuffer *
gst_rtmp_message_new (GstRtmpMessageType type, guint32 cstream, guint32 mstream)
{
  GstBuffer *buffer = gst_buffer_new ();
  GstRtmpMeta *meta = gst_buffer_add_rtmp_meta (buffer);

  meta->type = type;
  meta->cstream = cstream;
  meta->mstream = mstream;

  return buffer;
}

GstBuffer *
gst_rtmp_message_new_wrapped (GstRtmpMessageType type, guint32 cstream,
    guint32 mstream, guint8 * data, gsize size)
{
  GstBuffer *message = gst_rtmp_message_new (type, cstream, mstream);

  gst_buffer_append_memory (message,
      gst_memory_new_wrapped (0, data, size, 0, size, data, g_free));

  return message;
}

void
gst_rtmp_buffer_dump (GstBuffer * buffer, const gchar * prefix)
{
  GstRtmpMeta *meta;
  GstMapInfo map;

  if (G_LIKELY (GST_LEVEL_LOG > _gst_debug_min || GST_LEVEL_LOG >
          gst_debug_category_get_threshold (GST_CAT_DEFAULT))) {
    return;
  }

  g_return_if_fail (GST_IS_BUFFER (buffer));
  g_return_if_fail (prefix);

  GST_LOG ("%s %" GST_PTR_FORMAT, prefix, buffer);

  meta = gst_buffer_get_rtmp_meta (buffer);
  if (meta) {
    GST_LOG ("%s cstream:%-4" G_GUINT32_FORMAT " mstream:%-4" G_GUINT32_FORMAT
        " ts:%-8" G_GUINT32_FORMAT " len:%-6" G_GUINT32_FORMAT " type:%s",
        prefix, meta->cstream, meta->mstream, meta->ts_delta, meta->size,
        gst_rtmp_message_type_get_nick (meta->type));
  }

  if (G_LIKELY (GST_LEVEL_MEMDUMP > _gst_debug_min || GST_LEVEL_MEMDUMP >
          gst_debug_category_get_threshold (GST_CAT_DEFAULT))) {
    return;
  }

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ERROR ("Failed to map %" GST_PTR_FORMAT " for memdump", buffer);
    return;
  }

  if (map.size > 0) {
    GST_MEMDUMP (prefix, map.data, map.size);
  }

  gst_buffer_unmap (buffer, &map);
}

GstRtmpMessageType
gst_rtmp_message_get_type (GstBuffer * buffer)
{
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);
  g_return_val_if_fail (meta, GST_RTMP_MESSAGE_TYPE_INVALID);
  return meta->type;
}

gboolean
gst_rtmp_message_is_protocol_control (GstBuffer * buffer)
{
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);

  g_return_val_if_fail (meta, FALSE);

  if (!gst_rtmp_message_type_is_protocol_control (meta->type)) {
    return FALSE;
  }

  if (meta->cstream != GST_RTMP_CHUNK_STREAM_PROTOCOL) {
    GST_WARNING ("Protocol control message on chunk stream %"
        G_GUINT32_FORMAT ", not 2", meta->cstream);
  }

  if (meta->mstream != 0) {
    GST_WARNING ("Protocol control message on message stream %"
        G_GUINT32_FORMAT ", not 0", meta->mstream);
  }

  return TRUE;
}

gboolean
gst_rtmp_message_is_user_control (GstBuffer * buffer)
{
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);

  g_return_val_if_fail (meta, FALSE);

  if (meta->type != GST_RTMP_MESSAGE_TYPE_USER_CONTROL) {
    return FALSE;
  }

  if (meta->cstream != GST_RTMP_CHUNK_STREAM_PROTOCOL) {
    GST_WARNING ("User control message on chunk stream %"
        G_GUINT32_FORMAT ", not 2", meta->cstream);
  }

  if (meta->mstream != 0) {
    GST_WARNING ("User control message on message stream %"
        G_GUINT32_FORMAT ", not 0", meta->mstream);
  }

  return TRUE;
}

static inline gboolean
pc_has_param2 (GstRtmpMessageType type)
{
  return type == GST_RTMP_MESSAGE_TYPE_SET_PEER_BANDWIDTH;
}

gboolean
gst_rtmp_message_parse_protocol_control (GstBuffer * buffer,
    GstRtmpProtocolControl * out)
{
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);
  GstMapInfo map;
  GstRtmpProtocolControl pc;
  gsize pc_size = 4;
  gboolean ret = FALSE;

  g_return_val_if_fail (meta, FALSE);
  g_return_val_if_fail (gst_rtmp_message_type_is_protocol_control (meta->type),
      FALSE);

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ERROR ("can't map protocol control message");
    return FALSE;
  }

  pc.type = meta->type;
  pc_size = pc_has_param2 (pc.type) ? 5 : 4;

  if (map.size < pc_size) {
    GST_ERROR ("can't read protocol control param");
    goto err;
  } else if (map.size > pc_size) {
    GST_WARNING ("overlength protocol control: %" G_GSIZE_FORMAT " > %"
        G_GSIZE_FORMAT, map.size, pc_size);
  }

  pc.param = GST_READ_UINT32_BE (map.data);
  pc.param2 = pc_has_param2 (pc.type) ? GST_READ_UINT8 (map.data + 4) : 0;

  ret = TRUE;
  if (out) {
    *out = pc;
  }

err:
  gst_buffer_unmap (buffer, &map);
  return ret;
}

GstBuffer *
gst_rtmp_message_new_protocol_control (GstRtmpProtocolControl * pc)
{
  guint8 *data;
  gsize size;

  g_return_val_if_fail (pc, NULL);
  g_return_val_if_fail (gst_rtmp_message_type_is_protocol_control (pc->type),
      NULL);

  size = pc_has_param2 (pc->type) ? 5 : 4;

  data = g_malloc (size);
  GST_WRITE_UINT32_BE (data, pc->param);
  if (pc_has_param2 (pc->type)) {
    GST_WRITE_UINT8 (data + 4, pc->param2);
  }

  return gst_rtmp_message_new_wrapped (pc->type,
      GST_RTMP_CHUNK_STREAM_PROTOCOL, 0, data, size);
}

static inline gboolean
uc_has_param2 (GstRtmpUserControlType type)
{
  return type == GST_RTMP_USER_CONTROL_TYPE_SET_BUFFER_LENGTH;
}

gboolean
gst_rtmp_message_parse_user_control (GstBuffer * buffer,
    GstRtmpUserControl * out)
{
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);
  GstMapInfo map;
  GstRtmpUserControl uc;
  gsize uc_size;
  gboolean ret = FALSE;

  g_return_val_if_fail (meta, FALSE);
  g_return_val_if_fail (meta->type == GST_RTMP_MESSAGE_TYPE_USER_CONTROL,
      FALSE);

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ERROR ("can't map user control message");
    return FALSE;
  }

  if (map.size < 2) {
    GST_ERROR ("can't read user control type");
    goto err;
  }

  uc.type = GST_READ_UINT16_BE (map.data);
  uc_size = uc_has_param2 (uc.type) ? 10 : 6;

  if (map.size < uc_size) {
    GST_ERROR ("can't read user control param");
    goto err;
  } else if (map.size > uc_size) {
    GST_WARNING ("overlength user control: %" G_GSIZE_FORMAT " > %"
        G_GSIZE_FORMAT, map.size, uc_size);
  }

  uc.param = GST_READ_UINT32_BE (map.data + 2);
  uc.param2 = uc_has_param2 (uc.type) ? GST_READ_UINT32_BE (map.data + 6) : 0;

  ret = TRUE;
  if (out) {
    *out = uc;
  }

err:
  gst_buffer_unmap (buffer, &map);
  return ret;
}

GstBuffer *
gst_rtmp_message_new_user_control (GstRtmpUserControl * uc)
{
  guint8 *data;
  gsize size;

  g_return_val_if_fail (uc, NULL);

  size = uc_has_param2 (uc->type) ? 10 : 6;

  data = g_malloc (size);
  GST_WRITE_UINT16_BE (data, uc->type);
  GST_WRITE_UINT32_BE (data + 2, uc->param);
  if (uc_has_param2 (uc->type)) {
    GST_WRITE_UINT32_BE (data + 6, uc->param2);
  }

  return gst_rtmp_message_new_wrapped (GST_RTMP_MESSAGE_TYPE_USER_CONTROL,
      GST_RTMP_CHUNK_STREAM_PROTOCOL, 0, data, size);
}

gboolean
gst_rtmp_message_is_metadata (GstBuffer * buffer)
{
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);
  GstMapInfo map;
  GstAmfNode *node;
  gboolean ret = FALSE;

  g_return_val_if_fail (meta, FALSE);

  if (meta->type != GST_RTMP_MESSAGE_TYPE_DATA_AMF0) {
    return FALSE;
  }

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_ERROR ("can't map metadata message");
    return FALSE;
  }

  node = gst_amf_node_parse (map.data, map.size, NULL);
  if (!node) {
    GST_ERROR ("can't read metadata name");
    goto err;
  }

  switch (gst_amf_node_get_type (node)) {
    case GST_AMF_TYPE_STRING:
    case GST_AMF_TYPE_LONG_STRING:{
      const gchar *name = gst_amf_node_peek_string (node, NULL);
      ret = (strcmp (name, "onMetaData") == 0);
      break;
    }

    default:
      break;
  }

  gst_amf_node_free (node);

err:
  gst_buffer_unmap (buffer, &map);
  return ret;
}
