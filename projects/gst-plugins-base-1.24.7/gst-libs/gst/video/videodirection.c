/* GStreamer
 * Copyright (C) 2016 Igalia <calvaris@igalia.com>
 *
 * videodirection.c: video rotation and flipping interface
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

#include "video.h"

#define PROP_DIRECTION_DEFAULT GST_VIDEO_ORIENTATION_IDENTITY

/**
 * SECTION:gstvideodirection
 * @title: GstVideoDirection
 * @short_description: Interface for elements providing video
 * rotation and flipping controls
 *
 * The interface allows unified access to control flipping and rotation
 * operations of video-sources or operators.
 *
 * Since: 1.10
 */

G_DEFINE_INTERFACE (GstVideoDirection, gst_video_direction, 0);

static void
gst_video_direction_default_init (GstVideoDirectionInterface * iface)
{
  g_object_interface_install_property (iface,
      g_param_spec_enum ("video-direction", "Video direction",
          "Video direction: rotation and flipping",
          GST_TYPE_VIDEO_ORIENTATION_METHOD, PROP_DIRECTION_DEFAULT,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));
}
