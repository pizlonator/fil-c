/* GStreamer
 *
 * Copyright (C) 2019-2020 Stephan Hesse <stephan@emliri.com>
 * Copyright (C) 2020 Thibault Saunier <tsaunier@igalia.com>
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

#include "gsttranscoder.h"
#include "gsttranscoder-signal-adapter.h"

#include "gsttranscoder-private.h"

#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_transcoder_signal_adapter_debug);
#define GST_CAT_DEFAULT gst_transcoder_signal_adapter_debug

enum
{
  SIGNAL_POSITION_UPDATED,
  SIGNAL_DURATION_CHANGED,
  SIGNAL_STATE_CHANGED,
  SIGNAL_DONE,
  SIGNAL_ERROR,
  SIGNAL_WARNING,
  SIGNAL_LAST
};

enum
{
  PROP_0,
  PROP_TRANSCODER,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

struct _GstTranscoderSignalAdapterClass
{
  GObjectClass parent_class;
};

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_transcoder_signal_adapter_debug, "gst-transcoder-signaladapter", \
      0, "GstTranscoder signal adapter")

#define parent_class gst_transcoder_signal_adapter_parent_class
G_DEFINE_TYPE_WITH_CODE (GstTranscoderSignalAdapter,
    gst_transcoder_signal_adapter, G_TYPE_OBJECT, _do_init);

static guint signals[SIGNAL_LAST] = { 0, };

static void
gst_transcoder_signal_adapter_emit (GstTranscoderSignalAdapter * self,
    const GstStructure * message_data)
{
  GstTranscoderMessage transcoder_message_type;
  g_return_if_fail (g_str_equal (gst_structure_get_name (message_data),
          GST_TRANSCODER_MESSAGE_DATA));

  GST_LOG ("Emitting message %" GST_PTR_FORMAT, message_data);
  gst_structure_get (message_data, GST_TRANSCODER_MESSAGE_DATA_TYPE,
      GST_TYPE_TRANSCODER_MESSAGE, &transcoder_message_type, NULL);

  switch (transcoder_message_type) {
    case GST_TRANSCODER_MESSAGE_POSITION_UPDATED:{
      GstClockTime pos = GST_CLOCK_TIME_NONE;
      gst_structure_get (message_data, GST_TRANSCODER_MESSAGE_DATA_POSITION,
          GST_TYPE_CLOCK_TIME, &pos, NULL);
      g_signal_emit (self, signals[SIGNAL_POSITION_UPDATED], 0, pos);
      break;
    }
    case GST_TRANSCODER_MESSAGE_DURATION_CHANGED:{
      GstClockTime duration = GST_CLOCK_TIME_NONE;
      gst_structure_get (message_data, GST_TRANSCODER_MESSAGE_DATA_DURATION,
          GST_TYPE_CLOCK_TIME, &duration, NULL);
      g_signal_emit (self, signals[SIGNAL_DURATION_CHANGED], 0, duration);
      break;
    }
    case GST_TRANSCODER_MESSAGE_STATE_CHANGED:{
      GstTranscoderState state;
      gst_structure_get (message_data, GST_TRANSCODER_MESSAGE_DATA_STATE,
          GST_TYPE_TRANSCODER_STATE, &state, NULL);
      g_signal_emit (self, signals[SIGNAL_STATE_CHANGED], 0, state);
      break;
    }
    case GST_TRANSCODER_MESSAGE_DONE:
      g_signal_emit (self, signals[SIGNAL_DONE], 0);
      break;
    case GST_TRANSCODER_MESSAGE_ERROR:{
      GError *error = NULL;
      GstStructure *details = NULL;

      gst_structure_get (message_data, GST_TRANSCODER_MESSAGE_DATA_ERROR,
          G_TYPE_ERROR, &error, GST_TYPE_STRUCTURE, &details, NULL);
      g_signal_emit (self, signals[SIGNAL_ERROR], 0, error, details);
      g_error_free (error);
      if (details)
        gst_structure_free (details);
      break;
    }
    case GST_TRANSCODER_MESSAGE_WARNING:{
      GstStructure *details = NULL;
      GError *error = NULL;

      gst_structure_get (message_data, GST_TRANSCODER_MESSAGE_DATA_WARNING,
          G_TYPE_ERROR, &error, GST_TYPE_STRUCTURE, &details, NULL);
      g_signal_emit (self, signals[SIGNAL_WARNING], 0, error, details);
      g_error_free (error);
      if (details)
        gst_structure_free (details);
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
    gst_transcoder_signal_adapter_bus_sync_handler
    (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GstTranscoderSignalAdapter *self = GST_TRANSCODER_SIGNAL_ADAPTER (user_data);
  const GstStructure *message_data = gst_message_get_structure (message);
  gst_transcoder_signal_adapter_emit (self, message_data);
  gst_message_unref (message);
  return GST_BUS_DROP;
}

/*
 * callback for the bus-watch
 * pre: there is a message on the bus
 */
static gboolean
gst_transcoder_signal_adapter_on_message (GstBus * bus,
    GstMessage * message, gpointer user_data)
{
  GstTranscoderSignalAdapter *self = GST_TRANSCODER_SIGNAL_ADAPTER (user_data);
  const GstStructure *message_data = gst_message_get_structure (message);
  gst_transcoder_signal_adapter_emit (self, message_data);
  return TRUE;
}

/**
 * gst_transcoder_signal_adapter_new:
 * @transcoder: (transfer none): #GstTranscoder instance to emit signals for.
 * @context: (nullable): A #GMainContext on which the main-loop will process
 *                       transcoder bus messages on. Can be NULL (thread-default
 *                       context will be used then).
 *
 * A bus-watching #GSource will be created and attached to the context. The
 * attached callback will emit the corresponding signal for the message
 * received. Matching signals for transcoder messages from the bus will be
 * emitted by it on the created adapter object.
 *
 * Returns: (transfer full)(nullable): A new #GstTranscoderSignalAdapter to
 * connect signal handlers to.
 *
 * Since: 1.20
 */
GstTranscoderSignalAdapter *
gst_transcoder_signal_adapter_new (GstTranscoder * transcoder,
    GMainContext * context)
{
  GstTranscoderSignalAdapter *self = NULL;

  g_return_val_if_fail (GST_IS_TRANSCODER (transcoder), NULL);

  self = g_object_new (GST_TYPE_TRANSCODER_SIGNAL_ADAPTER, NULL);
  self->bus = gst_transcoder_get_message_bus (transcoder);
  self->source = gst_bus_create_watch (self->bus);

  if (!self->source) {
    GST_ERROR_OBJECT (transcoder, "Could not create watch.");

    gst_object_unref (self);

    return NULL;
  }

  g_weak_ref_set (&self->transcoder, transcoder);
  g_source_attach (self->source, context);
  g_source_set_callback (self->source,
      (GSourceFunc) gst_transcoder_signal_adapter_on_message, self, NULL);
  return self;
}

/**
 * gst_transcoder_signal_adapter_new_sync_emit:
 * @transcoder: (transfer none): #GstTranscoder instance to emit signals
 * synchronously for.
 *
 * Returns: (transfer full): A new #GstTranscoderSignalAdapter to connect signal
 * handlers to.
 *
 * Since: 1.20
 */
GstTranscoderSignalAdapter *
gst_transcoder_signal_adapter_new_sync_emit (GstTranscoder * transcoder)
{
  GstBus *bus = NULL;
  GstTranscoderSignalAdapter *self = NULL;

  g_return_val_if_fail (GST_IS_TRANSCODER (transcoder), NULL);

  bus = gst_transcoder_get_message_bus (transcoder);

  self = g_object_new (GST_TYPE_TRANSCODER_SIGNAL_ADAPTER, NULL);
  self->bus = bus;
  gst_bus_set_sync_handler (self->bus,
      gst_transcoder_signal_adapter_bus_sync_handler, self, NULL);
  return self;
}

static void
gst_transcoder_signal_adapter_init (GstTranscoderSignalAdapter * self)
{
  self->source = NULL;
}

static void
gst_transcoder_signal_adapter_dispose (GObject * object)
{
  GstTranscoderSignalAdapter *self = GST_TRANSCODER_SIGNAL_ADAPTER (object);

  if (self->source) {
    g_source_destroy (self->source);
    g_source_unref (self->source);
    self->source = NULL;
  }

  gst_clear_object (&self->bus);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_transcoder_signal_adapter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTranscoderSignalAdapter *self = GST_TRANSCODER_SIGNAL_ADAPTER (object);

  switch (prop_id) {
    case PROP_TRANSCODER:
      g_value_take_object (value, g_weak_ref_get (&self->transcoder));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_transcoder_signal_adapter_class_init (GstTranscoderSignalAdapterClass *
    klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = gst_transcoder_signal_adapter_dispose;
  gobject_class->get_property = gst_transcoder_signal_adapter_get_property;

  signals[SIGNAL_POSITION_UPDATED] =
      g_signal_new ("position-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_DURATION_CHANGED] =
      g_signal_new ("duration-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_DONE] =
      g_signal_new ("done", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  signals[SIGNAL_ERROR] =
      g_signal_new ("error", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_ERROR, GST_TYPE_STRUCTURE);

  signals[SIGNAL_WARNING] =
      g_signal_new ("warning", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_ERROR, GST_TYPE_STRUCTURE);

  signals[SIGNAL_STATE_CHANGED] =
      g_signal_new ("state-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_TRANSCODER_STATE);

  /**
   * GstTranscoderSignalAdapter:transcoder:
   *
   * The #GstTranscoder tracked by the adapter.
   *
   * Since: 1.20
   */
  param_specs[PROP_TRANSCODER] =
      g_param_spec_object ("transcoder", "Transcoder",
      "The GstTranscoder @self is tracking", GST_TYPE_TRANSCODER,
      G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}


/**
 * gst_transcoder_signal_adapter_get_transcoder:
 * @self: The #GstTranscoderSignalAdapter
 *
 * Returns: (transfer full)(nullable): The #GstTranscoder @self is tracking
 *
 * Since: 1.20
 */
GstTranscoder *
gst_transcoder_signal_adapter_get_transcoder (GstTranscoderSignalAdapter * self)
{
  return g_weak_ref_get (&self->transcoder);
}
