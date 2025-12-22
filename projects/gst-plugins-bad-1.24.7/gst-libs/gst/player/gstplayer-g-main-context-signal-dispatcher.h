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

#ifndef __GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_H__
#define __GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_H__

#include <gst/player/gstplayer-types.h>
#include <gst/player/gstplayer-signal-dispatcher.h>

G_BEGIN_DECLS

typedef struct _GstPlayerGMainContextSignalDispatcher
    GstPlayerGMainContextSignalDispatcher;
typedef struct _GstPlayerGMainContextSignalDispatcherClass
    GstPlayerGMainContextSignalDispatcherClass;

#define GST_TYPE_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER             (gst_player_g_main_context_signal_dispatcher_get_type ())
#define GST_IS_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER))
#define GST_IS_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER))
#define GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER, GstPlayerGMainContextSignalDispatcherClass))
#define GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER, GstPlayerGMainContextSignalDispatcher))
#define GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER, GstPlayerGMainContextSignalDispatcherClass))
#define GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_CAST(obj)        ((GstPlayerGMainContextSignalDispatcher*)(obj))

GST_PLAYER_API
GType gst_player_g_main_context_signal_dispatcher_get_type (void);

GST_PLAYER_API
GstPlayerSignalDispatcher * gst_player_g_main_context_signal_dispatcher_new (GMainContext * application_context);

G_END_DECLS

#endif /* __GST_PLAYER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_H__ */
