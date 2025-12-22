/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
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
 * SECTION:gstplayer-gmaincontextsignaldispatcher
 * @title: GstPlayerGMainContextSignalDispatcher
 * @short_description: Player GLib MainContext dispatcher
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstplayer-g-main-context-signal-dispatcher.h"

struct _GstPlayerGMainContextSignalDispatcher
{
  GObject parent;
  GMainContext *application_context;
};

struct _GstPlayerGMainContextSignalDispatcherClass
{
  GObjectClass parent_class;
};

static void
    gst_player_g_main_context_signal_dispatcher_interface_init
    (GstPlayerSignalDispatcherInterface * iface);

enum
{
  G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_0,
  G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT,
  G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_LAST
};

G_DEFINE_TYPE_WITH_CODE (GstPlayerGMainContextSignalDispatcher,
    gst_player_g_main_context_signal_dispatcher, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PLAYER_SIGNAL_DISPATCHER,
        gst_player_g_main_context_signal_dispatcher_interface_init));

static GParamSpec
    * g_main_context_signal_dispatcher_param_specs
    [G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_LAST] = { NULL, };

static void
gst_player_g_main_context_signal_dispatcher_finalize (GObject * object)
{
  GstPlayerGMainContextSignalDispatcher *self =
      GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (object);

  if (self->application_context)
    g_main_context_unref (self->application_context);

  G_OBJECT_CLASS
      (gst_player_g_main_context_signal_dispatcher_parent_class)->finalize
      (object);
}

static void
gst_player_g_main_context_signal_dispatcher_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstPlayerGMainContextSignalDispatcher *self =
      GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (object);

  switch (prop_id) {
    case G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT:
      self->application_context = g_value_dup_boxed (value);
      if (!self->application_context)
        self->application_context = g_main_context_ref_thread_default ();
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_player_g_main_context_signal_dispatcher_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstPlayerGMainContextSignalDispatcher *self =
      GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (object);

  switch (prop_id) {
    case G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT:
      g_value_set_boxed (value, self->application_context);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
    gst_player_g_main_context_signal_dispatcher_class_init
    (GstPlayerGMainContextSignalDispatcherClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize =
      gst_player_g_main_context_signal_dispatcher_finalize;
  gobject_class->set_property =
      gst_player_g_main_context_signal_dispatcher_set_property;
  gobject_class->get_property =
      gst_player_g_main_context_signal_dispatcher_get_property;

  g_main_context_signal_dispatcher_param_specs
      [G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT] =
      g_param_spec_boxed ("application-context", "Application Context",
      "Application GMainContext to dispatch signals to", G_TYPE_MAIN_CONTEXT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
      G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_LAST,
      g_main_context_signal_dispatcher_param_specs);
}

static void
    gst_player_g_main_context_signal_dispatcher_init
    (G_GNUC_UNUSED GstPlayerGMainContextSignalDispatcher * self)
{
}

typedef struct
{
  void (*emitter) (gpointer data);
  gpointer data;
  GDestroyNotify destroy;
} GMainContextSignalDispatcherData;

static gboolean
g_main_context_signal_dispatcher_dispatch_gsourcefunc (gpointer user_data)
{
  GMainContextSignalDispatcherData *data = user_data;

  data->emitter (data->data);

  return G_SOURCE_REMOVE;
}

static void
g_main_context_signal_dispatcher_dispatch_destroy (gpointer user_data)
{
  GMainContextSignalDispatcherData *data = user_data;

  if (data->destroy)
    data->destroy (data->data);
  g_free (data);
}

static void
gst_player_g_main_context_signal_dispatcher_dispatch (GstPlayerSignalDispatcher
    * iface, G_GNUC_UNUSED GstPlayer * player, void (*emitter) (gpointer data),
    gpointer data, GDestroyNotify destroy)
{
  GstPlayerGMainContextSignalDispatcher *self =
      GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (iface);
  GMainContextSignalDispatcherData *gsourcefunc_data =
      g_new (GMainContextSignalDispatcherData, 1);

  gsourcefunc_data->emitter = emitter;
  gsourcefunc_data->data = data;
  gsourcefunc_data->destroy = destroy;

  g_main_context_invoke_full (self->application_context,
      G_PRIORITY_DEFAULT, g_main_context_signal_dispatcher_dispatch_gsourcefunc,
      gsourcefunc_data, g_main_context_signal_dispatcher_dispatch_destroy);
}

static void
    gst_player_g_main_context_signal_dispatcher_interface_init
    (GstPlayerSignalDispatcherInterface * iface)
{
  iface->dispatch = gst_player_g_main_context_signal_dispatcher_dispatch;
}

/**
 * gst_player_g_main_context_signal_dispatcher_new:
 * @application_context: (allow-none): GMainContext to use or %NULL
 *
 * Creates a new GstPlayerSignalDispatcher that uses @application_context,
 * or the thread default one if %NULL is used. See gst_player_new().
 *
 * Returns: (transfer full): the new GstPlayerSignalDispatcher
 */
GstPlayerSignalDispatcher *
gst_player_g_main_context_signal_dispatcher_new (GMainContext *
    application_context)
{
  return g_object_new (GST_TYPE_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER,
      "application-context", application_context, NULL);
}
