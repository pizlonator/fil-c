/*
 * GStreamer
 * Copyright (C) 2023 Matthew Waters <matthew@centricular.com>
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

#include <gst/gst.h>
#include "gstvkdisplay_wayland.h"
#include <wayland-client.h>
#include "gst/vulkan/xdg-shell-client-protocol.h"

G_BEGIN_DECLS

typedef struct _GstVulkanDisplayWaylandPrivate GstVulkanDisplayWaylandPrivate;

struct _GstVulkanDisplayWaylandPrivate
{
  struct xdg_wm_base     *xdg_wm_base;
};

G_GNUC_INTERNAL
GstVulkanDisplayWaylandPrivate *
gst_vulkan_display_wayland_get_private (GstVulkanDisplayWayland * display_wayland);

G_END_DECLS
