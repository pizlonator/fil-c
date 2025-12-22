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
 * SECTION:element-unixfdsrc
 * @title: unixfdsrc
 *
 * Receive file-descriptor backed buffers (e.g. memfd, dmabuf) over unix socket
 * from matching unixfdsink.
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

GST_DEBUG_CATEGORY (unixfdsrc_debug);
#define GST_CAT_DEFAULT (unixfdsrc_debug)

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define GST_TYPE_UNIX_FD_SRC gst_unix_fd_src_get_type()
G_DECLARE_FINAL_TYPE (GstUnixFdSrc, gst_unix_fd_src, GST, UNIX_FD_SRC,
    GstPushSrc);

struct _GstUnixFdSrc
{
  GstPushSrc parent;

  gchar *socket_path;
  GUnixSocketAddressType socket_type;
  GSocket *socket;
  GCancellable *cancellable;

  GstAllocator *allocators[MEMORY_TYPE_LAST];
  GHashTable *memories;
  gboolean uses_monotonic_clock;
};

G_DEFINE_TYPE (GstUnixFdSrc, gst_unix_fd_src, GST_TYPE_PUSH_SRC);
GST_ELEMENT_REGISTER_DEFINE (unixfdsrc, "unixfdsrc", GST_RANK_NONE,
    GST_TYPE_UNIX_FD_SRC);

#define DEFAULT_SOCKET_TYPE G_UNIX_SOCKET_ADDRESS_PATH

enum
{
  PROP_0,
  PROP_SOCKET_PATH,
  PROP_SOCKET_TYPE,
};

typedef struct
{
  guint64 id;
  guint n_memory;
} BufferContext;

static void
release_buffer (GstUnixFdSrc * self, guint64 id)
{
  /* Notify that we are not using this buffer anymore */
  ReleaseBufferPayload payload = { id };
  GError *error = NULL;
  if (!gst_unix_fd_send_command (self->socket, COMMAND_TYPE_RELEASE_BUFFER,
          NULL, (guint8 *) & payload, sizeof (payload), &error)) {
    GST_WARNING_OBJECT (self, "Failed to send release-buffer command: %s",
        error->message);
    g_clear_error (&error);
  }
}

static void
memory_weak_ref_cb (GstUnixFdSrc * self, GstMemory * mem)
{
  GST_OBJECT_LOCK (self);

  BufferContext *ctx = g_hash_table_lookup (self->memories, mem);
  if (ctx != NULL) {
    if (--ctx->n_memory == 0) {
      release_buffer (self, ctx->id);
      g_free (ctx);
    }
    g_hash_table_remove (self->memories, mem);
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_unix_fd_src_init (GstUnixFdSrc * self)
{
  g_return_if_fail (GST_IS_UNIX_FD_SRC (self));

  self->cancellable = g_cancellable_new ();
  self->memories = g_hash_table_new (NULL, NULL);
  self->allocators[0] = gst_fd_allocator_new ();
  self->allocators[1] = gst_dmabuf_allocator_new ();
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
}

static void
gst_unix_fd_src_finalize (GObject * object)
{
  GstUnixFdSrc *self = GST_UNIX_FD_SRC (object);

  g_free (self->socket_path);
  g_object_unref (self->cancellable);
  g_hash_table_unref (self->memories);
  for (int i = 0; i < MEMORY_TYPE_LAST; i++)
    gst_object_unref (self->allocators[i]);

  G_OBJECT_CLASS (gst_unix_fd_src_parent_class)->finalize (object);
}

static void
gst_unix_fd_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUnixFdSrc *self = GST_UNIX_FD_SRC (object);

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
gst_unix_fd_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstUnixFdSrc *self = GST_UNIX_FD_SRC (object);

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
gst_unix_fd_src_start (GstBaseSrc * bsrc)
{
  GstUnixFdSrc *self = (GstUnixFdSrc *) bsrc;
  GSocketAddress *addr = NULL;
  GError *error = NULL;
  gboolean ret = TRUE;

  gst_base_src_set_format (bsrc, GST_FORMAT_TIME);

  GST_OBJECT_LOCK (self);

  self->socket =
      gst_unix_fd_socket_new (self->socket_path, self->socket_type, &addr,
      &error);
  if (self->socket == NULL) {
    GST_ERROR_OBJECT (self, "Failed to create UNIX socket: %s", error->message);
    ret = FALSE;
    goto out;
  }

  if (!g_socket_connect (self->socket, addr, NULL, &error)) {
    GST_ERROR_OBJECT (self, "Failed to connect socket: %s", error->message);
    g_clear_object (&self->socket);
    ret = FALSE;
    goto out;
  }

out:
  GST_OBJECT_UNLOCK (self);
  g_clear_error (&error);
  g_clear_object (&addr);
  return ret;
}

static gboolean
gst_unix_fd_src_stop (GstBaseSrc * bsrc)
{
  GstUnixFdSrc *self = (GstUnixFdSrc *) bsrc;

  GST_OBJECT_LOCK (self);

  /* Remove all weak refs we could still have to not be called back later.
   * Service side will cleanup pending buffers when socket gets closed. */
  GstMemory *mem;
  BufferContext *ctx;
  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->memories);
  while (g_hash_table_iter_next (&iter, (gpointer *) & mem, (gpointer *) & ctx)) {
    gst_mini_object_weak_unref (GST_MINI_OBJECT_CAST (mem),
        (GstMiniObjectNotify) memory_weak_ref_cb, self);
    if (--ctx->n_memory == 0)
      g_free (ctx);
  }
  g_hash_table_remove_all (self->memories);
  g_clear_object (&self->socket);

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_unix_fd_src_unlock (GstBaseSrc * bsrc)
{
  GstUnixFdSrc *self = GST_UNIX_FD_SRC (bsrc);
  g_cancellable_cancel (self->cancellable);
  return TRUE;
}

static gboolean
gst_unix_fd_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstUnixFdSrc *self = GST_UNIX_FD_SRC (bsrc);
  g_cancellable_reset (self->cancellable);
  return TRUE;
}

static GstClockTime
from_monotonic (GstClockTime timestamp, GstClockTime base_time,
    GstClockTimeDiff clock_diff)
{
  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    /* Convert from system monotonic clock time to pipeline clock time */
    if (clock_diff > 0 && clock_diff > timestamp)
      return 0;
    timestamp -= clock_diff;
    /* Convert to running time */
    if (base_time > timestamp)
      return 0;
    timestamp -= base_time;
  }
  return timestamp;
}

static void
close_and_free_fds (gint * fds, gint fds_len)
{
  for (int i = 0; i < fds_len; i++)
    g_close (fds[i], NULL);
  g_free (fds);
}

static GstFlowReturn
gst_unix_fd_src_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  GstUnixFdSrc *self = GST_UNIX_FD_SRC (psrc);
  CommandType command;
  GUnixFDList *fds = NULL;
  guint8 *payload = NULL;
  gsize payload_size;
  GError *error = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

again:
  /* Block until we receive a command */
  if (!gst_unix_fd_receive_command (self->socket, self->cancellable, &command,
          &fds, &payload, &payload_size, &error)) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      ret = GST_FLOW_FLUSHING;
      goto on_error;
    }
    GST_ERROR_OBJECT (self, "Failed to read from sink element: %s",
        error != NULL ? error->message : "Connection closed by peer");
    ret = GST_FLOW_ERROR;
    goto on_error;
  }

  switch (command) {
    case COMMAND_TYPE_RELEASE_BUFFER:
      GST_ERROR_OBJECT (self, "Received wrong command %d", command);
      ret = GST_FLOW_ERROR;
      goto on_error;
    case COMMAND_TYPE_NEW_BUFFER:{
      NewBufferPayload *new_buffer;
      guint32 payload_off = 0;
      if (!gst_unix_fd_parse_new_buffer (payload, payload_size, &new_buffer,
              &payload_off)) {
        GST_ERROR_OBJECT (self, "Received new-buffer with wrong payload size");
        ret = GST_FLOW_ERROR;
        goto on_error;
      }

      gint fds_arr_len = 0;
      gint *fds_arr =
          (fds != NULL) ? g_unix_fd_list_steal_fds (fds, &fds_arr_len) : NULL;
      if (fds_arr_len != new_buffer->n_memory) {
        GST_ERROR_OBJECT (self,
            "Received new buffer command with %d file descriptors instead of "
            "%d", fds_arr_len, new_buffer->n_memory);
        ret = GST_FLOW_ERROR;
        close_and_free_fds (fds_arr, fds_arr_len);
        goto on_error;
      }

      if (new_buffer->type >= MEMORY_TYPE_LAST) {
        GST_ERROR_OBJECT (self, "Unknown buffer type %d", new_buffer->type);
        ret = GST_FLOW_ERROR;
        close_and_free_fds (fds_arr, fds_arr_len);
        goto on_error;
      }
      GstAllocator *allocator = self->allocators[new_buffer->type];

      *outbuf = gst_buffer_new ();

      GstClockTime base_time =
          gst_element_get_base_time (GST_ELEMENT_CAST (self));
      GstClockTimeDiff clock_diff = 0;
      if (!self->uses_monotonic_clock) {
        clock_diff = GST_CLOCK_DIFF (g_get_monotonic_time () * GST_USECOND,
            gst_clock_get_time (GST_ELEMENT_CLOCK (self)));
      }

      GST_BUFFER_PTS (*outbuf) =
          from_monotonic (new_buffer->pts, base_time, clock_diff);
      GST_BUFFER_DTS (*outbuf) =
          from_monotonic (new_buffer->dts, base_time, clock_diff);
      GST_BUFFER_DURATION (*outbuf) = new_buffer->duration;
      GST_BUFFER_OFFSET (*outbuf) = new_buffer->offset;
      GST_BUFFER_OFFSET_END (*outbuf) = new_buffer->offset_end;
      GST_BUFFER_FLAGS (*outbuf) = new_buffer->flags;

      for (int i = 0; i < new_buffer->n_meta; i++) {
        guint32 consumed = 0;
        gst_meta_deserialize (*outbuf, (guint8 *) payload + payload_off,
            payload_size - payload_off, &consumed);
        if (consumed == 0) {
          GST_ERROR_OBJECT (self, "Malformed meta serialization");
          ret = GST_FLOW_ERROR;
          close_and_free_fds (fds_arr, fds_arr_len);
          gst_clear_buffer (outbuf);
          goto on_error;
        }
        payload_off += consumed;
      }

      GST_OBJECT_LOCK (self);
      if (new_buffer->n_memory > 0) {
        BufferContext *ctx = g_new0 (BufferContext, 1);
        ctx->id = new_buffer->id;
        ctx->n_memory = new_buffer->n_memory;
        for (int i = 0; i < new_buffer->n_memory; i++) {
          GstMemory *mem = gst_fd_allocator_alloc (allocator, fds_arr[i],
              new_buffer->memories[i].size, GST_FD_MEMORY_FLAG_NONE);
          gst_memory_resize (mem, new_buffer->memories[i].offset,
              new_buffer->memories[i].size);
          GST_MINI_OBJECT_FLAG_SET (mem, GST_MEMORY_FLAG_READONLY);

          g_hash_table_insert (self->memories, mem, ctx);
          gst_mini_object_weak_ref (GST_MINI_OBJECT_CAST (mem),
              (GstMiniObjectNotify) memory_weak_ref_cb, self);

          gst_buffer_append_memory (*outbuf, mem);
        }
      } else {
        /* This buffer has no memories, we can release it immediately otherwise
         * it gets leaked. */
        release_buffer (self, new_buffer->id);
      }
      GST_OBJECT_UNLOCK (self);

      g_free (fds_arr);

      break;
    }
    case COMMAND_TYPE_CAPS:{
      gchar *caps_str;
      if (!gst_unix_fd_parse_caps (payload, payload_size, &caps_str)) {
        GST_ERROR_OBJECT (self, "Received caps string is not nul-terminated");
        ret = GST_FLOW_ERROR;
        goto on_error;
      }
      GstCaps *caps = gst_caps_from_string (caps_str);
      GST_DEBUG_OBJECT (self, "Received caps %" GST_PTR_FORMAT, caps);
      gst_base_src_set_caps (GST_BASE_SRC_CAST (self), caps);
      gst_caps_unref (caps);
      break;
    }
    case COMMAND_TYPE_EOS:{
      GST_DEBUG_OBJECT (self, "Received EOS");
      ret = GST_FLOW_EOS;
      break;
    }
    default:
      /* Protocol could have been extended with new command */
      GST_DEBUG_OBJECT (self, "Ignoring unknown command %d", command);
      break;
  }

  if (*outbuf == NULL && ret == GST_FLOW_OK) {
    g_clear_object (&fds);
    g_clear_pointer (&payload, g_free);
    goto again;
  }

on_error:
  g_clear_error (&error);
  g_clear_object (&fds);
  g_free (payload);
  return ret;
}

static gboolean
gst_unix_fd_src_set_clock (GstElement * element, GstClock * clock)
{
  GstUnixFdSrc *self = (GstUnixFdSrc *) element;

  self->uses_monotonic_clock = FALSE;
  if (clock != NULL && G_OBJECT_TYPE (clock) == GST_TYPE_SYSTEM_CLOCK) {
    GstClockType clock_type;
    g_object_get (clock, "clock-type", &clock_type, NULL);
    self->uses_monotonic_clock = clock_type == GST_CLOCK_TYPE_MONOTONIC;
  }

  return GST_ELEMENT_CLASS (gst_unix_fd_src_parent_class)->set_clock (element,
      clock);
}

static void
gst_unix_fd_src_class_init (GstUnixFdSrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (unixfdsrc_debug, "unixfdsrc", 0,
      "Unix file descriptor source");
  gst_element_class_set_static_metadata (gstelement_class,
      "Unix file descriptor source", "Src", "Unix file descriptor source",
      "Xavier Claessens <xavier.claessens@collabora.com>");
  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  gobject_class->finalize = gst_unix_fd_src_finalize;
  gobject_class->set_property = gst_unix_fd_src_set_property;
  gobject_class->get_property = gst_unix_fd_src_get_property;

  gstelement_class->set_clock = GST_DEBUG_FUNCPTR (gst_unix_fd_src_set_clock);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_unix_fd_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_unix_fd_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_unix_fd_src_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_unix_fd_src_unlock_stop);

  gstpushsrc_class->create = gst_unix_fd_src_create;

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
