/*
 * GStreamer
 * Copyright (C) 2022 Matthew Waters <matthew@centricular.com>
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

#ifndef _VK_OVERLAY_COMPOSITOR_H_
#define _VK_OVERLAY_COMPOSITOR_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_OVERLAY_COMPOSITOR            (gst_vulkan_overlay_compositor_get_type())
G_DECLARE_FINAL_TYPE(GstVulkanOverlayCompositor, gst_vulkan_overlay_compositor, GST, VULKAN_OVERLAY_COMPOSITOR, GstVulkanVideoFilter);
GST_ELEMENT_REGISTER_DECLARE (vulkanoverlaycompositor);

G_END_DECLS

#endif
