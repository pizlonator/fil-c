/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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

#ifndef _APPLEMEDIA_METAL_HELPERS_H_
#define _APPLEMEDIA_METAL_HELPERS_H_

#include <gst/gst.h>
#include "corevideomemory.h"
#include "videotexturecache.h"

G_BEGIN_DECLS

VkFormat                metal_format_to_vulkan                          (unsigned int fmt);
unsigned int            video_info_to_metal_format                      (GstVideoInfo * info,
                                                                         guint plane);

GstMemory *             _create_vulkan_memory                           (GstAppleCoreVideoPixelBuffer * gpixbuf,
                                                                         GstVideoInfo * info,
                                                                         guint plane,
                                                                         gsize size,
                                                                         GstVideoTextureCache * cache);

void                    gst_io_surface_vulkan_memory_set_surface        (GstIOSurfaceVulkanMemory * memory,
                                                                         IOSurfaceRef surface);

G_END_DECLS
#endif /* _APPLEMEDIA_METAL_HELPERS_H_ */
