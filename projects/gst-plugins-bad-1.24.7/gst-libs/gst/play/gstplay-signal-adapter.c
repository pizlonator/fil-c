/* GStreamer
 *
 * Copyright (C) 2019-2020 Stephan Hesse <stephan@emliri.com>
 * Copyright (C) 2020 Philippe Normand <philn@igalia.com>
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

#include "gstplay.h"
#include "gstplay-signal-adapter.h"
#include "gstplay-message-private.h"

GST_DEBUG_CATEGORY_STATIC (gst_play_signal_adapter_debug);
#define GST_CAT_DEFAULT gst_play_signal_adapter_debug

enum
{
  SIGNAL_URI_LOADED,
  SIGNAL_POSITION_UPDATED,
  SIGNAL_DURATION_CHANGED,
  SIGNAL_STATE_CHANGED,
  SIGNAL_BUFFERING,
  SIGNAL_END_OF_STREAM,
  SIGNAL_ERROR,
  SIGNAL_WARNING,
  SIGNAL_VIDEO_DIMENSIONS_CHANGED,
  SIGNAL_MEDIA_INFO_UPDATED,
  SIGNAL_VOLUME_CHANGED,
  SIGNAL_MUTE_CHANGED,
  SIGNAL_SEEK_DONE,
  SIGNAL_LAST
};

enum
{
  PROP_0,
  PROP_PLAY,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

struct _GstPlaySignalAdapter
{
  GObject parent;
  GstBus *bus;
  GstPlay *play;
  GSource *source;
};

struct _GstPlaySignalAdapterClass
{
  GObjectClass parent_class;
};

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_play_signal_adapter_debug, "gst-play-signal-adapter", \
      0, "GstPlay signal adapter")

#define parent_class gst_play_signal_adapter_parent_class
G_DEFINE_TYPE_WITH_CODE (GstPlaySignalAdapter, gst_play_signal_adapter,
    G_TYPE_OBJECT, _do_init);

static guint signals[SIGNAL_LAST] = { 0, };

static void
gst_play_signal_adapter_emit (GstPlaySignalAdapter * self,
    const GstStructure * message_data)
{
  GstPlayMessage play_message_type;
  g_return_if_fail (g_str_equal (gst_structure_get_name (message_data),
          GST_PLAY_MESSAGE_DATA));

  GST_LOG ("Emitting message %" GST_PTR_FORMAT, message_data);
  gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_TYPE,
      GST_TYPE_PLAY_MESSAGE, &play_message_type, NULL);

  switch (play_message_type) {
    case GST_PLAY_MESSAGE_URI_LOADED:{
      const gchar *uri =
          gst_structure_get_string (message_data, GST_PLAY_MESSAGE_DATA_URI);
      g_signal_emit (self, signals[SIGNAL_URI_LOADED], 0, uri);
      break;
    }
    case GST_PLAY_MESSAGE_POSITION_UPDATED:{
      GstClockTime pos = GST_CLOCK_TIME_NONE;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_POSITION,
          GST_TYPE_CLOCK_TIME, &pos, NULL);
      g_signal_emit (self, signals[SIGNAL_POSITION_UPDATED], 0, pos);
      break;
    }
    case GST_PLAY_MESSAGE_DURATION_CHANGED:{
      GstClockTime duration = GST_CLOCK_TIME_NONE;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_DURATION,
          GST_TYPE_CLOCK_TIME, &duration, NULL);
      g_signal_emit (self, signals[SIGNAL_DURATION_CHANGED], 0, duration);
      break;
    }
    case GST_PLAY_MESSAGE_STATE_CHANGED:{
      GstPlayState state = 0;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_PLAY_STATE,
          GST_TYPE_PLAY_STATE, &state, NULL);
      g_signal_emit (self, signals[SIGNAL_STATE_CHANGED], 0, state);
      break;
    }
    case GST_PLAY_MESSAGE_BUFFERING:{
      guint percent = 0;
      gst_structure_get (message_data,
          GST_PLAY_MESSAGE_DATA_BUFFERING_PERCENT, G_TYPE_UINT, &percent, NULL);
      g_signal_emit (self, signals[SIGNAL_BUFFERING], 0, percent);
      break;
    }
    case GST_PLAY_MESSAGE_END_OF_STREAM:
      g_signal_emit (self, signals[SIGNAL_END_OF_STREAM], 0);
      break;
    case GST_PLAY_MESSAGE_ERROR:{
      GError *error = NULL;
      GstStructure *details = NULL;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_ERROR,
          G_TYPE_ERROR, &error, GST_PLAY_MESSAGE_DATA_ERROR_DETAILS,
          GST_TYPE_STRUCTURE, &details, NULL);
      g_signal_emit (self, signals[SIGNAL_ERROR], 0, error, details);
      g_error_free (error);
      if (details)
        gst_structure_free (details);
      break;
    }
    case GST_PLAY_MESSAGE_WARNING:{
      GError *error = NULL;
      GstStructure *details = NULL;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_WARNING,
          G_TYPE_ERROR, &error, GST_PLAY_MESSAGE_DATA_WARNING_DETAILS,
          GST_TYPE_STRUCTURE, &details, NULL);
      g_signal_emit (self, signals[SIGNAL_WARNING], 0, error, details);
      g_error_free (error);
      if (details)
        gst_structure_free (details);
      break;
    }
    case GST_PLAY_MESSAGE_VIDEO_DIMENSIONS_CHANGED:{
      guint width = 0;
      guint height = 0;
      gst_structure_get (message_data,
          GST_PLAY_MESSAGE_DATA_VIDEO_WIDTH, G_TYPE_UINT, &width,
          GST_PLAY_MESSAGE_DATA_VIDEO_HEIGHT, G_TYPE_UINT, &height, NULL);
      g_signal_emit (self, signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0,
          width, height);
      break;
    }
    case GST_PLAY_MESSAGE_MEDIA_INFO_UPDATED:{
      GstPlayMediaInfo *media_info;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_MEDIA_INFO,
          GST_TYPE_PLAY_MEDIA_INFO, &media_info, NULL);
      g_signal_emit (self, signals[SIGNAL_MEDIA_INFO_UPDATED], 0, media_info);
      g_object_unref (media_info);
      break;
    }
    case GST_PLAY_MESSAGE_VOLUME_CHANGED:{
      gdouble volume;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_VOLUME,
          G_TYPE_DOUBLE, &volume, NULL);
      g_signal_emit (self, signals[SIGNAL_VOLUME_CHANGED], 0, volume);
      break;
    }
    case GST_PLAY_MESSAGE_MUTE_CHANGED:{
      gboolean is_muted;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_IS_MUTED,
          G_TYPE_BOOLEAN, &is_muted, NULL);
      g_signal_emit (self, signals[SIGNAL_MUTE_CHANGED], 0, is_muted);
      break;
    }
    case GST_PLAY_MESSAGE_SEEK_DONE:{
      GstClockTime pos;
      gst_structure_get (message_data, GST_PLAY_MESSAGE_DATA_POSITION,
          GST_TYPE_CLOCK_TIME, &pos, NULL);
      g_signal_emit (self, signals[SIGNAL_SEEK_DONE], 0, pos);
      break;
    }
    default:
      g_assert_not_reached ();
      break;
  }
}

/*
 * callback for the bus-message in-sync handling
 */
static GstBusSyncReply
    gst_play_signal_adapter_bus_sync_handler
    (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GstPlaySignalAdapter *self = GST_PLAY_SIGNAL_ADAPTER (user_data);
  const GstStructure *message_data = gst_message_get_structure (message);
  gst_play_signal_adapter_emit (self, message_data);
  gst_message_unref (message);
  return GST_BUS_DROP;
}

/*
 * callback for the bus-watch
 * pre: there is a message on the bus
 */
static gboolean
gst_play_signal_adapter_on_message (GstBus * bus,
    GstMessage * message, gpointer user_data)
{
  GstPlaySignalAdapter *self = GST_PLAY_SIGNAL_ADAPTER (user_data);
  const GstStructure *message_data = gst_message_get_structure (message);
  gst_play_signal_adapter_emit (self, message_data);
  return TRUE;
}

/**
 * gst_play_signal_adapter_new:
 * @play: (transfer none): #GstPlay instance to emit signals for.
 *
 * A bus-watching #GSource will be created and attached to the the
 * thread-default #GMainContext. The attached callback will emit the
 * corresponding signal for the message received. Matching signals for play
 * messages from the bus will be emitted by it on the created adapter object.
 *
 * Returns: (transfer full): A new #GstPlaySignalAdapter to connect signal handlers to.
 *
 * Since: 1.20
 */
GstPlaySignalAdapter *
gst_play_signal_adapter_new (GstPlay * play)
{
  GstPlaySignalAdapter *self = NULL;
  GMainContext *context = NULL;

  g_return_val_if_fail (GST_IS_PLAY (play), NULL);

  self = g_object_new (GST_TYPE_PLAY_SIGNAL_ADAPTER, NULL);
  self->play = play;
  self->bus = gst_play_get_message_bus (play);
  self->source = gst_bus_create_watch (self->bus);

  context = g_main_context_get_thread_default ();
  g_source_attach (self->source, context);
  g_source_set_callback (self->source,
      (GSourceFunc) gst_play_signal_adapter_on_message, self, NULL);
  return self;
}

/**
 * gst_play_signal_adapter_new_with_main_context:
 * @play: (transfer none): #GstPlay instance to emit signals for.
 * @context: A #GMainContext on which the main-loop will process play bus messages on.
 *
 * A bus-watching #GSource will be created and attached to the @context. The
 * attached callback will emit the corresponding signal for the message
 * received. Matching signals for play messages from the bus will be emitted by
 * it on the created adapter object.
 *
 * Returns: (transfer full): A new #GstPlaySignalAdapter to connect signal handlers to.
 *
 * Since: 1.20
 */
GstPlaySignalAdapter *
gst_play_signal_adapter_new_with_main_context (GstPlay * play,
    GMainContext * context)
{
  GstPlaySignalAdapter *self = NULL;

  g_return_val_if_fail (GST_IS_PLAY (play), NULL);
  g_return_val_if_fail (context != NULL, NULL);

  self = g_object_new (GST_TYPE_PLAY_SIGNAL_ADAPTER, NULL);
  self->play = play;
  self->bus = gst_play_get_message_bus (play);
  self->source = gst_bus_create_watch (self->bus);

  g_source_attach (self->source, context);
  g_source_set_callback (self->source,
      (GSourceFunc) gst_play_signal_adapter_on_message, self, NULL);
  return self;
}

/**
 * gst_play_signal_adapter_new_sync_emit:
 * @play: (transfer none): #GstPlay instance to emit signals for.
 *
 * Create an adapter that synchronously emits its signals, from the thread in
 * which the messages have been posted.
 *
 * Returns: (transfer full): A new #GstPlaySignalAdapter to connect signal handlers to.
 *
 * Since: 1.20
 */
GstPlaySignalAdapter *
gst_play_signal_adapter_new_sync_emit (GstPlay * play)
{
  GstBus *bus = NULL;
  GstPlaySignalAdapter *self = NULL;

  g_return_val_if_fail (GST_IS_PLAY (play), NULL);

  bus = gst_play_get_message_bus (play);

  self = g_object_new (GST_TYPE_PLAY_SIGNAL_ADAPTER, NULL);
  self->play = play;
  self->bus = bus;
  gst_bus_set_sync_handler (self->bus,
      gst_play_signal_adapter_bus_sync_handler, self, NULL);
  return self;
}


/**
 * gst_play_signal_adapter_get_play:
 * @adapter: #GstPlaySignalAdapter instance
 *
 * Returns: (transfer none): The #GstPlay owning this signal adapter.
 *
 * Since: 1.20
 */
GstPlay *
gst_play_signal_adapter_get_play (GstPlaySignalAdapter * adapter)
{
  g_return_val_if_fail (GST_IS_PLAY_SIGNAL_ADAPTER (adapter), NULL);
  return adapter->play;
}

static void
gst_play_signal_adapter_init (GstPlaySignalAdapter * self)
{
  self->source = NULL;
}

static void
gst_play_signal_adapter_dispose (GObject * object)
{
  GstPlaySignalAdapter *self = GST_PLAY_SIGNAL_ADAPTER (object);

  if (self->source) {
    g_source_destroy (self->source);
    g_source_unref (self->source);
    self->source = NULL;
  }

  if (self->bus)
    gst_bus_set_flushing (self->bus, TRUE);
  gst_clear_object (&self->bus);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_play_signal_adapter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPlaySignalAdapter *self = GST_PLAY_SIGNAL_ADAPTER (object);

  switch (prop_id) {
    case PROP_PLAY:
      g_value_set_object (value, self->play);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_play_signal_adapter_class_init (GstPlaySignalAdapterClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = gst_play_signal_adapter_dispose;
  gobject_class->get_property = gst_play_signal_adapter_get_property;

  param_specs[PROP_PLAY] =
      g_param_spec_object ("play", "Play",
      "GstPlay owning this adapter",
      GST_TYPE_PLAY, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  signals[SIGNAL_URI_LOADED] =
      g_signal_new ("uri-loaded", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[SIGNAL_POSITION_UPDATED] =
      g_signal_new ("position-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_DURATION_CHANGED] =
      g_signal_new ("duration-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_STATE_CHANGED] =
      g_signal_new ("state-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_PLAY_STATE);

  signals[SIGNAL_BUFFERING] =
      g_signal_new ("buffering", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_END_OF_STREAM] =
      g_signal_new ("end-of-stream", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  /**
   * GstPlaySignalAdapter::error:
   * @adapter: The #GstPlaySignalAdapter
   * @error: The error
   * @details: (nullable): Additional error details
   *
   * Emitted on errors.
   */
  signals[SIGNAL_ERROR] =
      g_signal_new ("error", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_ERROR, GST_TYPE_STRUCTURE);

  signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED] =
      g_signal_new ("video-dimensions-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  signals[SIGNAL_MEDIA_INFO_UPDATED] =
      g_signal_new ("media-info-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_PLAY_MEDIA_INFO);

  signals[SIGNAL_VOLUME_CHANGED] =
      g_signal_new ("volume-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  signals[SIGNAL_MUTE_CHANGED] =
      g_signal_new ("mute-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  /**
   * GstPlaySignalAdapter::warning:
   * @adapter: The #GstPlaySignalAdapter
   * @error: The warning
   * @details: (nullable): Additional warning details
   *
   * Emitted on warnings.
   */
  signals[SIGNAL_WARNING] =
      g_signal_new ("warning", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_ERROR, GST_TYPE_STRUCTURE);

  signals[SIGNAL_SEEK_DONE] =
      g_signal_new ("seek-done", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
