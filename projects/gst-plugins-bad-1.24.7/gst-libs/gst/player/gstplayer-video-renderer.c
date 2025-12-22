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

#include "gstplayer-video-renderer.h"
#include "gstplayer-video-renderer-private.h"

G_DEFINE_INTERFACE (GstPlayerVideoRenderer, gst_player_video_renderer,
    G_TYPE_OBJECT);

static void
gst_player_video_renderer_default_init (G_GNUC_UNUSED
    GstPlayerVideoRendererInterface * iface)
{

}

GstElement *
gst_player_video_renderer_create_video_sink (GstPlayerVideoRenderer * self,
    GstPlayer * player)
{
  GstPlayerVideoRendererInterface *iface;

  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_RENDERER (self), NULL);
  iface = GST_PLAYER_VIDEO_RENDERER_GET_INTERFACE (self);
  g_return_val_if_fail (iface->create_video_sink != NULL, NULL);

  return iface->create_video_sink (self, player);
}
