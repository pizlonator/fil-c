/*
 * Copyright (C) 2019 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

/**
 * SECTION:mlaudiosink
 * @short_description: Audio sink for Magic Leap platform
 * @see_also: #GstAudioSink
 *
 * An audio sink element for LuminOS, the Magic Leap platform. There are 2 modes
 * supported: normal and spatial. By default the audio is output directly to the
 * stereo speakers, but in spatial mode the audio will be localised in the 3D
 * environment. The user ears the sound as coming from a point in space, from a
 * given distance and direction.
 *
 * To enable the spatial mode, the application needs to set a sync bus
 * handler, using gst_bus_set_sync_handler(), to catch messages of type
 * %GST_MESSAGE_ELEMENT named "gst.mlaudiosink.need-app" and
 * "gst.mlaudiosink.need-audio-node". The need-app message will be posted first,
 * application must then set the #GstMLAudioSink::app property with the pointer
 * to application's lumin::BaseApp C++ object. That property can also be set on
 * element creation in which case the need-app message won't be posted. After
 * that, and if #GstMLAudioSink::app has been set, the need-audio-node message
 * is posted from lumin::BaseApp's main thread. The application must then create
 * a lumin::AudioNode C++ object, using lumin::Prism::createAudioNode(), and set
 * the #GstMLAudioSink::audio-node property. Note that it is important that the
 * lumin::AudioNode object must be created from within that message handler,
 * and in the caller's thread, this is a limitation/bug of the platform
 * (atleast until version 0.97).
 *
 * Here is an example of bus message handler to enable spatial sound:
 * ```C
 * static GstBusSyncReply
 * bus_sync_handler_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
 * {
 *   MyApplication * self = user_data;
 *
 *   if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ELEMENT) {
 *     if (gst_message_has_name (msg, "gst.mlaudiosink.need-app")) {
 *       g_object_set (G_OBJECT (msg->src), "app", &self->app, NULL);
 *     } else if (gst_message_has_name (msg, "gst.mlaudiosink.need-audio-node")) {
 *       self->audio_node = self->prism->createAudioNode ();
 *       self->audio_node->setSpatialSoundEnable (true);
 *       self->ui_node->addChild(self->audio_node);
 *       g_object_set (G_OBJECT (msg->src), "audio-node", self->audio_node, NULL);
 *     }
 *   }
 *   return GST_BUS_PASS;
 * }
 * ```
 *
 * Since: 1.18
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mlaudiosink.h"
#include "mlaudiowrapper.h"

GST_DEBUG_CATEGORY_EXTERN (mgl_debug);
#define GST_CAT_DEFAULT mgl_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { S16LE }, "
        "channels = (int) [ 1, 2 ], "
        "rate = (int) [ 16000, 48000 ], " "layout = (string) interleaved"));

/* HACK: After calling MLAudioStopSound() there is no way to know when it will
 * actually stop calling buffer_cb(). If the sink is disposed first, it would
 * crash. Keep here a set of active sinks. */
static GHashTable *active_sinks;
static GMutex active_sinks_mutex;

struct _GstMLAudioSink
{
  GstAudioSink parent;

  gpointer audio_node;
  gpointer app;

  GstMLAudioWrapper *wrapper;
  MLAudioBufferFormat format;
  uint32_t recommended_buffer_size;
  MLAudioBuffer buffer;
  guint buffer_offset;
  gboolean has_buffer;
  gboolean paused;
  gboolean stopped;

  GMutex mutex;
  GCond cond;
};

G_DEFINE_TYPE (GstMLAudioSink, gst_ml_audio_sink, GST_TYPE_AUDIO_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (mlaudiosink, "mlaudiosink",
    GST_RANK_PRIMARY + 10, GST_TYPE_ML_AUDIO_SINK,
    GST_DEBUG_CATEGORY_INIT (mgl_debug, "magicleap", 0, "Magic Leap elements"));

enum
{
  PROP_0,
  PROP_AUDIO_NODE,
  PROP_APP,
};

static void
gst_ml_audio_sink_init (GstMLAudioSink * self)
{
  g_mutex_init (&self->mutex);
  g_cond_init (&self->cond);
}

static void
gst_ml_audio_sink_dispose (GObject * object)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (object);

  g_mutex_clear (&self->mutex);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (gst_ml_audio_sink_parent_class)->dispose (object);
}

static void
gst_ml_audio_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (object);

  switch (prop_id) {
    case PROP_AUDIO_NODE:
      self->audio_node = g_value_get_pointer (value);
      break;
    case PROP_APP:
      self->app = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ml_audio_sink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (object);

  switch (prop_id) {
    case PROP_AUDIO_NODE:
      g_value_set_pointer (value, self->audio_node);
      break;
    case PROP_APP:
      g_value_set_pointer (value, self->app);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_ml_audio_sink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstCaps *caps;

  caps = gst_static_caps_get (&sink_template.static_caps);

  if (filter) {
    gst_caps_replace (&caps,
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST));
  }

  return caps;
}

static gboolean
gst_ml_audio_sink_open (GstAudioSink * sink)
{
  /* Nothing to do in open/close */
  return TRUE;
}

static void
buffer_cb (MLHandle handle, gpointer user_data)
{
  GstMLAudioSink *self = user_data;

  g_mutex_lock (&active_sinks_mutex);
  if (!g_hash_table_contains (active_sinks, self))
    goto out;

  gst_ml_audio_wrapper_set_handle (self->wrapper, handle);

  g_mutex_lock (&self->mutex);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);

out:
  g_mutex_unlock (&active_sinks_mutex);
}

/* Must be called with self->mutex locked */
static gboolean
wait_for_buffer (GstMLAudioSink * self)
{
  gboolean ret = TRUE;

  while (!self->has_buffer && !self->stopped) {
    MLResult result;

    result = gst_ml_audio_wrapper_get_buffer (self->wrapper, &self->buffer);
    if (result == MLResult_Ok) {
      self->has_buffer = TRUE;
      self->buffer_offset = 0;
    } else if (result == MLAudioResult_BufferNotReady) {
      g_cond_wait (&self->cond, &self->mutex);
    } else {
      GST_ERROR_OBJECT (self, "Failed to get output buffer: %d", result);
      ret = FALSE;
      break;
    }
  }

  return ret;
}

static gboolean
create_sound_cb (GstMLAudioWrapper * wrapper, gpointer user_data)
{
  GstMLAudioSink *self = user_data;
  MLResult result;

  if (self->app) {
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_element (GST_OBJECT (self),
            gst_structure_new_empty ("gst.mlaudiosink.need-audio-node")));
  }

  gst_ml_audio_wrapper_set_node (self->wrapper, self->audio_node);

  result = gst_ml_audio_wrapper_create_sound (self->wrapper, &self->format,
      self->recommended_buffer_size, buffer_cb, self);
  if (result != MLResult_Ok) {
    GST_ERROR_OBJECT (self, "Failed to create output stream: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_ml_audio_sink_prepare (GstAudioSink * sink, GstAudioRingBufferSpec * spec)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (sink);
  float max_pitch = 1.0f;
  uint32_t min_size;
  MLResult result;

  result =
      MLAudioGetOutputStreamDefaults (GST_AUDIO_INFO_CHANNELS (&spec->info),
      GST_AUDIO_INFO_RATE (&spec->info), max_pitch, &self->format,
      &self->recommended_buffer_size, &min_size);
  if (result != MLResult_Ok) {
    GST_ERROR_OBJECT (self, "Failed to get output stream defaults: %d", result);
    return FALSE;
  }

  if (!self->app) {
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_element (GST_OBJECT (self),
            gst_structure_new_empty ("gst.mlaudiosink.need-app")));
  }

  self->wrapper = gst_ml_audio_wrapper_new (self->app);
  self->has_buffer = FALSE;
  self->stopped = FALSE;
  self->paused = FALSE;

  g_mutex_lock (&active_sinks_mutex);
  g_hash_table_add (active_sinks, self);
  g_mutex_unlock (&active_sinks_mutex);

  /* createAudioNode() and createSoundWithOutputStream() must both be called in
   * application's main thread, and in a single main loop iteration. */
  if (!gst_ml_audio_wrapper_invoke_sync (self->wrapper, create_sound_cb, self))
    return FALSE;

  return TRUE;
}

static void
release_current_buffer (GstMLAudioSink * self)
{
  if (self->has_buffer) {
    memset (self->buffer.ptr + self->buffer_offset, 0,
        self->buffer.size - self->buffer_offset);
    gst_ml_audio_wrapper_release_buffer (self->wrapper);
    self->has_buffer = false;
  }
}

static gboolean
gst_ml_audio_sink_unprepare (GstAudioSink * sink)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (sink);

  g_mutex_lock (&active_sinks_mutex);
  g_hash_table_remove (active_sinks, self);
  release_current_buffer (self);
  g_clear_pointer (&self->wrapper, gst_ml_audio_wrapper_free);
  g_mutex_unlock (&active_sinks_mutex);

  return TRUE;
}

static gboolean
gst_ml_audio_sink_close (GstAudioSink * sink)
{
  /* Nothing to do in open/close */
  return TRUE;
}

static gint
gst_ml_audio_sink_write (GstAudioSink * sink, gpointer data, guint length)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (sink);
  guint8 *input = data;
  gint written = 0;

  g_mutex_lock (&self->mutex);

  while (length > 0) {
    MLResult result;
    guint to_write;

    if (!wait_for_buffer (self)) {
      written = -1;
      break;
    }

    if (self->stopped) {
      /* Pretend we have written the full buffer (drop data) and return
       * immediately. */
      release_current_buffer (self);
      gst_ml_audio_wrapper_stop_sound (self->wrapper);
      written = length;
      break;
    }

    to_write = MIN (length, self->buffer.size - self->buffer_offset);
    memcpy (self->buffer.ptr + self->buffer_offset, input + written, to_write);
    self->buffer_offset += to_write;
    if (self->buffer_offset == self->buffer.size) {
      result = gst_ml_audio_wrapper_release_buffer (self->wrapper);
      if (result != MLResult_Ok) {
        GST_ERROR_OBJECT (self, "Failed to release buffer: %d", result);
        written = -1;
        break;
      }
      self->has_buffer = FALSE;
    }

    length -= to_write;
    written += to_write;
  }

  if (self->paused) {
    /* Pause was requested and we finished writing current buffer.
     * See https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/665
     */
    gst_ml_audio_wrapper_pause_sound (self->wrapper);
  }

  g_mutex_unlock (&self->mutex);

  return written;
}

static guint
gst_ml_audio_sink_delay (GstAudioSink * sink)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (sink);
  MLResult result;
  float latency_ms;

  result = gst_ml_audio_wrapper_get_latency (self->wrapper, &latency_ms);
  if (result != MLResult_Ok) {
    GST_ERROR_OBJECT (self, "Failed to get latency: %d", result);
    return 0;
  }

  return latency_ms * self->format.samples_per_second / 1000;
}

static void
gst_ml_audio_sink_pause (GstAudioSink * sink)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (sink);

  g_mutex_lock (&self->mutex);
  self->paused = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);
}

static void
gst_ml_audio_sink_resume (GstAudioSink * sink)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (sink);

  g_mutex_lock (&self->mutex);
  self->paused = FALSE;
  gst_ml_audio_wrapper_resume_sound (self->wrapper);
  g_mutex_unlock (&self->mutex);
}

static void
gst_ml_audio_sink_stop (GstAudioSink * sink)
{
  GstMLAudioSink *self = GST_ML_AUDIO_SINK (sink);

  g_mutex_lock (&self->mutex);
  self->stopped = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);
}

static void
gst_ml_audio_sink_class_init (GstMLAudioSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GstAudioSinkClass *audiosink_class = GST_AUDIO_SINK_CLASS (klass);

  active_sinks = g_hash_table_new (NULL, NULL);
  g_mutex_init (&active_sinks_mutex);

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_dispose);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_ml_audio_sink_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_ml_audio_sink_get_property);

  g_object_class_install_property (gobject_class,
      PROP_AUDIO_NODE, g_param_spec_pointer ("audio-node",
          "A pointer to a lumin::AudioNode object",
          "Enable spatial sound", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_APP, g_param_spec_pointer ("app",
          "A pointer to a lumin::BaseApp object",
          "Enable spatial sound", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Magic Leap Audio Sink",
      "Sink/Audio", "Plays audio on a Magic Leap device",
      "Xavier Claessens <xavier.claessens@collabora.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_getcaps);

  audiosink_class->open = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_open);
  audiosink_class->prepare = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_prepare);
  audiosink_class->unprepare = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_unprepare);
  audiosink_class->close = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_close);
  audiosink_class->write = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_write);
  audiosink_class->delay = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_delay);
  audiosink_class->pause = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_pause);
  audiosink_class->resume = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_resume);
  audiosink_class->stop = GST_DEBUG_FUNCPTR (gst_ml_audio_sink_stop);
}
