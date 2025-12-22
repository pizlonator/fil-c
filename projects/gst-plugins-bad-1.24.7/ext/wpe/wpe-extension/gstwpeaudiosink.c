/* Copyright (C) <2020> Philippe Normand <philn@igalia.com>
 * Copyright (C) <2021> Thibault Saunier <tsaunier@igalia.com>
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the
 * GNU Library General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License along with this
 * library; if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "gstwpeextension.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define gst_wpe_audio_sink_parent_class parent_class
GST_DEBUG_CATEGORY (wpe_audio_sink_debug);
#define GST_CAT_DEFAULT wpe_audio_sink_debug

struct _GstWpeAudioSink
{
  GstBaseSink parent;

  guint32 id;
  GCancellable *cancellable;;

  gchar *caps;

  GMutex buf_lock;
  GCond buf_cond;
  GUnixFDList *fdlist;
};

static guint id = -1;           /* atomic */

G_DEFINE_TYPE_WITH_CODE (GstWpeAudioSink, gst_wpe_audio_sink,
    GST_TYPE_BASE_SINK, GST_DEBUG_CATEGORY_INIT (wpe_audio_sink_debug,
        "wpeaudio_sink", 0, "WPE Sink"););

static GstStaticPadTemplate audio_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw"));

static void
message_consumed_cb (GObject * source_object, GAsyncResult * res,
    GstWpeAudioSink * self)
{
  g_mutex_lock (&self->buf_lock);
  g_cond_broadcast (&self->buf_cond);
  g_mutex_unlock (&self->buf_lock);
}

static GstFlowReturn
render (GstBaseSink * sink, GstBuffer * buf)
{
  gsize written_bytes;
  static int init = 0;
  char filename[1024];
  const gint *fds;
  WebKitUserMessage *msg;
  GstMapInfo info;
  GstWpeAudioSink *self = GST_WPE_AUDIO_SINK (sink);

  if (!self->caps) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Trying to render buffer before caps were set"), (NULL));

    return GST_FLOW_ERROR;
  }

  if (!gst_buffer_map (buf, &info, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Failed to map input buffer"),
        (NULL));

    return GST_FLOW_ERROR;
  }

  if (!self->fdlist) {
    gint fds[1] = { -1 };

#ifdef HAVE_MEMFD_CREATE
    fds[0] = memfd_create ("gstwpe-shm", MFD_CLOEXEC);
#endif

    if (fds[0] < 0) {
      /* allocate shm pool */
      snprintf (filename, 1024, "%s/%s-%d-%s", g_get_user_runtime_dir (),
          "gstwpe-shm", init++, "XXXXXX");

      fds[0] = g_mkstemp (filename);
      if (fds[0] < 0) {
        gst_buffer_unmap (buf, &info);
        GST_ELEMENT_ERROR (self, RESOURCE, READ,
            ("opening temp file %s failed: %s", filename, strerror (errno)),
            (NULL));
        return GST_FLOW_ERROR;
      }

      unlink (filename);
    }

    if (fds[0] <= 0)
      goto write_error;

    self->fdlist = g_unix_fd_list_new_from_array (fds, 1);
    msg = webkit_user_message_new_with_fd_list ("gstwpe.set_shm",
        g_variant_new ("(u)", self->id), self->fdlist);
    gst_wpe_extension_send_message (msg, self->cancellable, NULL, NULL);
  }

  fds = g_unix_fd_list_peek_fds (self->fdlist, NULL);
  if (ftruncate (fds[0], info.size) == -1)
    goto write_error;

  written_bytes = write (fds[0], info.data, info.size);
  if (written_bytes < 0)
    goto write_error;

  if (written_bytes != info.size)
    goto write_error;

  if (lseek (fds[0], 0, SEEK_SET) == -1)
    goto write_error;

  msg = webkit_user_message_new ("gstwpe.new_buffer",
      g_variant_new ("(ut)", self->id, info.size));

  g_mutex_lock (&self->buf_lock);
  gst_wpe_extension_send_message (msg, self->cancellable,
      (GAsyncReadyCallback) message_consumed_cb, self);
  g_cond_wait (&self->buf_cond, &self->buf_lock);
  g_mutex_unlock (&self->buf_lock);

  gst_buffer_unmap (buf, &info);

  return GST_FLOW_OK;

write_error:
  gst_buffer_unmap (buf, &info);
  GST_ELEMENT_ERROR (self, RESOURCE, WRITE, ("Couldn't write memfd: %s",
          strerror (errno)), (NULL));

  return GST_FLOW_ERROR;
}

static gboolean
set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstWpeAudioSink *self = GST_WPE_AUDIO_SINK (sink);
  gchar *stream_id;

  if (self->caps) {
    GST_ERROR_OBJECT (sink, "Renegotiation is not supported yet");

    return FALSE;
  }

  self->caps = gst_caps_to_string (caps);
  self->id = g_atomic_int_add (&id, 1);
  stream_id = gst_pad_get_stream_id (GST_BASE_SINK_PAD (sink));
  gst_wpe_extension_send_message (webkit_user_message_new ("gstwpe.new_stream",
          g_variant_new ("(uss)", self->id, self->caps, stream_id)),
      self->cancellable, NULL, NULL);
  g_free (stream_id);

  return TRUE;
}

static gboolean
unlock (GstBaseSink * sink)
{
  GstWpeAudioSink *self = GST_WPE_AUDIO_SINK (sink);

  g_cancellable_cancel (self->cancellable);

  return TRUE;
}

static gboolean
unlock_stop (GstBaseSink * sink)
{
  GstWpeAudioSink *self = GST_WPE_AUDIO_SINK (sink);
  GCancellable *cancellable = self->cancellable;

  self->cancellable = g_cancellable_new ();
  g_object_unref (cancellable);

  return TRUE;
}

static void
_cancelled_cb (GCancellable * _cancellable, GstWpeAudioSink * self)
{
  g_mutex_lock (&self->buf_lock);
  g_cond_broadcast (&self->buf_cond);
  g_mutex_unlock (&self->buf_lock);
}

static gboolean
stop (GstBaseSink * sink)
{
  GstWpeAudioSink *self = GST_WPE_AUDIO_SINK (sink);

  if (!self->caps) {
    GST_DEBUG_OBJECT (sink, "Stopped before started");

    return TRUE;
  }

  /* Stop processing and claim buffers back */
  g_cancellable_cancel (self->cancellable);

  GST_DEBUG_OBJECT (sink, "Stopping %d", self->id);
  gst_wpe_extension_send_message (webkit_user_message_new ("gstwpe.stop",
          g_variant_new_uint32 (self->id)), self->cancellable, NULL, NULL);

  return TRUE;
}

static GstStateChangeReturn
change_state (GstElement * element, GstStateChange transition)
{
  GstWpeAudioSink *self = GST_WPE_AUDIO_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
      if (g_cancellable_is_cancelled (self->cancellable)) {
        GCancellable *cancellable = self->cancellable;
        self->cancellable = g_cancellable_new ();

        g_object_unref (cancellable);
      }
      break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      g_cancellable_cancel (self->cancellable);

      gst_wpe_extension_send_message (webkit_user_message_new ("gstwpe.pause",
              g_variant_new_uint32 (self->id)), NULL, NULL, NULL);

      break;
    default:
      break;
  }

  return GST_CALL_PARENT_WITH_DEFAULT (GST_ELEMENT_CLASS,
      change_state, (element, transition), GST_STATE_CHANGE_SUCCESS);
}

static void
dispose (GObject * object)
{
  GstWpeAudioSink *self = GST_WPE_AUDIO_SINK (object);

  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->caps, g_free);
}

static void
gst_wpe_audio_sink_init (GstWpeAudioSink * self)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (self);
  GstPadTemplate *pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  g_return_if_fail (pad_template != NULL);

  self->cancellable = g_cancellable_new ();
  g_cancellable_connect (self->cancellable,
      G_CALLBACK (_cancelled_cb), self, NULL);
}

static void
gst_wpe_audio_sink_class_init (GstWpeAudioSinkClass * klass)
{
  GstPadTemplate *tmpl;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;

  object_class->dispose = dispose;

  gst_element_class_set_static_metadata (gstelement_class,
      "WPE internal audio sink", "Sink/Audio",
      "Internal sink to be used in wpe when running inside gstwpe",
      "Thibault Saunier <tsaunier@igalia.com>");

  tmpl = gst_static_pad_template_get (&audio_sink_factory);
  gst_element_class_add_pad_template (gstelement_class, tmpl);

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (change_state);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (stop);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (unlock);
  gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR (unlock_stop);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (render);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (set_caps);
}
