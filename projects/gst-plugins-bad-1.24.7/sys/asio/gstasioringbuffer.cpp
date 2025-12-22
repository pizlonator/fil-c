/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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


#include "gstasioringbuffer.h"
#include <string.h>
#include "gstasioutils.h"
#include "gstasioobject.h"

GST_DEBUG_CATEGORY_STATIC (gst_asio_ring_buffer_debug);
#define GST_CAT_DEFAULT gst_asio_ring_buffer_debug

struct _GstAsioRingBuffer
{
  GstAudioRingBuffer parent;

  GstAsioDeviceClassType type;

  GstAsioObject *asio_object;
  guint *channel_indices;
  guint num_channels;
  ASIOBufferInfo **infos;

  guint64 callback_id;
  gboolean callback_installed;

  gboolean running;
  guint buffer_size;

  /* Used to detect sample gap */
  gboolean is_first;
  guint64 expected_sample_position;
  gboolean trace_sample_position;
};

enum
{
  PROP_0,
  PROP_DEVICE_INFO,
};

static void gst_asio_ring_buffer_dispose (GObject * object);

static gboolean gst_asio_ring_buffer_open_device (GstAudioRingBuffer * buf);
static gboolean gst_asio_ring_buffer_close_device (GstAudioRingBuffer * buf);
static gboolean gst_asio_ring_buffer_acquire (GstAudioRingBuffer * buf,
    GstAudioRingBufferSpec * spec);
static gboolean gst_asio_ring_buffer_release (GstAudioRingBuffer * buf);
static gboolean gst_asio_ring_buffer_start (GstAudioRingBuffer * buf);
static gboolean gst_asio_ring_buffer_stop (GstAudioRingBuffer * buf);
static guint gst_asio_ring_buffer_delay (GstAudioRingBuffer * buf);

static gboolean gst_asio_buffer_switch_cb (GstAsioObject * obj,
    glong index, ASIOBufferInfo * infos, guint num_infos,
    ASIOChannelInfo * input_channel_infos,
    ASIOChannelInfo * output_channel_infos,
    ASIOSampleRate sample_rate, glong buffer_size, gpointer user_data);

#define gst_asio_ring_buffer_parent_class parent_class
G_DEFINE_TYPE (GstAsioRingBuffer, gst_asio_ring_buffer,
    GST_TYPE_AUDIO_RING_BUFFER);

static void
gst_asio_ring_buffer_class_init (GstAsioRingBufferClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAudioRingBufferClass *ring_buffer_class =
      GST_AUDIO_RING_BUFFER_CLASS (klass);

  gobject_class->dispose = gst_asio_ring_buffer_dispose;

  ring_buffer_class->open_device =
      GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_open_device);
  ring_buffer_class->close_device =
      GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_close_device);
  ring_buffer_class->acquire = GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_acquire);
  ring_buffer_class->release = GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_release);
  ring_buffer_class->start = GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_start);
  ring_buffer_class->resume = GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_start);
  ring_buffer_class->stop = GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_stop);
  ring_buffer_class->delay = GST_DEBUG_FUNCPTR (gst_asio_ring_buffer_delay);

  GST_DEBUG_CATEGORY_INIT (gst_asio_ring_buffer_debug,
      "asioringbuffer", 0, "asioringbuffer");
}

static void
gst_asio_ring_buffer_init (GstAsioRingBuffer * self)
{
}

static void
gst_asio_ring_buffer_dispose (GObject * object)
{
  GstAsioRingBuffer *self = GST_ASIO_RING_BUFFER (object);

  gst_clear_object (&self->asio_object);
  g_clear_pointer (&self->channel_indices, g_free);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gboolean
gst_asio_ring_buffer_open_device (GstAudioRingBuffer * buf)
{
  GstAsioRingBuffer *self = GST_ASIO_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (self, "Open");

  return TRUE;
}

static gboolean
gst_asio_ring_buffer_close_device (GstAudioRingBuffer * buf)
{
  GstAsioRingBuffer *self = GST_ASIO_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (self, "Close");

  return TRUE;
}

#define PACK_ASIO_64(v) ((v).lo | ((guint64)((v).hi) << 32))

static gboolean
gst_asio_buffer_switch_cb (GstAsioObject * obj, glong index,
    ASIOBufferInfo * infos, guint num_infos,
    ASIOChannelInfo * input_channel_infos,
    ASIOChannelInfo * output_channel_infos,
    ASIOSampleRate sample_rate, glong buffer_size,
    ASIOTime * time_info, gpointer user_data)
{
  GstAsioRingBuffer *self = (GstAsioRingBuffer *) user_data;
  GstAudioRingBuffer *ringbuffer = GST_AUDIO_RING_BUFFER_CAST (self);
  gint segment;
  guint8 *readptr;
  gint len;
  guint i, j;
  guint num_channels = 0;
  guint bps = GST_AUDIO_INFO_WIDTH (&ringbuffer->spec.info) >> 3;

  g_assert (index == 0 || index == 1);
  g_assert (num_infos >= self->num_channels);

  GST_TRACE_OBJECT (self, "Buffer Switch callback, index %ld", index);

  if (!gst_audio_ring_buffer_prepare_read (ringbuffer,
          &segment, &readptr, &len)) {
    GST_WARNING_OBJECT (self, "No segment available");
    return TRUE;
  }

  GST_TRACE_OBJECT (self, "segment %d, length %d", segment, len);

  /* Check missing frames */
  if (self->type == GST_ASIO_DEVICE_CLASS_CAPTURE) {
    if (self->is_first) {
      if (time_info) {
        self->expected_sample_position =
            PACK_ASIO_64 (time_info->timeInfo.samplePosition) + buffer_size;
        self->trace_sample_position = TRUE;
      } else {
        GST_WARNING_OBJECT (self, "ASIOTime is not available");
        self->trace_sample_position = FALSE;
      }

      self->is_first = FALSE;
    } else if (self->trace_sample_position) {
      if (!time_info) {
        GST_WARNING_OBJECT (self, "ASIOTime is not available");
        self->trace_sample_position = FALSE;
      } else {
        guint64 sample_position =
            PACK_ASIO_64 (time_info->timeInfo.samplePosition);
        if (self->expected_sample_position < sample_position) {
          guint64 gap_frames = sample_position - self->expected_sample_position;
          gint gap_size = gap_frames * bps;

          GST_WARNING_OBJECT (self, "%" G_GUINT64_FORMAT " frames are missing");

          while (gap_size >= len) {
            gst_audio_format_info_fill_silence (ringbuffer->spec.info.finfo,
                readptr, len);
            gst_audio_ring_buffer_advance (ringbuffer, 1);

            gst_audio_ring_buffer_prepare_read (ringbuffer,
                &segment, &readptr, &len);

            gap_size -= len;
          }
        }

        self->expected_sample_position = sample_position + buffer_size;
        GST_TRACE_OBJECT (self, "Sample Position %" G_GUINT64_FORMAT
            ", next: %" G_GUINT64_FORMAT, sample_position,
            self->expected_sample_position);
      }
    }
  }

  /* Given @infos might contain more channel data, pick channels what we want to
   * read */
  for (i = 0; i < num_infos; i++) {
    ASIOBufferInfo *info = &infos[i];

    if (self->type == GST_ASIO_DEVICE_CLASS_CAPTURE) {
      if (!info->isInput)
        continue;
    } else {
      if (info->isInput)
        continue;
    }

    for (j = 0; j < self->num_channels; j++) {
      if (self->channel_indices[j] != info->channelNum)
        continue;

      g_assert (num_channels < self->num_channels);
      self->infos[num_channels++] = info;
      break;
    }
  }

  if (num_channels < self->num_channels) {
    GST_ERROR_OBJECT (self, "Too small number of channel %d (expected %d)",
        num_channels, self->num_channels);
  } else {
    if (self->type == GST_ASIO_DEVICE_CLASS_CAPTURE ||
        self->type == GST_ASIO_DEVICE_CLASS_LOOPBACK_CAPTURE) {
      if (num_channels == 1) {
        memcpy (readptr, self->infos[0]->buffers[index], len);
      } else {
        guint gst_offset = 0, asio_offset = 0;

        /* Interleaves audio */
        while (gst_offset < len) {
          for (i = 0; i < num_channels; i++) {
            ASIOBufferInfo *info = self->infos[i];

            memcpy (readptr + gst_offset,
                ((guint8 *) info->buffers[index]) + asio_offset, bps);

            gst_offset += bps;
          }
          asio_offset += bps;
        }
      }
    } else {
      if (num_channels == 1) {
        memcpy (self->infos[0]->buffers[index], readptr, len);
      } else {
        guint gst_offset = 0, asio_offset = 0;

        /* Interleaves audio */
        while (gst_offset < len) {
          for (i = 0; i < num_channels; i++) {
            ASIOBufferInfo *info = self->infos[i];

            memcpy (((guint8 *) info->buffers[index]) + asio_offset,
                readptr + gst_offset, bps);

            gst_offset += bps;
          }
          asio_offset += bps;
        }
      }
    }
  }

  if (self->type == GST_ASIO_DEVICE_CLASS_RENDER)
    gst_audio_ring_buffer_clear (ringbuffer, segment);
  gst_audio_ring_buffer_advance (ringbuffer, 1);

  return TRUE;
}

static gboolean
gst_asio_ring_buffer_acquire (GstAudioRingBuffer * buf,
    GstAudioRingBufferSpec * spec)
{
  GstAsioRingBuffer *self = GST_ASIO_RING_BUFFER (buf);

  if (!self->asio_object) {
    GST_ERROR_OBJECT (self, "No configured ASIO object");
    return FALSE;
  }

  if (!self->channel_indices || self->num_channels == 0) {
    GST_ERROR_OBJECT (self, "No configured channels");
    return FALSE;
  }

  if (!gst_asio_object_set_sample_rate (self->asio_object,
          GST_AUDIO_INFO_RATE (&spec->info))) {
    GST_ERROR_OBJECT (self, "Failed to set sample rate");
    return FALSE;
  }

  spec->segsize = self->buffer_size *
      (GST_AUDIO_INFO_WIDTH (&spec->info) >> 3) *
      GST_AUDIO_INFO_CHANNELS (&spec->info);
  spec->segtotal = 2;

  buf->size = spec->segtotal * spec->segsize;
  buf->memory = (guint8 *) g_malloc (buf->size);
  gst_audio_format_info_fill_silence (buf->spec.info.finfo,
      buf->memory, buf->size);

  return TRUE;
}

static gboolean
gst_asio_ring_buffer_release (GstAudioRingBuffer * buf)
{
  GST_DEBUG_OBJECT (buf, "Release");

  g_clear_pointer (&buf->memory, g_free);

  return TRUE;
}

static gboolean
gst_asio_ring_buffer_start (GstAudioRingBuffer * buf)
{
  GstAsioRingBuffer *self = GST_ASIO_RING_BUFFER (buf);
  GstAsioObjectCallbacks callbacks;

  GST_DEBUG_OBJECT (buf, "Start");

  callbacks.buffer_switch = gst_asio_buffer_switch_cb;
  callbacks.user_data = self;

  self->is_first = TRUE;
  self->expected_sample_position = 0;

  if (!gst_asio_object_install_callback (self->asio_object, self->type,
          &callbacks, &self->callback_id)) {
    GST_ERROR_OBJECT (self, "Failed to install callback");
    return FALSE;
  }

  self->callback_installed = TRUE;

  if (!gst_asio_object_start (self->asio_object)) {
    GST_ERROR_OBJECT (self, "Failed to start");

    gst_asio_ring_buffer_stop (buf);

    return FALSE;
  }

  self->running = TRUE;

  return TRUE;
}

static gboolean
gst_asio_ring_buffer_stop (GstAudioRingBuffer * buf)
{
  GstAsioRingBuffer *self = GST_ASIO_RING_BUFFER (buf);

  GST_DEBUG_OBJECT (buf, "Stop");

  self->running = FALSE;

  if (!self->asio_object)
    return TRUE;

  if (self->callback_installed)
    gst_asio_object_uninstall_callback (self->asio_object, self->callback_id);

  self->callback_installed = FALSE;
  self->callback_id = 0;
  self->is_first = TRUE;
  self->expected_sample_position = 0;

  return TRUE;
}

static guint
gst_asio_ring_buffer_delay (GstAudioRingBuffer * buf)
{
  /* FIXME: impl. */

  return 0;
}

GstAsioRingBuffer *
gst_asio_ring_buffer_new (GstAsioObject * object, GstAsioDeviceClassType type,
    const gchar * name)
{
  GstAsioRingBuffer *self;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (object), nullptr);

  self =
      (GstAsioRingBuffer *) g_object_new (GST_TYPE_ASIO_RING_BUFFER,
      "name", name, nullptr);
  g_assert (self);

  self->type = type;
  self->asio_object = (GstAsioObject *) gst_object_ref (object);

  return self;
}

gboolean
gst_asio_ring_buffer_configure (GstAsioRingBuffer * buf,
    guint * channel_indices, guint num_channles, guint preferred_buffer_size)
{
  g_return_val_if_fail (GST_IS_ASIO_RING_BUFFER (buf), FALSE);
  g_return_val_if_fail (buf->asio_object != nullptr, FALSE);
  g_return_val_if_fail (num_channles > 0, FALSE);

  GST_DEBUG_OBJECT (buf, "Configure");

  buf->buffer_size = preferred_buffer_size;

  if (!gst_asio_object_create_buffers (buf->asio_object, buf->type,
          channel_indices, num_channles, &buf->buffer_size)) {
    GST_ERROR_OBJECT (buf, "Failed to configure");

    g_clear_pointer (&buf->channel_indices, g_free);
    buf->num_channels = 0;

    return FALSE;
  }

  GST_DEBUG_OBJECT (buf, "configured buffer size: %d", buf->buffer_size);

  g_free (buf->channel_indices);
  buf->channel_indices = g_new0 (guint, num_channles);

  for (guint i = 0; i < num_channles; i++)
    buf->channel_indices[i] = channel_indices[i];

  buf->num_channels = num_channles;

  g_clear_pointer (&buf->infos, g_free);
  buf->infos = g_new0 (ASIOBufferInfo *, num_channles);

  return TRUE;
}

GstCaps *
gst_asio_ring_buffer_get_caps (GstAsioRingBuffer * buf)
{
  g_return_val_if_fail (GST_IS_ASIO_RING_BUFFER (buf), nullptr);
  g_assert (buf->asio_object != nullptr);

  return gst_asio_object_get_caps (buf->asio_object,
      buf->type, buf->num_channels, buf->num_channels);
}
