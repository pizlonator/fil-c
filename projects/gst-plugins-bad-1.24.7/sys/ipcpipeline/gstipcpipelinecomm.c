/* GStreamer
 * Copyright (C) 2015-2017 YouView TV Ltd
 *   Author: Vincent Penquerch <vincent.penquerch@collabora.co.uk>
 *
 * gstipcpipelinecomm.c:
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
#  include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef _MSC_VER
/* ssize_t is not available, so match return value of recv()/send() on MSVC */
#  define ssize_t int
#  include <winsock2.h>
#endif
#include <errno.h>
#include <string.h>
#include <gst/base/gstbytewriter.h>
#include <gst/gstprotection.h>
#include "gstipcpipelinecomm.h"

GST_DEBUG_CATEGORY_STATIC (gst_ipc_pipeline_comm_debug);
#define GST_CAT_DEFAULT gst_ipc_pipeline_comm_debug

#define DEFAULT_ACK_TIME (10 * G_TIME_SPAN_SECOND)

GQuark QUARK_ID;

typedef enum
{
  ACK_TYPE_NONE,
  ACK_TYPE_TIMED,
  ACK_TYPE_BLOCKING
} AckType;

typedef enum
{
  COMM_REQUEST_TYPE_BUFFER,
  COMM_REQUEST_TYPE_EVENT,
  COMM_REQUEST_TYPE_QUERY,
  COMM_REQUEST_TYPE_STATE_CHANGE,
  COMM_REQUEST_TYPE_MESSAGE,
} CommRequestType;

typedef struct
{
  guint32 id;
  gboolean replied;
  gboolean comm_error;
  guint32 ret;
  GstQuery *query;
  CommRequestType type;
  GCond cond;
} CommRequest;

static const gchar *comm_request_ret_get_name (CommRequestType type,
    guint32 ret);
static guint32 comm_request_ret_get_failure_value (CommRequestType type);

static CommRequest *
comm_request_new (guint32 id, CommRequestType type, GstQuery * query)
{
  CommRequest *req;

  req = g_malloc (sizeof (CommRequest));
  req->id = id;
  g_cond_init (&req->cond);
  req->replied = FALSE;
  req->comm_error = FALSE;
  req->query = query;
  req->ret = comm_request_ret_get_failure_value (type);
  req->type = type;

  return req;
}

static guint32
comm_request_wait (GstIpcPipelineComm * comm, CommRequest * req,
    AckType ack_type)
{
  guint32 ret = comm_request_ret_get_failure_value (req->type);
  guint64 end_time;

  if (ack_type == ACK_TYPE_TIMED)
    end_time = g_get_monotonic_time () + comm->ack_time;
  else
    end_time = G_MAXUINT64;

  GST_TRACE_OBJECT (comm->element, "Waiting for ACK/NAK for request %u",
      req->id);
  while (!req->replied) {
    if (ack_type == ACK_TYPE_TIMED) {
      if (!g_cond_wait_until (&req->cond, &comm->mutex, end_time))
        break;
    } else
      g_cond_wait (&req->cond, &comm->mutex);
  }

  if (req->replied) {
    ret = req->ret;
    GST_TRACE_OBJECT (comm->element, "Got reply for request %u: %d (%s)",
        req->id, ret, comm_request_ret_get_name (req->type, ret));
  } else {
    req->comm_error = TRUE;
    GST_ERROR_OBJECT (comm->element, "Timeout waiting for reply for request %u",
        req->id);
  }

  return ret;
}

static void
comm_request_free (CommRequest * req)
{
  g_cond_clear (&req->cond);
  g_free (req);
}

static const gchar *
comm_request_ret_get_name (CommRequestType type, guint32 ret)
{
  switch (type) {
    case COMM_REQUEST_TYPE_BUFFER:
      return gst_flow_get_name (ret);
    case COMM_REQUEST_TYPE_EVENT:
    case COMM_REQUEST_TYPE_QUERY:
    case COMM_REQUEST_TYPE_MESSAGE:
      return ret ? "TRUE" : "FALSE";
    case COMM_REQUEST_TYPE_STATE_CHANGE:
      return gst_element_state_change_return_get_name (ret);
    default:
      g_assert_not_reached ();
  }
}

static guint32
comm_request_ret_get_failure_value (CommRequestType type)
{
  switch (type) {
    case COMM_REQUEST_TYPE_BUFFER:
      return GST_FLOW_COMM_ERROR;
    case COMM_REQUEST_TYPE_EVENT:
    case COMM_REQUEST_TYPE_MESSAGE:
    case COMM_REQUEST_TYPE_QUERY:
      return FALSE;
    case COMM_REQUEST_TYPE_STATE_CHANGE:
      return GST_STATE_CHANGE_FAILURE;
    default:
      g_assert_not_reached ();
  }
}

static const gchar *
gst_ipc_pipeline_comm_data_type_get_name (GstIpcPipelineCommDataType type)
{
  switch (type) {
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_ACK:
      return "ACK";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY_RESULT:
      return "QUERY_RESULT";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_BUFFER:
      return "BUFFER";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_EVENT:
      return "EVENT";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_SINK_MESSAGE_EVENT:
      return "SINK_MESSAGE_EVENT";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY:
      return "QUERY";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_CHANGE:
      return "STATE_CHANGE";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_LOST:
      return "STATE_LOST";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_MESSAGE:
      return "MESSAGE";
    case GST_IPC_PIPELINE_COMM_DATA_TYPE_GERROR_MESSAGE:
      return "GERROR_MESSAGE";
    default:
      return "UNKNOWN";
  }
}

static gboolean
gst_ipc_pipeline_comm_sync_fd (GstIpcPipelineComm * comm, guint32 id,
    GstQuery * query, guint32 * ret, AckType ack_type, CommRequestType type)
{
  CommRequest *req;
  gboolean comm_error;
  GHashTable *waiting_ids;

  if (ack_type == ACK_TYPE_NONE)
    return TRUE;

  req = comm_request_new (id, type, query);
  waiting_ids = g_hash_table_ref (comm->waiting_ids);
  g_hash_table_insert (waiting_ids, GINT_TO_POINTER (id), req);
  *ret = comm_request_wait (comm, req, ack_type);
  comm_error = req->comm_error;
  g_hash_table_remove (waiting_ids, GINT_TO_POINTER (id));
  g_hash_table_unref (waiting_ids);
  return !comm_error;
}

static gboolean
write_to_fd_raw (GstIpcPipelineComm * comm, const void *data, size_t size)
{
  size_t offset;
  gboolean ret = TRUE;

  offset = 0;
  GST_TRACE_OBJECT (comm->element, "Writing %u bytes to fdout",
      (unsigned) size);
  while (size) {
#ifdef _MSC_VER
    ssize_t written =
        send (comm->fdout, (const unsigned char *) data + offset, size, 0);
    if (written < 0) {
      int last_error = WSAGetLastError ();
      if (last_error == WSAEWOULDBLOCK)
        continue;
      gchar *error_text = g_win32_error_message (last_error);
      GST_ERROR_OBJECT (comm->element, "Failed to write to fd: %s", error_text);
      g_free (error_text);
      ret = FALSE;
      goto done;
    }
#else
    ssize_t written =
        write (comm->fdout, (const unsigned char *) data + offset, size);
    if (written < 0) {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      GST_ERROR_OBJECT (comm->element, "Failed to write to fd: %s",
          strerror (errno));
      ret = FALSE;
      goto done;
    }
#endif
    size -= written;
    offset += written;
  }

done:
  return ret;
}

static gboolean
write_byte_writer_to_fd (GstIpcPipelineComm * comm, GstByteWriter * bw)
{
  guint8 *data;
  gboolean ret;
  guint size;

  size = gst_byte_writer_get_size (bw);
  data = gst_byte_writer_reset_and_get_data (bw);
  if (!data)
    return FALSE;
  ret = write_to_fd_raw (comm, data, size);
  g_free (data);
  return ret;
}

static void
gst_ipc_pipeline_comm_write_ack_to_fd (GstIpcPipelineComm * comm, guint32 id,
    guint32 ret, CommRequestType type)
{
  const unsigned char payload_type = GST_IPC_PIPELINE_COMM_DATA_TYPE_ACK;
  guint32 size;
  GstByteWriter bw;

  g_mutex_lock (&comm->mutex);

  GST_TRACE_OBJECT (comm->element, "Writing ACK for %u: %s (%d)", id,
      comm_request_ret_get_name (type, ret), ret);
  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, id))
    goto write_failed;
  size = sizeof (ret);
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, ret))
    goto write_failed;

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

done:
  g_mutex_unlock (&comm->mutex);
  gst_byte_writer_reset (&bw);
  return;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  goto done;
}

void
gst_ipc_pipeline_comm_write_flow_ack_to_fd (GstIpcPipelineComm * comm,
    guint32 id, GstFlowReturn ret)
{
  gst_ipc_pipeline_comm_write_ack_to_fd (comm, id, (guint32) ret,
      COMM_REQUEST_TYPE_BUFFER);
}

void
gst_ipc_pipeline_comm_write_boolean_ack_to_fd (GstIpcPipelineComm * comm,
    guint32 id, gboolean ret)
{
  gst_ipc_pipeline_comm_write_ack_to_fd (comm, id, (guint32) ret,
      COMM_REQUEST_TYPE_EVENT);
}

void
gst_ipc_pipeline_comm_write_state_change_ack_to_fd (GstIpcPipelineComm * comm,
    guint32 id, GstStateChangeReturn ret)
{
  gst_ipc_pipeline_comm_write_ack_to_fd (comm, id, (guint32) ret,
      COMM_REQUEST_TYPE_STATE_CHANGE);
}

void
gst_ipc_pipeline_comm_write_query_result_to_fd (GstIpcPipelineComm * comm,
    guint32 id, gboolean result, GstQuery * query)
{
  const unsigned char payload_type =
      GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY_RESULT;
  guint8 result8 = result;
  guint32 size;
  size_t len;
  char *str = NULL;
  guint32 type;
  const GstStructure *structure;
  GstByteWriter bw;

  g_mutex_lock (&comm->mutex);

  GST_TRACE_OBJECT (comm->element,
      "Writing query result for %u: %d, %" GST_PTR_FORMAT, id, result, query);
  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, id))
    goto write_failed;
  structure = gst_query_get_structure (query);
  if (structure) {
    str = gst_structure_to_string (structure);
    len = strlen (str);
  } else {
    str = NULL;
    len = 0;
  }
  size = 1 + sizeof (guint32) + len + 1;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (!gst_byte_writer_put_uint8 (&bw, result8))
    goto write_failed;
  type = GST_QUERY_TYPE (query);
  if (!gst_byte_writer_put_uint32_le (&bw, type))
    goto write_failed;
  if (str) {
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) str, len + 1))
      goto write_failed;
  } else {
    if (!gst_byte_writer_put_uint8 (&bw, 0))
      goto write_failed;
  }

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

done:
  g_mutex_unlock (&comm->mutex);
  gst_byte_writer_reset (&bw);
  g_free (str);
  return;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  goto done;
}

static gboolean
gst_ipc_pipeline_comm_read_query_result (GstIpcPipelineComm * comm,
    guint32 size, GstQuery ** query)
{
  gchar *end = NULL;
  GstStructure *structure;
  guint8 result;
  guint32 type;
  const guint8 *payload = NULL;
  guint32 mapped_size = size;

  /* this should not be called if we don't have enough yet */
  *query = NULL;
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, FALSE);
  g_return_val_if_fail (size >= 1 + sizeof (guint32), FALSE);

  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload)
    return FALSE;
  result = *payload++;
  memcpy (&type, payload, sizeof (type));
  payload += sizeof (type);

  size -= 1 + sizeof (guint32);
  if (size == 0)
    goto done;

  if (payload[size - 1]) {
    result = FALSE;
    goto done;
  }
  if (*payload) {
    structure = gst_structure_from_string ((const char *) payload, &end);
  } else {
    structure = NULL;
  }
  if (!structure) {
    result = FALSE;
    goto done;
  }

  *query = gst_query_new_custom (type, structure);

done:
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);
  return result;
}

typedef struct
{
  guint32 bytes;

  guint64 size;
  guint32 flags;
  guint64 api;
  char *str;
} MetaBuildInfo;

typedef struct
{
  GstIpcPipelineComm *comm;
  guint32 n_meta;
  guint32 total_bytes;
  MetaBuildInfo *info;
} MetaListRepresentation;

static gboolean
build_meta (GstBuffer * buffer, GstMeta ** meta, gpointer user_data)
{
  MetaListRepresentation *repr = user_data;

  repr->n_meta++;
  repr->info = g_realloc (repr->info, repr->n_meta * sizeof (MetaBuildInfo));
  repr->info[repr->n_meta - 1].bytes =
      /* 4 byte bytes */
      4
      /* 4 byte GstMetaFlags */
      + 4
      /* GstMetaInfo::api */
      + 4 + strlen (g_type_name ((*meta)->info->api)) + 1
      /* GstMetaInfo::size */
      + 8
      /* str length */
      + 4;

  repr->info[repr->n_meta - 1].flags = (*meta)->flags;
  repr->info[repr->n_meta - 1].api = (*meta)->info->api;
  repr->info[repr->n_meta - 1].size = (*meta)->info->size;
  repr->info[repr->n_meta - 1].str = NULL;

  /* GstMeta is a base class, and actual useful classes are all different...
     So we list a few of them we know we want and ignore the open ended rest */
  if ((*meta)->info->api == GST_PROTECTION_META_API_TYPE) {
    GstProtectionMeta *m = (GstProtectionMeta *) * meta;
    repr->info[repr->n_meta - 1].str = gst_structure_to_string (m->info);
    repr->info[repr->n_meta - 1].bytes +=
        strlen (repr->info[repr->n_meta - 1].str) + 1;
    GST_TRACE_OBJECT (repr->comm->element, "Found GstMeta type %s: %s",
        g_type_name ((*meta)->info->api), repr->info[repr->n_meta - 1].str);
  } else {
    GST_WARNING_OBJECT (repr->comm->element, "Ignoring GstMeta type %s",
        g_type_name ((*meta)->info->api));
  }
  repr->total_bytes += repr->info[repr->n_meta - 1].bytes;
  return TRUE;
}

typedef struct
{
  guint64 pts;
  guint64 dts;
  guint64 duration;
  guint64 offset;
  guint64 offset_end;
  guint64 flags;
} CommBufferMetadata;

GstFlowReturn
gst_ipc_pipeline_comm_write_buffer_to_fd (GstIpcPipelineComm * comm,
    GstBuffer * buffer)
{
  const unsigned char payload_type = GST_IPC_PIPELINE_COMM_DATA_TYPE_BUFFER;
  GstMapInfo map;
  guint32 ret32 = GST_FLOW_OK;
  guint32 size, n;
  CommBufferMetadata meta;
  GstFlowReturn ret;
  MetaListRepresentation repr = { comm, 0, 4, NULL };   /* starts a 4 for n_meta */
  GstByteWriter bw;

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  GST_TRACE_OBJECT (comm->element, "Writing buffer %u: %" GST_PTR_FORMAT,
      comm->send_id, buffer);

  gst_byte_writer_init (&bw);

  meta.pts = GST_BUFFER_PTS (buffer);
  meta.dts = GST_BUFFER_DTS (buffer);
  meta.duration = GST_BUFFER_DURATION (buffer);
  meta.offset = GST_BUFFER_OFFSET (buffer);
  meta.offset_end = GST_BUFFER_OFFSET_END (buffer);
  meta.flags = GST_BUFFER_FLAGS (buffer);

  /* work out meta size */
  gst_buffer_foreach_meta (buffer, build_meta, &repr);

  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;
  size =
      gst_buffer_get_size (buffer) + sizeof (guint32) +
      sizeof (CommBufferMetadata) + repr.total_bytes;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (!gst_byte_writer_put_data (&bw, (const guint8 *) &meta, sizeof (meta)))
    goto write_failed;
  size = gst_buffer_get_size (buffer);
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ))
    goto map_failed;
  ret = write_to_fd_raw (comm, map.data, map.size);
  gst_buffer_unmap (buffer, &map);
  if (!ret)
    goto write_failed;

  /* meta */
  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint32_le (&bw, repr.n_meta))
    goto write_failed;
  for (n = 0; n < repr.n_meta; ++n) {
    const MetaBuildInfo *info = repr.info + n;
    guint32 len;
    const char *s;

    if (!gst_byte_writer_put_uint32_le (&bw, info->bytes))
      goto write_failed;

    if (!gst_byte_writer_put_uint32_le (&bw, info->flags))
      goto write_failed;

    s = g_type_name (info->api);
    len = strlen (s) + 1;
    if (!gst_byte_writer_put_uint32_le (&bw, len))
      goto write_failed;
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) s, len))
      goto write_failed;

    if (!gst_byte_writer_put_uint64_le (&bw, info->size))
      goto write_failed;

    s = info->str;
    len = s ? (strlen (s) + 1) : 0;
    if (!gst_byte_writer_put_uint32_le (&bw, len))
      goto write_failed;
    if (len)
      if (!gst_byte_writer_put_data (&bw, (const guint8 *) s, len))
        goto write_failed;
  }

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  if (!gst_ipc_pipeline_comm_sync_fd (comm, comm->send_id, NULL, &ret32,
          ACK_TYPE_BLOCKING, COMM_REQUEST_TYPE_BUFFER))
    goto wait_failed;
  ret = ret32;

done:
  g_mutex_unlock (&comm->mutex);
  gst_byte_writer_reset (&bw);
  for (n = 0; n < repr.n_meta; ++n)
    g_free (repr.info[n].str);
  g_free (repr.info);
  return ret;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  ret = GST_FLOW_COMM_ERROR;
  goto done;

wait_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to wait for reply on socket"));
  ret = GST_FLOW_COMM_ERROR;
  goto done;

map_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, READ, (NULL),
      ("Failed to map buffer"));
  ret = GST_FLOW_ERROR;
  goto done;
}

static GstBuffer *
gst_ipc_pipeline_comm_read_buffer (GstIpcPipelineComm * comm, guint32 size)
{
  GstBuffer *buffer;
  CommBufferMetadata meta;
  guint32 n_meta, n;
  const guint8 *payload = NULL;
  guint32 mapped_size, buffer_data_size;

  /* this should not be called if we don't have enough yet */
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, NULL);
  g_return_val_if_fail (size >= sizeof (CommBufferMetadata), NULL);

  mapped_size = sizeof (CommBufferMetadata) + sizeof (buffer_data_size);
  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload)
    return NULL;
  memcpy (&meta, payload, sizeof (CommBufferMetadata));
  payload += sizeof (CommBufferMetadata);
  memcpy (&buffer_data_size, payload, sizeof (buffer_data_size));
  size -= mapped_size;
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);

  if (buffer_data_size == 0) {
    buffer = gst_buffer_new ();
  } else {
    buffer = gst_adapter_get_buffer (comm->adapter, buffer_data_size);
    gst_adapter_flush (comm->adapter, buffer_data_size);
  }
  size -= buffer_data_size;

  GST_BUFFER_PTS (buffer) = meta.pts;
  GST_BUFFER_DTS (buffer) = meta.dts;
  GST_BUFFER_DURATION (buffer) = meta.duration;
  GST_BUFFER_OFFSET (buffer) = meta.offset;
  GST_BUFFER_OFFSET_END (buffer) = meta.offset_end;
  GST_BUFFER_FLAGS (buffer) = meta.flags;

  /* If you don't call that, the GType isn't yet known at the
     g_type_from_name below */
  gst_protection_meta_get_info ();

  mapped_size = size;
  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload) {
    gst_buffer_unref (buffer);
    return NULL;
  }
  memcpy (&n_meta, payload, sizeof (n_meta));
  payload += sizeof (n_meta);

  for (n = 0; n < n_meta; ++n) {
    guint32 flags, len, bytes;
    guint64 msize;
    GType api;
    GstMeta *meta;
    GstStructure *structure = NULL;

    memcpy (&bytes, payload, sizeof (bytes));
    payload += sizeof (bytes);

#define READ_FIELD(f) do { \
    memcpy (&f, payload, sizeof (f)); \
    payload += sizeof(f); \
    } while(0)

    READ_FIELD (flags);
    READ_FIELD (len);
    api = g_type_from_name ((const char *) payload);
    payload = (const guint8 *) strchr ((const char *) payload, 0) + 1;
    READ_FIELD (msize);
    READ_FIELD (len);
    if (len) {
      structure = gst_structure_new_from_string ((const char *) payload);
      payload += len + 1;
    }

    /* Seems we can add a meta from the api nor type ? */
    if (api == GST_PROTECTION_META_API_TYPE) {
      meta =
          gst_buffer_add_meta (buffer, gst_protection_meta_get_info (), NULL);
      ((GstProtectionMeta *) meta)->info = structure;
    } else {
      GST_WARNING_OBJECT (comm->element, "Unsupported meta: %s",
          g_type_name (api));
      if (structure)
        gst_structure_free (structure);
    }

#undef READ_FIELD

  }

  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);

  return buffer;
}

static gboolean
gst_ipc_pipeline_comm_write_sink_message_event_to_fd (GstIpcPipelineComm * comm,
    GstEvent * event)
{
  const unsigned char payload_type =
      GST_IPC_PIPELINE_COMM_DATA_TYPE_SINK_MESSAGE_EVENT;
  gboolean ret;
  guint32 type, size, eseqnum, mseqnum, ret32 = TRUE, slen, structure_slen;
  char *str = NULL;
  const GstStructure *structure;
  GstMessage *message = NULL;
  const char *name;
  GstByteWriter bw;

  g_return_val_if_fail (GST_EVENT_TYPE (event) == GST_EVENT_SINK_MESSAGE,
      FALSE);

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  GST_TRACE_OBJECT (comm->element,
      "Writing sink message event %u: %" GST_PTR_FORMAT, comm->send_id, event);

  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;
  name = gst_structure_get_name (gst_event_get_structure (event));
  slen = strlen (name) + 1;
  gst_event_parse_sink_message (event, &message);
  structure = gst_message_get_structure (message);
  if (structure) {
    str = gst_structure_to_string (structure);
    structure_slen = strlen (str);
  } else {
    str = NULL;
    structure_slen = 0;
  }
  size = sizeof (type) + sizeof (eseqnum) + sizeof (mseqnum) + sizeof (slen) +
      strlen (name) + 1 + structure_slen + 1;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;

  type = GST_MESSAGE_TYPE (message);
  if (!gst_byte_writer_put_uint32_le (&bw, type))
    goto write_failed;
  size -= sizeof (type);

  eseqnum = GST_EVENT_SEQNUM (event);
  if (!gst_byte_writer_put_uint32_le (&bw, eseqnum))
    goto write_failed;
  size -= sizeof (eseqnum);

  mseqnum = GST_MESSAGE_SEQNUM (message);
  if (!gst_byte_writer_put_uint32_le (&bw, mseqnum))
    goto write_failed;
  size -= sizeof (mseqnum);

  if (!gst_byte_writer_put_uint32_le (&bw, slen))
    goto write_failed;
  size -= sizeof (slen);

  if (!gst_byte_writer_put_data (&bw, (const guint8 *) name, slen))
    goto write_failed;
  size -= slen;

  if (str) {
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) str, size))
      goto write_failed;
  } else {
    if (!gst_byte_writer_put_uint8 (&bw, 0))
      goto write_failed;
  }

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  if (!gst_ipc_pipeline_comm_sync_fd (comm, comm->send_id, NULL, &ret32,
          GST_EVENT_IS_SERIALIZED (event) ? ACK_TYPE_BLOCKING : ACK_TYPE_TIMED,
          COMM_REQUEST_TYPE_EVENT))
    goto write_failed;

  ret = ret32;

done:
  g_mutex_unlock (&comm->mutex);
  gst_byte_writer_reset (&bw);
  g_free (str);
  if (message)
    gst_message_unref (message);
  return ret;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  ret = FALSE;
  goto done;
}

static GstEvent *
gst_ipc_pipeline_comm_read_sink_message_event (GstIpcPipelineComm * comm,
    guint32 size)
{
  GstMessage *message;
  GstEvent *event = NULL;
  gchar *end = NULL;
  GstStructure *structure;
  guint32 type, eseqnum, mseqnum, slen;
  const char *name;
  guint32 mapped_size = size;
  const guint8 *payload;

  /* this should not be called if we don't have enough yet */
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, NULL);
  g_return_val_if_fail (size >= sizeof (type) + sizeof (slen), NULL);

  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload)
    return NULL;
  memcpy (&type, payload, sizeof (type));
  payload += sizeof (type);
  size -= sizeof (type);
  if (size == 0)
    goto done;

  memcpy (&eseqnum, payload, sizeof (eseqnum));
  payload += sizeof (eseqnum);
  size -= sizeof (eseqnum);
  if (size == 0)
    goto done;

  memcpy (&mseqnum, payload, sizeof (mseqnum));
  payload += sizeof (mseqnum);
  size -= sizeof (mseqnum);
  if (size == 0)
    goto done;

  memcpy (&slen, payload, sizeof (slen));
  payload += sizeof (slen);
  size -= sizeof (slen);
  if (size == 0)
    goto done;

  if (payload[slen - 1])
    goto done;
  name = (const char *) payload;
  payload += slen;
  size -= slen;

  if ((payload)[size - 1]) {
    goto done;
  }
  if (*payload) {
    structure = gst_structure_from_string ((const char *) payload, &end);
  } else {
    structure = NULL;
  }

  message =
      gst_message_new_custom (type, GST_OBJECT (comm->element), structure);
  gst_message_set_seqnum (message, mseqnum);
  event = gst_event_new_sink_message (name, message);
  gst_event_set_seqnum (event, eseqnum);
  gst_message_unref (message);

done:
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);
  return event;
}

gboolean
gst_ipc_pipeline_comm_write_event_to_fd (GstIpcPipelineComm * comm,
    gboolean upstream, GstEvent * event)
{
  const unsigned char payload_type = GST_IPC_PIPELINE_COMM_DATA_TYPE_EVENT;
  gboolean ret;
  guint32 type, size, ret32 = TRUE, seqnum, slen;
  char *str = NULL;
  const GstStructure *structure;
  GstByteWriter bw;

  /* we special case sink-message event as gst can't serialize/de-serialize it */
  if (GST_EVENT_TYPE (event) == GST_EVENT_SINK_MESSAGE)
    return gst_ipc_pipeline_comm_write_sink_message_event_to_fd (comm, event);

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  GST_TRACE_OBJECT (comm->element, "Writing event %u: %" GST_PTR_FORMAT,
      comm->send_id, event);

  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;
  structure = gst_event_get_structure (event);
  if (structure) {

    if (GST_EVENT_TYPE (event) == GST_EVENT_STREAM_START) {
      GstStructure *s = gst_structure_copy (structure);
      gst_structure_remove_field (s, "stream");
      str = gst_structure_to_string (s);
      gst_structure_free (s);
    } else {
      str = gst_structure_to_string (structure);
    }

    slen = strlen (str);
  } else {
    str = NULL;
    slen = 0;
  }
  size = sizeof (type) + sizeof (seqnum) + 1 + slen + 1;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;

  type = GST_EVENT_TYPE (event);
  if (!gst_byte_writer_put_uint32_le (&bw, type))
    goto write_failed;

  seqnum = GST_EVENT_SEQNUM (event);
  if (!gst_byte_writer_put_uint32_le (&bw, seqnum))
    goto write_failed;

  if (!gst_byte_writer_put_uint8 (&bw, upstream ? 1 : 0))
    goto write_failed;

  if (str) {
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) str, slen + 1))
      goto write_failed;
  } else {
    if (!gst_byte_writer_put_uint8 (&bw, 0))
      goto write_failed;
  }

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  /* Upstream events get serialized, this is required to send seeks only
   * one at a time. */
  if (!gst_ipc_pipeline_comm_sync_fd (comm, comm->send_id, NULL, &ret32,
          (GST_EVENT_IS_SERIALIZED (event) || GST_EVENT_IS_UPSTREAM (event)) ?
          ACK_TYPE_BLOCKING : ACK_TYPE_NONE, COMM_REQUEST_TYPE_EVENT))
    goto write_failed;
  ret = ret32;

done:
  g_mutex_unlock (&comm->mutex);
  g_free (str);
  gst_byte_writer_reset (&bw);
  return ret;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  ret = FALSE;
  goto done;
}

static GstEvent *
gst_ipc_pipeline_comm_read_event (GstIpcPipelineComm * comm, guint32 size,
    gboolean * upstream)
{
  GstEvent *event = NULL;
  gchar *end = NULL;
  GstStructure *structure;
  guint32 type, seqnum;
  guint32 mapped_size = size;
  const guint8 *payload;

  /* this should not be called if we don't have enough yet */
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, NULL);
  g_return_val_if_fail (size >= sizeof (type), NULL);

  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload)
    return NULL;

  memcpy (&type, payload, sizeof (type));
  payload += sizeof (type);
  size -= sizeof (type);
  if (size == 0)
    goto done;

  memcpy (&seqnum, payload, sizeof (seqnum));
  payload += sizeof (seqnum);
  size -= sizeof (seqnum);
  if (size == 0)
    goto done;

  *upstream = (*payload) ? TRUE : FALSE;
  payload += 1;
  size -= 1;
  if (size == 0)
    goto done;

  if (payload[size - 1])
    goto done;
  if (*payload) {
    structure = gst_structure_from_string ((const char *) payload, &end);
  } else {
    structure = NULL;
  }

  event = gst_event_new_custom (type, structure);
  gst_event_set_seqnum (event, seqnum);

done:
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);
  return event;
}

gboolean
gst_ipc_pipeline_comm_write_query_to_fd (GstIpcPipelineComm * comm,
    gboolean upstream, GstQuery * query)
{
  const unsigned char payload_type = GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY;
  gboolean ret;
  guint32 type, size, ret32 = TRUE, slen;
  char *str = NULL;
  const GstStructure *structure;
  GstByteWriter bw;

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  GST_TRACE_OBJECT (comm->element, "Writing query %u: %" GST_PTR_FORMAT,
      comm->send_id, query);

  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;
  structure = gst_query_get_structure (query);
  if (structure) {
    str = gst_structure_to_string (structure);
    slen = strlen (str);
  } else {
    str = NULL;
    slen = 0;
  }
  size = sizeof (type) + 1 + slen + 1;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;

  type = GST_QUERY_TYPE (query);
  if (!gst_byte_writer_put_uint32_le (&bw, type))
    goto write_failed;

  if (!gst_byte_writer_put_uint8 (&bw, upstream ? 1 : 0))
    goto write_failed;

  if (str) {
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) str, slen + 1))
      goto write_failed;
  } else {
    if (!gst_byte_writer_put_uint8 (&bw, 0))
      goto write_failed;
  }

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  if (!gst_ipc_pipeline_comm_sync_fd (comm, comm->send_id, query, &ret32,
          GST_QUERY_IS_SERIALIZED (query) ? ACK_TYPE_BLOCKING : ACK_TYPE_TIMED,
          COMM_REQUEST_TYPE_QUERY))
    goto write_failed;

  ret = ret32;

done:
  g_mutex_unlock (&comm->mutex);
  g_free (str);
  gst_byte_writer_reset (&bw);
  return ret;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  ret = FALSE;
  goto done;
}

static GstQuery *
gst_ipc_pipeline_comm_read_query (GstIpcPipelineComm * comm, guint32 size,
    gboolean * upstream)
{
  GstQuery *query = NULL;
  gchar *end = NULL;
  GstStructure *structure;
  guint32 type;
  guint32 mapped_size = size;
  const guint8 *payload;

  /* this should not be called if we don't have enough yet */
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, NULL);
  g_return_val_if_fail (size >= sizeof (type), NULL);

  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload)
    return NULL;

  memcpy (&type, payload, sizeof (type));
  payload += sizeof (type);
  size -= sizeof (type);
  if (size == 0)
    goto done;

  *upstream = (*payload) ? TRUE : FALSE;
  payload += 1;
  size -= 1;
  if (size == 0)
    goto done;

  if (payload[size - 1])
    goto done;
  if (*payload) {
    structure = gst_structure_from_string ((const char *) payload, &end);
  } else {
    structure = NULL;
  }

  query = gst_query_new_custom (type, structure);

  /* CAPS queries contain a filter field, of GstCaps type, which can be NULL.
     This does not play well with the serialization/deserialization system,
     which will give us a non-NULL GstCaps which has a value of NULL. This
     in turn wreaks havoc with any code that tests whether filter is NULL
     (which basically means, am I being given an optional GstCaps ?).
     So we look for non-NULL GstCaps which have NULL contents, and replace
     them with NULL instead. */
  if (GST_QUERY_TYPE (query) == GST_QUERY_CAPS) {
    GstCaps *filter;
    gst_query_parse_caps (query, &filter);
    if (filter
        && !strcmp (gst_structure_get_name (gst_caps_get_structure (filter, 0)),
            "NULL")) {
      gst_query_unref (query);
      query = gst_query_new_caps (NULL);
    }
  }

done:
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);
  return query;
}

GstStateChangeReturn
gst_ipc_pipeline_comm_write_state_change_to_fd (GstIpcPipelineComm * comm,
    GstStateChange transition)
{
  const unsigned char payload_type =
      GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_CHANGE;
  GstStateChangeReturn ret;
  guint32 size, ret32 = GST_STATE_CHANGE_SUCCESS;
  GstByteWriter bw;

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  GST_TRACE_OBJECT (comm->element, "Writing state change %u: %s -> %s",
      comm->send_id,
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;
  size = sizeof (transition);
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, transition))
    goto write_failed;

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  if (!gst_ipc_pipeline_comm_sync_fd (comm, comm->send_id, NULL, &ret32,
          ACK_TYPE_TIMED, COMM_REQUEST_TYPE_STATE_CHANGE))
    goto write_failed;
  ret = ret32;

done:
  g_mutex_unlock (&comm->mutex);
  gst_byte_writer_reset (&bw);
  return ret;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  ret = GST_STATE_CHANGE_FAILURE;
  goto done;
}

static gboolean
is_valid_state_change (GstStateChange transition)
{
  if (transition == GST_STATE_CHANGE_NULL_TO_READY)
    return TRUE;
  if (transition == GST_STATE_CHANGE_READY_TO_PAUSED)
    return TRUE;
  if (transition == GST_STATE_CHANGE_PAUSED_TO_PLAYING)
    return TRUE;
  if (transition == GST_STATE_CHANGE_PLAYING_TO_PAUSED)
    return TRUE;
  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    return TRUE;
  if (transition == GST_STATE_CHANGE_READY_TO_NULL)
    return TRUE;
  if (GST_STATE_TRANSITION_CURRENT (transition) ==
      GST_STATE_TRANSITION_NEXT (transition))
    return TRUE;
  return FALSE;
}

static gboolean
gst_ipc_pipeline_comm_read_state_change (GstIpcPipelineComm * comm,
    guint32 size, guint32 * transition)
{
  guint32 mapped_size = size;
  const guint8 *payload;

  /* this should not be called if we don't have enough yet */
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, FALSE);
  g_return_val_if_fail (size >= sizeof (*transition), FALSE);

  payload = gst_adapter_map (comm->adapter, size);
  if (!payload)
    return FALSE;
  memcpy (transition, payload, sizeof (*transition));
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);
  return is_valid_state_change (*transition);
}

void
gst_ipc_pipeline_comm_write_state_lost_to_fd (GstIpcPipelineComm * comm)
{
  const unsigned char payload_type = GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_LOST;
  guint32 size;
  GstByteWriter bw;

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  GST_TRACE_OBJECT (comm->element, "Writing state-lost %u", comm->send_id);
  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;
  size = 0;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

done:
  g_mutex_unlock (&comm->mutex);
  gst_byte_writer_reset (&bw);
  return;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  goto done;
}

static gboolean
gst_ipc_pipeline_comm_read_state_lost (GstIpcPipelineComm * comm, guint32 size)
{
  /* no payload */
  return TRUE;
}

static gboolean
gst_ipc_pipeline_comm_write_gerror_message_to_fd (GstIpcPipelineComm * comm,
    GstMessage * message)
{
  const unsigned char payload_type =
      GST_IPC_PIPELINE_COMM_DATA_TYPE_GERROR_MESSAGE;
  gboolean ret;
  guint32 code, size, ret32 = TRUE;
  char *str = NULL;
  GError *error;
  char *extra_message;
  const char *domain_string;
  unsigned char msgtype;
  GstByteWriter bw;

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    gst_message_parse_error (message, &error, &extra_message);
    msgtype = 2;
  } else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_WARNING) {
    gst_message_parse_warning (message, &error, &extra_message);
    msgtype = 1;
  } else {
    gst_message_parse_info (message, &error, &extra_message);
    msgtype = 0;
  }
  code = error->code;
  domain_string = g_quark_to_string (error->domain);
  GST_TRACE_OBJECT (comm->element,
      "Writing error %u: domain %s, code %u, message %s, extra message %s",
      comm->send_id, domain_string, error->code, error->message, extra_message);

  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;

  size = sizeof (size);
  size += 1;
  size += strlen (domain_string) + 1;
  size += sizeof (code);
  size += sizeof (size);
  size += error->message ? strlen (error->message) + 1 : 0;
  size += sizeof (size);
  size += extra_message ? strlen (extra_message) + 1 : 0;

  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;

  if (!gst_byte_writer_put_uint8 (&bw, msgtype))
    goto write_failed;
  size = strlen (domain_string) + 1;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (!gst_byte_writer_put_data (&bw, (const guint8 *) domain_string, size))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, code))
    goto write_failed;
  size = error->message ? strlen (error->message) + 1 : 0;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (error->message) {
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) error->message, size))
      goto write_failed;
  }
  size = extra_message ? strlen (extra_message) + 1 : 0;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;
  if (extra_message) {
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) extra_message, size))
      goto write_failed;
  }

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  if (!gst_ipc_pipeline_comm_sync_fd (comm, comm->send_id, NULL, &ret32,
          ACK_TYPE_NONE, COMM_REQUEST_TYPE_MESSAGE))
    goto write_failed;

  ret = ret32;

done:
  g_mutex_unlock (&comm->mutex);
  g_free (str);
  g_error_free (error);
  g_free (extra_message);
  gst_byte_writer_reset (&bw);
  return ret;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  ret = FALSE;
  goto done;
}

static GstMessage *
gst_ipc_pipeline_comm_read_gerror_message (GstIpcPipelineComm * comm,
    guint32 size)
{
  GstMessage *message = NULL;
  guint32 code;
  GQuark domain;
  const char *msg, *extra_message;
  GError *error;
  unsigned char msgtype;
  guint32 mapped_size = size;
  const guint8 *payload;

  /* this should not be called if we don't have enough yet */
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, NULL);
  g_return_val_if_fail (size >= sizeof (code) + sizeof (size) * 3 + 1 + 1,
      NULL);

  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload)
    return NULL;
  msgtype = *payload++;
  memcpy (&size, payload, sizeof (size));
  payload += sizeof (size);
  if (payload[size - 1])
    goto done;
  domain = g_quark_from_string ((const char *) payload);
  payload += size;

  memcpy (&code, payload, sizeof (code));
  payload += sizeof (code);

  memcpy (&size, payload, sizeof (size));
  payload += sizeof (size);
  if (size) {
    if (payload[size - 1])
      goto done;
    msg = (const char *) payload;
  } else {
    msg = NULL;
  }
  payload += size;

  memcpy (&size, payload, sizeof (size));
  payload += sizeof (size);
  if (size) {
    if (payload[size - 1])
      goto done;
    extra_message = (const char *) payload;
  } else {
    extra_message = NULL;
  }
  payload += size;

  error = g_error_new (domain, code, "%s", msg);
  if (msgtype == 2)
    message =
        gst_message_new_error (GST_OBJECT (comm->element), error,
        extra_message);
  else if (msgtype == 1)
    message =
        gst_message_new_warning (GST_OBJECT (comm->element), error,
        extra_message);
  else
    message =
        gst_message_new_info (GST_OBJECT (comm->element), error, extra_message);
  g_error_free (error);

done:
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);

  return message;
}

gboolean
gst_ipc_pipeline_comm_write_message_to_fd (GstIpcPipelineComm * comm,
    GstMessage * message)
{
  const unsigned char payload_type = GST_IPC_PIPELINE_COMM_DATA_TYPE_MESSAGE;
  gboolean ret;
  guint32 type, size, ret32 = TRUE, slen;
  char *str = NULL;
  const GstStructure *structure;
  GstByteWriter bw;

  /* we special case error as gst can't serialize/de-serialize it */
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR
      || GST_MESSAGE_TYPE (message) == GST_MESSAGE_WARNING
      || GST_MESSAGE_TYPE (message) == GST_MESSAGE_INFO)
    return gst_ipc_pipeline_comm_write_gerror_message_to_fd (comm, message);

  g_mutex_lock (&comm->mutex);
  ++comm->send_id;

  GST_TRACE_OBJECT (comm->element, "Writing message %u: %" GST_PTR_FORMAT,
      comm->send_id, message);

  gst_byte_writer_init (&bw);
  if (!gst_byte_writer_put_uint8 (&bw, payload_type))
    goto write_failed;
  if (!gst_byte_writer_put_uint32_le (&bw, comm->send_id))
    goto write_failed;
  structure = gst_message_get_structure (message);
  if (structure) {
    str = gst_structure_to_string (structure);
    slen = strlen (str);
  } else {
    str = NULL;
    slen = 0;
  }
  size = sizeof (type) + slen + 1;
  if (!gst_byte_writer_put_uint32_le (&bw, size))
    goto write_failed;

  type = GST_MESSAGE_TYPE (message);
  if (!gst_byte_writer_put_uint32_le (&bw, type))
    goto write_failed;
  size -= sizeof (type);
  if (str) {
    if (!gst_byte_writer_put_data (&bw, (const guint8 *) str, size))
      goto write_failed;
  } else {
    if (!gst_byte_writer_put_uint8 (&bw, 0))
      goto write_failed;
  }

  if (!write_byte_writer_to_fd (comm, &bw))
    goto write_failed;

  if (!gst_ipc_pipeline_comm_sync_fd (comm, comm->send_id, NULL, &ret32,
          ACK_TYPE_NONE, COMM_REQUEST_TYPE_MESSAGE))
    goto write_failed;

  ret = ret32;

done:
  g_mutex_unlock (&comm->mutex);
  g_free (str);
  gst_byte_writer_reset (&bw);
  return ret;

write_failed:
  GST_ELEMENT_ERROR (comm->element, RESOURCE, WRITE, (NULL),
      ("Failed to write to socket"));
  ret = FALSE;
  goto done;
}

static GstMessage *
gst_ipc_pipeline_comm_read_message (GstIpcPipelineComm * comm, guint32 size)
{
  GstMessage *message = NULL;
  gchar *end = NULL;
  GstStructure *structure;
  guint32 type;
  guint32 mapped_size = size;
  const guint8 *payload;

  /* this should not be called if we don't have enough yet */
  g_return_val_if_fail (gst_adapter_available (comm->adapter) >= size, NULL);
  g_return_val_if_fail (size >= sizeof (type), NULL);

  payload = gst_adapter_map (comm->adapter, mapped_size);
  if (!payload)
    return NULL;
  memcpy (&type, payload, sizeof (type));
  payload += sizeof (type);
  size -= sizeof (type);
  if (size == 0)
    goto done;

  if (payload[size - 1])
    goto done;
  if (*payload) {
    structure = gst_structure_from_string ((const char *) payload, &end);
  } else {
    structure = NULL;
  }

  message =
      gst_message_new_custom (type, GST_OBJECT (comm->element), structure);

done:
  gst_adapter_unmap (comm->adapter);
  gst_adapter_flush (comm->adapter, mapped_size);

  return message;
}

void
gst_ipc_pipeline_comm_init (GstIpcPipelineComm * comm, GstElement * element)
{
  g_mutex_init (&comm->mutex);
  comm->element = element;
  comm->fdin = comm->fdout = -1;
  comm->ack_time = DEFAULT_ACK_TIME;
  comm->waiting_ids =
      g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      (GDestroyNotify) comm_request_free);
  comm->adapter = gst_adapter_new ();
  comm->poll = gst_poll_new (TRUE);
  gst_poll_fd_init (&comm->pollFDin);
}

void
gst_ipc_pipeline_comm_clear (GstIpcPipelineComm * comm)
{
  g_hash_table_destroy (comm->waiting_ids);
  gst_object_unref (comm->adapter);
  gst_poll_free (comm->poll);
  g_mutex_clear (&comm->mutex);
}

static void
cancel_request (gpointer key, gpointer value, gpointer user_data,
    GstFlowReturn fret)
{
  GstIpcPipelineComm *comm = (GstIpcPipelineComm *) user_data;
  guint32 id = GPOINTER_TO_INT (key);
  CommRequest *req = (CommRequest *) value;

  GST_TRACE_OBJECT (comm->element, "Cancelling request %u, type %d", id,
      req->type);
  req->ret = fret;
  req->replied = TRUE;
  g_cond_signal (&req->cond);
}

static void
cancel_request_error (gpointer key, gpointer value, gpointer user_data)
{
  CommRequest *req = (CommRequest *) value;
  GstFlowReturn fret = comm_request_ret_get_failure_value (req->type);

  cancel_request (key, value, user_data, fret);
}

void
gst_ipc_pipeline_comm_cancel (GstIpcPipelineComm * comm, gboolean cleanup)
{
  g_mutex_lock (&comm->mutex);
  g_hash_table_foreach (comm->waiting_ids, cancel_request_error, comm);
  if (cleanup) {
    g_hash_table_unref (comm->waiting_ids);
    comm->waiting_ids =
        g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
        (GDestroyNotify) comm_request_free);
  }
  g_mutex_unlock (&comm->mutex);
}

static gboolean
set_field (GQuark field_id, const GValue * value, gpointer user_data)
{
  GstStructure *structure = user_data;

  gst_structure_id_set_value (structure, field_id, value);

  return TRUE;
}

static gboolean
gst_ipc_pipeline_comm_reply_request (GstIpcPipelineComm * comm, guint32 id,
    GstFlowReturn ret, GstQuery * query)
{
  CommRequest *req;

  req = g_hash_table_lookup (comm->waiting_ids, GINT_TO_POINTER (id));
  if (!req) {
    GST_WARNING_OBJECT (comm->element, "Got reply for unknown request %u", id);
    return FALSE;
  }

  GST_TRACE_OBJECT (comm->element, "Got reply %d (%s) for request %u", ret,
      comm_request_ret_get_name (req->type, ret), req->id);
  req->replied = TRUE;
  req->ret = ret;
  if (query) {
    if (req->query) {
      /* We need to update the original query in place, as the caller
         will expect the object to be the same */
      GstStructure *structure = gst_query_writable_structure (req->query);
      gst_structure_remove_all_fields (structure);
      gst_structure_foreach (gst_query_get_structure (query), set_field,
          structure);
    } else {
      GST_WARNING_OBJECT (comm->element,
          "Got query reply, but no query was in the request");
    }
  }
  g_cond_signal (&req->cond);
  return TRUE;
}

static gint
update_adapter (GstIpcPipelineComm * comm)
{
  GstMemory *mem = NULL;
  GstBuffer *buf;
  GstMapInfo map;
  ssize_t sz;
  gint ret = 0;

again:
  /* update pollFDin if necessary (fdin changed or we lost our parent).
   * we do not allow a parent-less element to communicate with its peer
   * in order to avoid race conditions where the slave tries to change
   * the state of its parent pipeline while it is not yet added in that
   * pipeline. */
  if (comm->pollFDin.fd != comm->fdin || !GST_OBJECT_PARENT (comm->element)) {
    if (comm->pollFDin.fd != -1) {
      GST_DEBUG_OBJECT (comm->element, "Stop watching fd %d",
          comm->pollFDin.fd);
      gst_poll_remove_fd (comm->poll, &comm->pollFDin);
      gst_poll_fd_init (&comm->pollFDin);
    }
    if (comm->fdin != -1 && GST_OBJECT_PARENT (comm->element)) {
      GST_DEBUG_OBJECT (comm->element, "Start watching fd %d", comm->fdin);
      comm->pollFDin.fd = comm->fdin;
      gst_poll_add_fd (comm->poll, &comm->pollFDin);
      gst_poll_fd_ctl_read (comm->poll, &comm->pollFDin, TRUE);
    }
  }

  /* wait for activity on fdin or a flush */
  if (gst_poll_wait (comm->poll, 100 * GST_MSECOND) < 0) {
    if (errno == EAGAIN)
      goto again;
    /* error out, unless interrupted or flushing */
    if (errno != EINTR)
      ret = (errno == EBUSY) ? 2 : 1;
  }

  /* read from fdin if possible and push data to our adapter */
  if (comm->pollFDin.fd >= 0
      && gst_poll_fd_can_read (comm->poll, &comm->pollFDin)) {
    if (!mem)
      mem = gst_allocator_alloc (NULL, comm->read_chunk_size, NULL);

    gst_memory_map (mem, &map, GST_MAP_WRITE);
#ifdef _MSC_VER
    sz = recv (comm->pollFDin.fd, map.data, map.size, 0);
    if (sz < 0) {
      int last_error = WSAGetLastError ();
      if (last_error == WSAEWOULDBLOCK) {
        errno = EAGAIN;
      } else {
        errno = last_error;
      }
    }
#else
    sz = read (comm->pollFDin.fd, map.data, map.size);
#endif
    gst_memory_unmap (mem, &map);

    if (sz <= 0) {
      if (errno == EAGAIN)
        goto again;
      /* error out, unless interrupted */
      if (errno != EINTR)
        ret = 1;
    } else {
      gst_memory_resize (mem, 0, sz);
      buf = gst_buffer_new ();
      gst_buffer_append_memory (buf, mem);
      mem = NULL;
      GST_TRACE_OBJECT (comm->element, "Read %u bytes from fd", (unsigned) sz);
      gst_adapter_push (comm->adapter, buf);
    }
  }

  if (mem)
    gst_memory_unref (mem);

  return ret;
}

static gboolean
read_many (GstIpcPipelineComm * comm)
{
  gboolean ret = TRUE;
  gsize available;
  const guint8 *payload;

  while (1)
    switch (comm->state) {
      case GST_IPC_PIPELINE_COMM_STATE_TYPE:
      {
        guint8 type;
        guint32 mapped_size;

        available = gst_adapter_available (comm->adapter);
        mapped_size = 1 + sizeof (gint32) * 2;
        if (available < mapped_size)
          goto done;

        payload = gst_adapter_map (comm->adapter, mapped_size);
        type = *payload++;
        g_mutex_lock (&comm->mutex);
        memcpy (&comm->id, payload, sizeof (guint32));
        memcpy (&comm->payload_length, payload + 4, sizeof (guint32));
        g_mutex_unlock (&comm->mutex);
        gst_adapter_unmap (comm->adapter);
        gst_adapter_flush (comm->adapter, mapped_size);
        GST_TRACE_OBJECT (comm->element, "Got id %u, type %d, payload %u",
            comm->id, type, comm->payload_length);
        switch (type) {
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_ACK:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY_RESULT:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_BUFFER:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_EVENT:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_SINK_MESSAGE_EVENT:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_CHANGE:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_LOST:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_MESSAGE:
          case GST_IPC_PIPELINE_COMM_DATA_TYPE_GERROR_MESSAGE:
            GST_TRACE_OBJECT (comm->element, "switching to state %s",
                gst_ipc_pipeline_comm_data_type_get_name (type));
            comm->state = type;
            break;
          default:
            goto out_of_sync;
        }
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_ACK:
      {
        const guint8 *rets;
        guint32 ret32;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        if (available < sizeof (guint32))
          goto ack_failed;

        rets = gst_adapter_map (comm->adapter, sizeof (guint32));
        memcpy (&ret32, rets, sizeof (ret32));
        gst_adapter_unmap (comm->adapter);
        gst_adapter_flush (comm->adapter, sizeof (guint32));
        GST_TRACE_OBJECT (comm->element, "Got ACK %s for id %u",
            gst_flow_get_name (ret32), comm->id);

        g_mutex_lock (&comm->mutex);
        gst_ipc_pipeline_comm_reply_request (comm, comm->id, ret32, NULL);
        g_mutex_unlock (&comm->mutex);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY_RESULT:
      {
        GstQuery *query = NULL;
        gboolean qret;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        qret =
            gst_ipc_pipeline_comm_read_query_result (comm, comm->payload_length,
            &query);

        GST_TRACE_OBJECT (comm->element,
            "deserialized query result %p: %d, %" GST_PTR_FORMAT, query, qret,
            query);

        g_mutex_lock (&comm->mutex);
        gst_ipc_pipeline_comm_reply_request (comm, comm->id, qret, query);
        g_mutex_unlock (&comm->mutex);

        gst_query_unref (query);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_BUFFER:
      {
        GstBuffer *buf;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        buf = gst_ipc_pipeline_comm_read_buffer (comm, comm->payload_length);
        if (!buf)
          goto buffer_failed;

        /* set caps and push */
        GST_TRACE_OBJECT (comm->element,
            "deserialized buffer %p, pushing, timestamp %" GST_TIME_FORMAT
            ", duration %" GST_TIME_FORMAT ", offset %" G_GINT64_FORMAT
            ", offset_end %" G_GINT64_FORMAT ", size %" G_GSIZE_FORMAT
            ", flags 0x%x", buf, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
            GST_TIME_ARGS (GST_BUFFER_DURATION (buf)), GST_BUFFER_OFFSET (buf),
            GST_BUFFER_OFFSET_END (buf), gst_buffer_get_size (buf),
            GST_BUFFER_FLAGS (buf));

        gst_mini_object_set_qdata (GST_MINI_OBJECT (buf), QUARK_ID,
            GINT_TO_POINTER (comm->id), NULL);

        if (comm->on_buffer)
          (*comm->on_buffer) (comm->id, buf, comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_EVENT:
      {
        GstEvent *event;
        gboolean upstream;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        event = gst_ipc_pipeline_comm_read_event (comm, comm->payload_length,
            &upstream);
        if (!event)
          goto event_failed;

        GST_TRACE_OBJECT (comm->element, "deserialized event %p of type %s",
            event, gst_event_type_get_name (event->type));

        gst_mini_object_set_qdata (GST_MINI_OBJECT (event), QUARK_ID,
            GINT_TO_POINTER (comm->id), NULL);

        if (comm->on_event)
          (*comm->on_event) (comm->id, event, upstream, comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_SINK_MESSAGE_EVENT:
      {
        GstEvent *event;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        event = gst_ipc_pipeline_comm_read_sink_message_event (comm,
            comm->payload_length);
        if (!event)
          goto event_failed;

        GST_TRACE_OBJECT (comm->element, "deserialized sink message event %p",
            event);

        gst_mini_object_set_qdata (GST_MINI_OBJECT (event), QUARK_ID,
            GINT_TO_POINTER (comm->id), NULL);

        if (comm->on_event)
          (*comm->on_event) (comm->id, event, FALSE, comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_QUERY:
      {
        GstQuery *query;
        gboolean upstream;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        query = gst_ipc_pipeline_comm_read_query (comm, comm->payload_length,
            &upstream);
        if (!query)
          goto query_failed;

        GST_TRACE_OBJECT (comm->element, "deserialized query %p of type %s",
            query, gst_query_type_get_name (query->type));

        gst_mini_object_set_qdata (GST_MINI_OBJECT (query), QUARK_ID,
            GINT_TO_POINTER (comm->id), NULL);

        if (comm->on_query)
          (*comm->on_query) (comm->id, query, upstream, comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_CHANGE:
      {
        guint32 transition;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        if (!gst_ipc_pipeline_comm_read_state_change (comm,
                comm->payload_length, &transition))
          goto state_change_failed;

        GST_TRACE_OBJECT (comm->element,
            "deserialized state change request: %s -> %s",
            gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT
                (transition)),
            gst_element_state_get_name (GST_STATE_TRANSITION_NEXT
                (transition)));

        if (comm->on_state_change)
          (*comm->on_state_change) (comm->id, transition, comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_STATE_LOST:
      {
        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        if (!gst_ipc_pipeline_comm_read_state_lost (comm, comm->payload_length))
          goto event_failed;

        GST_TRACE_OBJECT (comm->element, "deserialized state-lost");

        if (comm->on_state_lost)
          (*comm->on_state_lost) (comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_MESSAGE:
      {
        GstMessage *message;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        message = gst_ipc_pipeline_comm_read_message (comm,
            comm->payload_length);
        if (!message)
          goto message_failed;

        GST_TRACE_OBJECT (comm->element, "deserialized message %p of type %s",
            message, gst_message_type_get_name (message->type));

        if (comm->on_message)
          (*comm->on_message) (comm->id, message, comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
      case GST_IPC_PIPELINE_COMM_DATA_TYPE_GERROR_MESSAGE:
      {
        GstMessage *message;

        available = gst_adapter_available (comm->adapter);
        if (available < comm->payload_length)
          goto done;

        message = gst_ipc_pipeline_comm_read_gerror_message (comm,
            comm->payload_length);
        if (!message)
          goto message_failed;

        GST_TRACE_OBJECT (comm->element, "deserialized message %p of type %s",
            message, gst_message_type_get_name (message->type));

        if (comm->on_message)
          (*comm->on_message) (comm->id, message, comm->user_data);

        GST_TRACE_OBJECT (comm->element, "switching to state TYPE");
        comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
        break;
      }
    }

done:
  return ret;

  /* ERRORS */
out_of_sync:
  {
    GST_ELEMENT_ERROR (comm->element, STREAM, DECODE, (NULL),
        ("Socket out of sync"));
    ret = FALSE;
    goto done;
  }
state_change_failed:
  {
    GST_ELEMENT_ERROR (comm->element, STREAM, DECODE, (NULL),
        ("could not read state change from fd"));
    ret = FALSE;
    goto done;
  }
ack_failed:
  {
    GST_ELEMENT_ERROR (comm->element, STREAM, DECODE, (NULL),
        ("could not read ack from fd"));
    ret = FALSE;
    goto done;
  }
buffer_failed:
  {
    GST_ELEMENT_ERROR (comm->element, STREAM, DECODE, (NULL),
        ("could not read buffer from fd"));
    ret = FALSE;
    goto done;
  }
event_failed:
  {
    GST_ELEMENT_ERROR (comm->element, STREAM, DECODE, (NULL),
        ("could not read event from fd"));
    ret = FALSE;
    goto done;
  }
message_failed:
  {
    GST_ELEMENT_ERROR (comm->element, STREAM, DECODE, (NULL),
        ("could not read message from fd"));
    ret = FALSE;
    goto done;
  }
query_failed:
  {
    GST_ELEMENT_ERROR (comm->element, STREAM, DECODE, (NULL),
        ("could not read query from fd"));
    ret = FALSE;
    goto done;
  }
}

static gpointer
reader_thread (gpointer data)
{
  GstIpcPipelineComm *comm = (GstIpcPipelineComm *) data;
  gboolean running = TRUE;
  gint ret = 0;

  while (running) {
    ret = update_adapter (comm);
    switch (ret) {
      case 1:
        GST_ELEMENT_ERROR (comm->element, RESOURCE, READ, (NULL),
            ("Failed to read from socket"));
        running = FALSE;
        break;
      case 2:
        GST_INFO_OBJECT (comm->element, "We're stopping, all good");
        running = FALSE;
        break;
      default:
        read_many (comm);
        break;
    }
  }

  GST_INFO_OBJECT (comm->element, "Reader thread ending");
  return NULL;
}

gboolean
gst_ipc_pipeline_comm_start_reader_thread (GstIpcPipelineComm * comm,
    void (*on_buffer) (guint32, GstBuffer *, gpointer),
    void (*on_event) (guint32, GstEvent *, gboolean, gpointer),
    void (*on_query) (guint32, GstQuery *, gboolean, gpointer),
    void (*on_state_change) (guint32, GstStateChange, gpointer),
    void (*on_state_lost) (gpointer),
    void (*on_message) (guint32, GstMessage *, gpointer), gpointer user_data)
{
  if (comm->reader_thread)
    return FALSE;

  comm->state = GST_IPC_PIPELINE_COMM_STATE_TYPE;
  comm->on_buffer = on_buffer;
  comm->on_event = on_event;
  comm->on_query = on_query;
  comm->on_state_change = on_state_change;
  comm->on_state_lost = on_state_lost;
  comm->on_message = on_message;
  comm->user_data = user_data;
  gst_poll_set_flushing (comm->poll, FALSE);
  comm->reader_thread =
      g_thread_new ("reader", (GThreadFunc) reader_thread, comm);
  return TRUE;
}

void
gst_ipc_pipeline_comm_stop_reader_thread (GstIpcPipelineComm * comm)
{
  if (!comm->reader_thread)
    return;

  gst_poll_set_flushing (comm->poll, TRUE);
  g_thread_join (comm->reader_thread);
  comm->reader_thread = NULL;
}

static gchar *
gst_value_serialize_event (const GValue * value)
{
  const GstStructure *structure;
  GstEvent *ev;
  gchar *type, *ts, *seqnum, *rt_offset, *str, *str64, *s;
  GValue val = G_VALUE_INIT;

  ev = g_value_get_boxed (value);

  g_value_init (&val, gst_event_type_get_type ());
  g_value_set_enum (&val, ev->type);
  type = gst_value_serialize (&val);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_UINT64);
  g_value_set_uint64 (&val, ev->timestamp);
  ts = gst_value_serialize (&val);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_UINT);
  g_value_set_uint (&val, ev->seqnum);
  seqnum = gst_value_serialize (&val);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_INT64);
  g_value_set_int64 (&val, gst_event_get_running_time_offset (ev));
  rt_offset = gst_value_serialize (&val);
  g_value_unset (&val);

  structure = gst_event_get_structure (ev);
  str = gst_structure_to_string (structure);
  str64 = g_base64_encode ((guchar *) str, strlen (str) + 1);
  g_strdelimit (str64, "=", '_');
  g_free (str);

  s = g_strconcat (type, ":", ts, ":", seqnum, ":", rt_offset, ":", str64,
      NULL);

  g_free (type);
  g_free (ts);
  g_free (seqnum);
  g_free (rt_offset);
  g_free (str64);

  return s;
}

static gboolean
gst_value_deserialize_event (GValue * dest, const gchar * s)
{
  GstEvent *ev = NULL;
  GValue val = G_VALUE_INIT;
  gboolean ret = FALSE;
  gchar **fields;
  gsize len;

  fields = g_strsplit (s, ":", -1);
  if (g_strv_length (fields) != 5)
    goto wrong_length;

  g_strdelimit (fields[4], "_", '=');
  g_base64_decode_inplace (fields[4], &len);

  g_value_init (&val, gst_event_type_get_type ());
  if (!gst_value_deserialize (&val, fields[0]))
    goto fail;
  ev = gst_event_new_custom (g_value_get_enum (&val),
      gst_structure_new_from_string (fields[4]));

  g_value_unset (&val);
  g_value_init (&val, G_TYPE_UINT64);
  if (!gst_value_deserialize (&val, fields[1]))
    goto fail;
  ev->timestamp = g_value_get_uint64 (&val);

  g_value_unset (&val);
  g_value_init (&val, G_TYPE_UINT);
  if (!gst_value_deserialize (&val, fields[2]))
    goto fail;
  ev->seqnum = g_value_get_uint (&val);

  g_value_unset (&val);
  g_value_init (&val, G_TYPE_INT64);
  if (!gst_value_deserialize (&val, fields[3]))
    goto fail;
  gst_event_set_running_time_offset (ev, g_value_get_int64 (&val));

  g_value_take_boxed (dest, ev);
  ev = NULL;
  ret = TRUE;

fail:
  g_clear_pointer (&ev, gst_event_unref);
  g_value_unset (&val);

wrong_length:
  g_strfreev (fields);
  return ret;
}

#define REGISTER_SERIALIZATION_NO_COMPARE(_gtype, _type)                \
G_STMT_START {                                                          \
  static GstValueTable gst_value =                                      \
    { 0, NULL,                                             \
    gst_value_serialize_ ## _type, gst_value_deserialize_ ## _type };    \
  gst_value.type = _gtype;                                              \
  gst_value_register (&gst_value);                                      \
} G_STMT_END

void
gst_ipc_pipeline_comm_plugin_init (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once)) {
    GST_DEBUG_CATEGORY_INIT (gst_ipc_pipeline_comm_debug, "ipcpipelinecomm", 0,
        "ipc pipeline comm");
    QUARK_ID = g_quark_from_static_string ("ipcpipeline-id");
    REGISTER_SERIALIZATION_NO_COMPARE (gst_event_get_type (), event);
    g_once_init_leave (&once, (gsize) 1);
  }
}
