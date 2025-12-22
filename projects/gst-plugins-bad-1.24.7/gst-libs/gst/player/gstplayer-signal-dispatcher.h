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

#ifndef __GST_PLAYER_SIGNAL_DISPATCHER_H__
#define __GST_PLAYER_SIGNAL_DISPATCHER_H__

#include <gst/gst.h>
#include <gst/player/gstplayer-types.h>

G_BEGIN_DECLS

typedef struct _GstPlayerSignalDispatcher GstPlayerSignalDispatcher;
typedef struct _GstPlayerSignalDispatcherInterface GstPlayerSignalDispatcherInterface;

#define GST_TYPE_PLAYER_SIGNAL_DISPATCHER                (gst_player_signal_dispatcher_get_type ())
#define GST_PLAYER_SIGNAL_DISPATCHER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER_SIGNAL_DISPATCHER, GstPlayerSignalDispatcher))
#define GST_IS_PLAYER_SIGNAL_DISPATCHER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER_SIGNAL_DISPATCHER))
#define GST_PLAYER_SIGNAL_DISPATCHER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_PLAYER_SIGNAL_DISPATCHER, GstPlayerSignalDispatcherInterface))

typedef void (*GstPlayerSignalDispatcherFunc) (gpointer data);

struct _GstPlayerSignalDispatcherInterface {
  GTypeInterface parent_iface;

  void (*dispatch) (GstPlayerSignalDispatcher * self,
                    GstPlayer * player,
                    GstPlayerSignalDispatcherFunc emitter,
                    gpointer data,
                    GDestroyNotify destroy);
};

GST_PLAYER_API
GType        gst_player_signal_dispatcher_get_type    (void);

G_END_DECLS

#endif /* __GST_PLAYER_SIGNAL_DISPATCHER_H__ */
