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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstplayer-signal-dispatcher.h"
#include "gstplayer-signal-dispatcher-private.h"

G_DEFINE_INTERFACE (GstPlayerSignalDispatcher, gst_player_signal_dispatcher,
    G_TYPE_OBJECT);

static void
gst_player_signal_dispatcher_default_init (G_GNUC_UNUSED
    GstPlayerSignalDispatcherInterface * iface)
{

}

void
gst_player_signal_dispatcher_dispatch (GstPlayerSignalDispatcher * self,
    GstPlayer * player, GstPlayerSignalDispatcherFunc emitter, gpointer data,
    GDestroyNotify destroy)
{
  GstPlayerSignalDispatcherInterface *iface;

  if (!self) {
    emitter (data);
    if (destroy)
      destroy (data);
    return;
  }

  g_return_if_fail (GST_IS_PLAYER_SIGNAL_DISPATCHER (self));
  iface = GST_PLAYER_SIGNAL_DISPATCHER_GET_INTERFACE (self);
  g_return_if_fail (iface->dispatch != NULL);

  iface->dispatch (self, player, emitter, data, destroy);
}
