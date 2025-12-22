/* GStreamer unix file-descriptor source/sink
 *
 * Copyright (C) 2023 Netflix Inc.
 *  Author: Xavier Claessens <xavier.claessens@collabora.com>
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

/**
 * SECTION:element-unixfdsink
 * @title: unixfdsink
 *
 * Send file-descriptor backed buffers (e.g. memfd, dmabuf) over unix socket to
 * matching unixfdsrc. There can be any number of clients, if none are connected
 * buffers are dropped.
 *
 * Buffers can have any number of #GstMemory, but it is an error if any one of
 * them lacks a file-descriptor.
 *
 * #GstShmAllocator is added into the allocation proposition, which makes
 * most sources write their data into shared memory automatically.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v videotestsrc ! unixfdsink socket-path=/tmp/blah
 * gst-launch-1.0 -v unixfdsrc socket-path=/tmp/blah ! autovideosink
 * ]|
 *
 * Since: 1.24
 */

#include "gstunixfd.h"

#include <gst/base/base.h>
#include <gst/allocators/allocators.h>

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

GST_DEBUG_CATEGORY (unixfdsink_debug);
#define GST_CAT_DEFAULT (unixfdsink_debug)

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define GST_TYPE_UNIX_FD_SINK gst_unix_fd_sink_get_type()
G_DECLARE_FINAL_TYPE (GstUnixFdSink, gst_unix_fd_sink, GST, UNIX_FD_SINK,
    GstBaseSink);

typedef struct
{
  GHashTable *buffers;
  GSource *source;
} Client;

struct _GstUnixFdSink
{
  GstBaseSink parent;

  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;

  gchar *socket_path;
  GUnixSocketAddressType socket_type;
  GSocket *socket;
  GSource *source;

  /* GSocket -> Client */
  GHashTable *clients;
  GstCaps *caps;
  gboolean uses_monotonic_clock;
  GByteArray *payload;
};

G_DEFINE_TYPE (GstUnixFdSink, gst_unix_fd_sink, GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE (unixfdsink, "unixfdsink", GST_RANK_NONE,
    GST_TYPE_UNIX_FD_SINK);

#define DEFAULT_SOCKET_TYPE G_UNIX_SOCKET_ADDRESS_PATH

enum
{
  PROP_0,
  PROP_SOCKET_PATH,
  PROP_SOCKET_TYPE,
};


static void
client_free (Client * client)
{
  g_hash_table_unref (client->buffers);
  g_source_destroy (client->source);
  g_source_unref (client->source);
  g_free (client);
}

static void
gst_unix_fd_sink_init (GstUnixFdSink * self)
{
  g_return_if_fail (GST_IS_UNIX_FD_SINK (self));

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
  self->clients =
      g_hash_table_new_full (NULL, NULL, g_object_unref,
      (GDestroyNotify) client_free);
}

static void
gst_unix_fd_sink_finalize (GObject * object)
{
  GstUnixFdSink *self = GST_UNIX_FD_SINK (object);

  g_free (self->socket_path);
  g_main_context_unref (self->context);
  g_main_loop_unref (self->loop);
  g_hash_table_unref (self->clients);

  G_OBJECT_CLASS (gst_unix_fd_sink_parent_class)->finalize (object);
}

static void
gst_unix_fd_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUnixFdSink *self = GST_UNIX_FD_SINK (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_SOCKET_PATH:
      if (self->socket) {
        GST_WARNING_OBJECT (self,
            "Can only change socket path in NULL or READY state");
        break;
      }
      g_free (self->socket_path);
      self->socket_path = g_value_dup_string (value);
      break;
    case PROP_SOCKET_TYPE:
      if (self->socket) {
        GST_WARNING_OBJECT (self,
            "Can only change socket type in NULL or READY state");
        break;
      }
      self->socket_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_unix_fd_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstUnixFdSink *self = GST_UNIX_FD_SINK (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_SOCKET_PATH:
      g_value_set_string (value, self->socket_path);
      break;
    case PROP_SOCKET_TYPE:
      g_value_set_enum (value, self->socket_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static gboolean
incoming_command_cb (GSocket * socket, GIOCondition cond, gpointer user_data)
{
  GstUnixFdSink *self = user_data;
  Client *client;
  CommandType command;
  guint8 *payload = NULL;
  gsize payload_size;
  GError *error = NULL;

  GST_OBJECT_LOCK (self);

  client = g_hash_table_lookup (self->clients, socket);

  if (client == NULL) {
    GST_ERROR_OBJECT (self, "Received data from unknown client");
    goto on_error;
  }

  if (!gst_unix_fd_receive_command (socket, NULL, &command, NULL, &payload,
          &payload_size, &error)) {
    GST_DEBUG_OBJECT (self, "Failed to receive message from client %p: %s",
        client, error != NULL ? error->message : "Connection closed by peer");
    goto on_error;
  }

  switch (command) {
    case COMMAND_TYPE_NEW_BUFFER:
    case COMMAND_TYPE_CAPS:
      GST_ERROR_OBJECT (self, "Received wrong command %d from client %p",
          command, client);
      goto on_error;
    case COMMAND_TYPE_RELEASE_BUFFER:{
      ReleaseBufferPayload *release_buffer;
      if (!gst_unix_fd_parse_release_buffer (payload, payload_size,
              &release_buffer)) {
        GST_ERROR_OBJECT (self,
            "Received release-buffer with wrong payload size from client %p",
            client);
        goto on_error;
      }
      /* id is actually the GstBuffer pointer casted to guint64.
       * We can now drop its reference kept for this client. */
      if (!g_hash_table_remove (client->buffers, (gpointer) release_buffer->id)) {
        GST_ERROR_OBJECT (self,
            "Received wrong id %" G_GUINT64_FORMAT
            " in release-buffer command from client %p", release_buffer->id,
            client);
        goto on_error;
      }
      break;
    }
    default:
      /* Protocol could have been extended with new command */
      GST_DEBUG_OBJECT (self, "Ignoring unknown command %d", command);
      break;
  }

  g_free (payload);
  GST_OBJECT_UNLOCK (self);

  return G_SOURCE_CONTINUE;

on_error:
  g_hash_table_remove (self->clients, socket);
  g_clear_error (&error);
  g_free (payload);
  GST_OBJECT_UNLOCK (self);
  return G_SOURCE_REMOVE;
}

static guint8 *
caps_to_payload (GstCaps * caps, gsize * payload_size)
{
  gchar *payload = gst_caps_to_string (caps);
  *payload_size = strlen (payload) + 1;
  return (guint8 *) payload;
}

static gboolean
new_client_cb (GSocket * socket, GIOCondition cond, gpointer user_data)
{
  GstUnixFdSink *self = user_data;
  Client *client;
  GError *error = NULL;

  GSocket *client_socket = g_socket_accept (self->socket, NULL, &error);
  if (client_socket == NULL) {
    GST_ERROR_OBJECT (self, "Failed to accept connection: %s", error->message);
    return G_SOURCE_CONTINUE;
  }

  client = g_new0 (Client, 1);
  client->buffers =
      g_hash_table_new_full (NULL, NULL, (GDestroyNotify) gst_buffer_unref,
      NULL);
  client->source = g_socket_create_source (client_socket, G_IO_IN, NULL);
  g_source_set_callback (client->source, (GSourceFunc) incoming_command_cb,
      self, NULL);
  g_source_attach (client->source, self->context);

  GST_OBJECT_LOCK (self);

  GST_DEBUG_OBJECT (self, "New client %p", client);
  g_hash_table_insert (self->clients, client_socket, client);

  /* Start by sending our current caps. Keep the lock while doing that because
   * we don't want this client to miss a caps event or receive a buffer while we
   * send initial caps. */
  gsize payload_size;
  guint8 *payload = caps_to_payload (self->caps, &payload_size);
  if (!gst_unix_fd_send_command (client_socket, COMMAND_TYPE_CAPS, NULL,
          payload, payload_size, &error)) {
    GST_ERROR_OBJECT (self, "Failed to send caps to new client %p: %s", client,
        error->message);
    g_hash_table_remove (self->clients, client_socket);
    g_clear_error (&error);
  }
  g_free (payload);

  GST_OBJECT_UNLOCK (self);

  return G_SOURCE_CONTINUE;
}

static gpointer
thread_cb (gpointer user_data)
{
  GstUnixFdSink *self = user_data;
  g_main_loop_run (self->loop);
  return NULL;
}

static gboolean
gst_unix_fd_sink_start (GstBaseSink * bsink)
{
  GstUnixFdSink *self = (GstUnixFdSink *) bsink;
  GSocketAddress *addr = NULL;
  GError *error = NULL;
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (self);

  self->socket =
      gst_unix_fd_socket_new (self->socket_path, self->socket_type, &addr,
      &error);
  if (self->socket == NULL) {
    GST_ERROR_OBJECT (self, "Failed to create UNIX socket: %s", error->message);
    ret = FALSE;
    goto out;
  }

  if (!g_socket_bind (self->socket, addr, TRUE, &error)) {
    GST_ERROR_OBJECT (self, "Failed to bind socket: %s", error->message);
    g_clear_object (&self->socket);
    ret = FALSE;
    goto out;
  }

  if (!g_socket_listen (self->socket, &error)) {
    GST_ERROR_OBJECT (self, "Failed to listen socket: %s", error->message);
    g_clear_object (&self->socket);
    ret = FALSE;
    goto out;
  }

  self->source = g_socket_create_source (self->socket, G_IO_IN, NULL);
  g_source_set_callback (self->source, (GSourceFunc) new_client_cb, self, NULL);
  g_source_attach (self->source, self->context);

  self->thread = g_thread_new ("unixfdsink", thread_cb, self);

  /* Preallocate the minimum payload size for a buffer with a single memory and
   * no metas. Chances are that every buffer will require roughly the same
   * payload size, by reusing the same GByteArray we avoid reallocations. */
  self->payload =
      g_byte_array_sized_new (sizeof (NewBufferPayload) +
      sizeof (MemoryPayload));

out:
  GST_OBJECT_UNLOCK (self);
  g_clear_error (&error);
  g_clear_object (&addr);
  return ret;
}

static gboolean
gst_unix_fd_sink_stop (GstBaseSink * bsink)
{
  GstUnixFdSink *self = (GstUnixFdSink *) bsink;

  g_main_loop_quit (self->loop);
  g_thread_join (self->thread);

  g_source_destroy (self->source);
  g_clear_pointer (&self->source, g_source_unref);
  g_clear_object (&self->socket);
  gst_clear_caps (&self->caps);
  g_hash_table_remove_all (self->clients);
  g_clear_pointer (&self->payload, g_byte_array_unref);

  if (self->socket_type == G_UNIX_SOCKET_ADDRESS_PATH)
    g_unlink (self->socket_path);

  return TRUE;
}

static void
send_command_to_all (GstUnixFdSink * self, CommandType type, GUnixFDList * fds,
    const guint8 * payload, gsize payload_size, GstBuffer * buffer)
{
  GHashTableIter iter;
  GSocket *socket;
  Client *client;
  GError *error = NULL;

  g_hash_table_iter_init (&iter, self->clients);
  while (g_hash_table_iter_next (&iter, (gpointer) & socket,
          (gpointer) & client)) {
    if (!gst_unix_fd_send_command (socket, type, fds, payload, payload_size,
            &error)) {
      GST_ERROR_OBJECT (self, "Failed to send command %d to client %p: %s",
          type, client, error->message);
      g_clear_error (&error);
      g_hash_table_iter_remove (&iter);
      continue;
    }
    /* Keep a ref on this buffer until all clients released it. */
    if (buffer != NULL)
      g_hash_table_add (client->buffers, gst_buffer_ref (buffer));
  }
}

static GstClockTime
to_monotonic (GstClockTime timestamp, const GstSegment * segment,
    GstClockTime base_time, GstClockTime latency, GstClockTimeDiff clock_diff)
{
  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    /* Convert running time to pipeline clock time */
    gint res =
        gst_segment_to_running_time_full (segment, GST_FORMAT_TIME, timestamp,
        &timestamp);
    if (res == 0)
      return GST_CLOCK_TIME_NONE;
    else if (res > 0)
      timestamp += base_time;
    else if (base_time > timestamp)
      timestamp = base_time - timestamp;
    else
      timestamp = 0;
    if (GST_CLOCK_TIME_IS_VALID (latency))
      timestamp += latency;
    /* Convert to system monotonic clock time */
    if (clock_diff < 0 && -clock_diff > timestamp)
      return 0;
    timestamp += clock_diff;
  }
  return timestamp;
}

static guint16
serialize_metas (GstBuffer * buffer, GByteArray * payload)
{
  gpointer state = NULL;
  GstMeta *meta;
  guint16 n_meta = 0;

  while ((meta = gst_buffer_iterate_meta (buffer, &state)) != NULL) {
    if (gst_meta_serialize_simple (meta, payload))
      n_meta++;
  }

  return n_meta;
}

static GstFlowReturn
gst_unix_fd_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstUnixFdSink *self = (GstUnixFdSink *) bsink;
  GstFlowReturn ret = GST_FLOW_OK;
  GError *error = NULL;

  guint n_memory = gst_buffer_n_memory (buffer);
  gsize struct_size =
      sizeof (NewBufferPayload) + sizeof (MemoryPayload) * n_memory;
  g_byte_array_set_size (self->payload, struct_size);
  guint32 n_meta = serialize_metas (buffer, self->payload);

  GstClockTime latency = gst_base_sink_get_latency (GST_BASE_SINK_CAST (self));
  GstClockTime base_time = gst_element_get_base_time (GST_ELEMENT_CAST (self));
  GstClockTimeDiff clock_diff = 0;
  if (!self->uses_monotonic_clock) {
    clock_diff = GST_CLOCK_DIFF (g_get_monotonic_time () * GST_USECOND,
        gst_clock_get_time (GST_ELEMENT_CLOCK (self)));
  }

  NewBufferPayload *new_buffer = (NewBufferPayload *) self->payload->data;
  /* Cast buffer pointer to guint64 identifier. Client will send us back that
   * id so we know which buffer to unref. */
  new_buffer->id = (guint64) buffer;
  new_buffer->pts =
      to_monotonic (GST_BUFFER_PTS (buffer),
      &GST_BASE_SINK_CAST (self)->segment, base_time, latency, clock_diff);
  new_buffer->dts =
      to_monotonic (GST_BUFFER_DTS (buffer),
      &GST_BASE_SINK_CAST (self)->segment, base_time, latency, clock_diff);
  new_buffer->duration = GST_BUFFER_DURATION (buffer);
  new_buffer->offset = GST_BUFFER_OFFSET (buffer);
  new_buffer->offset_end = GST_BUFFER_OFFSET_END (buffer);
  new_buffer->flags = GST_BUFFER_FLAGS (buffer);
  new_buffer->type = MEMORY_TYPE_DEFAULT;
  new_buffer->n_memory = n_memory;
  new_buffer->n_meta = n_meta;

  if ((GST_BUFFER_PTS_IS_VALID (buffer)
          && !GST_CLOCK_TIME_IS_VALID (new_buffer->pts))
      || (GST_BUFFER_DTS_IS_VALID (buffer)
          && !GST_CLOCK_TIME_IS_VALID (new_buffer->dts))) {
    GST_ERROR_OBJECT (self,
        "Could not convert buffer timestamp to running time");
    return GST_FLOW_ERROR;
  }

  gboolean dmabuf_count = 0;
  GUnixFDList *fds = g_unix_fd_list_new ();
  for (int i = 0; i < n_memory; i++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, i);
    if (!gst_is_fd_memory (mem)) {
      GST_ERROR_OBJECT (self, "Expecting buffers with FD memories");
      ret = GST_FLOW_ERROR;
      goto out;
    }

    if (gst_is_dmabuf_memory (mem))
      dmabuf_count++;

    if (g_unix_fd_list_append (fds, gst_fd_memory_get_fd (mem), &error) < 0) {
      GST_ERROR_OBJECT (self, "Failed to append FD: %s", error->message);
      ret = GST_FLOW_ERROR;
      goto out;
    }

    gsize offset;
    new_buffer->memories[i].size = gst_memory_get_sizes (mem, &offset, NULL);
    new_buffer->memories[i].offset = offset;
  }

  if (dmabuf_count > 0 && dmabuf_count != n_memory) {
    GST_ERROR_OBJECT (self, "Some but not all memories are DMABuf");
    ret = GST_FLOW_ERROR;
    goto out;
  }

  if (dmabuf_count > 0)
    new_buffer->type = MEMORY_TYPE_DMABUF;

  GST_OBJECT_LOCK (self);
  send_command_to_all (self, COMMAND_TYPE_NEW_BUFFER, fds,
      self->payload->data, self->payload->len, buffer);
  GST_OBJECT_UNLOCK (self);

out:
  g_clear_object (&fds);
  g_clear_error (&error);
  return ret;
}

static gboolean
gst_unix_fd_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstUnixFdSink *self = (GstUnixFdSink *) bsink;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GST_OBJECT_LOCK (self);
      gst_clear_caps (&self->caps);
      gst_event_parse_caps (event, &self->caps);
      gst_caps_ref (self->caps);
      GST_DEBUG_OBJECT (self, "Send new caps to all clients: %" GST_PTR_FORMAT,
          self->caps);
      gsize payload_size;
      guint8 *payload = caps_to_payload (self->caps, &payload_size);
      send_command_to_all (self, COMMAND_TYPE_CAPS, NULL, payload, payload_size,
          NULL);
      g_free (payload);
      GST_OBJECT_UNLOCK (self);
      break;
    }
    case GST_EVENT_EOS:{
      GST_OBJECT_LOCK (self);
      send_command_to_all (self, COMMAND_TYPE_EOS, NULL, NULL, 0, NULL);
      GST_OBJECT_UNLOCK (self);
      break;
    }
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (gst_unix_fd_sink_parent_class)->event (bsink,
      event);
}

static gboolean
gst_unix_fd_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstAllocator *allocator = gst_shm_allocator_get ();
  gst_query_add_allocation_param (query, allocator, NULL);
  gst_object_unref (allocator);

  return TRUE;
}

static gboolean
gst_unix_fd_sink_set_clock (GstElement * element, GstClock * clock)
{
  GstUnixFdSink *self = (GstUnixFdSink *) element;

  self->uses_monotonic_clock = FALSE;
  if (clock != NULL && G_OBJECT_TYPE (clock) == GST_TYPE_SYSTEM_CLOCK) {
    GstClockType clock_type;
    g_object_get (clock, "clock-type", &clock_type, NULL);
    self->uses_monotonic_clock = clock_type == GST_CLOCK_TYPE_MONOTONIC;
  }

  return GST_ELEMENT_CLASS (gst_unix_fd_sink_parent_class)->set_clock (element,
      clock);
}

static void
gst_unix_fd_sink_class_init (GstUnixFdSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;

  GST_DEBUG_CATEGORY_INIT (unixfdsink_debug, "unixfdsink", 0,
      "Unix file descriptor sink");
  gst_element_class_set_static_metadata (gstelement_class,
      "Unix file descriptor sink", "Sink", "Unix file descriptor sink",
      "Xavier Claessens <xavier.claessens@collabora.com>");
  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);

  gst_shm_allocator_init_once ();

  gobject_class->finalize = gst_unix_fd_sink_finalize;
  gobject_class->set_property = gst_unix_fd_sink_set_property;
  gobject_class->get_property = gst_unix_fd_sink_get_property;

  gstelement_class->set_clock = GST_DEBUG_FUNCPTR (gst_unix_fd_sink_set_clock);

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_unix_fd_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_unix_fd_sink_stop);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_unix_fd_sink_render);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_unix_fd_sink_event);
  gstbasesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_unix_fd_sink_propose_allocation);

  g_object_class_install_property (gobject_class, PROP_SOCKET_PATH,
      g_param_spec_string ("socket-path",
          "Path to the control socket",
          "The path to the control socket used to control the shared memory "
          "transport. This may be modified during the NULL->READY transition",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SOCKET_TYPE,
      g_param_spec_enum ("socket-type", "Socket type",
          "The type of underlying socket",
          G_TYPE_UNIX_SOCKET_ADDRESS_TYPE, DEFAULT_SOCKET_TYPE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT |
          GST_PARAM_MUTABLE_READY));
}
