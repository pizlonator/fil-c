/* GStreamer
 *
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

#include <gst/play/gstplay-video-renderer.h>

#include "gstplayer-wrapped-video-renderer-private.h"
#include "gstplayer.h"
#include "gstplayer-video-renderer-private.h"

/*
 * This object is an internal wrapper created by the GstPlayer, implementing the
 * new GstPlayVideoRenderer interface and acting as a bridge from the legacy
 * GstPlayerVideoRenderer interface.
 */

struct _GstPlayerWrappedVideoRenderer
{
  GObject parent;

  GstPlayerVideoRenderer *renderer;
  GstPlayer *player;
};

struct _GstPlayerWrappedVideoRendererClass
{
  GObjectClass parent_class;
};

static void
    gst_player_wrapped_video_renderer_interface_init
    (GstPlayVideoRendererInterface * iface);

G_DEFINE_TYPE_WITH_CODE (GstPlayerWrappedVideoRenderer,
    gst_player_wrapped_video_renderer, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PLAY_VIDEO_RENDERER,
        gst_player_wrapped_video_renderer_interface_init));

static void
gst_player_wrapped_video_renderer_finalize (GObject * object)
{
  GstPlayerWrappedVideoRenderer *self =
      GST_PLAYER_WRAPPED_VIDEO_RENDERER (object);

  g_clear_object (&self->renderer);

  G_OBJECT_CLASS
      (gst_player_wrapped_video_renderer_parent_class)->finalize (object);
}

static void
gst_player_wrapped_video_renderer_class_init (GstPlayerWrappedVideoRendererClass
    * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_player_wrapped_video_renderer_finalize;
}

static void
gst_player_wrapped_video_renderer_init (GstPlayerWrappedVideoRenderer * self)
{
}

static GstElement *
gst_player_wrapped_video_renderer_create_video_sink (GstPlayVideoRenderer *
    iface, GstPlay * player)
{
  GstPlayerWrappedVideoRenderer *self =
      GST_PLAYER_WRAPPED_VIDEO_RENDERER (iface);

  return gst_player_video_renderer_create_video_sink (self->renderer,
      self->player);
}

static void
gst_player_wrapped_video_renderer_interface_init (GstPlayVideoRendererInterface
    * iface)
{
  iface->create_video_sink =
      gst_player_wrapped_video_renderer_create_video_sink;
}

GstPlayerVideoRenderer *
gst_player_wrapped_video_renderer_new (GstPlayerVideoRenderer * renderer,
    GstPlayer * player)
{
  GstPlayerWrappedVideoRenderer *self =
      g_object_new (GST_TYPE_PLAYER_WRAPPED_VIDEO_RENDERER,
      NULL);
  self->renderer = g_object_ref (renderer);
  self->player = player;
  return (GstPlayerVideoRenderer *) self;
}
